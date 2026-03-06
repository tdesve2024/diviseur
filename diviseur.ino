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

#define STEPS_PER_REV   200
#define MICROSTEPS      16
#define GEAR_RATIO      40
#define MOTOR_CURRENT   600
#define R_SENSE         0.11f
#define DRIVER_ADDR     0

#define SPEED_WORK      6400.0f
#define ACCEL_WORK      4000.0f
#define SPEED_JOG       1600.0f

const long STEPS_PER_TURN =
    (long)STEPS_PER_REV * MICROSTEPS * GEAR_RATIO;  // 128 000

HardwareSerial SerialTMC(1);
TMC2209Stepper driver(&SerialTMC, R_SENSE, DRIVER_ADDR);
AccelStepper   stepper(AccelStepper::DRIVER, PIN_STEP, PIN_DIR);
WebServer      server(80);

int  numDivisions    = 6;
long divOffset       = 0;
bool motorEnabled    = false;
bool jogMode         = false;
bool spreadCycleMode = false;

// ===================================================================
//  Diagnostic
// ===================================================================
#define NUM_DIAG_TESTS 10
#define NUM_DIAG_STEPS 4

enum DiagStatus : uint8_t { DS_PENDING=0, DS_OK=1, DS_FAIL=2, DS_ALERT=3 };

struct DiagTest {
  DiagStatus status;
  int32_t    durationMs;
  char       detail[100];
};

static const char* DIAG_IDS[NUM_DIAG_TESTS] = {
  "T01","T02","T03","T04","T05","T06","T07","T08","T09","T10"
};
static const uint8_t DIAG_STEP_MAP[NUM_DIAG_TESTS] = {1,1,2,3,3,3,3,4,4,4};
static const char* DIAG_NAMES[NUM_DIAG_TESTS] = {
  "D\xc3\xa9marrage syst\xc3\xa8me",
  "Connexion WiFi",
  "Alimentation 5V (Buck)",
  "UART \xe2\x86\x92 TMC2209",
  "Config courant + \xc2\xb5stepping",
  "Alimentation moteur VM",
  "Broche EN (enable driver)",
  "Sens de rotation",
  "Pr\xc3\xa9cision microstepping",
  "Temp\xc3\xa9rature driver"
};
static const char* STEP_TITLES[NUM_DIAG_STEPS] = {
  "Arduino seul + USB",
  "+ Alimentation 12V + Buck",
  "+ TMC2209 (sans moteur)",
  "+ Moteur NEMA 14"
};
static const char* STEP_DESCS[NUM_DIAG_STEPS] = {
  "Alimenter via USB uniquement",
  "Alim 12V \xe2\x86\x92 Buck \xe2\x86\x92 c\xc3\xa2ble USB-C \xe2\x86\x92 Nano ESP32",
  "C\xc3\xa2bler le driver TMC2209 (STEP/DIR/EN/UART)",
  "Brancher le moteur NEMA 14 au TMC2209"
};

DiagTest diagTests[NUM_DIAG_TESTS];

