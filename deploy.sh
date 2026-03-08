#!/usr/bin/env bash
# =============================================================================
#  deploy.sh — Compile et téléverse un sketch Arduino Nano ESP32
#  Usage :
#    ./deploy.sh <projet>                  # compile + upload (port auto)
#    ./deploy.sh <projet> /dev/ttyACM0    # port explicite
#    ./deploy.sh <projet> --no-upload     # compile sans téléverser
#
#  <projet> est le nom d'un sous-dossier contenant <projet>.ino et sketch.cfg
# =============================================================================

set -euo pipefail

REPO_URL="https://github.com/tdesve2024/nano32"

CORE_PLATFORM="arduino:esp32"
CORE_INDEX="https://downloads.arduino.cc/packages/package_esp32_index.json"

# ── Couleurs terminal ──────────────────────────────────────────────────────────
GRN='\033[0;32m'; YLW='\033[0;33m'; RED='\033[0;31m'; BLU='\033[0;34m'; NC='\033[0m'
ok()  { echo -e "${GRN}✓ $*${NC}"; }
inf() { echo -e "${BLU}▸ $*${NC}"; }
wrn() { echo -e "${YLW}⚠ $*${NC}"; }
die() { echo -e "${RED}✗ $*${NC}" >&2; exit 1; }

