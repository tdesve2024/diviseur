#!/usr/bin/env bash
# =============================================================================
#  deploy.sh — Télécharge, compile et téléverse le diviseur RGB61
#  Usage :
#    ./deploy.sh               # compile seulement (détection port auto)
#    ./deploy.sh /dev/ttyACM0  # port explicite
#    ./deploy.sh --no-upload   # compile sans téléverser
# =============================================================================

set -euo pipefail

# ── Configuration ─────────────────────────────────────────────────────────────
REPO_URL="https://github.com/tdesve2024/diviseur"   # ← adapter si besoin

SKETCH_NAME="diviseur"
BUILD_DIR="${TMPDIR:-/tmp}/diviseur"
FQBN="arduino:esp32:nano_nora"      # Arduino Nano ESP32 (nora)

CORE_PLATFORM="arduino:esp32"
CORE_INDEX="https://downloads.arduino.cc/packages/package_esp32_index.json"

LIBRARIES=(
  "WiFiManager"   # tzapu
  "TMCStepper"    # teemuatlut
  "AccelStepper"  # Mike McCaulay
)

# ── Couleurs terminal ──────────────────────────────────────────────────────────
GRN='\033[0;32m'; YLW='\033[0;33m'; RED='\033[0;31m'; BLU='\033[0;34m'; NC='\033[0m'
ok()  { echo -e "${GRN}✓ $*${NC}"; }
inf() { echo -e "${BLU}▸ $*${NC}"; }
wrn() { echo -e "${YLW}⚠ $*${NC}"; }
die() { echo -e "${RED}✗ $*${NC}" >&2; exit 1; }

# ── Sélection interactive de la branche ───────────────────────────────────────
pick_branch() {
  # Branche courante du dépôt local où tourne ce script (défaut si interactif)
  local default_branch
  default_branch=$(git -C "$(dirname "$0")" rev-parse --abbrev-ref HEAD 2>/dev/null || echo "main")

  inf "Récupération des branches disponibles sur GitHub..."
  local branches
  branches=$(git ls-remote --heads "$REPO_URL" 2>/dev/null | sed 's|.*refs/heads/||' | sort)

  if [ -z "$branches" ]; then
    wrn "Impossible de lister les branches — utilisation de '${default_branch}'"
    echo "$default_branch"
    return
  fi

  echo ""
  echo "  Branches disponibles :"
  local i=1 default_idx=1
  local -a branch_array
  while IFS= read -r b; do
    branch_array+=("$b")
    if [ "$b" = "$default_branch" ]; then
      default_idx=$i
      echo -e "    ${GRN}[$i]${NC} $b  ${YLW}← défaut${NC}"
    else
      echo "    [$i] $b"
    fi
    (( i++ ))
  done <<< "$branches"

  echo ""
  # Si non interactif (CI / pipe), utiliser le défaut sans prompt
  if [ ! -t 0 ]; then
    echo "$default_branch"
    return
  fi

  printf "  Choisissez une branche [%s] : " "$default_idx"
  local choice
  read -r choice
  choice=${choice:-$default_idx}

  if [[ "$choice" =~ ^[0-9]+$ ]] && (( choice >= 1 && choice <= ${#branch_array[@]} )); then
    echo "${branch_array[$((choice - 1))]}"
  else
    wrn "Choix invalide — utilisation de '${default_branch}'"
    echo "$default_branch"
  fi
}

# ── Arguments ─────────────────────────────────────────────────────────────────
PORT_ARG=""
UPLOAD=true
for arg in "$@"; do
  case "$arg" in
    --no-upload) UPLOAD=false ;;
    /dev/*)      PORT_ARG="$arg" ;;
    *) die "Argument inconnu : $arg" ;;
  esac
done

echo ""
echo "════════════════════════════════════════"
echo "  Diviseur Cowells RGB61 — Deploy script"
echo "════════════════════════════════════════"
echo ""

# ── Vérification prérequis ─────────────────────────────────────────────────────
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

# ── Récupération du code ───────────────────────────────────────────────────────
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

SKETCH_PATH="$BUILD_DIR/$SKETCH_NAME.ino"
[ -f "$SKETCH_PATH" ] || die "Sketch introuvable : $SKETCH_PATH"

VERSION=$(grep 'FW_VERSION' "$SKETCH_PATH" | grep -o '"[0-9.]*"' | tr -d '"')
ok "Sketch v${VERSION} récupéré → $BUILD_DIR"

# ── Compilation ────────────────────────────────────────────────────────────────
COMPILE_OUT="$BUILD_DIR/build"
inf "Compilation pour ${FQBN}..."
arduino-cli compile \
  --fqbn "$FQBN" \
  --output-dir "$COMPILE_OUT" \
  --warnings default \
  "$BUILD_DIR" \
  2>&1 | grep -v "^$" | while IFS= read -r line; do
    # Colorer les warnings et erreurs
    if echo "$line" | grep -q "error:";   then echo -e "${RED}$line${NC}"
    elif echo "$line" | grep -q "warning:"; then echo -e "${YLW}$line${NC}"
    elif echo "$line" | grep -q "Sketch uses\|Global variables"; then echo -e "${GRN}$line${NC}"
    else echo "$line"
    fi
  done

FIRMWARE="$COMPILE_OUT/${SKETCH_NAME}.ino.bin"
[ -f "$FIRMWARE" ] || FIRMWARE="$(find "$COMPILE_OUT" -name '*.bin' | head -1)"
[ -f "$FIRMWARE" ] || die "Firmware .bin introuvable après compilation"
SIZE=$(du -h "$FIRMWARE" | cut -f1)
ok "Compilation réussie — firmware : $SIZE ($FIRMWARE)"

# ── Upload ────────────────────────────────────────────────────────────────────
if [ "$UPLOAD" = false ]; then
  wrn "Upload ignoré (--no-upload)"
  exit 0
fi

# Détection automatique du port si non fourni
if [ -z "$PORT_ARG" ]; then
  inf "Détection automatique du port série..."
  # Tenter plusieurs patterns typiques Arduino Nano ESP32
  for pattern in \
    "/dev/cu.usbmodem*" \
    "/dev/ttyACM*" \
    "/dev/ttyUSB*" \
    "/dev/cu.SLAB_*"
  do
    # shellcheck disable=SC2086
    PORT_FOUND=$(ls $pattern 2>/dev/null | head -1 || true)
    if [ -n "$PORT_FOUND" ]; then
      PORT_ARG="$PORT_FOUND"
      break
    fi
  done

  # Fallback : laisser arduino-cli trouver
  if [ -z "$PORT_ARG" ]; then
    wrn "Aucun port détecté automatiquement — essai avec arduino-cli board list..."
    PORT_ARG=$(arduino-cli board list 2>/dev/null \
      | grep -i "nano.*esp32\|esp32.*nano\|arduino" \
      | awk '{print $1}' | head -1 || true)
  fi

  [ -n "$PORT_ARG" ] || die "Port série introuvable. Précisez-le : $0 /dev/ttyACM0"
fi

ok "Port : $PORT_ARG"
inf "Téléversement du firmware v${VERSION}..."

arduino-cli upload \
  --fqbn "$FQBN" \
  --port "$PORT_ARG" \
  --input-dir "$COMPILE_OUT" \
  "$BUILD_DIR"

echo ""
ok "Téléversement terminé ! Diviseur RGB61 v${VERSION} opérationnel."
echo ""