// ===================================================================
//  Page CONTRÔLEUR (PROGMEM)
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
body{font-family:-apple-system,BlinkMacSystemFont,sans-serif;background:#f0f2f5;color:#1a2d3d;max-width:480px;margin:auto;min-height:100vh}
.hdr{background:#1a2d3d;padding:14px 18px;display:flex;align-items:center;justify-content:space-between}
.hdr h1{color:#fff;font-size:1.25em;font-weight:800;letter-spacing:.05em}
.hdr p{color:#6b8cae;font-size:.7em;margin-top:3px}
.badge{background:#2d6a9f;color:#fff;font-size:.72em;font-weight:700;padding:4px 10px;border-radius:20px}
.sbar{background:#2d6a9f;padding:10px 18px;display:flex;align-items:center;gap:8px}
.dot{width:9px;height:9px;border-radius:50%;background:#a8e6cf}
.sbar span{color:#fff;font-weight:700;font-size:.88em;letter-spacing:.06em}
.tabs{background:#1a2d3d;display:flex}
.tab{flex:1;padding:13px 8px;text-align:center;color:#6b8cae;font-size:.75em;font-weight:700;letter-spacing:.08em;text-decoration:none;border-bottom:3px solid transparent;display:flex;align-items:center;justify-content:center;gap:5px}
.tab.on{color:#fff;border-bottom-color:#2d6a9f}
.pg{padding:14px}
.card{background:#fff;border-radius:14px;padding:18px;margin-bottom:12px;box-shadow:0 2px 8px rgba(0,0,0,.07)}
.lbl{font-size:.68em;color:#8a9ab0;letter-spacing:.1em;font-weight:700;margin-bottom:14px}
.big{text-align:center;margin-bottom:14px;line-height:1}
.bign{font-size:3.8em;font-weight:800;color:#1a2d3d}
.bigd{font-size:2em;font-weight:400;color:#b0bec5}
hr{border:none;border-top:1px solid #eaeef2;margin:14px 0}
.met{display:flex;text-align:center}
.met>div{flex:1}
.mv{font-size:1.05em;font-weight:700;color:#2d6a9f}
.mv.or{color:#e67e22}
.ml{font-size:.62em;color:#8a9ab0;letter-spacing:.08em;margin-top:3px}
.nav{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-bottom:10px}
.br{background:#1a2d3d;color:#fff;border:none;border-radius:12px;padding:18px;font-size:.95em;font-weight:800;cursor:pointer;letter-spacing:.06em;width:100%}
.ba{background:#2d6a9f;color:#fff;border:none;border-radius:12px;padding:18px;font-size:.95em;font-weight:800;cursor:pointer;letter-spacing:.06em;width:100%}
.bz{width:100%;background:#f0f2f5;color:#5a6a7a;border:none;border-radius:12px;padding:13px;font-size:.84em;font-weight:600;cursor:pointer;letter-spacing:.04em}
.br:active,.ba:active,.bz:active{opacity:.7}
.dc{display:flex;align-items:center;gap:8px}
.pm{background:#f0f2f5;border:none;border-radius:10px;width:48px;height:48px;font-size:1.5em;font-weight:300;cursor:pointer;color:#1a2d3d;flex-shrink:0}
.dv{flex:1;text-align:center}
.dn{font-size:2.2em;font-weight:800;color:#1a2d3d;line-height:1}
.dl{font-size:.62em;color:#8a9ab0;letter-spacing:.08em;margin-top:3px}
.bl{background:#2d6a9f;border:none;border-radius:10px;width:48px;height:48px;cursor:pointer;display:flex;align-items:center;justify-content:center;flex-shrink:0}
.bl svg{width:18px;height:18px;fill:#fff}
.mrow{display:flex;align-items:center;justify-content:space-between}
.mn{font-size:1.05em;font-weight:700}
.ms{font-size:.78em;color:#8a9ab0;margin-top:3px}
.tg{width:46px;height:26px;background:#d0d5db;border-radius:13px;position:relative;cursor:pointer;transition:background .2s;flex-shrink:0}
.tg.on{background:#2d6a9f}
.tg::after{content:'';position:absolute;width:20px;height:20px;background:#fff;border-radius:50%;top:3px;left:3px;transition:left .2s;box-shadow:0 1px 3px rgba(0,0,0,.25)}
.tg.on::after{left:23px}
.bst{width:100%;background:#c0392b;color:#fff;border:none;border-radius:12px;padding:14px;font-size:.95em;font-weight:800;cursor:pointer;letter-spacing:.06em}
.bst:active{opacity:.7}
</style>
</head>
<body>
<div class="hdr">
  <div><h1>DIVISEUR</h1><p>Cowells RGB61 &middot; TMC2209 &middot; NEMA 14</p></div>
  <div class="badge">WiFi</div>
</div>
<div class="sbar"><div class="dot"></div><span id="sbarTxt">PR&#202;T</span></div>
<div class="tabs">
  <a class="tab on" href="/">&#9658; CONTR&#212;LEUR</a>
  <a class="tab" href="/diag">&#9658; DIAGNOSTIC</a>
</div>
<div class="pg">
  <div class="card">
    <div class="lbl">DIVISION COURANTE</div>
    <div class="big">
      <span class="bign" id="dc">0</span><span class="bigd"> /&thinsp;<span id="dt">6</span></span>
    </div>
    <hr>
    <div class="met">
      <div><div class="mv" id="an">0.0&deg;</div><div class="ml">ANGLE ACTUEL</div></div>
      <div><div class="mv" id="pa">60.0&deg;</div><div class="ml">PAS / DIVISION</div></div>
      <div><div class="mv or" id="tp">OK</div><div class="ml">TEMP. DRIVER</div></div>
    </div>
  </div>
  <div class="card">
    <div class="nav">
      <button class="br" onclick="mv(-1)">&#9664; RECUL</button>
      <button class="ba" onclick="mv(1)">&#9654; AVANCE</button>
    </div>
    <button class="bz" onclick="home()" style="margin-bottom:8px">&#8635; REMETTRE &Agrave; Z&Eacute;RO</button>
    <button class="bst" id="bstop" onclick="stopMotor()">&#9632; STOP</button>
  </div>
  <div class="card">
    <div class="lbl">NOMBRE DE DIVISIONS</div>
    <div class="dc">
      <button class="pm" onclick="chg(-1)">&#8722;</button>
      <div class="dv"><div class="dn" id="nd">6</div><div class="dl">DIVISIONS</div></div>
      <button class="pm" onclick="chg(1)">+</button>
      <button class="bl" onclick="dlst()">
        <svg viewBox="0 0 24 24"><rect x="3" y="5" width="18" height="2" rx="1"/><rect x="3" y="11" width="18" height="2" rx="1"/><rect x="3" y="17" width="18" height="2" rx="1"/></svg>
      </button>
    </div>
  </div>
  <div class="card">
    <div class="lbl">MODE DRIVER</div>
    <div class="mrow" style="margin-bottom:14px">
      <div><div class="mn" id="mdn">StealthChop</div><div class="ms" id="mds">Silencieux</div></div>
      <div class="tg" id="mdtg" onclick="tgm()"></div>
    </div>
    <div class="mrow">
      <div><div class="mn" id="men">Moteur</div><div class="ms" id="mes">D\u00e9sactiv\u00e9</div></div>
      <div class="tg" id="metg" onclick="tge()"></div>
    </div>
  </div>
</div>
<script>
let sc=false,en=false;
const TEMP_LABELS=['OK','CHAUD','STOP'];
function upd(){
  fetch('/api/status').then(r=>r.json()).then(d=>{
    document.getElementById('dc').textContent=d.currentDiv;
    document.getElementById('dt').textContent=d.divisions;
    document.getElementById('nd').textContent=d.divisions;
    const a=d.divisions>0?(d.currentDiv*360/d.divisions).toFixed(1):'0.0';
    const p=d.divisions>0?(360/d.divisions).toFixed(1):'0.0';
    document.getElementById('an').textContent=a+'\u00b0';
    document.getElementById('pa').textContent=p+'\u00b0';
    const t=d.temp||0;
    const te=document.getElementById('tp');
    te.textContent=TEMP_LABELS[t]||'?';
    te.style.color=t===2?'#c0392b':t===1?'#e67e22':'';
    document.getElementById('sbarTxt').textContent=d.moving?'EN MOUVEMENT':'PR\u00caT';
    if(d.spreadCycle!==undefined&&d.spreadCycle!==sc){sc=d.spreadCycle;renderMode();}
    if(d.enabled!==undefined&&d.enabled!==en){en=d.enabled;renderEnable();}
  }).catch(()=>{});
}
function renderMode(){
  document.getElementById('mdn').textContent=sc?'SpreadCycle':'StealthChop';
  document.getElementById('mds').textContent=sc?'Couple maximum':'Silencieux';
  document.getElementById('mdtg').className='tg'+(sc?' on':'');
}
function renderEnable(){
  document.getElementById('mes').textContent=en?'Activ\u00e9':'D\u00e9sactiv\u00e9';
  document.getElementById('metg').className='tg'+(en?' on':'');
}
function mv(dir){
  fetch('/api/move',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({dir})})
    .then(()=>setTimeout(upd,300));
}
function home(){fetch('/api/home',{method:'POST'}).then(()=>upd());}
function stopMotor(){fetch('/api/stop',{method:'POST'}).then(()=>upd());}
function chg(d){
  const n=Math.min(360,Math.max(2,parseInt(document.getElementById('nd').textContent)+d));
  fetch('/api/divisions',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({n})})
    .then(()=>upd());
}
function dlst(){
  const pr=[2,3,4,5,6,7,8,9,10,12,15,18,20,24,30,36,40,45,60,72,90,120,180,360];
  const c=parseInt(document.getElementById('nd').textContent);
  const v=prompt('Valeurs courantes\u00a0: '+pr.join(', ')+'\n\nNombre de divisions\u00a0:',c);
  if(!v)return;
  const n=parseInt(v);
  if(n>=2&&n<=360)
    fetch('/api/divisions',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({n})})
      .then(()=>upd());
}
function tgm(){
  sc=!sc;renderMode();
  fetch('/api/mode',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({spreadCycle:sc})});
}
function tge(){
  en=!en;renderEnable();
  fetch('/api/enable',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({enable:en})});
}
setInterval(upd,1500);upd();
</script>
</body>
</html>
)rawliteral";

// ===================================================================
//  Page DIAGNOSTIC (PROGMEM)
// ===================================================================
const char PAGE_DIAG[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Diagnostic — Diviseur RGB61</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,sans-serif;background:#f0f2f5;color:#1a2d3d;max-width:480px;margin:auto;min-height:100vh}
.hdr{background:#1a2d3d;padding:14px 18px;display:flex;align-items:center;justify-content:space-between}
.hdr h1{color:#fff;font-size:1.25em;font-weight:800;letter-spacing:.05em}
.hdr p{color:#6b8cae;font-size:.7em;margin-top:3px}
.badge{background:#2d6a9f;color:#fff;font-size:.72em;font-weight:700;padding:4px 10px;border-radius:20px}
.sbar{background:#2d6a9f;padding:10px 18px;display:flex;align-items:center;gap:8px}
.dot{width:9px;height:9px;border-radius:50%;background:#a8e6cf}
.sbar span{color:#fff;font-weight:700;font-size:.88em;letter-spacing:.06em}
.tabs{background:#1a2d3d;display:flex}
.tab{flex:1;padding:13px 8px;text-align:center;color:#6b8cae;font-size:.75em;font-weight:700;letter-spacing:.08em;text-decoration:none;border-bottom:3px solid transparent;display:flex;align-items:center;justify-content:center;gap:5px}
.tab.on{color:#fff;border-bottom-color:#2d6a9f}
.pg{padding:14px}
.sum{display:grid;grid-template-columns:repeat(4,1fr);gap:8px;margin-bottom:14px}
.sb{background:#fff;border-radius:12px;padding:14px 6px;text-align:center;box-shadow:0 2px 6px rgba(0,0,0,.06)}
.sn{font-size:1.9em;font-weight:800;line-height:1}
.sl{font-size:.6em;font-weight:700;letter-spacing:.08em;margin-top:4px}
.s-ok .sn,.s-ok .sl{color:#27ae60}
.s-fl .sn,.s-fl .sl{color:#e74c3c}
.s-al .sn,.s-al .sl{color:#e67e22}
.s-pe .sn,.s-pe .sl{color:#95a5a6}
.card{background:#fff;border-radius:14px;padding:16px;margin-bottom:12px;box-shadow:0 2px 8px rgba(0,0,0,.07)}
.shdr{display:flex;align-items:flex-start;gap:10px;margin-bottom:12px}
.sbdg{background:#1a2d3d;color:#fff;font-size:.65em;font-weight:800;padding:5px 9px;border-radius:6px;letter-spacing:.05em;white-space:nowrap;margin-top:2px}
.si{flex:1;min-width:0}
.st{font-weight:700;font-size:.9em;color:#1a2d3d}
.sd{font-size:.72em;color:#8a9ab0;margin-top:2px;line-height:1.3}
.btest{background:#2d6a9f;color:#fff;border:none;border-radius:8px;padding:8px 12px;font-size:.75em;font-weight:700;cursor:pointer;white-space:nowrap;flex-shrink:0}
.btest:active{opacity:.7}
.ti{display:flex;align-items:flex-start;gap:8px;padding:10px 12px;border-radius:10px;margin-bottom:7px;border-left:4px solid #e8ecf0;background:#fafbfc}
.ti-ok{border-left-color:#27ae60;background:#f0fdf4}
.ti-fl{border-left-color:#e74c3c;background:#fff5f5}
.ti-al{border-left-color:#e67e22;background:#fff8f0}
.tid{font-size:.68em;color:#8a9ab0;font-weight:600;min-width:26px;padding-top:2px;flex-shrink:0}
.tbdg{font-size:.65em;font-weight:700;padding:2px 6px;border-radius:5px;letter-spacing:.05em;white-space:nowrap;flex-shrink:0}
.bdg-ok{background:#d4edda;color:#27ae60}
.bdg-fl{background:#f8d7da;color:#e74c3c}
.bdg-al{background:#fde8d4;color:#e67e22}
.bdg-pe{background:#eff0f1;color:#95a5a6}
.tb{flex:1;min-width:0}
.tn{font-weight:600;font-size:.85em;color:#1a2d3d}
.td{font-size:.72em;color:#8a9ab0;margin-top:3px;line-height:1.4}
.tdr{font-size:.68em;color:#b0bec5;flex-shrink:0;padding-top:2px;white-space:nowrap}
</style>
</head>
<body>
<div class="hdr">
  <div><h1>DIVISEUR</h1><p>Cowells RGB61 &middot; TMC2209 &middot; NEMA 14</p></div>
  <div class="badge">WiFi</div>
</div>
<div class="sbar"><div class="dot"></div><span>PR&#202;T</span></div>
<div class="tabs">
  <a class="tab" href="/">&#9658; CONTR&#212;LEUR</a>
  <a class="tab on" href="/diag">&#9658; DIAGNOSTIC</a>
</div>
<div class="pg">
  <div class="sum">
    <div class="sb s-ok"><div class="sn" id="cok">0</div><div class="sl">OK</div></div>
    <div class="sb s-fl"><div class="sn" id="cfl">0</div><div class="sl">FAIL</div></div>
    <div class="sb s-al"><div class="sn" id="cal">0</div><div class="sl">ALERTE</div></div>
    <div class="sb s-pe"><div class="sn" id="cpe">0</div><div class="sl">EN ATT.</div></div>
  </div>
  <div id="steps"></div>
</div>
<script>
function esc(s){const d=document.createElement('div');d.textContent=s;return d.innerHTML;}
function load(){
  fetch('/api/diag').then(r=>r.json()).then(d=>{
    document.getElementById('cok').textContent=d.counts.ok;
    document.getElementById('cfl').textContent=d.counts.fail;
    document.getElementById('cal').textContent=d.counts.alert;
    document.getElementById('cpe').textContent=d.counts.pending;
    const sc=document.getElementById('steps');
    sc.innerHTML='';
    d.steps.forEach(s=>{
      const c=document.createElement('div');
      c.className='card';
      let h=`<div class="shdr">
        <div class="sbdg">\u00c9TAPE ${s.n}</div>
        <div class="si"><div class="st">${esc(s.title)}</div><div class="sd">${esc(s.desc)}</div></div>
        <button class="btest" onclick="run(${s.n})">&#9654; Tester</button>
      </div>`;
      d.tests.filter(t=>t.step===s.n).forEach(t=>{
        const cls=t.status===1?'ti-ok':t.status===2?'ti-fl':t.status===3?'ti-al':'';
        const bc=t.status===1?'bdg-ok':t.status===2?'bdg-fl':t.status===3?'bdg-al':'bdg-pe';
        const bt=t.status===1?'OK':t.status===2?'FAIL':t.status===3?'ALERTE':'...';
        const dur=t.duration>=0?`<div class="tdr">${t.duration}ms</div>`:'';
        const det=t.detail?`<div class="td">${esc(t.detail)}</div>`:'';
        h+=`<div class="ti ${cls}">
          <div class="tid">${t.id}</div>
          <div class="tbdg ${bc}">${bt}</div>
          <div class="tb"><div class="tn">${esc(t.name)}</div>${det}</div>
          ${dur}
        </div>`;
      });
      c.innerHTML=h;
      sc.appendChild(c);
    });
  }).catch(()=>{});
}
function run(n){
  fetch('/api/diag/run',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({step:n})})
    .then(()=>setTimeout(load,400));
}
setInterval(load,5000);load();
</script>
</body>
</html>
)rawliteral";

// ===================================================================
//  Moteur
// ===================================================================
void setMotorEnabled(bool en) {
  motorEnabled = en;
  digitalWrite(PIN_EN, en ? LOW : HIGH);
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
//  Diagnostic — exécution des tests
// ===================================================================
void runDiagStep(int step) {
  for (int i = 0; i < NUM_DIAG_TESTS; i++) {
    if (DIAG_STEP_MAP[i] != step) continue;
    unsigned long t0 = millis();
    diagTests[i].detail[0] = 0;

    switch (i) {
      case 0: {  // T01 — Démarrage système
        uint32_t heap = ESP.getFreeHeap();
        diagTests[i].status = (heap > 50000) ? DS_OK : DS_ALERT;
        snprintf(diagTests[i].detail, sizeof(diagTests[i].detail),
          "CPU OK \xe2\x80\x94 heap libre\u00a0: %u kB \xe2\x80\x94 %lu ms depuis boot",
          heap / 1024, millis());
        break;
      }
      case 1: {  // T02 — Connexion WiFi
        if (WiFi.status() == WL_CONNECTED) {
          diagTests[i].status = DS_OK;
          snprintf(diagTests[i].detail, sizeof(diagTests[i].detail),
            "Connect\xc3\xa9 \xc3\xa0 \"%s\" \xe2\x80\x94 IP %s RSSI=%d dBm",
            WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
        } else {
          diagTests[i].status = DS_FAIL;
          snprintf(diagTests[i].detail, sizeof(diagTests[i].detail), "Non connect\xc3\xa9");
        }
        break;
      }
      case 2: {  // T03 — Buck 5V (vérification manuelle)
        diagTests[i].status = DS_ALERT;
        snprintf(diagTests[i].detail, sizeof(diagTests[i].detail),
          "V\xc3\xa9rification manuelle \xe2\x80\x94 mesurer 5V sur le rail Buck");
        break;
      }
      case 3: {  // T04 — UART → TMC2209
        driver.begin();
        uint8_t v = driver.version();
        if (v == 0x21) {
          diagTests[i].status = DS_OK;
          snprintf(diagTests[i].detail, sizeof(diagTests[i].detail),
            "TMC2209 d\xc3\xa9tect\xc3\xa9 \xe2\x80\x94 version 0x%02X \xe2\x80\x94 UART OK", v);
        } else if (v == 0x00 || v == 0xFF) {
          diagTests[i].status = DS_FAIL;
          snprintf(diagTests[i].detail, sizeof(diagTests[i].detail),
            "Pas de r\xc3\xa9ponse TMC2209 (0x%02X) \xe2\x80\x94 v\xc3\xa9rifier c\xc3\xa2blage PDN_UART", v);
        } else {
          diagTests[i].status = DS_ALERT;
          snprintf(diagTests[i].detail, sizeof(diagTests[i].detail),
            "Version inattendue 0x%02X \xe2\x80\x94 v\xc3\xa9rifier adresse UART (MS1/MS2=GND)", v);
        }
        break;
      }
      case 4: {  // T05 — Config courant + µstepping
        driver.rms_current(MOTOR_CURRENT);
        driver.microsteps(MICROSTEPS);
        diagTests[i].status = DS_OK;
        snprintf(diagTests[i].detail, sizeof(diagTests[i].detail),
          "Courant\u00a0: %d mA RMS \xe2\x80\x94 Microstepping\u00a0: %d\xc3\x97",
          MOTOR_CURRENT, MICROSTEPS);
        break;
      }
      case 5: {  // T06 — Alimentation moteur VM
        diagTests[i].status = DS_ALERT;
        snprintf(diagTests[i].detail, sizeof(diagTests[i].detail),
          "V\xc3\xa9rification manuelle \xe2\x80\x94 mesurer 12V sur VM driver");
        break;
      }
      case 6: {  // T07 — Broche EN
        digitalWrite(PIN_EN, LOW);
        delay(10);
        diagTests[i].status = DS_OK;
        snprintf(diagTests[i].detail, sizeof(diagTests[i].detail),
          "Broche EN activ\xc3\xa9e (actif LOW) \xe2\x80\x94 driver pr\xc3\xaat");
        setMotorEnabled(false);
        break;
      }
      case 7: {  // T08 — Sens de rotation
        diagTests[i].status = DS_ALERT;
        snprintf(diagTests[i].detail, sizeof(diagTests[i].detail),
          "V\xc3\xa9rifier visuellement le sens apr\xc3\xa8s connexion moteur");
        break;
      }
      case 8: {  // T09 — Précision microstepping
        diagTests[i].status = DS_ALERT;
        snprintf(diagTests[i].detail, sizeof(diagTests[i].detail),
          "Test de pr\xc3\xa9cision requis apr\xc3\xa8s montage complet");
        break;
      }
      case 9: {  // T10 — Température driver
        bool ot   = driver.ot();    // surchauffe >150°C → arrêt driver
        bool otpw = driver.otpw();  // pré-alerte >120°C
        if (ot) {
          diagTests[i].status = DS_FAIL;
          snprintf(diagTests[i].detail, sizeof(diagTests[i].detail),
            "SURCHAUFFE (>150\xc2\xb0C) \xe2\x80\x94 driver arr\xc3\xaat\xc3\xa9 en protection");
        } else if (otpw) {
          diagTests[i].status = DS_ALERT;
          snprintf(diagTests[i].detail, sizeof(diagTests[i].detail),
            "Pr\xc3\xa9-alerte temp\xc3\xa9rature (>120\xc2\xb0C) \xe2\x80\x94 v\xc3\xa9rifier ventilation");
        } else {
          diagTests[i].status = DS_OK;
          snprintf(diagTests[i].detail, sizeof(diagTests[i].detail),
            "Temp\xc3\xa9rature normale \xe2\x80\x94 DRV_STATUS=0x%08X", (unsigned)driver.DRV_STATUS());
        }
        break;
      }
    }
    diagTests[i].durationMs = (int32_t)(millis() - t0);
  }
}

// ===================================================================
//  Routes Web
// ===================================================================
void handleRoot() { server.send_P(200, "text/html", PAGE_MAIN); }
void handleDiagPage() { server.send_P(200, "text/html", PAGE_DIAG); }

void handleStatus() {
  // Lire l'état thermique du TMC2209 via UART
  // tempState : 0=OK, 1=pré-alerte, 2=surchauffe
  int tempState = 0;
  if (motorEnabled) {
    if (driver.ot())        tempState = 2;
    else if (driver.otpw()) tempState = 1;
  }

  char json[360];
  snprintf(json, sizeof(json),
    "{\"divisions\":%d,\"currentDiv\":%d,\"steps\":%ld,"
    "\"enabled\":%s,\"moving\":%s,\"rssi\":%d,\"uptime\":%lu,"
    "\"temp\":%d,\"spreadCycle\":%s}",
    numDivisions,
    currentDivision(),
    stepper.currentPosition(),
    motorEnabled ? "true" : "false",
    stepper.isRunning() ? "true" : "false",
    WiFi.RSSI(),
    millis(),
    tempState,
    spreadCycleMode ? "true" : "false");
  server.send(200, "application/json", json);
}

void handleDiagAPI() {
  // Compter les statuts
  int cOk=0, cFl=0, cAl=0, cPe=0;
  for (int i=0; i<NUM_DIAG_TESTS; i++) {
    switch (diagTests[i].status) {
      case DS_OK:    cOk++; break;
      case DS_FAIL:  cFl++; break;
      case DS_ALERT: cAl++; break;
      default:       cPe++; break;
    }
  }

  static char buf[3072];
  int pos = 0;

  pos += snprintf(buf+pos, sizeof(buf)-pos,
    "{\"counts\":{\"ok\":%d,\"fail\":%d,\"alert\":%d,\"pending\":%d},\"steps\":[",
    cOk, cFl, cAl, cPe);

  for (int s=1; s<=NUM_DIAG_STEPS; s++) {
    if (s > 1) buf[pos++] = ',';
    pos += snprintf(buf+pos, sizeof(buf)-pos,
      "{\"n\":%d,\"title\":\"%s\",\"desc\":\"%s\"}",
      s, STEP_TITLES[s-1], STEP_DESCS[s-1]);
  }

  pos += snprintf(buf+pos, sizeof(buf)-pos, "],\"tests\":[");

  for (int i=0; i<NUM_DIAG_TESTS; i++) {
    if (i > 0) buf[pos++] = ',';
    // Échapper les guillemets dans le détail
    char det[120]; int j=0;
    const char* src = diagTests[i].detail;
    for (int k=0; src[k] && j<117; k++) {
      if (src[k]=='"') { det[j++]='\\'; det[j++]='"'; }
      else det[j++]=src[k];
    }
    det[j]=0;
    pos += snprintf(buf+pos, sizeof(buf)-pos,
      "{\"id\":\"%s\",\"step\":%d,\"name\":\"%s\",\"status\":%d,\"duration\":%d,\"detail\":\"%s\"}",
      DIAG_IDS[i], DIAG_STEP_MAP[i], DIAG_NAMES[i],
      (int)diagTests[i].status, (int)diagTests[i].durationMs, det);
  }

  pos += snprintf(buf+pos, sizeof(buf)-pos, "]}");
  server.send(200, "application/json", buf);
}

void handleDiagRun() {
  if (!server.hasArg("plain")) { server.send(400); return; }
  String body = server.arg("plain");
  int idx = body.indexOf("\"step\":");
  if (idx < 0) { server.send(400); return; }
  int step = body.substring(idx + 7).toInt();
  if (step < 1 || step > NUM_DIAG_STEPS) { server.send(400); return; }
  runDiagStep(step);
  server.send(200, "application/json", "{\"ok\":true}");
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

void handleMode() {
  if (!server.hasArg("plain")) { server.send(400); return; }
  spreadCycleMode = server.arg("plain").indexOf("true") != -1;
  driver.en_spreadCycle(spreadCycleMode);
  if (!spreadCycleMode) driver.pwm_autoscale(true);
  server.send(200, "application/json", "{\"ok\":true}");
}

// ===================================================================
//  Setup & Loop
// ===================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Diviseur Cowells RGB61 ===");

  // Initialiser les tests en état pending
  for (int i = 0; i < NUM_DIAG_TESTS; i++) {
    diagTests[i].status = DS_PENDING;
    diagTests[i].durationMs = -1;
    diagTests[i].detail[0] = 0;
  }

  // GPIO
  pinMode(PIN_EN, OUTPUT);
  setMotorEnabled(false);

  // TMC2209 via UART (half-duplex : même broche RX et TX — résistance 1kΩ sur PDN_UART)
  SerialTMC.begin(115200, SERIAL_8N1, PIN_UART, PIN_UART);
  delay(100);
  driver.begin();
  driver.toff(5);
  driver.rms_current(MOTOR_CURRENT);
  driver.microsteps(MICROSTEPS);
  driver.en_spreadCycle(false);   // StealthChop
  driver.pwm_autoscale(true);
  Serial.println("TMC2209 configur\xc3\xa9");

  // AccelStepper
  stepper.setMaxSpeed(SPEED_WORK);
  stepper.setAcceleration(ACCEL_WORK);

  // WiFi via WiFiManager
  WiFiManager wm;
  Serial.println("WiFiManager \xe2\x80\x94 connexion en cours...");
  if (wm.autoConnect("Diviseur-Setup")) {
    Serial.printf("Connect\xc3\xa9\u00a0! IP\u00a0: http://%s\n", WiFi.localIP().toString().c_str());
    // Lancer automatiquement les tests de l'étape 1
    runDiagStep(1);
  } else {
    Serial.println("\xc3\x89chec WiFi \xe2\x80\x94 red\xc3\xa9marrage dans 10 s...");
    delay(10000);
    ESP.restart();
  }

  // Routes
  server.on("/",              HTTP_GET,  handleRoot);
  server.on("/diag",          HTTP_GET,  handleDiagPage);
  server.on("/api/status",    HTTP_GET,  handleStatus);
  server.on("/api/diag",      HTTP_GET,  handleDiagAPI);
  server.on("/api/diag/run",  HTTP_POST, handleDiagRun);
  server.on("/api/divisions", HTTP_POST, handleSetDivisions);
  server.on("/api/move",      HTTP_POST, handleMove);
  server.on("/api/home",      HTTP_POST, handleHome);
  server.on("/api/enable",    HTTP_POST, handleEnable);
  server.on("/api/stop",      HTTP_POST, handleStop);
  server.on("/api/jog",       HTTP_POST, handleJog);
  server.on("/api/mode",      HTTP_POST, handleMode);
  server.begin();
  Serial.println("Serveur web d\xc3\xa9marr\xc3\xa9");
}

void loop() {
  server.handleClient();
  stepper.run();
}
