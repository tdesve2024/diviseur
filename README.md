# Diviseur - Arduino Nano ESP32

## Configuration requise

| Composant | Version |
|-----------|---------|
| **Board** | Arduino Nano ESP32 |
| **FQBN** | `arduino:esp32:nano_nora` |
| **Core Arduino ESP32** | `arduino:esp32` ≥ 3.x |
| **arduino-cli** | 1.x recommandé |

## Vérifier la version du core ESP32

```bash
./check_version.sh
```

Ou manuellement avec arduino-cli :

```bash
# Lister les cores installés
arduino-cli core list

# Rechercher les versions disponibles
arduino-cli core search esp32

# Installer/mettre à jour le core
arduino-cli core install arduino:esp32
```

## Compilation

```bash
arduino-cli compile --fqbn arduino:esp32:nano_nora diviseur.ino
```

## Upload

```bash
arduino-cli upload -p /dev/ttyUSB0 --fqbn arduino:esp32:nano_nora diviseur.ino
```
