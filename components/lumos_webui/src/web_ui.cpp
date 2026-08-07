#include "lumos/webui/web_ui.hpp"

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
pre{white-space:pre-wrap;background:#0f141b;padding:.75rem;border-radius:8px;font-size:.8rem;color:var(--muted)}
.hint{font-size:.8rem;color:var(--muted);margin-top:.5rem}
.check{display:flex;align-items:center;gap:.5rem;margin:.75rem 0 .25rem;color:var(--text)}
.check input{width:auto}
#staticFields{display:none}
#staticFields.show{display:block}
.preview-wrap{position:relative;width:100%;aspect-ratio:16/9;background:#07090d;border-radius:10px;overflow:hidden;border:1px solid var(--line)}
.preview-wrap canvas{display:block;width:100%;height:100%}
</style>
</head>
<body>
<header><h1>LumosOS</h1><p>Recovery &amp; local configuration</p></header>
<main>
<section>
<h2>Live preview</h2>
<div class="preview-wrap"><canvas id="ledPreview" width="640" height="360"></canvas></div>
<p class="hint">16:9 view of what LumosOS received (clockwise from top-left: top→right→bottom→left). Layout for 140 LEDs is 44/26/44/26. If only one edge lights, HyperHDR LED count/layout doesn’t match.</p>
</section>
<section>
<h2>Status</h2>
<pre id="status">Loading…</pre>
</section>
<section>
<h2>WiFi</h2>
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
    <div><label>DNS 1</label><input id="dns1" placeholder="e.g. 1.1.1.1 (default: gateway)"/></div>
    <div><label>DNS 2</label><input id="dns2" placeholder="e.g. 8.8.8.8 (optional)"/></div>
  </div>
</div>
<button onclick="saveWifi()">Save &amp; Connect</button>
<p class="hint">Static IP is stored on the device (like OS network settings). Pick an address outside your router’s DHCP pool to avoid conflicts.</p>
</section>
<section>
<h2>Lighting</h2>
<label>Plugin</label><select id="plugin"></select>
<label>Brightness (0–255)</label><input id="brightness" type="number" min="0" max="255"/>
<label>LED count</label><input id="ledCount" type="number" min="1" max="2000" placeholder="e.g. 140 for 44+26+44+26"/>
<p class="hint">Must match HyperHDR exactly (e.g. 44×2 + 26×2 = 140). Changing count reboots the device.</p>
<button onclick="applyLighting()">Apply</button>
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
let ledCount=0;
async function j(url,opts){const r=await fetch(url,opts); if(!r.ok) throw new Error(await r.text()); return r.json()}
function toggleStatic(){
  document.getElementById('staticFields').classList.toggle('show', useStatic.checked);
}
function renderStatus(s){
  const view=Object.assign({},s);
  delete view.type;
  document.getElementById('status').textContent=JSON.stringify(view,null,2);
  if(typeof s.brightness==='number') document.getElementById('brightness').value=s.brightness;
  if(!ip.value && s.wifi && s.wifi.ip) ip.value=s.wifi.ip;
  if(!gateway.value && s.wifi && s.wifi.gateway) gateway.value=s.wifi.gateway;
  if((!netmask.value || netmask.value==='255.255.255.0') && s.wifi && s.wifi.netmask) netmask.value=s.wifi.netmask;
  if(!dns1.value && s.wifi && s.wifi.dns1) dns1.value=s.wifi.dns1;
  if(!dns2.value && s.wifi && s.wifi.dns2) dns2.value=s.wifi.dns2;
}
function hexToBytes(hex){
  const out=new Uint8Array(hex.length/2);
  for(let i=0;i<out.length;i++) out[i]=parseInt(hex.substr(i*2,2),16);
  return out;
}
function sideCounts(n){
  // Common HyperHDR 16:9 TV layout: top/bottom 44, left/right 26.
  if(n===140) return {top:44,right:26,bottom:44,left:26};
  if(n===340) return {top:144,right:26,bottom:144,left:26};
  // Fallback: distribute by 16:9 perimeter ratio (16+9+16+9=50).
  const top=Math.round(n*16/50);
  const right=Math.round(n*9/50);
  const bottom=Math.round(n*16/50);
  let left=n-top-right-bottom;
  if(left<0){left=0;}
  return {top,right,bottom,left};
}
function drawPreview(){
  const canvas=document.getElementById('ledPreview');
  const ctx=canvas.getContext('2d');
  const W=canvas.width, H=canvas.height;
  ctx.clearRect(0,0,W,H);
  ctx.fillStyle='#07090d';
  ctx.fillRect(0,0,W,H);

  const pad=18;
  const screenX=pad+14, screenY=pad+14;
  const screenW=W-2*(pad+14), screenH=H-2*(pad+14);
  ctx.fillStyle='#121722';
  ctx.strokeStyle='#2a3340';
  ctx.lineWidth=2;
  roundRect(ctx,screenX,screenY,screenW,screenH,8);
  ctx.fill(); ctx.stroke();
  ctx.fillStyle='#8b95a8';
  ctx.font='14px system-ui,sans-serif';
  ctx.textAlign='center';
  const sides=ledCount?sideCounts(ledCount):null;
  let lit=0;
  if(ledRgb && ledCount){
    for(let i=0;i<ledCount;i++){
      const o=i*3; if(ledRgb[o]|ledRgb[o+1]|ledRgb[o+2]) lit++;
    }
  }
  const label=ledCount
    ? (lit+' lit / '+ledCount+(sides?(' · '+sides.top+'/'+sides.right+'/'+sides.bottom+'/'+sides.left):''))
    : 'Waiting for frames…';
  ctx.fillText(label, W/2, H/2);

  if(!ledRgb || !ledCount) return;
  const band=12;
  let idx=0;
  // Top: left → right
  for(let i=0;i<sides.top && idx<ledCount;i++,idx++){
    const t=sides.top<=1?0.5:i/(sides.top-1);
    const x=pad + t*(W-2*pad);
    drawLed(ctx,x,pad,band,ledRgb,idx);
  }
  // Right: top → bottom
  for(let i=0;i<sides.right && idx<ledCount;i++,idx++){
    const t=sides.right<=1?0.5:i/(sides.right-1);
    const y=pad + t*(H-2*pad);
    drawLed(ctx,W-pad,y,band,ledRgb,idx);
  }
  // Bottom: right → left
  for(let i=0;i<sides.bottom && idx<ledCount;i++,idx++){
    const t=sides.bottom<=1?0.5:i/(sides.bottom-1);
    const x=W-pad - t*(W-2*pad);
    drawLed(ctx,x,H-pad,band,ledRgb,idx);
  }
  // Left: bottom → top
  for(let i=0;i<sides.left && idx<ledCount;i++,idx++){
    const t=sides.left<=1?0.5:i/(sides.left-1);
    const y=H-pad - t*(H-2*pad);
    drawLed(ctx,pad,y,band,ledRgb,idx);
  }
}
function drawLed(ctx,x,y,size,rgb,idx){
  let r=rgb[idx*3], g=rgb[idx*3+1], b=rgb[idx*3+2];
  const lit=r|g|b;
  if(!lit){ r=28; g=34; b=44; } // dim placeholder so layout is visible when off
  ctx.beginPath();
  ctx.fillStyle='rgb('+r+','+g+','+b+')';
  if(lit){
    ctx.shadowColor='rgba('+r+','+g+','+b+',0.55)';
    ctx.shadowBlur=10;
  }
  ctx.arc(x,y,size/2,0,Math.PI*2);
  ctx.fill();
  ctx.shadowBlur=0;
}
function roundRect(ctx,x,y,w,h,r){
  ctx.beginPath();
  ctx.moveTo(x+r,y);
  ctx.arcTo(x+w,y,x+w,y+h,r);
  ctx.arcTo(x+w,y+h,x,y+h,r);
  ctx.arcTo(x,y+h,x,y,r);
  ctx.arcTo(x,y,x+w,y,r);
  ctx.closePath();
}
async function loadSettings(){
  try{
    const s=await j('/api/v1/settings');
    ssid.value=s.wifi_ssid||'';
    useStatic.checked=!!s.wifi_use_static;
    ip.value=s.wifi_ip||'';
    gateway.value=s.wifi_gateway||'';
    netmask.value=s.wifi_netmask||'255.255.255.0';
    dns1.value=s.wifi_dns1||'';
    dns2.value=s.wifi_dns2||'';
    if(typeof s.led_count==='number') ledCount.value=s.led_count;
    if(typeof s.brightness==='number') brightness.value=s.brightness;
    toggleStatic();
  }catch{}
}
async function refreshPlugins(){
  const p=await j('/api/v1/plugins');
  const sel=document.getElementById('plugin');
  const current=sel.value;
  sel.innerHTML='';
  for(const plug of p.plugins){
    const o=document.createElement('option');
    o.value=plug.id;o.textContent=plug.name;
    if(plug.id===p.active || plug.id===current)o.selected=true;
    sel.appendChild(o);
  }
}
async function refresh(){
  try{
    if(!wsLive){
      const s=await j('/api/v1/status');
      renderStatus(s);
    }
    await refreshPlugins();
  }catch(e){
    document.getElementById('status').textContent='Status unavailable: '+e.message;
  }
}
async function scanWifi(){
  const sel=document.getElementById('netlist');
  sel.innerHTML='<option value="">Scanning…</option>';
  try{
    const data=await j('/api/v1/wifi/scan');
    sel.innerHTML='';
    const blank=document.createElement('option');
    blank.value=''; blank.textContent=data.networks.length?'Select a network…':'No networks found — type SSID';
    sel.appendChild(blank);
    for(const n of data.networks){
      const o=document.createElement('option');
      o.value=n.ssid;
      o.textContent=n.ssid+'  ('+n.rssi+' dBm'+(n.secure?' · locked':' · open')+')';
      sel.appendChild(o);
    }
  }catch(e){
    sel.innerHTML='<option value="">Scan failed — type SSID</option>';
  }
}
document.getElementById('netlist').addEventListener('change',e=>{
  if(e.target.value) document.getElementById('ssid').value=e.target.value;
});
async function saveWifi(){
  const name=ssid.value.trim();
  if(!name){alert('Select or enter an SSID');return;}
  if(useStatic.checked && (!ip.value.trim() || !gateway.value.trim())){
    alert('Static IP needs IP address and gateway');return;
  }
  const body={
    ssid:name,
    password:pass.value,
    use_static:useStatic.checked,
    ip:ip.value.trim(),
    gateway:gateway.value.trim(),
    netmask:netmask.value.trim()||'255.255.255.0',
    dns1:dns1.value.trim(),
    dns2:dns2.value.trim()
  };
  try{
    await j('/api/v1/wifi',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
    const target=useStatic.checked?('http://'+ip.value.trim()):'http://lumosos.local';
    alert('Connecting… then open '+target);
  }catch(e){
    alert('Connect failed: '+e.message);
  }
}
async function applyLighting(){
  await j('/api/v1/brightness',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({brightness:Number(brightness.value)})});
  await j('/api/v1/plugin/'+plugin.value,{method:'POST',headers:{'Content-Type':'application/json'},body:'{}'});
  const n=Number(ledCount.value);
  if(n>0){
    try{
      const r=await fetch('/api/v1/settings',{method:'POST',headers:{'Content-Type':'application/json'},
        body:JSON.stringify({led_count:n})});
      const t=await r.text();
      if(t.indexOf('"reboot":true')>=0){
        alert('LED count saved — device rebooting… refresh this page in a few seconds');
        return;
      }
    }catch{}
  }
  refresh();
}
async function uploadOta(){
  const f=firmware.files[0]; if(!f){alert('Choose a .bin');return;}
  ota.textContent='Uploading…';
  const r=await fetch('/api/v1/ota',{method:'POST',body:f,headers:{'Content-Type':'application/octet-stream'}});
  ota.textContent=await r.text();
}
async function pollLeds(){
  try{
    const m=await j('/api/v1/leds');
    if(m && m.rgb_hex){
      ledCount=m.count||0;
      ledRgb=hexToBytes(m.rgb_hex);
      drawPreview();
    }
  }catch(e){
    // keep last frame
  }
}
function onWsMessage(m){
  if(m.type==='state') renderStatus(m);
}
loadSettings(); refresh(); scanWifi(); drawPreview();
pollLeds(); setInterval(pollLeds, 150);
setInterval(refresh,5000);
try{
  const ws=new WebSocket((location.protocol==='https:'?'wss://':'ws://')+location.host+'/ws');
  ws.onopen=()=>{wsLive=true};
  ws.onclose=()=>{wsLive=false};
  ws.onerror=()=>{wsLive=false};
  ws.onmessage=e=>{try{onWsMessage(JSON.parse(e.data));}catch{}};
}catch{}
</script>
</body>
</html>
)HTML";

} // namespace

esp_err_t WebUi::get_index(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, kIndexHtml, HTTPD_RESP_USE_STRLEN);
}

esp_err_t WebUi::get_captive(httpd_req_t* req) {
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    return httpd_resp_send(req, nullptr, 0);
}

Result<void> WebUi::start(httpd_handle_t server) {
    const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = get_index, .user_ctx = nullptr},
        {.uri = "/generate_204", .method = HTTP_GET, .handler = get_captive, .user_ctx = nullptr},
        {.uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = get_captive, .user_ctx = nullptr},
        {.uri = "/canonical.html", .method = HTTP_GET, .handler = get_captive, .user_ctx = nullptr},
        {.uri = "/ncsi.txt", .method = HTTP_GET, .handler = get_captive, .user_ctx = nullptr},
    };
    for (const auto& route : routes) {
        if (httpd_register_uri_handler(server, &route) != ESP_OK) {
            return Result<void>::fail(ErrorCode::IoError, "failed to register web UI route");
        }
    }
    return Result<void>::ok();
}

} // namespace lumos
