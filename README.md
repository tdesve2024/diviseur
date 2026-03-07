# Diviseur Cowells RGB61 — Guide complet

Diviseur rotatif motorisé piloté depuis un smartphone via Wi-Fi.
Rapport 40:1 · NEMA 14 · TMC2209 · Arduino Nano ESP32.

---

## Table des matières

1. [Architecture matérielle](#1-architecture-matérielle)
2. [Architecture logicielle](#2-architecture-logicielle)
3. [Matériel nécessaire](#3-matériel-nécessaire)
4. [Montage pas à pas](#4-montage-pas-à-pas)
   - [Étape 1 — Arduino + USB](#étape-1--arduino--usb)
   - [Étape 2 — Alimentation 12 V + Buck](#étape-2--alimentation-12-v--buck)
   - [Étape 3 — Driver TMC2209](#étape-3--driver-tmc2209-sans-moteur)
   - [Étape 4 — Moteur NEMA 14](#étape-4--moteur-nema-14)
5. [Installation du firmware](#5-installation-du-firmware)
6. [Configuration Wi-Fi](#6-configuration-wi-fi)
7. [Interface web](#7-interface-web)
8. [API REST](#8-api-rest)
9. [Paramètres et calibration](#9-paramètres-et-calibration)
10. [Dépannage](#10-dépannage)

---

## 1. Architecture matérielle

### Vue d'ensemble

```
┌─────────────────────────────────────────────────────┐
│                  Alimentation                        │
│                                                      │
│  Secteur ──► [Bloc 12 V DC]                          │
│                    │                                 │
│            ┌───────┴──────────┐                      │
│            ▼                  ▼                      │
│       VM (12 V)          [Buck 5 V]                  │
│       TMC2209            USB-C ──► Arduino Nano ESP32│
└─────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────┐
│  Arduino Nano ESP32            TMC2209               │
│  (ESP32-S3 · 3,3 V)           (driver pas à pas)    │
│                                                      │
│  D2 ──────────────────────── STEP                    │
│  D3 ──────────────────────── DIR                     │
│  D4 ──────────────────────── EN   (actif LOW)        │
│  D5 ────[1 kΩ]─────────────── PDN_UART (half-duplex) │
│  3V3 ─────────────────────── VIO  (logique)          │
│  GND ─────────────────────── GND                     │
│  GND ─────────────────────── MS1  (adresse UART = 0) │
│  GND ─────────────────────── MS2  (adresse UART = 0) │
│                               VM ◄── 12 V            │
│                               A1/A2 ──► NEMA 14 bobine A │
│                               B1/B2 ──► NEMA 14 bobine B │
└─────────────────────────────────────────────────────┘
```

### Alimentation détaillée

```
Bloc secteur 12 V DC
        │
        ├──────────────────────────► VM  du TMC2209  (puissance moteur)
        │
        └──► [Buck converter 5 V] ──► USB-C ──► Arduino Nano ESP32
                                                        │
                                                       3V3 ──► VIO du TMC2209
```

> **Condensateurs obligatoires sur VM** : 100 nF céramique + 100 µF électrolytique
> au plus près des broches VM/GND du TMC2209 pour absorber les pics de courant.

### Paramètres mécaniques

| Paramètre | Valeur | Calcul |
|-----------|--------|--------|
| Pas/tour moteur | 200 | — |
| Microstepping | 16× | via UART |
| Rapport réducteur | 40:1 | Cowells RGB61 |
| **Pas/tour diviseur** | **128 000** | 200 × 16 × 40 |
| Courant RMS | 600 mA | TMC2209 UART |
| Vitesse travail | 6 400 pas/s | ≈ 3 tr/min diviseur |
| Accélération | 4 000 pas/s² | profil trapézoïdal |

### UART half-duplex TMC2209

Le TMC2209 utilise un seul fil (PDN_UART) pour RX et TX.
La résistance 1 kΩ en série sur D5 limite le courant lorsque RX et TX
sont brièvement en opposition. MS1=GND et MS2=GND fixent l'adresse UART à 0.

---

## 2. Architecture logicielle

### Firmware (Arduino / ESP32)

```
loop()
  ├── server.handleClient()   ← traite les requêtes HTTP entrantes
  ├── stepper.run()           ← génère les impulsions STEP (profil AccelStepper)
  └── tickDiag()              ← exécute un test diagnostic par itération

Librairies
  ├── AccelStepper  → profil trapézoïdal STEP/DIR
  ├── TMCStepper    → configuration UART : courant, µstepping, StealthChop/SpreadCycle
  ├── WebServer     → API REST + pages HTML en PROGMEM
  └── WiFiManager   → portail captif pour la configuration Wi-Fi
```

### Pages HTML

Les deux pages (contrôleur et diagnostic) sont stockées en **PROGMEM** (Flash)
pour ne pas consommer de SRAM précieuse. Elles sont servies directement depuis
la Flash par `server.send_P()`.

### Polling adaptatif

| Contexte | Intervalle |
|----------|-----------|
| Contrôleur — repos | 1 500 ms |
| Contrôleur — mouvement en cours | 300 ms (barre de progression fluide) |
| Diagnostic — inactif | 5 000 ms |
| Diagnostic — tests en cours | 400 ms |

### Tests diagnostics non-bloquants

La page `/diag` exécute les tests **un par un** dans `loop()` via `tickDiag()`.
Le POST `/api/diag/run` retourne immédiatement ; le client interroge `/api/diag`
(champ `running`) jusqu'à la fin des tests.

---

## 3. Matériel nécessaire

| Composant | Référence | Quantité |
|-----------|-----------|----------|
| Microcontrôleur | Arduino Nano ESP32 (ABX00083) | 1 |
| Driver moteur | TMC2209 (module breakout) | 1 |
| Moteur | NEMA 14, 200 pas/tr, 600 mA RMS ou plus | 1 |
| Alimentation | Bloc secteur 12 V DC, min 2 A | 1 |
| Buck converter | 12 V → 5 V, min 1 A (ex. LM2596) | 1 |
| Résistance | 1 kΩ ¼ W | 1 |
| Condensateurs | 100 nF céramique + 100 µF 25 V électrolytique | 1 de chaque |
| Câble USB-C | pour programmation et alimentation | 1 |
| Fils de connexion | — | — |

**Logiciel requis sur le PC**

| Outil | Version min |
|-------|-------------|
| `arduino-cli` | 1.4.1 |
| Core `arduino:esp32` | 2.0.18-arduino.5 |
| TMCStepper | Library Manager |
| AccelStepper | Library Manager |
| WiFiManager (tzapu) | Library Manager |

---

## 4. Montage pas à pas

Le montage est organisé en **4 étapes** qui correspondent exactement aux
**4 groupes de tests** de la page `/diag`. Effectuez les tests avant de
passer à l'étape suivante.

---

### Étape 1 — Arduino + USB

**Objectif** : vérifier que le firmware fonctionne et que le Wi-Fi se connecte.

#### Câblage

Aucun câblage externe. Relier uniquement l'Arduino au PC via USB-C.

#### Flasher le firmware

```bash
# Option A — script automatique (recommandé)
deploy              # détecte le port, compile, téléverse

# Option B — manuel
arduino-cli lib install "TMCStepper" "AccelStepper" "WiFiManager"
arduino-cli compile --fqbn arduino:esp32:nano_nora diviseur/
arduino-cli upload  --fqbn arduino:esp32:nano_nora -p /dev/ttyACM0 diviseur/
```

> **Note** : le script `deploy` doit être installé dans `/usr/local/bin`.
> Voir section [Installation du firmware](#5-installation-du-firmware).

#### Configuration Wi-Fi (première mise en route)

1. Au premier démarrage, l'ESP32 crée le réseau **`Diviseur-Setup`**
2. Connecter le smartphone à ce réseau
3. Une page de configuration s'ouvre automatiquement (portail captif)
4. Entrer le SSID et le mot de passe du réseau local
5. L'ESP32 redémarre et affiche l'IP dans le moniteur série

#### Tests à effectuer (Étape 1)

Ouvrir `http://<ip>/diag` → **Étape 1** → **▶ Tester**

| Test | Attendu |
|------|---------|
| T01 Démarrage système | OK — heap libre > 50 kB |
| T02 Connexion Wi-Fi | OK — SSID + IP + RSSI affichés |

---

### Étape 2 — Alimentation 12 V + Buck

**Objectif** : valider l'alimentation externe avant de connecter le driver.

#### Câblage

```
Bloc 12 V
    │
    ├──► IN+ du Buck converter
    │         │
    │        OUT+ (régler à 5,0 V) ──► USB-C Arduino (via adaptateur)
    │        OUT- ──► GND Buck
    │
    └──► VM du TMC2209  (à l'étape 3)
         GND ──► GND commun
```

> **Avant de connecter quoi que ce soit**, régler le potentiomètre du Buck
> à **5,0 V** en mesurant la sortie à vide avec un multimètre.

#### Vérifications

1. Mesurer OUT+ du Buck = **5,0 V ± 0,1 V**
2. Alimenter l'Arduino depuis le Buck (USB-C)
3. Vérifier que l'interface web est toujours accessible

#### Tests à effectuer (Étape 2)

Page `/diag` → **Étape 2** → **▶ Tester**

| Test | Attendu |
|------|---------|
| T03 Alimentation 5 V (Buck) | ALERTE — confirmation manuelle requise |

> T03 est une vérification manuelle : confirmer la mesure au multimètre,
> puis marquer mentalement l'étape comme validée.

---

### Étape 3 — Driver TMC2209 (sans moteur)

**Objectif** : vérifier la communication UART avec le driver.

#### Câblage complet

```
Arduino Nano ESP32          TMC2209
──────────────────          ───────────────────────────
D2  ──────────────────────► STEP
D3  ──────────────────────► DIR
D4  ──────────────────────► EN        (actif LOW)
D5  ──── [1 kΩ] ──────────► PDN_UART  (half-duplex UART)
3V3 ──────────────────────► VIO       (tension logique)
GND ──────────────────────► GND
GND ──────────────────────► MS1       (adresse UART 0)
GND ──────────────────────► MS2       (adresse UART 0)

Bloc 12 V ────────────────► VM        (puissance moteur)
GND ──────────────────────► GND VM

[100 nF céramique + 100 µF électrolytique entre VM et GND, au plus près du TMC2209]
```

> **Ne pas connecter le moteur pour l'instant.**

#### Vérifications avant mise sous tension

- [ ] Résistance 1 kΩ bien en série sur PDN_UART (pas en parallèle)
- [ ] MS1 et MS2 reliés à GND (adresse UART 0)
- [ ] VIO relié au **3V3** de l'Arduino (pas au 5V, le Nano ESP32 est 3,3 V)
- [ ] Condensateurs posés sur VM/GND
- [ ] VM relié au 12 V, GND commun

#### Tests à effectuer (Étape 3)

Page `/diag` → **Étape 3** → **▶ Tester**

| Test | Attendu |
|------|---------|
| T04 UART → TMC2209 | OK — version 0x21 détectée |
| T05 Config courant + µstepping | OK — 600 mA, 16× |
| T06 Alimentation moteur VM | ALERTE — mesurer 12 V sur VM |
| T07 Broche EN | OK — driver activé puis désactivé |

> Si T04 échoue (0x00 ou 0xFF) : vérifier le câblage PDN_UART, la résistance 1 kΩ,
> MS1/MS2 à GND, et VIO alimenté. Voir section [Dépannage](#10-dépannage).

---

### Étape 4 — Moteur NEMA 14

**Objectif** : valider le mouvement, le sens de rotation et la thermique.

#### Connexion du moteur

Identifier les bobines avec un multimètre (mesurer la résistance) :
- **Bobine A** : paire de fils avec résistance ~10-20 Ω → A1 / A2 du TMC2209
- **Bobine B** : paire de fils avec résistance ~10-20 Ω → B1 / B2 du TMC2209

```
NEMA 14
  Bobine A ─── A1 ──► TMC2209 OA1
              A2 ──► TMC2209 OA2
  Bobine B ─── B1 ──► TMC2209 OB1
              B2 ──► TMC2209 OB2
```

> **Ne pas connecter / déconnecter le moteur sous tension.**

#### Tests à effectuer (Étape 4)

Page `/diag` → **Étape 4** → **▶ Tester**

| Test | Attendu |
|------|---------|
| T08 Sens de rotation | ALERTE — à vérifier visuellement (voir ci-dessous) |
| T09 Précision microstepping | ALERTE — à valider après montage complet |
| T10 Température driver | OK — DRV_STATUS nominal |

#### Vérification du sens de rotation

1. Page `/` → activer le moteur (toggle **Moteur**)
2. Appuyer sur **AVANCE ▶**
3. Le diviseur doit avancer dans le sens horaire (vu de face)
4. Si le sens est inversé : permuter A1↔A2 **ou** B1↔B2 (pas les deux)

#### Test de précision (T09)

1. Marquer la position zéro au feutre sur le diviseur
2. Régler **6 divisions** (60° par pas)
3. Faire 6 avances successives → vérifier le retour à zéro
4. Répéter avec **3 divisions** (120°) et **4 divisions** (90°)

---

## 5. Installation du firmware

### Méthode recommandée — script `deploy`

```bash
# Installer le script (une seule fois)
sudo cp deploy.sh /usr/local/bin/deploy
sudo chmod +x /usr/local/bin/deploy

# Utilisation
deploy                    # compile + téléverse (port auto)
deploy /dev/ttyACM0       # port explicite
deploy --no-upload        # compiler seulement
```

Le script gère automatiquement :
- La vérification et l'installation du core `arduino:esp32`
- L'installation des librairies
- La récupération du code depuis GitHub
- La détection du port série
- La compilation et le téléversement

### Méthode manuelle

```bash
# Librairies
arduino-cli lib install "TMCStepper" "AccelStepper" "WiFiManager"

# Core
arduino-cli core update-index \
  --additional-urls https://downloads.arduino.cc/packages/package_esp32_index.json
arduino-cli core install arduino:esp32 \
  --additional-urls https://downloads.arduino.cc/packages/package_esp32_index.json

# Compiler
arduino-cli compile --fqbn arduino:esp32:nano_nora diviseur/

# Téléverser
arduino-cli upload --fqbn arduino:esp32:nano_nora -p /dev/ttyACM0 diviseur/
```

### Versionnage

Un hook git pre-commit incrémente automatiquement `FW_VERSION` à chaque commit.
La version courante est affichée dans l'en-tête de l'interface web et retournée
par `/api/status` (champ `version`).

---

## 6. Configuration Wi-Fi

| Situation | Comportement |
|-----------|-------------|
| Première mise en route | Crée le réseau **`Diviseur-Setup`** |
| Réseau mémorisé introuvable | Recrée le réseau **`Diviseur-Setup`** après 3 min |
| Connexion réussie | IP affichée dans le moniteur série (115 200 bauds) |

Les identifiants sont stockés dans la Flash (NVS via WiFiManager) — aucune
recompilation nécessaire lors d'un changement de réseau.

Pour **réinitialiser la configuration Wi-Fi** : maintenir le bouton RESET de
l'Arduino pendant 10 s au démarrage (active le portail captif de WiFiManager).

---

## 7. Interface web

### Page Contrôleur (`http://<ip>/`)

| Zone | Fonctionnalité |
|------|----------------|
| En-tête | Nom, rapport, version firmware |
| Barre de statut | Indicateur pulsé PRÊT / EN MOUVEMENT |
| Division courante | N / Total, angle, angle/division, température driver |
| Navigation | ◀ RECUL · AVANCE ▶ (bloqués pendant le mouvement) |
| Barre de progression | Avancement, phase (Accel. / Vitesse max / Décel.), pas/s |
| Nombre de divisions | −/+, clavier numérique popup avec préréglages |
| Réglages driver | StealthChop ↔ SpreadCycle, activation moteur |
| Système | RAM libre, Flash utilisé (kB et %) |

### Page Diagnostic (`http://<ip>/diag`)

| Zone | Fonctionnalité |
|------|----------------|
| Résumé | Compteurs OK / Fail / Alerte / Attente |
| Étapes 1–4 | Tests groupés par étape matérielle |
| Bouton Tester | Lance les tests en arrière-plan, polling auto |

---

## 8. API REST

### `/api/status` — GET

```json
{
  "divisions": 6,
  "currentDiv": 2,
  "steps": 42666,
  "enabled": true,
  "moving": false,
  "rssi": -62,
  "uptime": 123456,
  "temp": 0,
  "spreadCycle": false,
  "heap": 180000,
  "heapTotal": 327680,
  "sketch": 420000,
  "flash": 16777216,
  "distanceToGo": 0,
  "moveTotal": 21333,
  "speed": 0,
  "maxSpeed": 6400,
  "version": "1.7"
}
```

| Champ | Description |
|-------|-------------|
| `temp` | 0=OK, 1=CHAUD (>120°C), 2=STOP (>150°C) |
| `distanceToGo` | Pas restants (abs) du mouvement courant |
| `moveTotal` | Pas totaux du dernier mouvement |
| `speed` | Vitesse courante en pas/s (abs) |
| `maxSpeed` | Vitesse maximale configurée en pas/s |

### Endpoints disponibles

| Méthode | Endpoint | Corps JSON | Description |
|---------|----------|------------|-------------|
| GET | `/api/status` | — | État complet |
| POST | `/api/divisions` | `{"n":6}` | Nombre de divisions (2–360) |
| POST | `/api/move` | `{"dir":1}` | Avancer (1) ou reculer (-1) |
| POST | `/api/home` | — | Déclarer zéro à la position courante |
| POST | `/api/enable` | `{"enable":true}` | Activer / désactiver le moteur |
| POST | `/api/stop` | — | Arrêt d'urgence immédiat |
| POST | `/api/jog` | `{"steps":100}` | Jog de N pas |
| POST | `/api/mode` | `{"spreadCycle":true}` | Basculer StealthChop ↔ SpreadCycle |
| GET | `/api/diag` | — | État des tests (champ `running`) |
| POST | `/api/diag/run` | `{"step":1}` | Lancer une étape de test (1–4) |
| POST | `/api/diag/reset` | `{"step":1}` | Remettre tous les tests d'une étape en attente |
| POST | `/api/diag/set` | `{"idx":2,"status":1}` | Valider manuellement un test (status: 1=OK, 2=FAIL) |

---

## 9. Paramètres et calibration

### Constantes firmware (`diviseur.ino`)

```cpp
#define STEPS_PER_REV   200      // pas/tour du moteur
#define MICROSTEPS       16      // microstepping (1,2,4,8,16,32,64,256)
#define GEAR_RATIO       40      // rapport du diviseur Cowells RGB61
#define MOTOR_CURRENT   600      // courant RMS en mA
#define R_SENSE         0.11f    // résistance de détection du TMC2209
#define SPEED_WORK     6400.0f   // vitesse travail en pas/s
#define ACCEL_WORK     4000.0f   // accélération en pas/s²
#define SPEED_JOG      1600.0f   // vitesse jog en pas/s
```

### Adapter à un autre moteur

| Paramètre | Formule |
|-----------|---------|
| `MOTOR_CURRENT` | Courant nominal RMS du moteur en mA |
| `STEPS_PER_REV` | Pas/tour (moteur 1,8° → 200, moteur 0,9° → 400) |
| `GEAR_RATIO` | Rapport de réduction du diviseur |
| `MICROSTEPS` | 16 est un bon compromis silence / précision |

### Modes de fonctionnement

| Mode | Usage | Activation |
|------|-------|------------|
| **StealthChop** (défaut) | Fonctionnement silencieux | Toggle interface |
| **SpreadCycle** | Couple maximal, plus bruyant | Toggle interface |

---

## 10. Dépannage

### T04 échoue — UART TMC2209 (0x00 ou 0xFF)

| Vérification | Action |
|-------------|--------|
| Résistance 1 kΩ | Mesurer la résistance entre D5 et PDN_UART — doit être ~1 kΩ |
| MS1 / MS2 | Confirmer la connexion à GND (non à VIO) |
| VIO | Doit être à 3,3 V (broche 3V3 de l'Arduino) |
| Soudures TMC2209 | Inspecter visuellement les broches du module |
| UART conflict | S'assurer que `SerialTMC` utilise bien le port UART1 de l'ESP32-S3 |

### Moteur qui vibre sans avancer

- Courant trop faible → augmenter `MOTOR_CURRENT` (max ~1 000 mA pour NEMA 14)
- Bobines inversées → permuter A1↔A2 ou B1↔B2

### Surchauffe driver (T10 = FAIL)

- Ajouter un dissipateur thermique sur le TMC2209
- Réduire `MOTOR_CURRENT`
- Vérifier les condensateurs sur VM

### Interface web inaccessible

1. Vérifier l'IP dans le moniteur série (115 200 bauds)
2. Confirmer que le PC/smartphone est sur le même réseau
3. Si aucun réseau mémorisé : rejoindre `Diviseur-Setup` pour reconfigurer

### La barre de progression ne s'affiche pas

- Normal : elle n'est visible que pendant un mouvement (`moving: true`)
- Vérifier que le moteur est activé (toggle **Moteur** dans les réglages)
