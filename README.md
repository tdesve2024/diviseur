# Diviseur Cowells RGB61 — Guide complet

Diviseur rotatif motorisé piloté depuis un smartphone via Wi-Fi.
Rapport 40:1 · NEMA 14 · TMC2209 · Arduino Nano ESP32.

---

## Table des matières

1. [Avant de commencer](#1-avant-de-commencer)
2. [Identifier les composants](#2-identifier-les-composants)
3. [Architecture matérielle](#3-architecture-matérielle)
4. [Matériel nécessaire](#4-matériel-nécessaire)
5. [Montage pas à pas](#5-montage-pas-à-pas)
   - [Étape 1 — Arduino + USB](#étape-1--arduino--usb)
   - [Étape 2 — Alimentation 12 V + Buck converter](#étape-2--alimentation-12-v--buck-converter)
   - [Étape 3 — Condensateurs sur VM](#étape-3--condensateurs-sur-vm)
   - [Étape 4 — Résistance PDN_UART](#étape-4--résistance-pdn_uart)
   - [Étape 5 — Driver TMC2209 (sans moteur)](#étape-5--driver-tmc2209-sans-moteur)
   - [Étape 6 — Moteur NEMA 14](#étape-6--moteur-nema-14)
6. [Installation du firmware](#6-installation-du-firmware)
7. [Configuration Wi-Fi](#7-configuration-wi-fi)
8. [Interface web](#8-interface-web)
9. [API REST](#9-api-rest)
10. [Paramètres et calibration](#10-paramètres-et-calibration)
11. [Dépannage](#11-dépannage)

---

## 1. Avant de commencer

### Outils nécessaires

- Multimètre (indispensable — utilisé à chaque étape)
- Pince brucelles ou petite pince plate
- Fer à souder (si vous utilisez une breadboard à souder ou des connecteurs)
- Fils de connexion avec embouts Dupont mâle/femelle (idéalement plusieurs couleurs)
- Tournevis fin (potentiomètre du Buck converter)

### Conventions de couleur recommandées

Respectez ces couleurs pour les fils — elles vous sauveront en cas d'erreur :

| Couleur | Usage |
|---------|-------|
| **Rouge** | + / alimentation positive |
| **Noir** | GND / masse |
| **Jaune ou orange** | Signaux (STEP, DIR, UART…) |
| **Bleu** | EN (enable) |
| **Vert** | 3,3 V logique (VIO) |

### Règles de sécurité — à lire avant tout montage

> Ces règles s'appliquent à chaque étape. Les ignorer peut détruire un composant ou endommager l'Arduino.

1. **Toujours couper l'alimentation** avant d'ajouter ou de modifier un câblage.
2. **Vérifier deux fois** chaque connexion avant de mettre sous tension.
3. **Ne jamais connecter ni déconnecter le moteur sous tension** — cela peut détruire le driver TMC2209 instantanément.
4. **Ne jamais alimenter le TMC2209 en VIO avant le 12 V VM** (séquence d'alimentation sans importance pour ce montage, mais rester prudent).
5. **Le VIO du TMC2209 doit être alimenté en 3,3 V** — jamais en 5 V. L'Arduino Nano ESP32 est un circuit 3,3 V.
6. **Un condensateur électrolytique a une polarité** — le brancher à l'envers peut le faire exploser.

---

## 2. Identifier les composants

Avant le montage, vous devez être capable d'identifier et de mesurer chaque composant.

### La résistance 1 kΩ

Une résistance se lit grâce aux **bandes de couleur** peintes sur son corps.

Pour une résistance **1 kΩ** (4 bandes) :

```
  │ Brun │ Noir │ Rouge │ Or │
  │   1  │   0  │  ×100 │ 5% │
  →  1 × 10 × 100 = 1 000 Ω = 1 kΩ
```

**Vérification au multimètre :**
- Sélectionner le mode Ω (ohmmètre)
- Toucher les deux pattes de la résistance avec les pointes de mesure
- Lire : doit afficher entre **950 Ω et 1 050 Ω** (tolérance 5 %)

> Une résistance n'a pas de sens — les deux pattes sont interchangeables.

---

### Le condensateur céramique 100 nF

Le condensateur **céramique** est un petit disque ou pastille plate, souvent jaune ou orangé, avec deux pattes.

```
     ┌──┐
     │  │  ← pastille céramique (peut indiquer "104" = 100 nF)
    ─┘  └─
```

**Lire la valeur "104" inscrite dessus :**
- `10` = 10
- `4` = × 10⁴ (10 000)
- → 100 000 pF = **100 nF**

**Il n'a pas de polarité** — les deux pattes sont interchangeables. Vous pouvez le brancher dans n'importe quel sens.

**Vérification au multimètre :**
- Sélectionner le mode capacimètre (symbole ⊣⊢ ou "CAP")
- Doit afficher **≈ 100 nF** (souvent entre 80 et 120 nF, c'est normal)
- Si votre multimètre ne mesure pas les capacités, ce n'est pas grave — identifiez-le visuellement.

---

### Le condensateur électrolytique 100 µF

Le condensateur **électrolytique** est un petit cylindre avec une bande blanche ou grise sur un côté.

```
        ─── (patte courte = négatif -)
    ┌───────┐
    │  ███  │ ← bande blanche ou grise = côté NÉGATIF
    │ 100µF │
    │  25V  │
    └───────┘
        ─── (patte longue = positif +)
```

**Identifier la polarité :**

| Signe | Côté |
|-------|------|
| Patte **longue** | **+ (positif)** à relier au +12 V / VM |
| Patte **courte** | **− (négatif)** à relier au GND |
| Bande blanche/grise | Côté **− (négatif)** |

> **Attention :** brancher un condensateur électrolytique à l'envers peut le faire gonfler, fuir ou exploser. Toujours vérifier la polarité avant mise sous tension.

**Vérification au multimètre (mode capacimètre) :**
- Doit afficher entre **80 µF et 120 µF** (tolérance courante ±20 %)

---

## 3. Architecture matérielle

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

### Pourquoi la résistance 1 kΩ sur PDN_UART ?

Le TMC2209 utilise un seul fil (PDN_UART) pour envoyer **et** recevoir des données en même temps (half-duplex). L'ESP32 a deux broches séparées en interne : TX (émission) et RX (réception). Ces deux broches sont réunies sur le même fil physique.

Sans résistance, si TX et RX sont simultanément à des niveaux opposés (0 V et 3,3 V), un court-circuit de quelques millisecondes se produit. La résistance **1 kΩ** limite ce courant à 3,3 V / 1 000 Ω = 3,3 mA — inoffensif pour les composants.

```
Arduino D5 (TX+RX) ──[1 kΩ]──── PDN_UART du TMC2209
                                      │
                                      └──► également relié au RX interne de D5
```

### Pourquoi les condensateurs sur VM ?

Lorsque le moteur change de direction ou démarre, il tire brusquement du courant. Cette variation rapide crée des **pics de tension** (surtensions) sur le fil 12 V qui peuvent dépasser la limite du TMC2209 et le détruire.

- Le **100 nF céramique** absorbe les pics très rapides (haute fréquence)
- Le **100 µF électrolytique** absorbe les pics plus lents (basse fréquence, grande énergie)

Les deux condensateurs se complètent. Ils doivent être placés **au plus près possible** des broches VM et GND du TMC2209 (moins de 2 cm si possible) pour être efficaces.

---

## 4. Matériel nécessaire

| Composant | Référence | Quantité |
|-----------|-----------|----------|
| Microcontrôleur | Arduino Nano ESP32 (ABX00083) | 1 |
| Driver moteur | TMC2209 (module breakout) | 1 |
| Moteur | NEMA 14, 200 pas/tr, 600 mA RMS ou plus | 1 |
| Alimentation | Bloc secteur 12 V DC, min 2 A | 1 |
| Buck converter | 12 V → 5 V, min 1 A (ex. LM2596) | 1 |
| Résistance | 1 kΩ ¼ W (bandes : brun-noir-rouge-or) | 1 |
| Condensateur céramique | 100 nF (marquage "104") | 1 |
| Condensateur électrolytique | 100 µF 25 V (ou plus) | 1 |
| Câble USB-C | pour programmation et alimentation | 1 |
| Fils de connexion | plusieurs couleurs recommandées | — |

**Logiciel requis sur le PC**

| Outil | Version min |
|-------|-------------|
| `arduino-cli` | 1.4.1 |
| Core `arduino:esp32` | 2.0.18-arduino.5 |
| TMCStepper | Library Manager |
| AccelStepper | Library Manager |
| WiFiManager (tzapu) | Library Manager |

---

## 5. Montage pas à pas

Le montage est organisé en **6 étapes progressives**. Chaque étape doit être validée avant de passer à la suivante. Les tests logiciels de la page `/diag` couvrent les étapes 1, 2, 5 et 6.

> **Règle d'or :** ne jamais mettre sous tension sans avoir coché toutes les cases de vérification de l'étape.

---

### Étape 1 — Arduino + USB

**Objectif :** vérifier que le firmware fonctionne et que le Wi-Fi se connecte. Aucun composant externe.

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
> Voir section [Installation du firmware](#6-installation-du-firmware).

#### Configuration Wi-Fi (première mise en route)

1. Au premier démarrage, l'ESP32 crée le réseau **`Diviseur-Setup`**
2. Connecter le smartphone à ce réseau
3. Une page de configuration s'ouvre automatiquement (portail captif)
4. Entrer le SSID et le mot de passe du réseau local
5. L'ESP32 redémarre et affiche l'IP dans le moniteur série

#### Tests logiciels — Groupe 1

Ouvrir `http://<ip>/diag` → **Étape 1** → **▶ Tester**

| Test | Attendu |
|------|---------|
| T01 Démarrage système | OK — heap libre > 50 kB |
| T02 Connexion Wi-Fi | OK — SSID + IP + RSSI affichés |

---

### Étape 2 — Alimentation 12 V + Buck converter

**Objectif :** valider l'alimentation externe avant de connecter le moindre composant électronique.

#### Réglage du Buck converter (AVANT tout câblage)

Le Buck converter sort une tension réglable via un petit potentiomètre à vis.

> **Régler la tension à vide, bloc 12 V branché mais rien d'autre connecté.**

```
Bloc 12 V ──► IN+ du Buck
              IN- du Buck ──► GND

Multimètre sur OUT+ / OUT- du Buck
Tourner le potentiomètre jusqu'à lire 5,0 V
```

- Tourner dans le sens horaire → tension augmente
- Tourner dans le sens antihoraire → tension diminue
- Cible : **5,0 V ± 0,1 V**

#### Câblage de l'alimentation Arduino

Une fois réglé à 5,0 V :

```
Buck OUT+ (5 V) ──► USB-C de l'Arduino (via adaptateur ou câble modifié)
Buck OUT-       ──► GND commun
```

#### Liste de vérification avant mise sous tension

- [ ] Buck réglé à **5,0 V** mesuré à vide au multimètre
- [ ] Aucun autre composant connecté au Buck ou au 12 V
- [ ] Fils d'alimentation bien fixés (pas de court-circuit possible)

#### Tests à effectuer

1. Mettre le bloc 12 V sous tension
2. Mesurer OUT+ du Buck = **5,0 V ± 0,1 V** (confirmer la stabilité sous charge)
3. Vérifier que l'interface web est toujours accessible depuis le smartphone

#### Tests logiciels — Groupe 2

Page `/diag` → **Étape 2** → **▶ Tester**

| Test | Attendu |
|------|---------|
| T03 Alimentation 5 V (Buck) | ALERTE — confirmation manuelle requise |

> T03 est une vérification manuelle : confirmer la mesure au multimètre, puis valider avec le bouton **✓ OK** dans l'interface.

---

### Étape 3 — Condensateurs sur VM

**Objectif :** placer les condensateurs de protection sur la ligne 12 V du driver. C'est une étape purement mécanique — pas de mise sous tension.

> Placez les condensateurs **avant** de câbler le TMC2209 pour ne pas oublier.

#### Placement sur la breadboard ou le PCB

Les deux condensateurs se placent **en parallèle** entre VM (+12 V) et GND, au plus près des broches VM/GND du TMC2209.

```
        VM (+12 V)
           │
    ┌──────┴──────┐
    │             │
   ═╪═           ╫   ← condensateur céramique 100 nF (disque/pastille)
    │             │       pas de polarité — sens indifférent
    │            ═╪═
    │             │   ← condensateur électrolytique 100 µF
    │             │       patte longue (+) vers VM
    │             │       patte courte (−) vers GND
    └──────┬──────┘
           │
          GND
```

**En pratique sur une breadboard :**

```
Colonne + (bus rouge) = VM (+12 V)
Colonne − (bus bleu)  = GND

Condensateur céramique 100 nF :
  Une patte dans le bus rouge (+)
  Autre patte dans le bus bleu (−)
  → sens indifférent

Condensateur électrolytique 100 µF :
  Patte LONGUE (+) dans le bus rouge (+)
  Patte COURTE (−) dans le bus bleu (−)
  → RESPECTER LA POLARITÉ
```

#### Vérification avant de continuer

- [ ] Condensateur céramique : une patte sur VM, une patte sur GND
- [ ] Condensateur électrolytique : patte longue sur VM, patte courte sur GND
- [ ] La bande blanche/grise du condensateur électrolytique est du côté GND
- [ ] Les deux condensateurs sont dans la zone VM/GND du TMC2209 (moins de 2 cm des broches)
- [ ] Aucun fil ne touche les pattes des condensateurs (court-circuit)

> Pas de test logiciel pour cette étape — vérification visuelle uniquement.

---

### Étape 4 — Résistance PDN_UART

**Objectif :** préparer le câble de communication UART avant de connecter le TMC2209.

#### Pourquoi câbler la résistance maintenant ?

Il est plus facile de monter la résistance séparément, de la mesurer, puis de l'intégrer dans le câblage global à l'étape suivante.

#### Montage de la résistance

La résistance se place **en série** sur le fil qui relie la broche D5 de l'Arduino à la broche PDN_UART du TMC2209. "En série" signifie que le courant doit obligatoirement passer à travers elle.

```
Arduino D5 ──── [fil] ──── [1 kΩ] ──── [fil] ──── PDN_UART TMC2209
                              ↑
                    La résistance est intercalée
                    dans le fil de connexion
```

**Méthode pratique (breadboard) :**

```
1. Insérer la résistance dans deux trous adjacents de la breadboard
         Trou A ──[résistance]──Trou B

2. Brancher un fil de D5 de l'Arduino → Trou A
3. Brancher un fil de Trou B → PDN_UART du TMC2209
```

**NE PAS faire :**
```
❌ Placer la résistance en parallèle (entre VM et GND) — ce serait un court-circuit
❌ Relier D5 directement à PDN_UART sans résistance
```

#### Vérification au multimètre

Avant de connecter quoi que ce soit :

1. Débrancher le fil côté Arduino (D5)
2. Multimètre en mode Ω — mesurer entre le point D5 et le point PDN_UART
3. Doit lire : **950 Ω à 1 050 Ω**

Si la mesure est 0 Ω : la résistance est court-circuitée ou absente.
Si la mesure est `OL` (infini) : connexion interrompue, vérifier les fils.

---

### Étape 5 — Driver TMC2209 (sans moteur)

**Objectif :** câbler le driver complet et vérifier la communication UART. Le moteur n'est pas connecté.

#### Câblage complet

```
Arduino Nano ESP32          TMC2209
──────────────────          ───────────────────────────
D2  ──────────────────────► STEP
D3  ──────────────────────► DIR
D4  ──────────────────────► EN        (actif LOW)
D5  ──── [1 kΩ] ──────────► PDN_UART  (half-duplex UART)
3V3 ──────────────────────► VIO       (tension logique 3,3 V)
GND ──────────────────────► GND
GND ──────────────────────► MS1       (adresse UART 0)
GND ──────────────────────► MS2       (adresse UART 0)

Bloc 12 V ────────────────► VM        (puissance moteur)
GND ──────────────────────► GND VM    (masse commune)

[100 nF + 100 µF entre VM et GND — montés à l'étape 3]
```

> **Ne pas connecter le moteur pour l'instant.**

#### Liste de vérification avant mise sous tension

- [ ] Résistance 1 kΩ bien **en série** sur le fil D5 → PDN_UART (mesurée à l'étape 4)
- [ ] MS1 relié à **GND** (pas à VIO)
- [ ] MS2 relié à **GND** (pas à VIO)
- [ ] VIO relié au **3V3** de l'Arduino (pas au 5 V !)
- [ ] GND Arduino relié au GND TMC2209
- [ ] GND du 12 V relié au GND commun (masse commune entre Arduino et TMC2209)
- [ ] VM relié au **+12 V** du bloc secteur
- [ ] Condensateurs en place (étape 3 validée)
- [ ] Le moteur **n'est pas connecté**

#### Vérification avec le multimètre avant mise sous tension

Avant tout, vérifier qu'il n'y a pas de court-circuit sur l'alimentation :
1. Multimètre en mode Ω — mesurer entre VM et GND : doit afficher une valeur élevée (plusieurs kΩ ou `OL`)
2. Si vous lisez 0 Ω : court-circuit, ne pas mettre sous tension, vérifier le câblage

#### Tests logiciels — Groupe 3

Page `/diag` → **Étape 3** → **▶ Tester**

| Test | Attendu |
|------|---------|
| T04 UART → TMC2209 | OK — version 0x21 détectée |
| T05 Config courant + µstepping | OK — 600 mA, 16× |
| T06 Alimentation moteur VM | ALERTE — mesurer 12 V sur VM, valider manuellement |
| T07 Broche EN | OK — driver activé puis désactivé |

> Si T04 échoue (0x00 ou 0xFF) : voir la section [Dépannage T04](#t04-échoue--uart-tmc2209-0x00-ou-0xff).

---

### Étape 6 — Moteur NEMA 14

**Objectif :** connecter le moteur, valider le mouvement, le sens de rotation et la thermique.

#### Identifier les bobines au multimètre

Le moteur NEMA 14 a 4 fils (2 bobines de 2 fils chacune). Il faut identifier quelle paire appartient à quelle bobine.

1. Multimètre en mode Ω
2. Tester toutes les combinaisons de paires de fils
3. Une paire de la **même bobine** → résistance **10 à 20 Ω**
4. Deux fils de bobines différentes → résistance infinie (`OL`)

```
Fils souvent colorés :
  Bobine A : noir + vert  (ou rouge + bleu selon fabricant)
  Bobine B : rouge + bleu (ou jaune + orange)

→ Mesurer pour confirmer
```

#### Connexion du moteur

```
NEMA 14
  Bobine A ─── fil 1 ──► TMC2209 OA1
              fil 2 ──► TMC2209 OA2
  Bobine B ─── fil 1 ──► TMC2209 OB1
              fil 2 ──► TMC2209 OB2
```

> **Couper l'alimentation avant de connecter le moteur. Ne jamais brancher ou débrancher le moteur sous tension.**

#### Liste de vérification avant mise sous tension

- [ ] Moteur éteint — alimentation coupée pendant le câblage
- [ ] Bobine A identifiée et connectée à OA1 / OA2
- [ ] Bobine B identifiée et connectée à OB1 / OB2
- [ ] Aucune patte du moteur ne touche le châssis ou d'autres fils

#### Tests logiciels — Groupe 4

Page `/diag` → **Étape 4** → **▶ Tester**

| Test | Attendu |
|------|---------|
| T08 Sens de rotation | ALERTE — à vérifier visuellement |
| T09 Précision microstepping | ALERTE — à valider après montage complet |
| T10 Température driver | OK — DRV_STATUS nominal |

#### Vérification du sens de rotation (T08)

1. Page `/` → activer le moteur (toggle **Moteur**)
2. Appuyer sur **AVANCE ▶**
3. Le diviseur doit tourner dans le sens horaire (vu de face)
4. Si le sens est inversé : couper l'alimentation, permuter **A1↔A2** sur OA1/OA2 (ou B1↔B2, pas les deux)
5. Valider T08 manuellement avec **✓ OK**

#### Test de précision (T09)

1. Marquer la position zéro au feutre sur le diviseur
2. Régler **6 divisions** dans l'interface (60° par pas)
3. Faire 6 avances successives → le diviseur doit revenir exactement à zéro
4. Répéter avec **3 divisions** (120°) et **4 divisions** (90°)
5. Valider T09 manuellement avec **✓ OK**

---

## 6. Installation du firmware

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

## 7. Configuration Wi-Fi

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

## 8. Interface web

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

## 9. API REST

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

## 10. Paramètres et calibration

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

## 11. Dépannage

### T04 échoue — UART TMC2209 (0x00 ou 0xFF)

| Vérification | Action |
|-------------|--------|
| Résistance 1 kΩ | Multimètre entre D5 et PDN_UART → doit lire ~1 kΩ |
| Résistance en série | Vérifier qu'elle est bien **dans** le fil, pas en parallèle |
| MS1 / MS2 | Confirmer la connexion à GND (et non à VIO) |
| VIO | Doit être à **3,3 V** (broche 3V3 de l'Arduino, pas le 5 V du Buck) |
| GND commun | Arduino GND et TMC2209 GND doivent être reliés |
| Soudures TMC2209 | Inspecter visuellement les broches du module |

### Moteur qui vibre sans avancer

- Courant trop faible → augmenter `MOTOR_CURRENT` (max ~1 000 mA pour NEMA 14)
- Bobines inversées → permuter A1↔A2 ou B1↔B2 (jamais les deux en même temps)
- GND non commun entre Arduino et TMC2209

### Surchauffe driver (T10 = FAIL)

- Ajouter un dissipateur thermique sur le TMC2209
- Réduire `MOTOR_CURRENT`
- Vérifier la présence des condensateurs sur VM (étape 3)

### Interface web inaccessible

1. Vérifier l'IP dans le moniteur série (115 200 bauds)
2. Confirmer que le PC/smartphone est sur le même réseau
3. Si aucun réseau mémorisé : rejoindre `Diviseur-Setup` pour reconfigurer

### La barre de progression ne s'affiche pas

- Normal : elle n'est visible que pendant un mouvement (`moving: true`)
- Vérifier que le moteur est activé (toggle **Moteur** dans les réglages)

### Condensateur électrolytique chaud ou gonflé

- Couper l'alimentation immédiatement
- Le condensateur est monté à l'envers (polarité inversée)
- Le remplacer et vérifier : patte longue (+) sur VM, patte courte (−) sur GND
