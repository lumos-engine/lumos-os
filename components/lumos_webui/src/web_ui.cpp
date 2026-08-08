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
.grid4{display:grid;grid-template-columns:repeat(4,1fr);gap:.5rem}
pre{white-space:pre-wrap;background:#0f141b;padding:.75rem;border-radius:8px;font-size:.8rem;color:var(--muted)}
.hint{font-size:.8rem;color:var(--muted);margin-top:.5rem}
.check{display:flex;align-items:center;gap:.5rem;margin:.75rem 0 .25rem;color:var(--text)}
.check input{width:auto}
#staticFields{display:none}
#staticFields.show{display:block}
.preview-wrap{position:relative;width:100%;aspect-ratio:16/9;background:#07090d;border-radius:10px;overflow:hidden;border:1px solid var(--line)}
.preview-wrap canvas{display:block;width:100%;height:100%}
.param{margin-top:.5rem}
.advanced{opacity:.85}
</style>
</head>
<body>
<header><h1>LumosOS</h1><p>Recovery &amp; local configuration · API 0.3</p></header>
<main>
<section>
<h2>Live preview</h2>
<div class="preview-wrap"><canvas id="ledPreview" width="640" height="360"></canvas></div>
<p class="hint">Clockwise from top-left. Layout comes from device settings (not hardcoded).</p>
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
    <div><label>DNS 1</label><input id="dns1"/></div>
    <div><label>DNS 2</label><input id="dns2"/></div>
  </div>
</div>
<button onclick="saveWifi()">Save &amp; Connect</button>
</section>
<section>
<h2>Strip &amp; layout</h2>
<label>LED count</label><input id="ledCount" type="number" min="1" max="2000"/>
<label>GPIO</label><input id="gpio" type="number" min="0" max="39"/>
<label>Chipset</label>
<select id="chipset">
  <option value="0">WS2815 (RGB)</option>
  <option value="1">WS2812B</option>
  <option value="2">WS2813</option>
  <option value="3">SK6812 RGB</option>
  <option value="4">SK6812 RGBW</option>
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
<p class="hint" id="colorOrderHint">Changing this applies on the next frame (no reboot). Use Fire or a solid red Static color to judge.</p>
<label>Layout (top / right / bottom / left) — must sum to LED count</label>
<div class="grid4">
  <input id="layTop" type="number" min="0" placeholder="top"/>
  <input id="layRight" type="number" min="0" placeholder="right"/>
  <input id="layBottom" type="number" min="0" placeholder="bottom"/>
  <input id="layLeft" type="number" min="0" placeholder="left"/>
</div>
<p class="hint" id="layoutSum">Sum: —</p>
<button onclick="saveStrip()">Save strip settings</button>
<p class="hint">Chipset / GPIO / LED count changes reboot the device. Color order applies live.</p>
</section>
<section>
<h2>Lighting</h2>
<label>Plugin</label><select id="plugin" onchange="renderPluginParams()"></select>
<div id="pluginParams"></div>
<label>Brightness (0–255)</label><input id="brightness" type="number" min="0" max="255"/>
<label>Channel balance R / G / B (255 = unity)</label>
<div class="grid4">
  <input id="balR" type="number" min="0" max="255" value="255" title="Red gain"/>
  <input id="balG" type="number" min="0" max="255" value="255" title="Green gain"/>
  <input id="balB" type="number" min="0" max="255" value="255" title="Blue gain"/>
  <button type="button" onclick="applyBalanceLive()">Apply balance</button>
</div>
<p class="hint">SK6812 RGBW often needs green ~80–100 so yellow is amber, not lime. Applies live.</p>
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
let pluginsCache=[];
async function j(url,opts){const r=await fetch(url,opts); if(!r.ok) throw new Error(await r.text()); return r.json()}
function toggleStatic(){document.getElementById('staticFields').classList.toggle('show', useStatic.checked);}
function applyLayoutSides(sides){
  layTop.value=sides.top; layRight.value=sides.right;
  layBottom.value=sides.bottom; layLeft.value=sides.left;
  layoutSides=sides;
}
function updateLayoutSum(){
  const s=[layTop,layRight,layBottom,layLeft].map(el=>Number(el.value)||0).reduce((a,b)=>a+b,0);
  const want=Number(ledCount.value)||0;
  layoutSum.textContent='Sum: '+s+(want?(' / '+want+(s===want?' ✓':' — mismatch')):'');
}
['layTop','layRight','layBottom','layLeft'].forEach(id=>{
  document.getElementById(id).addEventListener('input', updateLayoutSum);
});
ledCount.addEventListener('input',()=>{
  const n=Number(ledCount.value)||0;
  const s=[layTop,layRight,layBottom,layLeft].map(el=>Number(el.value)||0).reduce((a,b)=>a+b,0);
  if(n>0 && s!==n) applyLayoutSides(sideCounts(n));
  updateLayoutSum();
});
function renderStatus(s){
  const view=Object.assign({},s); delete view.type;
  status.textContent=JSON.stringify(view,null,2);
  if(typeof s.brightness==='number') brightness.value=s.brightness;
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
  const cur={top:Number(layTop.value)||0,right:Number(layRight.value)||0,bottom:Number(layBottom.value)||0,left:Number(layLeft.value)||0};
  if((cur.top+cur.right+cur.bottom+cur.left)===n) return cur;
  if(layoutSides && (layoutSides.top+layoutSides.right+layoutSides.bottom+layoutSides.left)===n) return layoutSides;
  if(n===140) return {top:44,right:26,bottom:44,left:26};
  if(n===340) return {top:144,right:26,bottom:144,left:26};
  const top=Math.round(n*16/50), right=Math.round(n*9/50), bottom=Math.round(n*16/50);
  let left=n-top-right-bottom; if(left<0) left=0;
  return {top,right,bottom,left};
}
function drawPreview(){
  const canvas=ledPreview, ctx=canvas.getContext('2d');
  const W=canvas.width, H=canvas.height;
  ctx.clearRect(0,0,W,H); ctx.fillStyle='#07090d'; ctx.fillRect(0,0,W,H);
  const pad=18;
  ctx.fillStyle='#121722'; ctx.strokeStyle='#2a3340'; ctx.lineWidth=2;
  roundRect(ctx,pad+14,pad+14,W-2*(pad+14),H-2*(pad+14),8); ctx.fill(); ctx.stroke();
  const sides=previewCount?sideCounts(previewCount):null;
  let lit=0;
  if(ledRgb && previewCount){ for(let i=0;i<previewCount;i++){ const o=i*3; if(ledRgb[o]|ledRgb[o+1]|ledRgb[o+2]) lit++; } }
  ctx.fillStyle='#8b95a8'; ctx.font='14px system-ui,sans-serif'; ctx.textAlign='center';
  ctx.fillText(previewCount?(lit+' lit / '+previewCount+(sides?(' · '+sides.top+'/'+sides.right+'/'+sides.bottom+'/'+sides.left):'')):'Waiting for frames…', W/2, H/2);
  if(!ledRgb || !previewCount || !sides) return;
  const band=12; let idx=0;
  for(let i=0;i<sides.top && idx<previewCount;i++,idx++){ const t=sides.top<=1?0.5:i/(sides.top-1); drawLed(ctx,pad+t*(W-2*pad),pad,band,ledRgb,idx); }
  for(let i=0;i<sides.right && idx<previewCount;i++,idx++){ const t=sides.right<=1?0.5:i/(sides.right-1); drawLed(ctx,W-pad,pad+t*(H-2*pad),band,ledRgb,idx); }
  for(let i=0;i<sides.bottom && idx<previewCount;i++,idx++){ const t=sides.bottom<=1?0.5:i/(sides.bottom-1); drawLed(ctx,W-pad-t*(W-2*pad),H-pad,band,ledRgb,idx); }
  for(let i=0;i<sides.left && idx<previewCount;i++,idx++){ const t=sides.left<=1?0.5:i/(sides.left-1); drawLed(ctx,pad,H-pad-t*(H-2*pad),band,ledRgb,idx); }
}
function drawLed(ctx,x,y,size,rgb,idx){
  let r=rgb[idx*3], g=rgb[idx*3+1], b=rgb[idx*3+2];
  const lit=r|g|b; if(!lit){ r=28;g=34;b=44; }
  ctx.beginPath(); ctx.fillStyle='rgb('+r+','+g+','+b+')';
  if(lit){ ctx.shadowColor='rgba('+r+','+g+','+b+',0.55)'; ctx.shadowBlur=10; }
  ctx.arc(x,y,size/2,0,Math.PI*2); ctx.fill(); ctx.shadowBlur=0;
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
    if(typeof s.led_count==='number') ledCount.value=s.led_count;
    if(typeof s.brightness==='number') brightness.value=s.brightness;
    if(typeof s.gpio==='number') gpio.value=s.gpio;
    if(typeof s.chipset==='number') chipset.value=String(s.chipset);
    if(typeof s.color_order==='number') colorOrder.value=String(s.color_order);
    if(typeof s.balance_r==='number') balR.value=s.balance_r;
    if(typeof s.balance_g==='number') balG.value=s.balance_g;
    if(typeof s.balance_b==='number') balB.value=s.balance_b;
    if(s.layout){
      layTop.value=s.layout.top; layRight.value=s.layout.right;
      layBottom.value=s.layout.bottom; layLeft.value=s.layout.left;
      layoutSides={top:s.layout.top,right:s.layout.right,bottom:s.layout.bottom,left:s.layout.left};
    }
    toggleStatic(); updateLayoutSum();
  }catch{}
}
function renderPluginParams(){
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
  sel.innerHTML='';
  for(const plug of pluginsCache){
    const o=document.createElement('option');
    o.value=plug.id; o.textContent=plug.name+(plug.capabilities&&plug.capabilities.category?(' · '+plug.capabilities.category):'');
    if(plug.id===p.active || plug.id===current) o.selected=true;
    sel.appendChild(o);
  }
  renderPluginParams();
}
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
    led_count:Number(ledCount.value),
    gpio:Number(gpio.value),
    chipset:Number(chipset.value),
    color_order:Number(colorOrder.value),
    layout:{top:Number(layTop.value)||0,right:Number(layRight.value)||0,bottom:Number(layBottom.value)||0,left:Number(layLeft.value)||0}
  };
  if(body.layout.top+body.layout.right+body.layout.bottom+body.layout.left !== body.led_count){
    alert('Layout sides must sum to LED count'); return;
  }
  try{
    const r=await fetch('/api/v1/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
    const t=await r.text();
    layoutSides=body.layout;
    if(t.indexOf('"reboot":true')>=0){ alert('Saved — rebooting… refresh shortly'); return; }
    if(!r.ok) throw new Error(t);
    alert('Strip settings saved');
  }catch(e){ alert('Save failed: '+e.message); }
}
async function applyLighting(){
  await j('/api/v1/brightness',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({brightness:Number(brightness.value)})});
  const params={};
  for(const el of pluginParams.querySelectorAll('[data-param-id]')){
    params[el.dataset.paramId]=el.type==='number'?Number(el.value):el.value;
  }
  await j('/api/v1/plugin/'+plugin.value,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(params)});
  refresh();
}
async function uploadOta(){
  const f=firmware.files[0]; if(!f){alert('Choose a .bin');return;}
  ota.textContent='Uploading…';
  const r=await fetch('/api/v1/ota',{method:'POST',body:f,headers:{'Content-Type':'application/octet-stream'}});
  ota.textContent=await r.text();
}
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
async function pollLeds(){
  try{
    const m=await j('/api/v1/leds');
    if(m && m.rgb_hex){ previewCount=m.count||0; ledRgb=hexToBytes(m.rgb_hex); drawPreview(); }
  }catch{}
}
async function loadNeighbors(){
  try{
    const data=await j('/api/v1/neighbors');
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
loadSettings(); refresh(); scanWifi(); drawPreview();
loadNeighbors();
pollLeds(); setInterval(pollLeds,150); setInterval(refresh,5000);
setInterval(loadNeighbors,30000);
try{
  const ws=new WebSocket((location.protocol==='https:'?'wss://':'ws://')+location.host+'/ws');
  ws.onopen=()=>{wsLive=true}; ws.onclose=()=>{wsLive=false}; ws.onerror=()=>{wsLive=false};
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
