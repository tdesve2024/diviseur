/**
 * Diviseur Cowells RGB61 — Arduino Nano ESP32
 *
 * Moteur : NEMA 14, 600 mA RMS
 * Driver : TMC2209 — UART + STEP/DIR
 * Rapport : 40:1  →  128 000 pas/tour diviseur (16× microstepping)
 *
 * Câblage TMC2209 :
 *   D2  → STEP
 *   D3  → DIR
 *   D4  → EN (active LOW)
 *   D5  → PDN_UART via résistance 1 kΩ
 *   GND → MS1, MS2  (adresse UART 0 — microstepping configuré via UART)
 *
 * Librairies requises (Library Manager) :
 *   - TMCStepper  by teemuatlut
 *   - AccelStepper by Mike McCaulay
 *   - WiFiManager  by tzapu
 */

#include <WiFi.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <AccelStepper.h>
#include <TMCStepper.h>

#define PIN_STEP      D2
#define PIN_DIR       D3
#define PIN_EN        D4
#define PIN_UART      D5

#define STEPS_PER_REV   200       // pas/tour moteur NEMA 14
#define MICROSTEPS      16        // microstepping TMC2209
#define GEAR_RATIO      40        // rapport diviseur Cowells RGB61
#define MOTOR_CURRENT   600       // courant RMS (mA)
#define R_SENSE         0.11f     // résistance de shunt TMC2209 (Ω)
#define DRIVER_ADDR     0         // adresse UART (MS1=GND, MS2=GND)

#define SPEED_WORK      6400.0f   // vitesse normale (pas/s) ≈ 3 tr/min diviseur
#define ACCEL_WORK      4000.0f   // accélération (pas/s²)
#define SPEED_JOG       1600.0f   // vitesse jog page test (pas/s)
// ===================================================================

const long STEPS_PER_TURN =
    (long)STEPS_PER_REV * MICROSTEPS * GEAR_RATIO;  // 128 000

HardwareSerial SerialTMC(1);
TMC2209Stepper driver(&SerialTMC, R_SENSE, DRIVER_ADDR);
AccelStepper   stepper(AccelStepper::DRIVER, PIN_STEP, PIN_DIR);
WebServer      server(80);

int  numDivisions = 6;
long divOffset    = 0;      // divisions avancées depuis le zéro (signé, cumulatif)
bool motorEnabled = false;
bool jogMode      = false;  // true = vitesse jog active