# ── Sélection interactive de la branche ───────────────────────────────────────
pick_branch() {
  local default_branch
  default_branch=$(git -C "$(dirname "$0")" rev-parse --abbrev-ref HEAD 2>/dev/null || echo "main")

  inf "Récupération des branches disponibles sur GitHub..." >&2
  local branches
  branches=$(git ls-remote --heads "$REPO_URL" 2>/dev/null | sed 's|.*refs/heads/||' | sort)

  if [ -z "$branches" ]; then
    wrn "Impossible de lister les branches — utilisation de '${default_branch}'" >&2
    echo "$default_branch"
    return
  fi

  echo "" >&2
  echo "  Branches disponibles :" >&2
  local i=1 default_idx=1
  local -a branch_array
  while IFS= read -r b; do
    branch_array+=("$b")
    if [ "$b" = "$default_branch" ]; then
      default_idx=$i
      echo -e "    ${GRN}[$i]${NC} $b  ${YLW}← défaut${NC}" >&2
    else
      echo "    [$i] $b" >&2
    fi
    (( i++ ))
  done <<< "$branches"

  echo "" >&2
  if [ ! -t 0 ]; then
    echo "$default_branch"
    return
  fi

  while true; do
    printf "  Choisissez une branche [%s] : " "$default_idx" >&2
    local choice
    read -r choice
    choice=${choice:-$default_idx}

    if [[ "$choice" =~ ^[0-9]+$ ]] && (( choice >= 1 && choice <= ${#branch_array[@]} )); then
      echo "${branch_array[$((choice - 1))]}"
      return
    fi
    echo -e "  ${RED}Choix invalide — entrez un numéro entre 1 et ${#branch_array[@]}${NC}" >&2
  done
}

# ── Arguments ─────────────────────────────────────────────────────────────────
SKETCH_NAME=""
PORT_ARG=""
UPLOAD=true

for arg in "$@"; do
  case "$arg" in
    --no-upload) UPLOAD=false ;;
    /dev/*)      PORT_ARG="$arg" ;;
    -*)          die "Option inconnue : $arg" ;;
    *)           [ -z "$SKETCH_NAME" ] && SKETCH_NAME="$arg" || die "Argument inattendu : $arg" ;;
  esac
done

[ -n "$SKETCH_NAME" ] || die "Usage : $0 <projet> [/dev/ttyACM0] [--no-upload]"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SKETCH_CFG="$SCRIPT_DIR/$SKETCH_NAME/sketch.cfg"
[ -f "$SKETCH_CFG" ] || die "Projet introuvable : '$SKETCH_NAME' (pas de $SKETCH_CFG)"

# Valeurs par défaut surchargées par sketch.cfg
FQBN="arduino:esp32:nano_nora"
LIBRARIES=()
# shellcheck source=/dev/null
source "$SKETCH_CFG"

BUILD_DIR="${TMPDIR:-/tmp}/nano32_${SKETCH_NAME}"

echo ""
echo "════════════════════════════════════════"
echo "  nano32 / ${SKETCH_NAME} — Deploy script"
echo "════════════════════════════════════════"
echo ""

# ── Vérification prérequis ────────────────────────────────────────────────────
inf "Vérification des prérequis..."
command -v arduino-cli >/dev/null 2>&1 || die "arduino-cli introuvable — https://arduino.github.io/arduino-cli/latest/installation/"
command -v git        >/dev/null 2>&1 || die "git introuvable"
ok "arduino-cli $(arduino-cli version | head -1 | awk '{print $3}')"

# ── Sélection de la branche ───────────────────────────────────────────────────
BRANCH=$(pick_branch)
ok "Branche sélectionnée : ${BRANCH}"
echo ""

# ── Core ESP32 ────────────────────────────────────────────────────────────────
inf "Vérification du core arduino:esp32..."
if ! arduino-cli core list 2>/dev/null | grep -q "arduino:esp32"; then
  inf "Installation du core arduino:esp32 (peut prendre quelques minutes)..."
  arduino-cli core update-index --additional-urls "$CORE_INDEX"
  arduino-cli core install "$CORE_PLATFORM" --additional-urls "$CORE_INDEX"
  ok "Core installé"
else
  ok "Core arduino:esp32 déjà présent"
fi

# ── Bibliothèques ─────────────────────────────────────────────────────────────
if [ ${#LIBRARIES[@]} -gt 0 ]; then
  inf "Vérification des bibliothèques..."
  for lib in "${LIBRARIES[@]}"; do
    if arduino-cli lib list 2>/dev/null | grep -qi "^${lib}"; then
      ok "  $lib"
    else
      inf "  Installation de ${lib}..."
      arduino-cli lib install "$lib"
      ok "  $lib installé"
    fi
  done
fi

# ── Récupération du code ──────────────────────────────────────────────────────
inf "Récupération du code depuis GitHub..."
if [ -d "$BUILD_DIR/.git" ]; then
  inf "Mise à jour du dépôt existant..."
  git -C "$BUILD_DIR" fetch origin
  git -C "$BUILD_DIR" checkout "$BRANCH"
  git -C "$BUILD_DIR" reset --hard "origin/$BRANCH"
else
  rm -rf "$BUILD_DIR"
  git clone --depth 1 --branch "$BRANCH" "$REPO_URL" "$BUILD_DIR"
fi

SKETCH_PATH="$BUILD_DIR/$SKETCH_NAME/$SKETCH_NAME.ino"
[ -f "$SKETCH_PATH" ] || die "Sketch introuvable : $SKETCH_PATH"

VERSION=$(grep 'FW_VERSION' "$SKETCH_PATH" | grep -o '"[0-9.]*"' | tr -d '"')
ok "Sketch v${VERSION} récupéré → $SKETCH_PATH"

# ── Compilation ───────────────────────────────────────────────────────────────
COMPILE_OUT="$BUILD_DIR/$SKETCH_NAME/build"
inf "Compilation pour ${FQBN}..."
arduino-cli compile \
  --fqbn "$FQBN" \
  --output-dir "$COMPILE_OUT" \
  --warnings default \
  "$BUILD_DIR/$SKETCH_NAME" \
  2>&1 | grep -v "^$" | while IFS= read -r line; do
    if echo "$line" | grep -q "error:";                        then echo -e "${RED}$line${NC}"
    elif echo "$line" | grep -q "warning:";                    then echo -e "${YLW}$line${NC}"
    elif echo "$line" | grep -q "Sketch uses\|Global variables"; then echo -e "${GRN}$line${NC}"
    else echo "$line"
    fi
  done

FIRMWARE="$COMPILE_OUT/${SKETCH_NAME}.ino.bin"
[ -f "$FIRMWARE" ] || FIRMWARE="$(find "$COMPILE_OUT" -name '*.bin' | head -1)"
[ -f "$FIRMWARE" ] || die "Firmware .bin introuvable après compilation"
SIZE=$(du -h "$FIRMWARE" | cut -f1)
ok "Compilation réussie — firmware : $SIZE"

# ── Upload ────────────────────────────────────────────────────────────────────
if [ "$UPLOAD" = false ]; then
  wrn "Upload ignoré (--no-upload)"
  exit 0
fi

if [ -z "$PORT_ARG" ]; then
  inf "Détection automatique du port série..."
  for pattern in "/dev/cu.usbmodem*" "/dev/ttyACM*" "/dev/ttyUSB*" "/dev/cu.SLAB_*"; do
    # shellcheck disable=SC2086
    PORT_FOUND=$(ls $pattern 2>/dev/null | head -1 || true)
    if [ -n "$PORT_FOUND" ]; then
      PORT_ARG="$PORT_FOUND"
      break
    fi
  done

  if [ -z "$PORT_ARG" ]; then
    wrn "Aucun port détecté automatiquement — essai avec arduino-cli board list..."
    PORT_ARG=$(arduino-cli board list 2>/dev/null \
      | grep -i "nano.*esp32\|esp32.*nano\|arduino" \
      | awk '{print $1}' | head -1 || true)
  fi

  [ -n "$PORT_ARG" ] || die "Port série introuvable. Précisez-le : $0 $SKETCH_NAME /dev/ttyACM0"
fi

ok "Port : $PORT_ARG"
inf "Téléversement du firmware v${VERSION}..."

arduino-cli upload \
  --fqbn "$FQBN" \
  --port "$PORT_ARG" \
  --input-dir "$COMPILE_OUT" \
  "$BUILD_DIR/$SKETCH_NAME"

echo ""
ok "Téléversement terminé ! ${SKETCH_NAME} v${VERSION} opérationnel."
echo ""
