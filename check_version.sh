#!/bin/bash
# Script de vérification de la version du core ESP32 pour Arduino Nano ESP32

BOARD_FQBN="arduino:esp32:nano_nora"
PLATFORM="arduino:esp32"

echo "=== Vérification de la version ESP32 pour Arduino Nano ESP32 ==="
echo ""

# Vérifier si arduino-cli est disponible
if ! command -v arduino-cli &> /dev/null; then
    echo "[ERREUR] arduino-cli n'est pas installé."
    echo ""
    echo "Installation (Linux):"
    echo "  curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh"
    echo "  sudo mv bin/arduino-cli /usr/local/bin/"
    echo ""
    exit 1
fi

echo "arduino-cli version: $(arduino-cli version)"
echo ""

# Initialiser la configuration si nécessaire
arduino-cli config init --overwrite > /dev/null 2>&1

# Ajouter le gestionnaire de paquets ESP32 Arduino
arduino-cli config add board_manager.additional_urls \
    https://raw.githubusercontent.com/arduino/arduino-esp32/gh-pages/package_esp32_index.json \
    > /dev/null 2>&1

# Mettre à jour l'index des plateformes
echo "Mise à jour de l'index des plateformes..."
arduino-cli core update-index

echo ""
echo "Version installée du core ESP32:"
arduino-cli core list | grep "esp32" || echo "  (aucune version installée)"

echo ""
echo "Versions disponibles du core ESP32:"
arduino-cli core search esp32 | grep "arduino:esp32"

echo ""
echo "Board FQBN: $BOARD_FQBN"
echo ""

# Installer le core si pas encore installé
if ! arduino-cli core list | grep -q "arduino:esp32"; then
    echo "Installation du core arduino:esp32..."
    arduino-cli core install arduino:esp32
fi