// ===================================================================
//  Page principale (PROGMEM)
// ===================================================================
const char PAGE_MAIN[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Diviseur RGB61</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:sans-serif;background:#1a1a2e;color:#eee;padding:16px;max-width:480px;margin:auto}
h1{text-align:center;color:#e94560;margin-bottom:20px;font-size:1.4em;letter-spacing:.05em}
.card{background:#16213e;border-radius:12px;padding:20px;margin-bottom:14px}
.card h2{font-size:.9em;color:#a0c4ff;margin-bottom:14px;text-transform:uppercase;letter-spacing:.08em}
.big{font-size:3.4em;text-align:center;color:#e94560;font-weight:bold;line-height:1}
.sub{text-align:center;color:#888;font-size:.85em;margin-top:4px}
.btn{display:inline-flex;align-items:center;justify-content:center;padding:14px 20px;
     border:none;border-radius:10px;font-size:1.05em;cursor:pointer;font-weight:bold;margin:4px;transition:opacity .15s}
.btn:active{opacity:.65}
.btn-primary{background:#e94560;color:#fff}
.btn-muted{background:#0f3460;color:#fff}
.btn-nav{background:#533483;color:#fff;font-size:1.8em;padding:18px 36px}
.row{display:flex;justify-content:center;flex-wrap:wrap;gap:6px;margin-top:10px}
input[type=number]{background:#0f3460;border:1px solid #533483;color:#fff;
                   padding:10px 12px;border-radius:8px;font-size:1em;width:110px}
.status{text-align:center;color:#4caf50;margin-top:8px;font-size:.9em;min-height:1.2em}
a.link{color:#a0c4ff;text-decoration:none;display:block;text-align:center;
       margin-top:18px;padding:12px;background:#16213e;border-radius:10px}
.moving{color:#ff9800;animation:blink 1s infinite}
@keyframes blink{0%,100%{opacity:1}50%{opacity:.3}}
</style>
</head>
<body>
<h1>⚙ Diviseur Cowells RGB61</h1>

<div class="card">
  <h2>Position</h2>
  <div class="big" id="divDisplay">0&nbsp;/&nbsp;6</div>
  <div class="sub" id="stepDisplay">0 pas</div>
  <div class="sub" id="movingLabel" style="margin-top:6px;min-height:1.2em"></div>
  <div class="row" style="margin-top:16px;justify-content:center">
    <svg width="160" height="160" viewBox="0 0 160 160">
      <circle cx="80" cy="80" r="68" fill="none" stroke="#0f3460" stroke-width="14"/>
      <circle id="arc" cx="80" cy="80" r="68" fill="none" stroke="#e94560" stroke-width="14"
              stroke-dasharray="0 427" stroke-linecap="round"
              transform="rotate(-90 80 80)" style="transition:stroke-dasharray .4s"/>
      <text id="arcTxt" x="80" y="80" fill="#eee" font-size="30" font-weight="bold"
            dominant-baseline="middle" text-anchor="middle">0</text>
    </svg>
  </div>
</div>

<div class="card">
  <h2>Navigation</h2>
  <div class="row">
    <button class="btn btn-nav" onclick="moveDivision(-1)">&#9664;</button>
    <button class="btn btn-nav" onclick="moveDivision(1)">&#9654;</button>
  </div>
  <div class="row">
    <button class="btn btn-muted" onclick="goHome()">⌂ Zéro ici</button>
  </div>
</div>

<div class="card">
  <h2>Nombre de divisions</h2>
  <div class="row" style="align-items:center">
    <input type="number" id="divInput" min="2" max="360" value="6">
    <button class="btn btn-primary" onclick="setDivisions()">Appliquer</button>
  </div>
  <div class="status" id="divStatus"></div>
</div>

<a class="link" href="/test">&#128295; Page de test</a>

<script>
const CIRC = 2 * Math.PI * 68;

function updateStatus() {
  fetch('/api/status').then(r => r.json()).then(d => {
    document.getElementById('divDisplay').innerHTML = d.currentDiv + '&nbsp;/&nbsp;' + d.divisions;
    document.getElementById('stepDisplay').textContent = d.steps + ' pas';
    document.getElementById('arcTxt').textContent = d.currentDiv;
    document.getElementById('divInput').value = d.divisions;
    document.getElementById('movingLabel').className = d.moving ? 'sub moving' : 'sub';
    document.getElementById('movingLabel').textContent = d.moving ? '⟳ Déplacement...' : '';
    const frac = d.divisions > 0 ? d.currentDiv / d.divisions : 0;
    document.getElementById('arc').setAttribute('stroke-dasharray',
      (frac * CIRC).toFixed(1) + ' ' + CIRC.toFixed(1));
  }).catch(() => {});
}

function moveDivision(dir) {
  fetch('/api/move', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({dir})
  }).then(() => setTimeout(updateStatus, 300));
}

function goHome() {
  fetch('/api/home', {method: 'POST'}).then(() => updateStatus());
}

function setDivisions() {
  const n = parseInt(document.getElementById('divInput').value);
  if (n < 2 || n > 360) {
    document.getElementById('divStatus').textContent = '✗ Valeur invalide (2–360)';
    return;
  }
  fetch('/api/divisions', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({n})
  }).then(r => r.json()).then(d => {
    document.getElementById('divStatus').textContent =
      d.ok ? '✓ ' + n + ' divisions appliquées — position remise à zéro' : '✗ Erreur';
    updateStatus();
  });
}

setInterval(updateStatus, 1500);
updateStatus();
</script>
</body>
</html>
)rawliteral";

// ===================================================================
//  Page de test (PROGMEM)
// ===================================================================
const char PAGE_TEST[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Test — Diviseur RGB61</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:sans-serif;background:#1a1a2e;color:#eee;padding:16px;max-width:480px;margin:auto}
h1{text-align:center;color:#ff9800;margin-bottom:20px;font-size:1.4em}
.card{background:#16213e;border-radius:12px;padding:20px;margin-bottom:14px}
.card h2{font-size:.9em;color:#a0c4ff;margin-bottom:14px;text-transform:uppercase;letter-spacing:.08em}
.btn{display:inline-flex;align-items:center;justify-content:center;padding:12px 16px;
     border:none;border-radius:10px;font-size:.95em;cursor:pointer;font-weight:bold;margin:3px;transition:opacity .15s}
.btn:active{opacity:.65}
.btn-on{background:#4caf50;color:#fff}
.btn-off{background:#f44336;color:#fff}
.btn-jog{background:#0f3460;color:#fff;min-width:58px}
.btn-turn{background:#533483;color:#fff}
.btn-stop{background:#ff9800;color:#000;font-size:1.1em;padding:14px 28px}
.row{display:flex;justify-content:center;flex-wrap:wrap;gap:4px;margin-top:8px}
.info{background:#0a0a1a;border-radius:8px;padding:12px;font-size:.85em;line-height:1.8}
.info .val{color:#ff9800;font-weight:bold}
.led{width:14px;height:14px;border-radius:50%;display:inline-block;vertical-align:middle;margin-right:6px}
.led-on{background:#4caf50;box-shadow:0 0 8px #4caf50}
.led-off{background:#555}
.log{background:#0a0a1a;border-radius:6px;padding:10px;font-size:.78em;
     height:110px;overflow-y:auto;font-family:monospace;margin-top:8px;color:#aaa}
a.link{color:#a0c4ff;text-decoration:none;display:block;text-align:center;
       margin-top:18px;padding:12px;background:#16213e;border-radius:10px}
</style>
</head>
<body>
<h1>&#128295; Page de test</h1>

<div class="card">
  <h2>Moteur</h2>
  <div style="text-align:center">
    <span class="led" id="motorLed"></span>
    <span id="motorState">—</span>
  </div>
  <div class="row" style="margin-top:12px">
    <button class="btn btn-on"  onclick="enableMotor(true)">Activer</button>
    <button class="btn btn-off" onclick="enableMotor(false)">Désactiver</button>
    <button class="btn btn-stop" onclick="stopMotor()">■ Stop</button>
  </div>
</div>

<div class="card">
  <h2>Jog manuel (pas)</h2>
  <div class="row">
    <button class="btn btn-jog" onclick="jog(-1000)">−1000</button>
    <button class="btn btn-jog" onclick="jog(-100)">−100</button>
    <button class="btn btn-jog" onclick="jog(-10)">−10</button>
    <button class="btn btn-jog" onclick="jog(-1)">−1</button>
    <button class="btn btn-jog" onclick="jog(1)">+1</button>
    <button class="btn btn-jog" onclick="jog(10)">+10</button>
    <button class="btn btn-jog" onclick="jog(100)">+100</button>
    <button class="btn btn-jog" onclick="jog(1000)">+1000</button>
  </div>
  <div class="row" style="margin-top:6px">
    <button class="btn btn-turn" onclick="jog(-6400)">◀ ½ tour</button>
    <button class="btn btn-turn" onclick="jog(-12800)">◀ 1 tour</button>
    <button class="btn btn-turn" onclick="jog(6400)">½ tour ▶</button>
    <button class="btn btn-turn" onclick="jog(12800)">1 tour ▶</button>
  </div>
</div>

<div class="card">
  <h2>État système</h2>
  <div class="info">
    Position absolue&nbsp;: <span class="val" id="stepPos">—</span> pas<br>
    Moteur&nbsp;: <span class="val" id="motorInfo">—</span><br>
    Déplacement&nbsp;: <span class="val" id="movingInfo">—</span><br>
    WiFi RSSI&nbsp;: <span class="val" id="rssi">—</span> dBm<br>
    Uptime&nbsp;: <span class="val" id="uptime">—</span>
  </div>
  <div class="log" id="logDiv">Prêt...<br></div>
</div>

<a class="link" href="/">← Retour à la page principale</a>

<script>
function log(msg) {
  const d = document.getElementById('logDiv');
  const t = new Date().toLocaleTimeString('fr', {hour:'2-digit',minute:'2-digit',second:'2-digit'});
  d.innerHTML += t + ' ' + msg + '<br>';
  d.scrollTop = d.scrollHeight;
}

function updateStatus() {
  fetch('/api/status').then(r => r.json()).then(d => {
    document.getElementById('stepPos').textContent = d.steps;
    document.getElementById('motorInfo').textContent = d.enabled ? 'Activé ✓' : 'Désactivé';
    document.getElementById('movingInfo').textContent = d.moving ? '⟳ En cours' : 'Arrêté';
    document.getElementById('rssi').textContent = d.rssi;
    document.getElementById('uptime').textContent = Math.floor(d.uptime / 1000) + ' s';
    const led = document.getElementById('motorLed');
    led.className = 'led ' + (d.enabled ? 'led-on' : 'led-off');
    document.getElementById('motorState').textContent = d.enabled ? 'Activé' : 'Désactivé';
  }).catch(() => {});
}

function enableMotor(en) {
  fetch('/api/enable', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({enable: en})
  }).then(() => { log(en ? '▶ Moteur activé' : '■ Moteur désactivé'); updateStatus(); });
}

function stopMotor() {
  fetch('/api/stop', {method: 'POST'}).then(() => { log('⚠ Stop moteur'); updateStatus(); });
}

function jog(steps) {
  fetch('/api/jog', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({steps})
  }).then(() => {
    log('Jog ' + (steps > 0 ? '+' : '') + steps + ' pas');
    setTimeout(updateStatus, 400);
  });
}

setInterval(updateStatus, 1500);
updateStatus();
</script>
</body>
</html>
)rawliteral";

// ===================================================================
//  Moteur
// ===================================================================
void setMotorEnabled(bool en) {
  motorEnabled = en;
  digitalWrite(PIN_EN, en ? LOW : HIGH);  // EN actif LOW
}

int currentDivision() {
  return (int)(((divOffset % numDivisions) + numDivisions) % numDivisions);
}

void doMoveDivision(int dir) {
  if (!motorEnabled) setMotorEnabled(true);
  if (jogMode) {
    stepper.setMaxSpeed(SPEED_WORK);
    stepper.setAcceleration(ACCEL_WORK);
    jogMode = false;
  }
  divOffset += dir;
  stepper.moveTo(divOffset * (STEPS_PER_TURN / numDivisions));
}

// ===================================================================
//  Routes Web
// ===================================================================
void handleRoot() { server.send_P(200, "text/html", PAGE_MAIN); }
void handleTest() { server.send_P(200, "text/html", PAGE_TEST); }

void handleStatus() {
  char json[256];
  snprintf(json, sizeof(json),
    "{\"divisions\":%d,\"currentDiv\":%d,\"steps\":%ld,"
    "\"enabled\":%s,\"moving\":%s,\"rssi\":%d,\"uptime\":%lu}",
    numDivisions,
    currentDivision(),
    stepper.currentPosition(),
    motorEnabled ? "true" : "false",
    stepper.isRunning() ? "true" : "false",
    WiFi.RSSI(),
    millis());
  server.send(200, "application/json", json);
}

void handleSetDivisions() {
  if (!server.hasArg("plain")) { server.send(400); return; }
  int idx = server.arg("plain").indexOf("\"n\":");
  if (idx < 0) { server.send(400); return; }
  int n = server.arg("plain").substring(idx + 4).toInt();
  if (n < 2 || n > 360) {
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  numDivisions = n;
  divOffset    = 0;
  stepper.setCurrentPosition(0);
  stepper.moveTo(0);
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleMove() {
  if (!server.hasArg("plain")) { server.send(400); return; }
  String body = server.arg("plain");
  int dir = (body.indexOf("-1") != -1) ? -1 : 1;
  doMoveDivision(dir);
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleHome() {
  divOffset = 0;
  stepper.setCurrentPosition(0);
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleEnable() {
  if (!server.hasArg("plain")) { server.send(400); return; }
  setMotorEnabled(server.arg("plain").indexOf("true") != -1);
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleStop() {
  stepper.stop();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleJog() {
  if (!server.hasArg("plain")) { server.send(400); return; }
  String body = server.arg("plain");
  int idx = body.indexOf("\"steps\":");
  if (idx < 0) { server.send(400); return; }
  long steps = body.substring(idx + 8).toInt();
  if (!motorEnabled) setMotorEnabled(true);
  if (!jogMode) {
    stepper.setMaxSpeed(SPEED_JOG);
    stepper.setAcceleration(ACCEL_WORK);
    jogMode = true;
  }
  stepper.moveTo(stepper.currentPosition() + steps);
  server.send(200, "application/json", "{\"ok\":true}");
}

// ===================================================================
//  Setup & Loop
// ===================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Diviseur Cowells RGB61 ===");

  // GPIO
  pinMode(PIN_EN, OUTPUT);
  setMotorEnabled(false);

  // TMC2209 via UART (TX seulement — write-only pour la configuration)
  SerialTMC.begin(115200, SERIAL_8N1, -1, PIN_UART);
  delay(100);
  driver.begin();
  driver.toff(5);                      // Activer le driver
  driver.rms_current(MOTOR_CURRENT);   // Courant RMS
  driver.microsteps(MICROSTEPS);       // 16× microstepping via UART
  driver.en_spreadCycle(false);        // StealthChop (silencieux)
  driver.pwm_autoscale(true);          // Autoscale du PWM
  Serial.println("TMC2209 configuré");

  // AccelStepper
  stepper.setMaxSpeed(SPEED_WORK);
  stepper.setAcceleration(ACCEL_WORK);

  // WiFi via WiFiManager
  // Premier démarrage : crée un AP "Diviseur-Setup", connectez-vous
  // avec votre téléphone et entrez vos identifiants WiFi.
  // Les démarrages suivants se reconnectent automatiquement.
  WiFiManager wm;
  Serial.println("WiFiManager — connexion en cours...");
  if (wm.autoConnect("Diviseur-Setup")) {
    Serial.printf("Connecté ! IP : http://%s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("Échec WiFi — redémarrage dans 10 s...");
    delay(10000);
    ESP.restart();
  }

  // Routes
  server.on("/",              HTTP_GET,  handleRoot);
  server.on("/test",          HTTP_GET,  handleTest);
  server.on("/api/status",    HTTP_GET,  handleStatus);
  server.on("/api/divisions", HTTP_POST, handleSetDivisions);
  server.on("/api/move",      HTTP_POST, handleMove);
  server.on("/api/home",      HTTP_POST, handleHome);
  server.on("/api/enable",    HTTP_POST, handleEnable);
  server.on("/api/stop",      HTTP_POST, handleStop);
  server.on("/api/jog",       HTTP_POST, handleJog);
  server.begin();
  Serial.println("Serveur web démarré");
}

void loop() {
  server.handleClient();
  stepper.run();
}
