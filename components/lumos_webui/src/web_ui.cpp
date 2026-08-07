#include "lumos/webui/web_ui.hpp"

namespace lumos {
namespace {

// Minimal recovery UI — WiFi scan, plugin select, brightness, OTA, diagnostics.
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
pre{white-space:pre-wrap;background:#0f141b;padding:.75rem;border-radius:8px;font-size:.8rem;color:var(--muted)}
.hint{font-size:.8rem;color:var(--muted);margin-top:.5rem}
</style>
</head>
<body>
<header><h1>LumosOS</h1><p>Recovery &amp; local configuration</p></header>
<main>
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
<label>Password</label><input id="pass" type="password" placeholder="Router Wi‑Fi password"/>
<button onclick="saveWifi()">Save &amp; Connect</button>
<p class="hint">Phones can’t share their Wi‑Fi password with this page (browser security). Pick your network, then enter the password once.</p>
</section>
<section>
<h2>Lighting</h2>
<label>Plugin</label><select id="plugin"></select>
<label>Brightness (0–255)</label><input id="brightness" type="number" min="0" max="255"/>
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
async function j(url,opts){const r=await fetch(url,opts);return r.json()}
async function refresh(){
  const s=await j('/api/v1/status');
  document.getElementById('status').textContent=JSON.stringify(s,null,2);
  document.getElementById('brightness').value=s.brightness;
  const p=await j('/api/v1/plugins');
  const sel=document.getElementById('plugin');
  sel.innerHTML='';
  for(const plug of p.plugins){
    const o=document.createElement('option');
    o.value=plug.id;o.textContent=plug.name;
    if(plug.id===p.active)o.selected=true;
    sel.appendChild(o);
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
  await j('/api/v1/wifi',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({ssid:name,password:pass.value})});
  alert('Connecting… rejoin your home Wi‑Fi, then open http://lumosos.local');
}
async function applyLighting(){
  await j('/api/v1/brightness',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({brightness:Number(brightness.value)})});
  await j('/api/v1/plugin/'+plugin.value,{method:'POST',headers:{'Content-Type':'application/json'},body:'{}'});
  refresh();
}
async function uploadOta(){
  const f=firmware.files[0]; if(!f){alert('Choose a .bin');return;}
  ota.textContent='Uploading…';
  const r=await fetch('/api/v1/ota',{method:'POST',body:f,headers:{'Content-Type':'application/octet-stream'}});
  ota.textContent=await r.text();
}
refresh(); scanWifi(); setInterval(refresh,3000);
try{
  const ws=new WebSocket((location.protocol==='https:'?'wss://':'ws://')+location.host+'/ws');
  ws.onmessage=e=>{try{const m=JSON.parse(e.data);if(m.type==='state'){
    document.getElementById('status').textContent=JSON.stringify(m,null,2);
  }}catch{}};
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
