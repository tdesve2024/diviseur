# Diviseur Cowells RGB61 — Arduino Nano ESP32

## Configuration requise

| Composant | Version |
|-----------|---------|
| **Board** | Arduino Nano ESP32 |
| **FQBN** | `arduino:esp32:nano_nora` |
| **Core Arduino ESP32** | `arduino:esp32` 2.0.18-arduino.5 |
| **arduino-cli** | 1.4.1 |

## Librairies à installer

```bash
arduino-cli lib install "TMCStepper"
arduino-cli lib install "AccelStepper"
arduino-cli lib install "WiFiManager"
```

Ou via l'IDE : Outils → Gérer les bibliothèques → chercher `TMCStepper`, `AccelStepper`, `WiFiManager`.

## Câblage TMC2209 → Arduino Nano ESP32

```
Arduino Nano ESP32        TMC2209
─────────────────         ───────
D2  ──────────────────── STEP
D3  ──────────────────── DIR
D4  ──────────────────── EN
D5  ──── [1 kΩ] ──────── PDN_UART  (même broche RX et TX — half-duplex)
GND ──────────────────── MS1
GND ──────────────────── MS2
GND ──────────────────── GND
5V / VCC ─────────────── VIO (logique)
[alimentation séparée] ── VM (moteur, 12–24 V)
```

> **MS1=GND, MS2=GND** → adresse UART 0, microstepping configuré via UART (16×).
>
> La broche D5 est utilisée en half-duplex (RX et TX sur le même GPIO, via la résistance 1 kΩ),
> ce qui permet la lecture des registres TMC2209 (version, statut thermique, StallGuard).

## Paramètres mécaniques

| Paramètre | Valeur |
|-----------|--------|
| Moteur | NEMA 14, 600 mA RMS |
| Pas/tour moteur | 200 |
| Microstepping | 16× |
| Rapport diviseur | 40:1 |
| **Pas/tour diviseur** | **128 000** |
| Courant RMS | 600 mA |

## Configuration WiFi

Au premier démarrage (ou si le réseau configuré est introuvable), l'ESP32 crée un point d'accès Wi-Fi :

- **SSID** : `Diviseur-Setup`
- Se connecter avec un smartphone ou PC, puis entrer les identifiants du réseau cible
- L'adresse IP est affichée dans le moniteur série après connexion

Les identifiants sont mémorisés dans la flash — aucune recompilation nécessaire.

## Interface web

| URL | Description |
|-----|-------------|
| `http://<ip>/` | Page principale : divisions, navigation, position |
| `http://<ip>/diag` | Page diagnostic : tests par étape, état driver |

## API REST

| Méthode | Endpoint | Corps JSON | Description |
|---------|----------|------------|-------------|
| GET | `/api/status` | — | État JSON complet |
| POST | `/api/divisions` | `{"n":6}` | Définir le nombre de divisions |
| POST | `/api/move` | `{"dir":1}` ou `{"dir":-1}` | Avancer / reculer d'une division |
| POST | `/api/home` | — | Déclarer la position courante comme zéro |
| POST | `/api/enable` | `{"enable":true}` | Activer / désactiver le moteur |
| POST | `/api/stop` | — | Arrêt immédiat du moteur |
| POST | `/api/jog` | `{"steps":100}` | Jog de N pas |
| POST | `/api/mode` | `{"spreadCycle":true}` | Basculer StealthChop ↔ SpreadCycle |
| GET | `/api/diag` | — | État JSON de tous les tests diagnostic |
| POST | `/api/diag/run` | `{"step":1}` | Lancer les tests d'une étape (1–4) |

## Compilation et upload

```bash
# Vérifier la version du core
./check_version.sh

# Compiler
arduino-cli compile --fqbn arduino:esp32:nano_nora diviseur.ino

# Upload
arduino-cli upload -p /dev/cu.usbmodem* --fqbn arduino:esp32:nano_nora diviseur.ino
```
