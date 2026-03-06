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

#define FW_VERSION      "1.0"

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
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>Diviseur RGB61</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,'SF Pro Display',sans-serif;background:#eef2fb;color:#1a2d4a;max-width:430px;margin:auto;min-height:100vh;padding-bottom:env(safe-area-inset-bottom)}
/* Header */
.hdr{background:#fff;padding:14px 20px;padding-top:calc(14px + env(safe-area-inset-top));display:flex;align-items:center;justify-content:space-between;box-shadow:0 1px 0 #dde6f5}
.hdr h1{color:#1a2d4a;font-size:1.2em;font-weight:800;letter-spacing:.04em}
.hdr p{color:#7a90a8;font-size:.67em;margin-top:3px}
.badge{background:#eff4ff;color:#3d7ae8;font-size:.7em;font-weight:700;padding:4px 11px;border-radius:20px;border:1px solid #c7d7f8}
/* Status bar */
.sbar{background:#fff;padding:9px 20px;display:flex;align-items:center;gap:10px;box-shadow:0 1px 0 #dde6f5}
.dot{width:10px;height:10px;border-radius:50%;background:#34c759;box-shadow:0 0 5px #34c75950;flex-shrink:0}
@keyframes pulse{0%,100%{opacity:1;box-shadow:0 0 5px #34c75950}50%{opacity:.35;box-shadow:0 0 2px #34c75920}}
.dot.moving{animation:pulse 1s ease-in-out infinite}
.sbar span{color:#1a2d4a;font-weight:600;font-size:.82em;letter-spacing:.05em}
/* Tabs */
.tabs{background:#fff;display:flex;box-shadow:0 1px 0 #dde6f5}
.tab{flex:1;padding:12px 8px;text-align:center;color:#7a90a8;font-size:.72em;font-weight:700;letter-spacing:.07em;text-decoration:none;border-bottom:3px solid transparent;display:flex;align-items:center;justify-content:center;gap:5px;transition:color .15s}
.tab.on{color:#3d7ae8;border-bottom-color:#3d7ae8}
/* Content */
.pg{padding:14px 14px 6px}
.card{background:#fff;border-radius:18px;padding:20px;margin-bottom:12px;box-shadow:0 2px 12px rgba(30,60,120,.07)}
.lbl{font-size:.65em;color:#7a90a8;letter-spacing:.12em;font-weight:700;margin-bottom:16px;text-transform:uppercase}
/* Position display */
.big{text-align:center;margin-bottom:16px;line-height:1}
.bign{font-size:4.2em;font-weight:800;color:#1a2d4a;letter-spacing:-.02em}
.bigd{font-size:2.1em;font-weight:300;color:#b8c8d8}
hr{border:none;border-top:1px solid #e8eef6;margin:16px 0}
.met{display:flex;text-align:center}
.met>div{flex:1}
.mv{font-size:1.05em;font-weight:700;color:#3d7ae8}
.ml{font-size:.6em;color:#7a90a8;letter-spacing:.08em;margin-top:4px;text-transform:uppercase}
/* Navigation buttons */
.nav{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-bottom:10px}
.br,.ba{border:none;border-radius:16px;padding:22px 12px;font-size:1em;font-weight:800;cursor:pointer;letter-spacing:.05em;width:100%;transition:transform .1s,opacity .1s;-webkit-tap-highlight-color:transparent}
.br{background:#eff4ff;color:#3d7ae8;border:1.5px solid #c7d7f8}
.ba{background:#3d7ae8;color:#fff;box-shadow:0 4px 16px rgba(61,122,232,.28)}
.br:active,.ba:active{transform:scale(.96);opacity:.85}
.bz{width:100%;background:#f5f7fb;color:#7a90a8;border:1.5px solid #dde6f5;border-radius:14px;padding:15px;font-size:.88em;font-weight:600;cursor:pointer;letter-spacing:.04em;margin-bottom:10px;transition:opacity .1s;-webkit-tap-highlight-color:transparent}
.bz:active{opacity:.7}
.bst{width:100%;background:#fff0f0;color:#e03030;border:1.5px solid #ffd0cd;border-radius:14px;padding:15px;font-size:.95em;font-weight:800;cursor:pointer;letter-spacing:.06em;transition:opacity .1s;-webkit-tap-highlight-color:transparent}
.bst:active{opacity:.7}
/* Division picker */
.dc{display:flex;align-items:center;gap:10px}
.pm{background:#eff4ff;border:1.5px solid #c7d7f8;border-radius:12px;width:52px;height:52px;font-size:1.6em;font-weight:300;cursor:pointer;color:#3d7ae8;flex-shrink:0;transition:opacity .1s;-webkit-tap-highlight-color:transparent}
.pm:active{opacity:.6}
.dv{flex:1;text-align:center;cursor:pointer}
.dn{font-size:2.5em;font-weight:800;color:#1a2d4a;line-height:1}
.dl{font-size:.6em;color:#7a90a8;letter-spacing:.1em;margin-top:4px;text-transform:uppercase}
.bl{background:#3d7ae8;border:none;border-radius:12px;width:52px;height:52px;cursor:pointer;display:flex;align-items:center;justify-content:center;flex-shrink:0;-webkit-tap-highlight-color:transparent;box-shadow:0 3px 10px rgba(61,122,232,.3)}
.bl:active{opacity:.7}
.bl svg{width:20px;height:20px;fill:#fff}
/* Toggles */
.mrow{display:flex;align-items:center;justify-content:space-between}
.mn{font-size:1em;font-weight:600;color:#1a2d4a}
.ms{font-size:.75em;color:#7a90a8;margin-top:4px}
.tg{width:50px;height:28px;background:#d0dae8;border-radius:14px;position:relative;cursor:pointer;transition:background .2s;flex-shrink:0;-webkit-tap-highlight-color:transparent}
.tg.on{background:#3d7ae8}
.tg::after{content:'';position:absolute;width:22px;height:22px;background:#fff;border-radius:50%;top:3px;left:3px;transition:left .2s;box-shadow:0 1px 4px rgba(0,0,0,.2)}
.tg.on::after{left:25px}
.sep{border:none;border-top:1px solid #e8eef6;margin:14px 0}
/* System info */
.sys{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.si{background:#f5f8ff;border-radius:12px;padding:10px 14px;border:1px solid #dde6f5}
.sv{font-size:.9em;font-weight:700;color:#1a2d4a}
.sk{font-size:.6em;color:#7a90a8;letter-spacing:.06em;margin-top:2px;text-transform:uppercase}
/* Keypad modal */
.kbd{display:none;position:fixed;inset:0;background:rgba(10,20,50,.4);z-index:100;align-items:flex-end;justify-content:center}
.kbd.show{display:flex}
.kbd-panel{background:#fff;border-radius:26px 26px 0 0;padding:22px 20px calc(22px + env(safe-area-inset-bottom));width:100%;max-width:430px}
.kbd-disp{text-align:center;font-size:3.4em;font-weight:800;color:#1a2d4a;min-height:1.15em;letter-spacing:-.02em;margin-bottom:6px}
.kbd-hint{text-align:center;font-size:.62em;color:#7a90a8;letter-spacing:.1em;text-transform:uppercase;margin-bottom:14px}
.kbd-chips{display:flex;flex-wrap:wrap;gap:6px;justify-content:center;margin-bottom:16px}
.kbd-chip{background:#eff4ff;color:#3d7ae8;border:1px solid #c7d7f8;border-radius:20px;padding:6px 13px;font-size:.78em;font-weight:700;cursor:pointer;-webkit-tap-highlight-color:transparent}
.kbd-chip:active{opacity:.7}
.kbd-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}
.kbd-btn{background:#f0f4fb;border:none;border-radius:14px;padding:18px 10px;font-size:1.3em;font-weight:600;cursor:pointer;color:#1a2d4a;-webkit-tap-highlight-color:transparent;transition:opacity .1s}
.kbd-btn:active{opacity:.55}
.kbd-ok{background:#3d7ae8;color:#fff;font-size:1em;font-weight:800;box-shadow:0 4px 14px rgba(61,122,232,.3)}
.kbd-del{background:#fff0f0;color:#e03030}
.kbd-cancel{background:#f5f7fb;color:#7a90a8;font-size:.85em}
</style>
</head>
<body>
<div class="hdr">
  <div><h1>DIVISEUR</h1><p>Cowells RGB61 &middot; 40:1 &middot; NEMA&nbsp;14 &middot; <span id="ver">v?</span></p></div>
  <div class="badge">WiFi</div>
</div>
<div class="sbar"><div class="dot" id="dot"></div><span id="sbarTxt">PR&#202;T</span></div>
<div class="tabs">
  <a class="tab on" href="/">&#9658; CONTR&#212;LEUR</a>
  <a class="tab" href="/diag">&#9658; DIAGNOSTIC</a>
</div>
<div class="pg">
  <div class="card">
    <div class="lbl">Division courante</div>
    <div class="big">
      <span class="bign" id="dc">0</span><span class="bigd">&thinsp;/&thinsp;<span id="dt">6</span></span>
    </div>
    <hr>
    <div class="met">
      <div><div class="mv" id="an">0.0&deg;</div><div class="ml">Angle</div></div>
      <div><div class="mv" id="pa">60.0&deg;</div><div class="ml">Pas / div.</div></div>
      <div><div class="mv" id="tp">OK</div><div class="ml">Temp. driver</div></div>
    </div>
  </div>

  <div class="card">
    <div class="nav">
      <button class="br" onclick="mv(-1)">&#9664; RECUL</button>
      <button class="ba" onclick="mv(1)">AVANCE &#9654;</button>
    </div>
    <button class="bz" onclick="home()">&#8635;&nbsp; REMETTRE &Agrave; Z&Eacute;RO</button>
    <button class="bst" onclick="stopMotor()">&#9632;&nbsp; STOP D&rsquo;URGENCE</button>
  </div>

  <div class="card">
    <div class="lbl">Nombre de divisions</div>
    <div class="dc">
      <button class="pm" onclick="chg(-1)">&#8722;</button>
      <div class="dv" onclick="kbdOpen()"><div class="dn" id="nd">6</div><div class="dl">Divisions &mdash; appuyer pour saisir</div></div>
      <button class="pm" onclick="chg(1)">+</button>
      <button class="bl" onclick="kbdOpen()">
        <svg viewBox="0 0 24 24"><rect x="4" y="5" width="4" height="4" rx="1"/><rect x="10" y="5" width="4" height="4" rx="1"/><rect x="16" y="5" width="4" height="4" rx="1"/><rect x="4" y="11" width="4" height="4" rx="1"/><rect x="10" y="11" width="4" height="4" rx="1"/><rect x="16" y="11" width="4" height="4" rx="1"/><rect x="4" y="17" width="4" height="4" rx="1"/><rect x="10" y="17" width="4" height="4" rx="1"/><rect x="16" y="17" width="4" height="4" rx="1"/></svg>
      </button>
    </div>
  </div>

  <div class="card">
    <div class="lbl">R&eacute;glages driver</div>
    <div class="mrow">
      <div><div class="mn" id="mdn">StealthChop</div><div class="ms" id="mds">Silencieux</div></div>
      <div class="tg" id="mdtg" onclick="tgm()"></div>
    </div>
    <hr class="sep">
    <div class="mrow">
      <div><div class="mn">Moteur</div><div class="ms" id="mes">D&eacute;sactiv&eacute;</div></div>
      <div class="tg" id="metg" onclick="tge()"></div>
    </div>
  </div>

  <div class="card">
    <div class="lbl">Syst&egrave;me</div>
    <div class="sys">
      <div class="si"><div class="sv" id="s-heap">— kB</div><div class="sk">RAM utilis&eacute;e</div></div>
      <div class="si"><div class="sv" id="s-flash">— kB</div><div class="sk">Flash utilis&eacute;</div></div>
    </div>
  </div>
</div>

<!-- Clavier numérique -->
<div class="kbd" id="kbd" onclick="kbdClose(event)">
  <div class="kbd-panel" onclick="event.stopPropagation()">
    <div class="kbd-disp" id="kbdDisp">6</div>
    <div class="kbd-hint">Divisions (2 &ndash; 360)</div>
    <div class="kbd-chips" id="kbdChips"></div>
    <div class="kbd-grid">
      <button class="kbd-btn" onclick="kk(7)">7</button>
      <button class="kbd-btn" onclick="kk(8)">8</button>
      <button class="kbd-btn" onclick="kk(9)">9</button>
      <button class="kbd-btn" onclick="kk(4)">4</button>
      <button class="kbd-btn" onclick="kk(5)">5</button>
      <button class="kbd-btn" onclick="kk(6)">6</button>
      <button class="kbd-btn" onclick="kk(1)">1</button>
      <button class="kbd-btn" onclick="kk(2)">2</button>
      <button class="kbd-btn" onclick="kk(3)">3</button>
      <button class="kbd-btn kbd-cancel" onclick="kbdCancel()">Ann.</button>
      <button class="kbd-btn" onclick="kk(0)">0</button>
      <button class="kbd-btn kbd-ok" onclick="kbdOk()">OK</button>
    </div>
  </div>
</div>

<script>
let sc=false,en=false,kv='';
const TEMP_LABELS=['OK','CHAUD','STOP'];
const TEMP_COLORS=['#34c759','#ff9500','#e03030'];
const PRESETS=[2,3,4,5,6,8,10,12,15,18,20,24,30,36,40,45,60,72,90,120,180,360];
function upd(){
  fetch('/api/status').then(r=>r.json()).then(d=>{
    document.getElementById('dc').textContent=d.currentDiv;
    document.getElementById('dt').textContent=d.divisions;
    document.getElementById('nd').textContent=d.divisions;
    const a=d.divisions>0?(d.currentDiv*360/d.divisions).toFixed(1):'0.0';
    const p=d.divisions>0?(360/d.divisions).toFixed(1):'0.0';
    document.getElementById('an').textContent=a+'\u00b0';
    document.getElementById('pa').textContent=p+'\u00b0';
    const t=Math.min(2,d.temp||0);
    const te=document.getElementById('tp');
    te.textContent=TEMP_LABELS[t];
    te.style.color=TEMP_COLORS[t];
    const moving=d.moving;
    document.getElementById('sbarTxt').textContent=moving?'EN MOUVEMENT':'PR\u00caT';
    document.getElementById('dot').className='dot'+(moving?' moving':'');
    if(d.version)document.getElementById('ver').textContent='v'+d.version;
    if(d.heap&&d.heapTotal){
      const used=Math.round((d.heapTotal-d.heap)/1024);
      const ht=Math.round(d.heapTotal/1024);
      document.getElementById('s-heap').textContent=used+'\u202fkB / '+ht+'\u202fkB ('+Math.round((d.heapTotal-d.heap)/d.heapTotal*100)+'%)';
    }
    if(d.sketch&&d.flash){
      const sk=Math.round(d.sketch/1024);
      const fl=Math.round(d.flash/1024);
      document.getElementById('s-flash').textContent=sk+'\u202fkB / '+fl+'\u202fkB ('+Math.round(d.sketch/d.flash*100)+'%)';
    }
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
  fetch('/api/divisions',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({n})}).then(()=>upd());
}
function tgm(){sc=!sc;renderMode();fetch('/api/mode',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({spreadCycle:sc})});}
function tge(){en=!en;renderEnable();fetch('/api/enable',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({enable:en})});}
/* Clavier numérique */
function kbdOpen(){
  kv=document.getElementById('nd').textContent;
  document.getElementById('kbdDisp').textContent=kv;
  const cc=document.getElementById('kbdChips');
  cc.innerHTML='';
  const cur=parseInt(kv);
  PRESETS.forEach(p=>{
    const b=document.createElement('button');
    b.className='kbd-chip'+(p===cur?' kbd-chip-on':'');
    b.textContent=p;
    b.onclick=()=>{kv=String(p);document.getElementById('kbdDisp').textContent=kv;};
    cc.appendChild(b);
  });
  document.getElementById('kbd').classList.add('show');
}
function kbdClose(e){if(e&&e.target===document.getElementById('kbd'))kbdCancel();}
function kbdCancel(){document.getElementById('kbd').classList.remove('show');}
function kk(d){
  if(kv.length>=3)return;
  kv+=d;
  const n=parseInt(kv);
  document.getElementById('kbdDisp').textContent=kv;
  if(n>360){kv=kv.slice(0,-1);document.getElementById('kbdDisp').textContent=kv;}
}
function kbdOk(){
  const n=parseInt(kv||'0');
  document.getElementById('kbd').classList.remove('show');
  if(n>=2&&n<=360)
    fetch('/api/divisions',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({n})}).then(()=>upd());
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
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>Diagnostic &mdash; Diviseur RGB61</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,'SF Pro Display',sans-serif;background:#eef2fb;color:#1a2d4a;max-width:430px;margin:auto;min-height:100vh;padding-bottom:env(safe-area-inset-bottom)}
.hdr{background:#fff;padding:14px 20px;padding-top:calc(14px + env(safe-area-inset-top));display:flex;align-items:center;justify-content:space-between;box-shadow:0 1px 0 #dde6f5}
.hdr h1{color:#1a2d4a;font-size:1.2em;font-weight:800;letter-spacing:.04em}
.hdr p{color:#7a90a8;font-size:.67em;margin-top:3px}
.badge{background:#eff4ff;color:#3d7ae8;font-size:.7em;font-weight:700;padding:4px 11px;border-radius:20px;border:1px solid #c7d7f8}
.sbar{background:#fff;padding:9px 20px;display:flex;align-items:center;gap:10px;box-shadow:0 1px 0 #dde6f5}
.dot{width:10px;height:10px;border-radius:50%;background:#34c759;box-shadow:0 0 5px #34c75950;flex-shrink:0}
.sbar span{color:#1a2d4a;font-weight:600;font-size:.82em;letter-spacing:.05em}
.tabs{background:#fff;display:flex;box-shadow:0 1px 0 #dde6f5}
.tab{flex:1;padding:12px 8px;text-align:center;color:#7a90a8;font-size:.72em;font-weight:700;letter-spacing:.07em;text-decoration:none;border-bottom:3px solid transparent;display:flex;align-items:center;justify-content:center;gap:5px}
.tab.on{color:#3d7ae8;border-bottom-color:#3d7ae8}
.pg{padding:14px 14px 6px}
/* Summary */
.sum{display:grid;grid-template-columns:repeat(4,1fr);gap:8px;margin-bottom:14px}
.sb{background:#fff;border-radius:14px;padding:14px 6px;text-align:center;box-shadow:0 2px 10px rgba(30,60,120,.06)}
.sn{font-size:2em;font-weight:800;line-height:1}
.sl{font-size:.58em;font-weight:700;letter-spacing:.1em;margin-top:5px;text-transform:uppercase}
.s-ok .sn,.s-ok .sl{color:#34c759}
.s-fl .sn,.s-fl .sl{color:#e03030}
.s-al .sn,.s-al .sl{color:#ff9500}
.s-pe .sn,.s-pe .sl{color:#7a90a8}
/* Step cards */
.card{background:#fff;border-radius:18px;padding:16px;margin-bottom:12px;box-shadow:0 2px 12px rgba(30,60,120,.07)}
.shdr{display:flex;align-items:flex-start;gap:10px;margin-bottom:12px}
.sbdg{background:#eff4ff;color:#3d7ae8;font-size:.65em;font-weight:800;padding:5px 10px;border-radius:8px;letter-spacing:.05em;white-space:nowrap;margin-top:2px;border:1px solid #c7d7f8}
.si{flex:1;min-width:0}
.st{font-weight:700;font-size:.88em;color:#1a2d4a}
.sd{font-size:.72em;color:#7a90a8;margin-top:3px;line-height:1.4}
.btest{background:#3d7ae8;color:#fff;border:none;border-radius:10px;padding:9px 14px;font-size:.75em;font-weight:700;cursor:pointer;white-space:nowrap;flex-shrink:0;-webkit-tap-highlight-color:transparent;box-shadow:0 3px 10px rgba(61,122,232,.25)}
.btest:active{opacity:.7}
.btest.running{background:#eff4ff;color:#7a90a8;box-shadow:none}
/* Test items */
.ti{display:flex;align-items:flex-start;gap:8px;padding:10px 12px;border-radius:10px;margin-bottom:6px;border-left:3px solid #e8eef6;background:#f8faff}
.ti-ok{border-left-color:#34c759;background:#f0fdf4}
.ti-fl{border-left-color:#e03030;background:#fff5f5}
.ti-al{border-left-color:#ff9500;background:#fffaf0}
.tid{font-size:.65em;color:#7a90a8;font-weight:600;min-width:28px;padding-top:2px;flex-shrink:0}
.tbdg{font-size:.62em;font-weight:700;padding:3px 7px;border-radius:6px;letter-spacing:.05em;white-space:nowrap;flex-shrink:0}
.bdg-ok{background:#d4f7e0;color:#1a8a3a}
.bdg-fl{background:#fde0e0;color:#c02020}
.bdg-al{background:#ffefd5;color:#b86000}
.bdg-pe{background:#e8eef6;color:#7a90a8}
.tb{flex:1;min-width:0}
.tn{font-weight:600;font-size:.83em;color:#1a2d4a}
.td{font-size:.7em;color:#7a90a8;margin-top:4px;line-height:1.4}
.tdr{font-size:.65em;color:#b8c8d8;flex-shrink:0;padding-top:2px;white-space:nowrap}
</style>
</head>
<body>
<div class="hdr">
  <div><h1>DIVISEUR</h1><p>Cowells RGB61 &middot; 40:1 &middot; NEMA&nbsp;14</p></div>
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
    <div class="sb s-fl"><div class="sn" id="cfl">0</div><div class="sl">Fail</div></div>
    <div class="sb s-al"><div class="sn" id="cal">0</div><div class="sl">Alerte</div></div>
    <div class="sb s-pe"><div class="sn" id="cpe">0</div><div class="sl">Attente</div></div>
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
        <button class="btest" id="btn${s.n}" onclick="run(${s.n},this)">&#9654; Tester</button>
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
function run(n,btn){
  if(btn){btn.textContent='\u23f3 Test\u2026';btn.className='btest running';btn.disabled=true;}
  fetch('/api/diag/run',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({step:n})})
    .then(()=>{setTimeout(load,200);})
    .catch(()=>load());
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
  int tempState = 0;
  if (motorEnabled) {
    if (driver.ot())        tempState = 2;
    else if (driver.otpw()) tempState = 1;
  }

  uint32_t freeHeap  = ESP.getFreeHeap();
  uint32_t heapTotal = ESP.getHeapSize();
  uint32_t sketch    = ESP.getSketchSize();
  uint32_t flash     = ESP.getFlashChipSize();

  char json[512];
  snprintf(json, sizeof(json),
    "{\"divisions\":%d,\"currentDiv\":%d,\"steps\":%ld,"
    "\"enabled\":%s,\"moving\":%s,\"rssi\":%d,\"uptime\":%lu,"
    "\"temp\":%d,\"spreadCycle\":%s,"
    "\"heap\":%u,\"heapTotal\":%u,\"sketch\":%u,\"flash\":%u,"
    "\"version\":\"" FW_VERSION "\"}",
    numDivisions,
    currentDivision(),
    stepper.currentPosition(),
    motorEnabled ? "true" : "false",
    stepper.isRunning() ? "true" : "false",
    WiFi.RSSI(),
    millis(),
    tempState,
    spreadCycleMode ? "true" : "false",
    freeHeap, heapTotal, sketch, flash);
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
