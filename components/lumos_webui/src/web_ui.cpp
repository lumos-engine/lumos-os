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
<label>Password</label><input id="pass" type="password" placeholder="Leave blank to keep saved password"/>
<label class="check"><input id="useStatic" type="checkbox" onchange="toggleStatic()"/> Use static IP</label>
<div id="staticFields">
  <div class="grid2">
    <div><label>IP address</label><input id="ip" placeholder="192.168.1.50"/></div>
    <div><label>Gateway</label><input id="gateway" placeholder="192.168.1.1"/></div>
  </div>
  <div class="grid2">
    <div><label>Subnet mask</label><input id="netmask" placeholder="255.255.255.0"/></div>
    <div><label>DNS</label><input id="dns" placeholder="optional, defaults to gateway"/></div>
  </div>
</div>
<button onclick="saveWifi()">Save &amp; Connect</button>
<p class="hint">Static IP is stored on the device (like OS network settings). Pick an address outside your router’s DHCP pool to avoid conflicts.</p>
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
async function j(url,opts){const r=await fetch(url,opts); if(!r.ok) throw new Error(await r.text()); return r.json()}
function toggleStatic(){
  document.getElementById('staticFields').classList.toggle('show', useStatic.checked);
}
async function loadSettings(){
  try{
    const s=await j('/api/v1/settings');
    ssid.value=s.wifi_ssid||'';
    useStatic.checked=!!s.wifi_use_static;
    ip.value=s.wifi_ip||'';
    gateway.value=s.wifi_gateway||'';
    netmask.value=s.wifi_netmask||'255.255.255.0';
    dns.value=s.wifi_dns||'';
    toggleStatic();
  }catch{}
}
async function refresh(){
  const s=await j('/api/v1/status');
  document.getElementById('status').textContent=JSON.stringify(s,null,2);
  document.getElementById('brightness').value=s.brightness;
  // Prefill static fields from live DHCP values if empty
  if(!ip.value && s.wifi && s.wifi.ip) ip.value=s.wifi.ip;
  if(!gateway.value && s.wifi && s.wifi.gateway) gateway.value=s.wifi.gateway;
  if((!netmask.value || netmask.value==='255.255.255.0') && s.wifi && s.wifi.netmask) netmask.value=s.wifi.netmask;
  if(!dns.value && s.wifi && s.wifi.dns) dns.value=s.wifi.dns;
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
    dns:dns.value.trim()
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
  refresh();
}
async function uploadOta(){
  const f=firmware.files[0]; if(!f){alert('Choose a .bin');return;}
  ota.textContent='Uploading…';
  const r=await fetch('/api/v1/ota',{method:'POST',body:f,headers:{'Content-Type':'application/octet-stream'}});
  ota.textContent=await r.text();
}
loadSettings(); refresh(); scanWifi(); setInterval(refresh,3000);
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
