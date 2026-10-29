#include "lumos/webui/web_ui.hpp"
#include "lumos/wifi/wifi_service.hpp"

namespace lumos {
namespace {

constexpr const char* kIndexHtml = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>LumosOS</title>
<style>
:root{--bg:#0e1116;--card:#171c24;--text:#e8edf5;--muted:#8b95a8;--accent:#6cb6ff;--line:#2a3340}
*{box-sizing:border-box}body{margin:0;font:15px/1.45 system-ui,sans-serif;background:var(--bg);color:var(--text)}
header{padding:1.25rem 1.25rem .5rem}h1{margin:0;font-size:1.4rem;letter-spacing:.04em}
p{color:var(--muted)}main{padding:0 1.25rem 2rem;display:grid;gap:1rem;max-width:720px}
section{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:1rem}
label{display:block;margin:.5rem 0 .25rem;color:var(--muted);font-size:.85rem}
input,select,button{width:100%;padding:.65rem .75rem;border-radius:8px;border:1px solid var(--line);background:#0f141b;color:var(--text)}
button{background:var(--accent);color:#041018;border:none;font-weight:600;margin-top:.75rem;cursor:pointer}
button.secondary{background:transparent;color:var(--accent);border:1px solid var(--accent)}
.row{display:grid;grid-template-columns:1fr auto;gap:.75rem;align-items:end}
.grid2{display:grid;grid-template-columns:1fr 1fr;gap:.75rem}
.grid4{display:grid;grid-template-columns:repeat(4,1fr);gap:.5rem}
pre{white-space:pre-wrap;background:#0f141b;padding:.75rem;border-radius:8px;font-size:.8rem;color:var(--muted)}
.hint{font-size:.8rem;color:var(--muted);margin-top:.5rem}
.check{display:flex;align-items:center;gap:.5rem;margin:.75rem 0 .25rem;color:var(--text)}
.check input{width:auto}
#staticFields{display:none}
#staticFields.show{display:block}
.kv{display:grid;grid-template-columns:7.5rem 1fr;gap:.2rem .75rem;margin:0 0 .9rem;font-size:.9rem}
.kv dt{color:var(--muted);margin:0}
.kv dd{margin:0;word-break:break-all}
.preview-wrap{position:relative;width:100%;aspect-ratio:16/9;background:#07090d;border-radius:10px;overflow:hidden;border:1px solid var(--line)}
.preview-wrap canvas{display:block;width:100%;height:100%;cursor:crosshair}
.preview-wrap.cal-on{outline:1px solid #c9a227}
.param{margin-top:.5rem}
.advanced{opacity:.85}
.pair{display:flex;gap:.75rem;align-items:flex-start;padding:.9rem;border-radius:10px;border:1px solid var(--line);margin:0 0 .75rem}
.pair .dot{width:.7rem;height:.7rem;border-radius:50%;margin-top:.28rem;flex:0 0 auto}
.pair h3{margin:0;font-size:1.02rem;color:var(--text)}
.pair p{margin:.25rem 0 0;font-size:.85rem}
.pair.ok{border-color:#2f6f52;background:#102018}
.pair.ok .dot{background:#3ecf8e}
.pair.no{border-color:#7a5a28;background:#1c160e}
.pair.no .dot{background:#e0a04a}
.pair.wait{border-color:#2a5a7a;background:#101820}
.pair.wait .dot{background:#6cb6ff}
</style>
</head>
<body>
<header>
  <h1>LumosOS</h1>
  <p>Recovery &amp; local configuration · API 0.3</p>
  <p><a href="/doorbell" style="color:var(--accent);font-weight:600;text-decoration:none">Doorbell →</a></p>
</header>
<main>
<section>
<h2>Live preview</h2>
<label class="check"><input id="livePreview" type="checkbox" onchange="toggleLivePreview()"/> Enable live preview (~10 FPS; uses Wi‑Fi, can flicker LEDs)</label>
<div class="preview-wrap"><canvas id="ledPreview" width="640" height="360"></canvas></div>
<p class="hint" id="previewHint">Off by default. TV view shows active HyperHDR LEDs only.</p>
</section>
<section>
<h2>Status</h2>
<div id="pairCard" class="pair no"><div class="dot"></div><div><h3>Doorbell</h3><p>Loading pairing status…</p></div></div>
<pre id="status">Loading…</pre>
</section>
<section>
<h2>WiFi</h2>
<dl id="wifiNow" class="kv">
  <dt>SSID</dt><dd id="wifiSsid">—</dd>
  <dt>Status</dt><dd id="wifiStatus">—</dd>
  <dt>Mode</dt><dd id="wifiMode">—</dd>
  <dt>Current IP</dt><dd id="wifiIp">—</dd>
  <dt>Gateway</dt><dd id="wifiGw">—</dd>
  <dt>Netmask</dt><dd id="wifiMask">—</dd>
  <dt>DNS</dt><dd id="wifiDns">—</dd>
  <dt>ESP MAC</dt><dd id="wifiMac">—</dd>
</dl>
<label>Nearby networks</label>
<div class="row">
  <select id="netlist"><option value="">Scanning…</option></select>
  <button class="secondary" type="button" onclick="scanWifi()" style="margin-top:0;width:auto;padding:.65rem 1rem">Scan</button>
</div>
<label>SSID</label><input id="ssid" placeholder="Select above or type manually"/>
<label>Password</label><input id="pass" type="password" placeholder="Leave blank to keep saved password"/>
<label class="check"><input id="useStatic" type="checkbox" onchange="toggleStatic()"/> Use static IP</label>
<div id="staticFields">
  <div class="grid2">
    <div><label>IP address</label><input id="ip" placeholder="192.168.1.50"/></div>
    <div><label>Gateway</label><input id="gateway" placeholder="192.168.1.1"/></div>
  </div>
  <label>Subnet mask</label><input id="netmask" placeholder="255.255.255.0"/>
  <div class="grid2">
    <div><label>DNS 1</label><input id="dns1"/></div>
    <div><label>DNS 2</label><input id="dns2"/></div>
  </div>
</div>
<button onclick="saveWifi()">Save &amp; Connect</button>
<button id="retryWifiBtn" class="secondary" type="button" onclick="retryWifi()" style="display:none">Retry home Wi-Fi</button>
<p id="wifiHint" class="hint"></p>
</section>
<section>
<h2>Strip</h2>
<p class="hint" id="stripCounts">Physical LEDs: — · Active (HyperHDR): —</p>
<label>GPIO</label><input id="gpio" type="number" min="0" max="39"/>
<label>Chipset</label>
<select id="chipset">
  <option value="0">WS2815 (RGB)</option>
  <option value="1">WS2812B</option>
  <option value="2">WS2813</option>
  <option value="3">SK6812 RGB</option>
  <option value="4">SK6812 RGBW</option>
  <option value="5">WS2815 RGBW</option>
</select>
<label>Color order</label>
<select id="colorOrder" onchange="applyColorOrderLive()">
  <option value="0">GRB (WS2812/WS2815 common)</option>
  <option value="1">RGB</option>
  <option value="2">BRG</option>
  <option value="3">RBG</option>
  <option value="4">GBR</option>
  <option value="5">BGR</option>
</select>
<p class="hint" id="colorOrderHint">Color order applies live. LED counts are set in Calibration.</p>
<button onclick="saveStrip()">Save strip hardware</button>
<!-- hidden layout fields kept for preview geometry -->
<input id="ledCount" type="hidden" value="140"/>
<input id="layTop" type="hidden" value="44"/>
<input id="layRight" type="hidden" value="26"/>
<input id="layBottom" type="hidden" value="44"/>
<input id="layLeft" type="hidden" value="26"/>
<p class="hint" id="layoutSum" style="display:none"></p>
</section>
<section>
<h2>Calibration wizard</h2>
<p class="hint" id="wizStepLabel">Step 1 / 6 — Orientation</p>
<div id="wiz0">
  <p class="hint">Where does the data wire start, and which way does the strip run?</p>
  <label>Wire starts at corner</label>
  <select id="periStart">
    <option value="0">Top-left</option>
    <option value="1">Top-right</option>
    <option value="2">Bottom-right</option>
    <option value="3">Bottom-left</option>
  </select>
  <label>Wire direction</label>
  <select id="periDir">
    <option value="0">Clockwise</option>
    <option value="1">Counter-clockwise (inverse)</option>
  </select>
  <button type="button" onclick="wizSaveOrient()">Save &amp; test sides</button>
  <p class="hint">Legend: <span style="color:#ff5050">Red=Top</span> · <span style="color:#50ff50">Green=Right</span> · <span style="color:#5078ff">Blue=Bottom</span> · <span style="color:#ffc828">Amber=Left</span></p>
</div>
<div id="wiz1" style="display:none">
  <p class="hint">Light the first N LEDs. Increase until the last LED on the strip lights (nothing beyond).</p>
  <label>N (prefix)</label><input id="prefixN" type="number" min="1" max="2000" value="1"/>
  <div class="grid4">
    <button type="button" onclick="wizPrefixDelta(-10)">−10</button>
    <button type="button" onclick="wizPrefixDelta(-1)">−1</button>
    <button type="button" onclick="wizPrefixDelta(1)">+1</button>
    <button type="button" onclick="wizPrefixDelta(10)">+10</button>
  </div>
  <button type="button" onclick="wizLightPrefix()">Light first N</button>
  <button type="button" onclick="wizSavePhysical()">Save as physical LED count</button>
  <p class="hint">Saving a new physical count reboots the device.</p>
</div>
<div id="wiz2" style="display:none">
  <p class="hint">Unused LEDs at the wire ends (always off).</p>
  <div class="grid2">
    <div><label>Skip start</label><input id="skipStart" type="number" min="0" max="500" value="0"/></div>
    <div><label>Skip end</label><input id="skipEnd" type="number" min="0" max="500" value="0"/></div>
  </div>
  <button type="button" onclick="wizSaveSkips()">Save skips &amp; preview</button>
</div>
<div id="wiz3" style="display:none">
  <p class="hint">Disable LEDs that must stay off and are <b>not</b> part of any TV edge (true folds / dead pixels). Do not mark corner LEDs you will include when measuring edges — edge measure clears ignores inside each confirmed range.</p>
  <label class="check"><input id="calPick" type="checkbox" onchange="toggleCalPick()"/> Tap preview to toggle ignore</label>
  <div class="grid2">
    <div><label>Wire index</label><input id="midIndex" type="number" min="0" max="2000" value="0"/></div>
    <button type="button" onclick="wizToggleMidIndex()" style="margin-top:1.5rem">Toggle index</button>
  </div>
  <button class="secondary" type="button" onclick="clearIgnores()">Clear middle ignores</button>
  <p class="hint" id="calStatus">Ignored: 0</p>
</div>
<div id="wiz4" style="display:none">
  <p class="hint" id="edgePrompt">Light the <b>first</b> non-skipped LED of this edge.</p>
  <label>Wire index</label><input id="edgeIdx" type="number" min="0" max="2000" value="0"/>
  <div class="grid2">
    <button type="button" onclick="wizEdgeLight()">Light this index</button>
    <button type="button" onclick="wizEdgeConfirm()" style="margin-top:.75rem">Confirm</button>
  </div>
  <p class="hint">Each LED belongs to one edge. A lower last index wraps through 0 (110 then 9). Skipped LEDs stay off. The next edge starts at the next LED that is not skipped.</p>
  <p class="hint" id="edgeStatus">—</p>
</div>
<div id="wiz5" style="display:none">
  <h3 style="margin:.25rem 0">HyperHDR</h3>
  <pre id="hhCard">—</pre>
  <button type="button" onclick="wizCopyHh()">Copy summary</button>
  <button class="secondary" type="button" onclick="runCalMode('sides')">Re-test side colors</button>
  <button class="secondary" type="button" onclick="runCalMode('map')">Show active/ignored map</button>
</div>
<div class="grid2">
  <button class="secondary" type="button" id="wizBack" onclick="wizNav(-1)" style="margin-top:.75rem">Back</button>
  <button type="button" id="wizNext" onclick="wizNav(1)" style="margin-top:.75rem">Next</button>
</div>
</section>
<section>
<h2>Lighting</h2>
<label>Plugin</label><select id="plugin" onchange="renderPluginParams()"></select>
<div id="pluginParams"></div>
<label>Brightness (0–255)</label><input id="brightness" type="number" min="0" max="255"/>
<p class="hint" id="lightingHint">Apply to set plugin/brightness. HyperHDR and Lumos Vision cannot override while a local plugin is active.</p>
<label>Channel balance R / G / B (255 = unity)</label>
<div class="grid4">
  <input id="balR" type="number" min="0" max="255" value="255" title="Red gain"/>
  <input id="balG" type="number" min="0" max="255" value="255" title="Green gain"/>
  <input id="balB" type="number" min="0" max="255" value="255" title="Blue gain"/>
  <button type="button" onclick="applyBalanceLive()">Apply balance</button>
</div>
<p class="hint">RGBW strips often need green ~80–100 so yellow is amber, not lime. Applies live.</p>
<button onclick="applyLighting()">Apply</button>
</section>
<section>
<h2>Backup &amp; restore</h2>
<p class="hint">Download a JSON config from this device, then upload it on a new ESP32 after flashing the same firmware. One-shot clone of strip, lighting, plugin params, and optional Wi‑Fi.</p>
<label class="check"><input id="cfgSecrets" type="checkbox"/> Include Wi‑Fi password in download</label>
<label class="check"><input id="cfgClearIp" type="checkbox" checked/> On import: clear static IP (use for a second device on the same LAN)</label>
<div class="grid2">
  <button type="button" onclick="downloadConfig()">Download config JSON</button>
  <button class="secondary" type="button" onclick="cfgFile.click()" style="margin-top:.75rem">Upload config JSON…</button>
</div>
<input id="cfgFile" type="file" accept="application/json,.json" style="display:none" onchange="uploadConfig(event)"/>
<pre id="cfgStatus"></pre>
</section>
<section>
<h2>Nearby LumosOS</h2>
<pre id="neighbors">Loading…</pre>
<button class="secondary" type="button" onclick="loadNeighbors()">Refresh</button>
<p class="hint">Read-only mDNS discovery on <code>_lumosos._tcp</code>. Open a peer by IP.</p>
</section>
<section>
<h2>OTA Update</h2>
<input id="firmware" type="file" accept=".bin"/>
<button onclick="uploadOta()">Upload Firmware</button>
<pre id="ota"></pre>
</section>
</main>
<script>
let wsLive=false;
let ledRgb=null;
let previewCount=0;
let layoutSides={top:44,right:26,bottom:44,left:26};
let activeToPhysical=[]; // logical CW-from-TL → physical wire index
let geometryValid=false;
let pluginsCache=[];
let pluginDirty=false; // user changed dropdown; don't let 5s refresh snap it back
let ignoredSet=new Set();
let ledPositions=[]; // {i,x,y} physical wire index
let calPickOn=false;
let wizStep=0;
let edgePhase=0; // pairs of first/last along wire_side_order
let edgeRanges={top:[0,0],right:[0,0],bottom:[0,0],left:[0,0]};
let edgeRangesSet=false;
const sideKeys=['top','right','bottom','left'];
const sideLabels=['TOP','RIGHT','BOTTOM','LEFT'];
const wizLabels=['Orientation','Find total LEDs','Skip front / end','Middle disables','Measure edges','HyperHDR summary'];
async function j(url,opts){const r=await fetch(url,opts); if(!r.ok) throw new Error(await r.text()); return r.json()}
function toggleStatic(){document.getElementById('staticFields').classList.toggle('show', useStatic.checked);}
function applyLayoutSides(sides){
  layTop.value=sides.top; layRight.value=sides.right;
  layBottom.value=sides.bottom; layLeft.value=sides.left;
  layoutSides=sides;
}
function updateStripCounts(phys, active){
  stripCounts.textContent='Physical LEDs: '+(phys??'—')+' · Active (HyperHDR): '+(active??'—');
}
function updateLayoutSum(){}
// Wire travel order of TV sides (matches firmware wire_side_order).
function wireSideOrder(start, dir){
  const cwFirst=[0,1,2,3]; // TL→Top, TR→Right, BR→Bottom, BL→Left
  const ccwFirst=[3,0,1,2]; // TL→Left, TR→Top, BR→Right, BL→Bottom
  const first=(dir===1?ccwFirst:cwFirst)[start|0];
  const step=dir===1?-1:1;
  const out=[];
  for(let i=0;i<4;i++) out.push((first+step*i+4)%4);
  return out;
}
function stripLength(){ return Number(ledCount.value)||Number(prefixN.value)||0; }
function isSkippedIndex(i, n){
  n=n||stripLength();
  if(n<=0 || i<0 || i>=n) return true;
  const skip0=Math.min(Number(skipStart.value)||0, n);
  const skip1=Math.min(Number(skipEnd.value)||0, n-skip0);
  if(i<skip0) return true;
  if(skip1>0 && i>=n-skip1) return true;
  return ignoredSet.has(i);
}
function nextActiveIndex(after){
  const n=stripLength();
  if(n<=0) return 0;
  let i=(Number(after)+1)%n;
  for(let s=0;s<n;s++){
    if(!isSkippedIndex(i, n)) return i;
    i=(i+1)%n;
  }
  return Number(after)||0;
}
function firstActiveAtOrAfter(i){
  const n=stripLength();
  if(n<=0) return 0;
  i=((Number(i)%n)+n)%n;
  for(let s=0;s<n;s++){
    if(!isSkippedIndex(i, n)) return i;
    i=(i+1)%n;
  }
  return 0;
}
function walkEdge(n, start, end){
  const out=[];
  if(!n || start<0 || end<0 || start>=n || end>=n) return out;
  let i=start;
  for(let s=0;s<n;s++){
    if(!isSkippedIndex(i, n)) out.push(i);
    if(i===end) break;
    i=(i+1)%n;
  }
  return out;
}
function currentEdgeSide(){
  const order=wireSideOrder(Number(periStart.value)||0, Number(periDir.value)||0);
  return order[Math.floor(edgePhase/2)]|0;
}
// Build logical→physical when API map missing (skips + ignores + orientation + layout).
function buildActiveToPhysicalLocal(physCount){
  if(edgeRangesSet && physCount){
    const rev=(Number(periDir.value)||0)===1;
    const walked={};
    for(const k of sideKeys){
      const r=edgeRanges[k]||[0,0];
      walked[k]=walkEdge(physCount, r[0]|0, r[1]|0);
    }
    const counts={top:walked.top.length,right:walked.right.length,bottom:walked.bottom.length,left:walked.left.length};
    const active=counts.top+counts.right+counts.bottom+counts.left;
    const base={top:0,right:counts.top,bottom:counts.top+counts.right,left:counts.top+counts.right+counts.bottom};
    const map=new Array(active);
    for(const k of sideKeys){
      const leds=walked[k].slice();
      if(rev) leds.reverse();
      for(let i=0;i<leds.length;i++) map[base[k]+i]=leds[i];
    }
    return map;
  }
  const T=layoutSides.top|0,R=layoutSides.right|0,B=layoutSides.bottom|0,L=layoutSides.left|0;
  const active=T+R+B+L;
  if(!physCount||!active) return [];
  const skip0=Math.min(Number(skipStart.value)||0, physCount);
  const skip1=Math.min(Number(skipEnd.value)||0, physCount-skip0);
  const span=[];
  for(let w=skip0;w<physCount-skip1;w++){ if(!ignoredSet.has(w)) span.push(w); }
  if(span.length<active) return span.slice();
  const start=Number(periStart.value)||0, dir=Number(periDir.value)||0;
  const order=wireSideOrder(start, dir);
  const counts=[T,R,B,L];
  const base=[0,T,T+R,T+R+B];
  const rev=dir===1;
  const map=new Array(active);
  let wire=0;
  for(const side of order){
    const n=counts[side];
    for(let i=0;i<n;i++){
      const logical=base[side]+(rev?(n-1-i):i);
      if(wire<span.length && logical<active) map[logical]=span[wire];
      wire++;
    }
  }
  return map;
}
function setPairCard(el,d,peer){
  if(!el) return;
  const pairing=!!(d&&(d.pairing||d.scanning));
  const mac=(peer||(d&&(d.paired_tx_mac||d.rx_mac))||'').trim();
  const paired=!!(d&&d.paired&&mac);
  el.className='pair '+(pairing?'wait':(paired?'ok':'no'));
  const title=pairing?'Pairing…':(paired?'Paired with doorbell TX':'Doorbell not paired');
  let detail;
  if(pairing) detail='ESP-NOW discovery is running. Being on the same Wi‑Fi is not enough.';
  else if(paired) detail='Transmitter '+mac+(d.enabled===false?' · receiver is disabled.':' · ESP-NOW link saved.');
  else detail='Same Wi‑Fi does not pair the boards. Open /doorbell → Start pairing, then Find nearby on LumosOS-Bell.';
  el.innerHTML='<div class="dot"></div><div><h3>'+title+'</h3><p>'+detail+'</p></div>';
}
function renderStatus(s){
  const view=Object.assign({},s); delete view.type;
  setPairCard(document.getElementById('pairCard'), s.doorbell||{}, (s.doorbell&&s.doorbell.paired_tx_mac)||'');
  status.textContent=JSON.stringify(view,null,2);
  if(typeof s.brightness==='number') brightness.value=s.brightness;
  if(!ip.value && s.wifi && s.wifi.ip) ip.value=s.wifi.ip;
  if(!gateway.value && s.wifi && s.wifi.gateway) gateway.value=s.wifi.gateway;
  if((!netmask.value || netmask.value==='255.255.255.0') && s.wifi && s.wifi.netmask) netmask.value=s.wifi.netmask;
  if(!dns1.value && s.wifi && s.wifi.dns1) dns1.value=s.wifi.dns1;
  if(!dns2.value && s.wifi && s.wifi.dns2) dns2.value=s.wifi.dns2;
  updateWifiRetryUi(s.wifi);
  renderWifiNow(s.wifi);
}
function dash(v){ return (v==null || v==='')?'—':v; }
function renderWifiNow(w){
  if(!w) return;
  const setup=!!w.setup_mode;
  const connected=!!w.connected;
  document.getElementById('wifiSsid').textContent=dash(w.ssid);
  document.getElementById('wifiStatus').textContent=connected?'Connected':(setup?'Setup AP':'Disconnected');
  document.getElementById('wifiMode').textContent=connected?'sta':(setup?'ap':'—');
  document.getElementById('wifiIp').textContent=dash(w.ip);
  document.getElementById('wifiGw').textContent=dash(w.gateway);
  document.getElementById('wifiMask').textContent=dash(w.netmask);
  const dns=[w.dns1,w.dns2].filter(Boolean).join(', ');
  document.getElementById('wifiDns').textContent=dash(dns);
  document.getElementById('wifiMac').textContent=dash(w.mac);
  updateLightingHint();
}
function updateWifiRetryUi(w){
  const btn=document.getElementById('retryWifiBtn');
  if(!btn) return;
  btn.style.display=(w && w.setup_mode && w.has_saved_wifi)?'block':'none';
}
function hexToBytes(hex){
  const out=new Uint8Array(hex.length/2);
  for(let i=0;i<out.length;i++) out[i]=parseInt(hex.substr(i*2,2),16);
  return out;
}
function drawPreview(){
  const canvas=ledPreview, ctx=canvas.getContext('2d');
  const W=canvas.width, H=canvas.height;
  ledPositions=[];
  ctx.clearRect(0,0,W,H); ctx.fillStyle='#07090d'; ctx.fillRect(0,0,W,H);
  const pad=22;
  ctx.fillStyle='#121722'; ctx.strokeStyle='#2a3340'; ctx.lineWidth=2;
  roundRect(ctx,pad+16,pad+16,W-2*(pad+16),H-2*(pad+16),8); ctx.fill(); ctx.stroke();
  const T=layoutSides.top|0,R=layoutSides.right|0,B=layoutSides.bottom|0,L=layoutSides.left|0;
  const act=T+R+B+L;
  // Count lit among active (TV) LEDs only.
  let map=activeToPhysical;
  if(map.length!==act) map=buildActiveToPhysicalLocal(previewCount);
  let lit=0;
  if(ledRgb && map.length===act){
    for(let i=0;i<act;i++){
      const phys=map[i]; if(phys==null||phys>=previewCount) continue;
      const o=phys*3; if(ledRgb[o]|ledRgb[o+1]|ledRgb[o+2]) lit++;
    }
  }
  const skip0=Number(skipStart.value)||0, skip1=Number(skipEnd.value)||0;
  ctx.fillStyle='#8b95a8'; ctx.font='13px system-ui,sans-serif'; ctx.textAlign='center';
  ctx.fillText(act?(lit+' lit · TV '+act+' ('+T+'/'+R+'/'+B+'/'+L+')'+(geometryValid?'':' · calib incomplete')):'Waiting for calibration…', W/2, H/2-8);
  if(previewCount) ctx.fillText('wire '+previewCount+' · skip '+skip0+'+'+skip1+' (hidden)', W/2, H/2+12);
  if(!previewCount || act<=0 || map.length!==act) return;

  // TV view only: active HyperHDR LEDs on the four sides (CW from top-left).
  // Skips / unused wire LEDs are intentionally not drawn.
  const band=Math.max(5, Math.min(11, Math.floor(Math.min(W,H)/(Math.max(T,R,B,L)+8))));
  let logical=0;
  const put=(count, xy)=>{
    for(let i=0;i<count;i++,logical++){
      const t=count<=1?0.5:i/(count-1);
      const p=xy(t);
      const phys=map[logical];
      if(phys==null||phys>=previewCount) continue;
      ledPositions.push({i:phys,x:p.x,y:p.y});
      drawLed(ctx,p.x,p.y,band,ledRgb,phys);
    }
  };
  put(T, t=>({x:pad+t*(W-2*pad), y:pad}));
  put(R, t=>({x:W-pad, y:pad+t*(H-2*pad)}));
  put(B, t=>({x:W-pad-t*(W-2*pad), y:H-pad}));
  put(L, t=>({x:pad, y:H-pad-t*(H-2*pad)}));
  if(typeof calStatus!=='undefined' && calStatus) calStatus.textContent='Middle ignored: '+ignoredSet.size;
}
function drawLed(ctx,x,y,size,rgb,idx){
  const ignored=ignoredSet.has(idx);
  let r=28,g=34,b=44;
  if(rgb){ r=rgb[idx*3]; g=rgb[idx*3+1]; b=rgb[idx*3+2]; }
  const lit=r|g|b; if(!lit){ r=28;g=34;b=44; }
  if(ignored){ r=90; g=50; b=20; }
  ctx.beginPath(); ctx.fillStyle='rgb('+r+','+g+','+b+')';
  if(lit && !ignored && !livePreviewOn){ ctx.shadowColor='rgba('+r+','+g+','+b+',0.55)'; ctx.shadowBlur=10; }
  ctx.arc(x,y,size/2,0,Math.PI*2); ctx.fill(); ctx.shadowBlur=0;
  if(ignored){
    ctx.beginPath(); ctx.strokeStyle='#c9a227'; ctx.lineWidth=1.5;
    ctx.arc(x,y,size/2+1,0,Math.PI*2); ctx.stroke();
  }
}
function roundRect(ctx,x,y,w,h,r){
  ctx.beginPath(); ctx.moveTo(x+r,y); ctx.arcTo(x+w,y,x+w,y+h,r); ctx.arcTo(x+w,y+h,x,y+h,r);
  ctx.arcTo(x,y+h,x,y,r); ctx.arcTo(x,y,x+w,y,r); ctx.closePath();
}
async function loadSettings(){
  try{
    const s=await j('/api/v1/settings');
    ssid.value=s.wifi_ssid||'';
    useStatic.checked=!!s.wifi_use_static;
    ip.value=s.wifi_ip||''; gateway.value=s.wifi_gateway||'';
    netmask.value=s.wifi_netmask||'255.255.255.0';
    dns1.value=s.wifi_dns1||''; dns2.value=s.wifi_dns2||'';
    if(typeof s.led_count==='number'){ ledCount.value=s.led_count; prefixN.value=s.led_count; }
    if(typeof s.brightness==='number') brightness.value=s.brightness;
    if(typeof s.gpio==='number') gpio.value=s.gpio;
    if(typeof s.chipset==='number') chipset.value=String(s.chipset);
    if(typeof s.color_order==='number') colorOrder.value=String(s.color_order);
    if(typeof s.balance_r==='number') balR.value=s.balance_r;
    if(typeof s.balance_g==='number') balG.value=s.balance_g;
    if(typeof s.balance_b==='number') balB.value=s.balance_b;
    if(typeof s.perimeter_start==='number') periStart.value=String(s.perimeter_start);
    if(typeof s.perimeter_direction==='number') periDir.value=String(s.perimeter_direction);
    if(s.edge_ignore){
      skipStart.value=s.edge_ignore.skip_start||0;
      skipEnd.value=s.edge_ignore.skip_end||0;
    }
    ignoredSet=new Set(Array.isArray(s.ignored_leds)?s.ignored_leds:[]);
    edgeRangesSet=!!s.edge_ranges_set && !!s.edge_ranges;
    if(s.edge_ranges){
      for(const k of sideKeys){
        const r=s.edge_ranges[k];
        if(Array.isArray(r) && r.length>=2) edgeRanges[k]=[Number(r[0]), Number(r[1])];
      }
    }
    if(s.layout){
      layTop.value=s.layout.top; layRight.value=s.layout.right;
      layBottom.value=s.layout.bottom; layLeft.value=s.layout.left;
      layoutSides={top:s.layout.top,right:s.layout.right,bottom:s.layout.bottom,left:s.layout.left};
    }
    activeToPhysical=Array.isArray(s.active_to_physical)?s.active_to_physical.map(Number):[];
    geometryValid=!!s.geometry_valid;
    updateStripCounts(s.physical_led_count||s.led_count, s.active_led_count||(s.layout?(s.layout.top+s.layout.right+s.layout.bottom+s.layout.left):0));
    if(s.hyperhdr) updateHhCard(s.hyperhdr, s.geometry_valid);
    toggleStatic();
    drawPreview();
  }catch{}
}
function updateLightingHint(){
  if(!lightingHint) return;
  const id=plugin.value;
  if(id==='hyperhdr'){
    lightingHint.textContent='HyperHDR active — stream can take over lighting.';
    return;
  }
  if(id==='lumos_vision'){
    const ipEl=document.getElementById('wifiIp');
    const ip=(ipEl&&ipEl.textContent&&ipEl.textContent!=='—')?ipEl.textContent:'this box IP';
    lightingHint.textContent='Lumos Vision Direct DDP — host '+ip+', UDP 4048, RGBW 4-byte. Vision converts at white_kelvin; this box does not extract white again.';
    return;
  }
  lightingHint.textContent='Local control on — HyperHDR WLED and Lumos Vision output-switch will not steal plugin/brightness. Select HyperHDR or Lumos Vision + Apply to hand back.';
}
function renderPluginParams(){
  updateLightingHint();
  const box=pluginParams; box.innerHTML='';
  const plug=pluginsCache.find(p=>p.id===plugin.value);
  if(!plug || !plug.parameters) return;
  const showAdv=false;
  for(const p of plug.parameters){
    if(p.advanced && !showAdv) continue;
    const wrap=document.createElement('div'); wrap.className='param'+(p.advanced?' advanced':'');
    const lab=document.createElement('label');
    lab.textContent=p.name+(p.unit?(' ('+p.unit+')'):'');
    wrap.appendChild(lab);
    let el;
    if(p.type==='enum'){
      el=document.createElement('select');
      for(const v of (p.enum||[])){ const o=document.createElement('option'); o.value=v; o.textContent=v; el.appendChild(o); }
    } else if(p.type==='bool'){
      el=document.createElement('select');
      el.innerHTML='<option value="0">Off</option><option value="1">On</option>';
    } else {
      el=document.createElement('input');
      el.type=p.type==='float'?'number':'number';
      if(p.min!=null) el.min=p.min; if(p.max!=null) el.max=p.max; if(p.step) el.step=p.step;
    }
    el.id='param_'+p.id; el.dataset.paramId=p.id; el.value=p.default||'';
    wrap.appendChild(el);
    if(p.description){ const h=document.createElement('p'); h.className='hint'; h.textContent=p.description; wrap.appendChild(h); }
    box.appendChild(wrap);
  }
}
async function refreshPlugins(){
  const p=await j('/api/v1/plugins');
  pluginsCache=p.plugins||[];
  const sel=plugin; const current=sel.value;
  // Prefer the user's in-progress choice over active, so HyperHDR/status polls don't
  // yank the dropdown back before Apply.
  const prefer=pluginDirty?current:(p.active||current);
  sel.innerHTML='';
  for(const plug of pluginsCache){
    const o=document.createElement('option');
    o.value=plug.id; o.textContent=plug.name+(plug.capabilities&&plug.capabilities.category?(' · '+plug.capabilities.category):'');
    if(plug.id===prefer) o.selected=true;
    sel.appendChild(o);
  }
  renderPluginParams();
  updateLightingHint();
}
plugin.addEventListener('change',()=>{ pluginDirty=true; updateLightingHint(); });
async function refresh(){
  try{
    if(!wsLive){ renderStatus(await j('/api/v1/status')); }
    await refreshPlugins();
  }catch(e){ status.textContent='Status unavailable: '+e.message; }
}
async function scanWifi(){
  netlist.innerHTML='<option value="">Scanning…</option>';
  try{
    const data=await j('/api/v1/wifi/scan');
    netlist.innerHTML='';
    const blank=document.createElement('option');
    blank.value=''; blank.textContent=data.networks.length?'Select a network…':'No networks found';
    netlist.appendChild(blank);
    for(const n of data.networks){
      const o=document.createElement('option'); o.value=n.ssid;
      o.textContent=n.ssid+'  ('+n.rssi+' dBm)'; netlist.appendChild(o);
    }
  }catch{ netlist.innerHTML='<option value="">Scan failed</option>'; }
}
netlist.addEventListener('change',e=>{ if(e.target.value) ssid.value=e.target.value; });
async function saveWifi(){
  const name=ssid.value.trim();
  if(!name){alert('Select or enter an SSID');return;}
  if(useStatic.checked && (!ip.value.trim()||!gateway.value.trim())){alert('Static IP needs IP and gateway');return;}
  try{
    await j('/api/v1/wifi',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({
      ssid:name,password:pass.value,use_static:useStatic.checked,
      ip:ip.value.trim(),gateway:gateway.value.trim(),netmask:netmask.value.trim()||'255.255.255.0',
      dns1:dns1.value.trim(),dns2:dns2.value.trim()
    })});
    alert('Connecting… then open '+(useStatic.checked?('http://'+ip.value.trim()):'http://lumosos.local (or your hostname)'));
  }catch(e){ alert('Connect failed: '+e.message); }
}
async function retryWifi(){
  const hint=document.getElementById('wifiHint');
  try{
    await j('/api/v1/wifi/retry',{method:'POST'});
    if(hint) hint.textContent='Retrying saved home Wi-Fi… this page stays up if it fails.';
  }catch(e){ if(hint) hint.textContent='Retry failed: '+e.message; }
}
function pulsePresence(){ fetch('/api/v1/wifi/presence',{method:'POST'}).catch(()=>{}); }
async function applyBalanceLive(){
  const body={
    balance_r:Math.max(0,Math.min(255,Number(balR.value)||255)),
    balance_g:Math.max(0,Math.min(255,Number(balG.value)||255)),
    balance_b:Math.max(0,Math.min(255,Number(balB.value)||255)),
    gamma:2.2
  };
  try{
    await j('/api/v1/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
    await j('/api/v1/plugin/static',{method:'POST',headers:{'Content-Type':'application/json'},
      body:JSON.stringify({r:255,g:255,b:0})});
    alert('Balance saved — strip showing (255,255,0). Tweak green until yellow looks right.');
  }catch(e){ alert('Balance failed: '+e.message); }
}
async function applyColorOrderLive(){
  const order=Number(colorOrder.value);
  const names=['GRB','RGB','BRG','RBG','GBR','BGR'];
  colorOrderHint.textContent='Applying '+names[order]+'…';
  try{
    // Dim test only — full-bright solid red can brown-out a weak 5V supply and reboot the board.
    await j('/api/v1/settings',{method:'POST',headers:{'Content-Type':'application/json'},
      body:JSON.stringify({color_order:order,brightness:96})});
    brightness.value=96;
    await j('/api/v1/plugin/static',{method:'POST',headers:{'Content-Type':'application/json'},
      body:JSON.stringify({r:80,g:0,b:0})});
    colorOrderHint.textContent=names[order]+' + dim red test. Correct order looks red (not green/blue). Then raise brightness.';
  }catch(e){ colorOrderHint.textContent='Failed: '+e.message; }
}
async function saveStrip(){
  const body={
    gpio:Number(gpio.value),
    chipset:Number(chipset.value),
    color_order:Number(colorOrder.value)
  };
  try{
    const r=await fetch('/api/v1/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
    const t=await r.text();
    if(t.indexOf('"reboot":true')>=0){ alert('Saved — rebooting… refresh shortly'); return; }
    if(!r.ok) throw new Error(t);
    alert('Strip hardware saved');
  }catch(e){ alert('Save failed: '+e.message); }
}
async function applyLighting(){
  try{
    await j('/api/v1/brightness',{method:'POST',headers:{'Content-Type':'application/json'},
      body:JSON.stringify({brightness:Number(brightness.value)})});
    const params={};
    for(const el of pluginParams.querySelectorAll('[data-param-id]')){
      params[el.dataset.paramId]=el.type==='number'?Number(el.value):el.value;
    }
    await j('/api/v1/plugin/'+plugin.value,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(params)});
    pluginDirty=false;
    updateLightingHint();
    refresh();
  }catch(e){ alert('Apply failed: '+e.message); }
}
async function uploadOta(){
  const f=firmware.files[0]; if(!f){alert('Choose a .bin');return;}
  ota.textContent='Uploading…';
  const r=await fetch('/api/v1/ota',{method:'POST',body:f,headers:{'Content-Type':'application/octet-stream'}});
  ota.textContent=await r.text();
}
function toggleCalPick(){
  calPickOn=!!calPick.checked;
  ledPreview.parentElement.classList.toggle('cal-on', calPickOn);
  previewHint.textContent=calPickOn
    ? 'Pick mode — tap a LED (physical wire index) to ignore/restore.'
    : 'Preview is TV layout (CW from top-left). Wire start/direction + skips are applied via the calibration map.';
}
async function persistIgnores(){
  const list=[...ignoredSet].sort((a,b)=>a-b);
  await j('/api/v1/settings',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({ignored_leds:list})});
  if(calStatus) calStatus.textContent='Middle ignored: '+list.length+' · saved';
  drawPreview();
}
async function clearIgnores(){
  ignoredSet=new Set();
  try{ await persistIgnores(); }catch(e){ alert(e.message); }
}
async function runCalMode(mode, extra){
  const body=Object.assign({mode:mode}, extra||{});
  await j('/api/v1/plugin/calibration',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
  plugin.value='calibration';
}
function updateHhCard(hh, valid){
  let text='LED count: '+hh.leds+'\nTop: '+hh.top+'\nRight: '+hh.right+'\nBottom: '+hh.bottom+'\nLeft: '+hh.left+'\nOrder: '+(hh.order||'clockwise_top_left');
  if(typeof valid==='boolean') text+='\nGeometry: '+(valid?'valid':'check skips / edges / ignores');
  text+='\nMiddle ignores: '+ignoredSet.size;
  hhCard.textContent=text;
}
function showWiz(){
  for(let i=0;i<6;i++){ const el=document.getElementById('wiz'+i); if(el) el.style.display=i===wizStep?'block':'none'; }
  wizStepLabel.textContent='Step '+(wizStep+1)+' / 6 — '+wizLabels[wizStep];
  wizBack.disabled=wizStep===0;
  wizNext.textContent=wizStep===5?'Done':'Next';
  if(wizStep===3){ calPick.checked=true; toggleCalPick(); }
  if(wizStep===4) updateEdgePrompt();
  if(wizStep===5) loadSettings();
}
function wizNav(d){
  if(wizStep===5 && d>0) return;
  wizStep=Math.max(0,Math.min(5,wizStep+d));
  showWiz();
}
async function wizSaveOrient(){
  try{
    await j('/api/v1/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({
      perimeter_start:Number(periStart.value), perimeter_direction:Number(periDir.value)})});
    await loadSettings();
    await runCalMode('sides');
  }catch(e){ alert(e.message); }
}
async function wizPrefixDelta(d){
  prefixN.value=Math.max(1, (Number(prefixN.value)||1)+d);
  await wizLightPrefix();
}
async function wizLightPrefix(){
  try{ await runCalMode('prefix',{prefix_n:Number(prefixN.value)||1}); }
  catch(e){ alert(e.message); }
}
async function wizSavePhysical(){
  try{
    const n=Number(prefixN.value)||1;
    const r=await fetch('/api/v1/settings',{method:'POST',headers:{'Content-Type':'application/json'},
      body:JSON.stringify({physical_led_count:n,led_count:n})});
    const t=await r.text();
    if(t.indexOf('"reboot":true')>=0){ alert('Physical count '+n+' saved — rebooting…'); return; }
    if(!r.ok) throw new Error(t);
    ledCount.value=n; updateStripCounts(n, layoutSides.top+layoutSides.right+layoutSides.bottom+layoutSides.left);
    alert('Physical count saved: '+n);
  }catch(e){ alert(e.message); }
}
async function wizSaveSkips(){
  try{
    await j('/api/v1/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({
      edge_ignore:{skip_start:Number(skipStart.value)||0, skip_end:Number(skipEnd.value)||0,
        corner_tr:0,corner_br:0,corner_bl:0,corner_tl:0}})});
    await loadSettings();
    await runCalMode('skips');
  }catch(e){ alert(e.message); }
}
async function wizToggleMidIndex(){
  try{
    const i=Number(midIndex.value)||0;
    if(ignoredSet.has(i)) ignoredSet.delete(i); else ignoredSet.add(i);
    await persistIgnores();
    await runCalMode('index',{index:i});
  }catch(e){ alert(e.message); }
}
function updateEdgePrompt(){
  const side=currentEdgeSide();
  const which=(edgePhase%2===0)?'first':'last';
  edgePrompt.innerHTML='Light the <b>'+which+'</b> non-skipped LED of '+sideLabels[side]+'.';
  edgeStatus.textContent='Measuring '+sideLabels[side]+' · '+which+' · phase '+(edgePhase+1)+'/8';
}
async function wizEdgeLight(){
  try{
    const i=firstActiveAtOrAfter(Number(edgeIdx.value)||0);
    edgeIdx.value=i;
    const key=sideKeys[currentEdgeSide()];
    if(edgePhase%2===0) edgeRanges[key]=[i,i];
    else edgeRanges[key][1]=i;
    if(edgePhase%2===0){
      await runCalMode('index',{index:i});
    } else {
      await runCalMode('edge_range',{range_start:edgeRanges[key][0], range_end:edgeRanges[key][1]});
    }
  }catch(e){ alert(e.message); }
}
async function wizEdgeConfirm(){
  try{
    const wasEnd=edgePhase%2===1;
    await wizEdgeLight();
    const endIdx=Number(edgeIdx.value)||0;
    edgePhase++;
    if(edgePhase>=8){
      await j('/api/v1/settings',{method:'POST',headers:{'Content-Type':'application/json'},
        body:JSON.stringify({edge_ranges:edgeRanges})});
      await loadSettings();
      edgePhase=0;
      wizStep=5; showWiz();
      alert('Edges saved — see HyperHDR summary');
      return;
    }
    if(wasEnd) edgeIdx.value=nextActiveIndex(endIdx);
    updateEdgePrompt();
  }catch(e){ alert(e.message); }
}
async function wizCopyHh(){
  try{ await navigator.clipboard.writeText(hhCard.textContent); alert('Copied'); }catch{ alert(hhCard.textContent); }
}
ledPreview.addEventListener('click', async (ev)=>{
  if(!calPickOn || !ledPositions.length) return;
  const rect=ledPreview.getBoundingClientRect();
  const scaleX=ledPreview.width/rect.width, scaleY=ledPreview.height/rect.height;
  const x=(ev.clientX-rect.left)*scaleX, y=(ev.clientY-rect.top)*scaleY;
  let best=null, bestD=18*18;
  for(const p of ledPositions){
    const d=(p.x-x)*(p.x-x)+(p.y-y)*(p.y-y);
    if(d<bestD){ bestD=d; best=p; }
  }
  if(!best||best.i<0) return;
  if(ignoredSet.has(best.i)) ignoredSet.delete(best.i); else ignoredSet.add(best.i);
  midIndex.value=best.i;
  drawPreview();
  try{ await persistIgnores(); }catch(e){ if(calStatus) calStatus.textContent='Save failed: '+e.message; }
});
async function downloadConfig(){
  cfgStatus.textContent='Building config…';
  try{
    const url='/api/v1/config'+(cfgSecrets.checked?'?secrets=1':'');
    const r=await fetch(url);
    if(!r.ok) throw new Error(await r.text());
    const text=await r.text();
    const blob=new Blob([text],{type:'application/json'});
    const a=document.createElement('a');
    a.href=URL.createObjectURL(blob);
    a.download='lumosos-config.json';
    a.click();
    URL.revokeObjectURL(a.href);
    cfgStatus.textContent='Downloaded lumosos-config.json'+(cfgSecrets.checked?' (includes Wi‑Fi password — keep private)':'');
  }catch(e){ cfgStatus.textContent='Download failed: '+e.message; }
}
async function uploadConfig(ev){
  const f=ev.target.files&&ev.target.files[0];
  ev.target.value='';
  if(!f) return;
  cfgStatus.textContent='Reading '+f.name+'…';
  try{
    const text=await f.text();
    const parsed=JSON.parse(text);
    if(parsed&&parsed.device&&cfgClearIp.checked){
      parsed.clear_static_ip=true;
    } else if(parsed&&!parsed.device&&cfgClearIp.checked){
      // Flat settings blob — strip static IP fields before apply.
      parsed.wifi_use_static=false;
      parsed.wifi_ip='';
    }
    const r=await fetch('/api/v1/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(parsed)});
    const t=await r.text();
    if(!r.ok) throw new Error(t);
    if(t.indexOf('"reboot":true')>=0){
      cfgStatus.textContent='Config applied — rebooting… refresh shortly.';
      alert('Config applied — device rebooting. Refresh this page in a few seconds.');
      return;
    }
    cfgStatus.textContent='Config applied.';
    await loadSettings(); await refresh();
    alert('Config applied');
  }catch(e){ cfgStatus.textContent='Upload failed: '+e.message; alert('Upload failed: '+e.message); }
}
let livePreviewOn=false;
let pollTimer=null;
function toggleLivePreview(){
  livePreviewOn=!!livePreview.checked;
  previewHint.textContent=livePreviewOn
    ? 'Live preview ~10 FPS. Uses Wi‑Fi — turn off if the strip flickers.'
    : 'Off by default. TV view shows active HyperHDR LEDs only.';
  if(pollTimer){ clearTimeout(pollTimer); pollTimer=null; }
  if(livePreviewOn) pollLeds();
}
async function pollLeds(){
  if(!livePreviewOn) return;
  if(document.hidden){ pollTimer=setTimeout(pollLeds, 400); return; }
  const t0=performance.now();
  try{
    const r=await fetch('/api/v1/leds.bin',{cache:'no-store'});
    if(r.ok){
      const buf=new Uint8Array(await r.arrayBuffer());
      if(buf.length>=2){
        const count=buf[0]|(buf[1]<<8);
        const need=2+count*3;
        if(count>0 && buf.length>=need){
          previewCount=count;
          ledRgb=buf.subarray(2, need);
          drawPreview();
        }
      }
    }
  }catch{}
  if(!livePreviewOn) return;
  const wait=Math.max(20, 100-(performance.now()-t0));
  pollTimer=setTimeout(pollLeds, wait);
}
async function loadNeighbors(){
  try{
    neighbors.textContent='Scanning…';
    // refresh=1 only on explicit click — auto mDNS every 30s was flashing LEDs white.
    const data=await j('/api/v1/neighbors?refresh=1');
    const list=data.neighbors||[];
    if(!list.length){ neighbors.textContent='No other LumosOS devices found on this LAN.'; return; }
    neighbors.textContent=list.map(n=>{
      const host=n.hostname||'device';
      const ip=n.ip||'?';
      const ver=n.version?(' v'+n.version):'';
      const leds=n.leds?(' · '+n.leds+' LEDs'):'';
      return host+ver+leds+'\n  http://'+ip+(n.port&&n.port!==80?(':'+n.port):'');
    }).join('\n\n');
  }catch(e){ neighbors.textContent='Neighbors unavailable: '+e.message; }
}
function onWsMessage(m){ if(m.type==='state') renderStatus(m); }
showWiz();
loadSettings(); refresh(); scanWifi(); drawPreview();
neighbors.textContent='Click Refresh to scan for nearby LumosOS devices.';
// No auto LED poll / WebSocket — both caused RMT flicker via Wi‑Fi. Status uses HTTP refresh.
pulsePresence();
setInterval(pulsePresence,3000);
setInterval(refresh,15000);
</script>
</body>
</html>
)HTML";

constexpr const char* kDoorbellHtml = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>LumosOS · Doorbell</title>
<style>
:root{--bg:#0e1116;--card:#171c24;--text:#e8edf5;--muted:#8b95a8;--accent:#6cb6ff;--line:#2a3340}
*{box-sizing:border-box}body{margin:0;font:15px/1.45 system-ui,sans-serif;background:var(--bg);color:var(--text)}
header{padding:1.25rem 1.25rem .5rem}h1{margin:0;font-size:1.4rem;letter-spacing:.04em}
p{color:var(--muted)}main{padding:0 1.25rem 2rem;display:grid;gap:1rem;max-width:720px}
section{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:1rem}
label{display:block;margin:.5rem 0 .25rem;color:var(--muted);font-size:.85rem}
input,select,button{width:100%;padding:.65rem .75rem;border-radius:8px;border:1px solid var(--line);background:#0f141b;color:var(--text)}
button{background:var(--accent);color:#041018;border:none;font-weight:600;margin-top:.75rem;cursor:pointer}
button.secondary{background:transparent;color:var(--accent);border:1px solid var(--accent)}
.hint{font-size:.8rem;color:var(--muted);margin-top:.5rem}
.check{display:flex;align-items:center;gap:.5rem;margin:.75rem 0 .25rem;color:var(--text)}
.check input{width:auto}
pre{white-space:pre-wrap;background:#0f141b;padding:.75rem;border-radius:8px;font-size:.8rem;color:var(--muted)}
a{color:var(--accent);font-weight:600;text-decoration:none}
.peer{display:block;text-align:left;margin-top:.5rem}
.pair{display:flex;gap:.75rem;align-items:flex-start;padding:.9rem;border-radius:10px;border:1px solid var(--line);margin:0 0 .75rem}
.pair .dot{width:.7rem;height:.7rem;border-radius:50%;margin-top:.28rem;flex:0 0 auto}
.pair h3{margin:0;font-size:1.02rem;color:var(--text)}
.pair p{margin:.25rem 0 0;font-size:.85rem}
.pair.ok{border-color:#2f6f52;background:#102018}
.pair.ok .dot{background:#3ecf8e}
.pair.no{border-color:#7a5a28;background:#1c160e}
.pair.no .dot{background:#e0a04a}
.pair.wait{border-color:#2a5a7a;background:#101820}
.pair.wait .dot{background:#6cb6ff}
</style>
</head>
<body>
<header>
  <h1>Doorbell</h1>
  <p>ESP-NOW receiver · relay pulse for battery bell</p>
  <p><a href="/">← LumosOS / LEDs</a></p>
</header>
<main>
<section>
<h2>Pairing status</h2>
<div id="pairCard" class="pair no"><div class="dot"></div><div><h3>Doorbell</h3><p>Loading…</p></div></div>
<p class="hint">Same home Wi‑Fi is only for the web UI. Pairing is a separate ESP-NOW MAC link.</p>
</section>
<section>
<h2>Pair nearby transmitter</h2>
<p class="hint">On the doorbell TX, open the LumosOS-Bell page and tap <b>Find nearby</b> at the same time. Then tap the device here (or there). 60 seconds.</p>
<button type="button" onclick="startPair()">Start pairing</button>
<pre id="pairStatus">Idle</pre>
<div id="peers"></div>
</section>
<section>
<h2>Settings</h2>
<label class="check"><input id="enabled" type="checkbox"/> Enable doorbell receiver</label>
<label>Relay GPIO</label>
<input id="relayPin" type="number" min="4" max="33" value="17"/>
<p class="hint">Default 17 (safe on ESP32-WROOM when LED data is on 16). Avoid 0/2/5/6–12/15 and input-only 34–39.</p>
<label class="check"><input id="activeHigh" type="checkbox" checked/> Relay active-HIGH</label>
<p class="hint">Uncheck for active-LOW / low-level trigger modules.</p>
<label class="check"><input id="tone" type="checkbox"/> Buzzer tone (2.5 kHz PWM)</label>
<p class="hint">Required for a piezo/buzzer on GPIO. Leave unchecked if you use a relay.</p>
<label>Press duration (ms)</label>
<input id="pressMs" type="number" min="100" max="4000" value="1500"/>
<label>Paired transmitter MAC</label>
<input id="txMac" placeholder="AA:BB:CC:DD:EE:FF"/>
<p class="hint">Required if you skip pairing. Empty or invalid MAC = ignore all ESP-NOW doorbell packets.</p>
<button type="button" onclick="saveDoorbell()">Save</button>
<button class="secondary" type="button" onclick="testRelay()">Test relay pulse</button>
<pre id="msg"></pre>
</section>
<section>
<h2>Status</h2>
<pre id="status">Loading…</pre>
<button class="secondary" type="button" onclick="loadDoorbell()">Refresh</button>
</section>
</main>
<script>
let pairTimer=null;
function setPairCard(d){
  const el=document.getElementById('pairCard');
  const pairing=!!d.pairing;
  const mac=(d.paired_tx_mac||'').trim();
  const paired=!!d.paired && !!mac;
  el.className='pair '+(pairing?'wait':(paired?'ok':'no'));
  const title=pairing?'Pairing…':(paired?'Paired with transmitter':'Not paired');
  let detail;
  if(pairing) detail='Listening for LumosOS-Bell. Start Find nearby on the transmitter now.';
  else if(paired){
    const ring=d.last_ring_ms>0?' Last ring this boot at '+d.last_ring_ms+' ms.':' No ring yet this boot — use Test send on the transmitter.';
    detail='Transmitter '+mac+'.'+(d.enabled?' Receiver enabled.':' Receiver disabled.')+ring;
  } else detail='No transmitter MAC saved. Start pairing below, or paste the TX MAC in Settings.';
  el.innerHTML='<div class="dot"></div><div><h3>'+title+'</h3><p>'+detail+'</p></div>';
}
function renderPeers(d){
  const box=document.getElementById('peers');
  const list=d.peers||[];
  if(!list.length){
    box.innerHTML=d.pairing?'<p class="hint">Listening for LumosOS-Bell…</p>':'';
    return;
  }
  box.innerHTML=list.map(p=>'<button type="button" class="peer" onclick="selectPeer(\''+p.mac+'\')">'+
    (p.name||'Doorbell TX')+' · '+p.mac+' · ch '+p.channel+' · RSSI '+p.rssi+'</button>').join('');
}
async function loadDoorbell(){
  try{
    const r=await fetch('/api/v1/doorbell');
    const d=await r.json();
    enabled.checked=!!d.enabled;
    relayPin.value=d.relay_pin??17;
    activeHigh.checked=d.active_high!==false;
    tone.checked=!!d.tone;
    pressMs.value=d.press_ms??400;
    txMac.value=d.paired_tx_mac||'';
    setPairCard(d);
    status.textContent=[
      'own_mac: '+(d.own_mac||'—'),
      'wifi_channel: '+(d.wifi_channel??'—'),
      'enabled: '+!!d.enabled,
      'espnow_ready: '+!!d.espnow_ready,
      'paired: '+!!d.paired,
      'relay_pin: '+(d.relay_pin??'—'),
      'active_high: '+!!d.active_high,
      'tone: '+!!d.tone,
      'press_ms: '+(d.press_ms??'—'),
      'paired_tx_mac: '+(d.paired_tx_mac||'(none)'),
      'last_ring_ms: '+(d.last_ring_ms||0),
      'last_seq: '+(d.last_seq??0),
      'relay_active: '+!!d.relay_active
    ].join('\n');
    pairStatus.textContent=d.pairing?('Pairing… '+Math.round((d.pairing_ms||0)/1000)+'s left'):'Idle';
    renderPeers(d);
    if(d.pairing && !pairTimer){ pairTimer=setInterval(loadDoorbell,1000); }
    if(!d.pairing && pairTimer){ clearInterval(pairTimer); pairTimer=null; }
  }catch(e){ status.textContent='Error: '+e.message; }
}
async function startPair(){
  pairStatus.textContent='Starting…';
  try{
    await fetch('/api/v1/doorbell/pair/start',{method:'POST'});
    await loadDoorbell();
  }catch(e){ pairStatus.textContent='Error: '+e.message; }
}
async function selectPeer(mac){
  pairStatus.textContent='Pairing '+mac+'…';
  try{
    const r=await fetch('/api/v1/doorbell/pair',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({mac})});
    const d=await r.json();
    pairStatus.textContent=d.ok?'Paired '+mac:'Error: '+(d.error||r.status);
    await loadDoorbell();
  }catch(e){ pairStatus.textContent='Error: '+e.message; }
}
async function saveDoorbell(){
  msg.textContent='Saving…';
  try{
    const body={
      enabled:enabled.checked,
      relay_pin:Number(relayPin.value)||17,
      active_high:activeHigh.checked,
      tone:tone.checked,
      press_ms:Number(pressMs.value)||1500,
      paired_tx_mac:(txMac.value||'').trim()
    };
    const r=await fetch('/api/v1/doorbell',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
    const d=await r.json();
    msg.textContent=d.ok?'Saved.':'Error: '+(d.error||r.status);
    await loadDoorbell();
  }catch(e){ msg.textContent='Error: '+e.message; }
}
async function testRelay(){
  msg.textContent='Pulsing relay…';
  try{
    const r=await fetch('/api/v1/doorbell/test',{method:'POST'});
    const d=await r.json();
    msg.textContent=d.ok?'Pulse sent.':'Error: '+(d.error||r.status);
    await loadDoorbell();
  }catch(e){ msg.textContent='Error: '+e.message; }
}
loadDoorbell();
setInterval(loadDoorbell,10000);
fetch('/api/v1/wifi/presence',{method:'POST'}).catch(()=>{});
setInterval(()=>fetch('/api/v1/wifi/presence',{method:'POST'}).catch(()=>{}),3000);
</script>
</body>
</html>
)HTML";

} // namespace

esp_err_t WebUi::get_index(httpd_req_t* req) {
    WifiService::note_ui_activity_global();
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, kIndexHtml, HTTPD_RESP_USE_STRLEN);
}

esp_err_t WebUi::get_doorbell(httpd_req_t* req) {
    WifiService::note_ui_activity_global();
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, kDoorbellHtml, HTTPD_RESP_USE_STRLEN);
}

esp_err_t WebUi::get_android_probe(httpd_req_t* req) {
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, nullptr, 0);
}

esp_err_t WebUi::get_apple_probe(httpd_req_t* req) {
    static constexpr const char* kSuccess =
        "<!DOCTYPE HTML PUBLIC \"-//Apple//DTD HTML 3.2//EN\">"
        "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>";
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, kSuccess, HTTPD_RESP_USE_STRLEN);
}

esp_err_t WebUi::get_windows_probe(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, "Microsoft NCSI", HTTPD_RESP_USE_STRLEN);
}

Result<void> WebUi::start(httpd_handle_t server) {
    const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = get_index, .user_ctx = nullptr},
        {.uri = "/doorbell", .method = HTTP_GET, .handler = get_doorbell, .user_ctx = nullptr},
        {.uri = "/generate_204", .method = HTTP_GET, .handler = get_android_probe, .user_ctx = nullptr},
        {.uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = get_apple_probe, .user_ctx = nullptr},
        {.uri = "/canonical.html", .method = HTTP_GET, .handler = get_apple_probe, .user_ctx = nullptr},
        {.uri = "/ncsi.txt", .method = HTTP_GET, .handler = get_windows_probe, .user_ctx = nullptr},
    };
    for (const auto& route : routes) {
        if (httpd_register_uri_handler(server, &route) != ESP_OK) {
            return Result<void>::fail(ErrorCode::IoError, "failed to register web UI route");
        }
    }
    return Result<void>::ok();
}

} // namespace lumos
