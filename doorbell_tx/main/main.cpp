#include "lumos/core/logger.hpp"
#include "lumos/doorbell/doorbell_mac.hpp"
#include "lumos/doorbell/doorbell_transmitter.hpp"
#include "lumos/ota/ota_service.hpp"
#include "lumos/wifi/captive_dns.hpp"

#include "cJSON.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mdns.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace {

lumos::Logger log{"doorbell_tx_main"};
lumos::DoorbellTransmitter* g_tx{nullptr};
lumos::CaptiveDns g_captive_dns;
lumos::OtaService g_ota;
esp_netif_t* g_ap_netif{nullptr};
esp_netif_t* g_sta_netif{nullptr};
bool g_want_sta{false};
int g_sta_fails{0};
std::string g_sta_ip;
int g_sta_rssi{0};

constexpr const char* kApSsid = "LumosOS-Bell";
constexpr const char* kWifiNs = "dbwifi";

struct WifiPrefs {
    std::string ssid;
    std::string password;
    std::string hostname{"LumosOS-Bell"};
    bool use_static{false};
    std::string ip;
    std::string gateway;
    std::string netmask{"255.255.255.0"};
    std::string dns1;
    std::string dns2;
};

WifiPrefs g_wifi;

constexpr const char* kIndexHtml = R"HTML(<!DOCTYPE html>
<html lang="en"><head>
<meta charset="utf-8"/><meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>Doorbell TX</title>
<style>
:root{--bg:#0e1116;--card:#171c24;--text:#e8edf5;--muted:#8b95a8;--accent:#6cb6ff;--line:#2a3340}
*{box-sizing:border-box}body{margin:0;font:15px/1.45 system-ui,sans-serif;background:var(--bg);color:var(--text)}
header{padding:1.25rem}h1{margin:0;font-size:1.3rem}
p,label{color:var(--muted)}main{padding:0 1.25rem 2rem;max-width:520px}
section{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:1rem;margin-bottom:1rem}
label{display:block;margin:.5rem 0 .25rem;font-size:.85rem}
input,select,button{width:100%;padding:.65rem .75rem;border-radius:8px;border:1px solid var(--line);background:#0f141b;color:var(--text)}
button{background:var(--accent);color:#041018;border:none;font-weight:600;margin-top:.75rem;cursor:pointer}
button.secondary{background:transparent;color:var(--accent);border:1px solid var(--accent)}
.check{display:flex;align-items:center;gap:.5rem;color:var(--text);margin:.6rem 0}
.check input{width:auto}
.hint{font-size:.8rem;color:var(--muted)}
.row{display:grid;grid-template-columns:1fr auto;gap:.75rem;align-items:end}
.grid2{display:grid;grid-template-columns:1fr 1fr;gap:.75rem}
pre{white-space:pre-wrap;background:#0f141b;padding:.75rem;border-radius:8px;font-size:.8rem}
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
#staticFields{display:none}#staticFields.show{display:block}
</style></head>
<body>
<header><h1>Doorbell transmitter</h1><p>ESP-NOW · join home Wi-Fi like LumosOS</p></header>
<main>
<section>
<h2>Status</h2>
<div id="pairCard" class="pair no"><div class="dot"></div><div><h3>LED board</h3><p>Loading pairing status…</p></div></div>
<pre id="status">Loading…</pre>
</section>
<section>
<h2>Wi-Fi</h2>
<p class="hint">Connect this bell to the same router as the LED board. After that, use <b>http://lumosos-bell.local</b> (or the static IP) from your phone — no hotspot needed.</p>
<label>Nearby networks</label>
<div class="row">
  <select id="netlist"><option value="">Scan to list…</option></select>
  <button class="secondary" type="button" onclick="scanWifi()" style="margin-top:0;width:auto;padding:.65rem 1rem">Scan</button>
</div>
<label>SSID</label><input id="ssid" placeholder="Select above or type"/>
<label>Password</label><input id="pass" type="password" placeholder="Leave blank to keep saved"/>
<label>Hostname</label><input id="hostname" placeholder="LumosOS-Bell"/>
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
<button type="button" onclick="saveWifi()">Save &amp; Connect</button>
<button class="secondary" type="button" onclick="forgetWifi()">Forget Wi-Fi (setup hotspot)</button>
<pre id="wifiMsg"></pre>
</section>
<section>
<h2>Pair LED board</h2>
<p class="hint">On LumosOS open <b>/doorbell</b> → Start pairing, then Find nearby here. If this board is on home Wi-Fi, pairing stays on that channel (home Wi-Fi is not dropped).</p>
<pre id="pairBits"></pre>
<label>LED / receiver MAC</label>
<input id="rxMac" placeholder="AA:BB:CC:DD:EE:FF"/>
<label>ESP-NOW channel (1–13, auto when on Wi-Fi)</label>
<input id="channel" type="number" min="1" max="13" value="1"/>
<label>Optocoupler GPIO</label>
<input id="pin" type="number" min="4" max="39" value="4"/>
<label class="check"><input id="activeLow" type="checkbox" checked/> Active-LOW (typical optocoupler)</label>
<button type="button" onclick="save()">Save doorbell</button>
<button class="secondary" type="button" onclick="testSend()">Test send</button>
<button class="secondary" type="button" onclick="findNearby()">Find nearby LED board</button>
<pre id="msg"></pre>
<div id="peers"></div>
</section>
<section>
<h2>Backup &amp; restore</h2>
<p class="hint">Clone pairing + Wi-Fi to another bell ESP32 after flashing the same firmware.</p>
<label class="check"><input id="cfgSecrets" type="checkbox"/> Include Wi-Fi password in download</label>
<label class="check"><input id="cfgClearIp" type="checkbox" checked/> On import: clear static IP</label>
<button type="button" onclick="downloadConfig()">Download config JSON</button>
<button class="secondary" type="button" onclick="cfgFile.click()">Upload config JSON…</button>
<input id="cfgFile" type="file" accept="application/json,.json" style="display:none" onchange="uploadConfig(event)"/>
<pre id="cfgStatus"></pre>
</section>
<section>
<h2>OTA update</h2>
<input id="firmware" type="file" accept=".bin"/>
<button type="button" onclick="uploadOta()">Upload firmware</button>
<pre id="ota"></pre>
</section>
</main>
<script>
let pairTimer=null;
function toggleStatic(){staticFields.classList.toggle('show', useStatic.checked);}
function setPairCard(d){
  const el=document.getElementById('pairCard');
  const pairing=!!(d.pairing||d.scanning);
  const mac=(d.rx_mac||'').trim();
  const paired=!!d.paired && !!mac;
  el.className='pair '+(pairing?'wait':(paired?'ok':'no'));
  const title=pairing?'Pairing…':(paired?'Paired with LED board':'Not paired');
  let detail;
  if(pairing) detail='Searching for LumosOS. Tap Start pairing on the LED board /doorbell page.';
  else if(paired){
    const sent=d.last_send_ms>0?' Last ESP-NOW send this boot at '+d.last_send_ms+' ms.':' No press sent yet this boot — use Test send.';
    detail='Receiver '+mac+'.'+sent;
  } else detail='Same Wi‑Fi is not pairing. Start pairing on LumosOS /doorbell, then Find nearby here.';
  el.innerHTML='<div class="dot"></div><div><h3>'+title+'</h3><p>'+detail+'</p></div>';
}
function renderPeers(d){
  const box=document.getElementById('peers');
  const list=d.peers||[];
  if(!list.length){
    box.innerHTML=d.scanning?'<p class="hint">Searching…</p>':
      (d.pairing?'<p class="hint">No LumosOS receiver heard. Start pairing on /doorbell first.</p>':'');
    return;
  }
  box.innerHTML=list.map(p=>'<button type="button" class="secondary" onclick="pick(\''+p.mac+'\')">'+
    (p.name||'LumosOS')+' · '+p.mac+' · ch '+p.channel+' · RSSI '+p.rssi+'</button>').join('');
}
async function load(){
  const r=await fetch('/api'); const d=await r.json();
  rxMac.value=d.rx_mac||'';
  channel.value=d.channel||1;
  pin.value=d.opto_pin||4;
  activeLow.checked=!!d.active_low;
  ssid.value=d.wifi_ssid||ssid.value||'';
  hostname.value=d.hostname||'LumosOS-Bell';
  useStatic.checked=!!d.wifi_use_static; toggleStatic();
  ip.value=d.wifi_ip||''; gateway.value=d.wifi_gateway||'';
  netmask.value=d.wifi_netmask||'255.255.255.0';
  dns1.value=d.wifi_dns1||''; dns2.value=d.wifi_dns2||'';
  setPairCard(d);
  status.textContent=[
    'this_mac: '+(d.own_mac||'—'),
    'wifi: '+(d.wifi_connected?(d.wifi_ssid+'  '+d.sta_ip):('setup AP '+ (d.ap_ip||'192.168.4.1'))),
    'hostname: '+(d.hostname||'—')+(d.wifi_connected?'  → http://lumosos-bell.local':''),
    'espnow: '+!!d.espnow_ready+'  paired: '+!!d.paired+'  if: '+(d.sta_linked?'STA':'AP'),
    'channel: '+(d.channel||'—')
  ].join('\n');
  pairBits.textContent='scanning: '+!!d.scanning+'  pairing: '+!!d.pairing;
  renderPeers(d);
  if((d.pairing||d.scanning) && !pairTimer){ pairTimer=setInterval(load,1000); }
  if(!d.pairing && !d.scanning && pairTimer){ clearInterval(pairTimer); pairTimer=null; }
}
async function scanWifi(){
  netlist.innerHTML='<option>Scanning…</option>';
  try{
    const data=await (await fetch('/api/v1/wifi/scan')).json();
    netlist.innerHTML='';
    const blank=document.createElement('option');
    blank.value=''; blank.textContent=(data.networks&&data.networks.length)?'Select a network…':'No networks found';
    netlist.appendChild(blank);
    for(const n of (data.networks||[])){
      const o=document.createElement('option'); o.value=n.ssid;
      o.textContent=n.ssid+'  ('+n.rssi+' dBm, ch '+n.channel+')'; netlist.appendChild(o);
    }
  }catch{ netlist.innerHTML='<option>Scan failed</option>'; }
}
netlist.addEventListener('change',e=>{ if(e.target.value) ssid.value=e.target.value; });
async function saveWifi(){
  const name=ssid.value.trim();
  if(!name){alert('Select or enter an SSID');return;}
  if(useStatic.checked && (!ip.value.trim()||!gateway.value.trim())){alert('Static IP needs IP and gateway');return;}
  wifiMsg.textContent='Connecting…';
  try{
    const r=await fetch('/api/v1/wifi',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({
      ssid:name,password:pass.value,hostname:hostname.value.trim(),
      use_static:useStatic.checked,ip:ip.value.trim(),gateway:gateway.value.trim(),
      netmask:netmask.value.trim()||'255.255.255.0',dns1:dns1.value.trim(),dns2:dns2.value.trim()
    })});
    const t=await r.text();
    wifiMsg.textContent=t;
    alert('Connecting… then open http://lumosos-bell.local or the static IP. Setup hotspot will go away.');
  }catch(e){ wifiMsg.textContent='Failed: '+e.message; }
}
async function forgetWifi(){
  if(!confirm('Drop saved Wi-Fi and reopen LumosOS-Bell hotspot?')) return;
  await fetch('/api/v1/wifi',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({forget:true})});
  wifiMsg.textContent='Forgetting… device reboots to setup AP.';
}
async function save(){
  msg.textContent='Saving…';
  const body=new URLSearchParams({
    rx_mac:rxMac.value.trim(), channel:String(channel.value),
    pin:String(pin.value), active_low:activeLow.checked?'1':'0'
  });
  const r=await fetch('/save',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
  msg.textContent=await r.text(); await load();
}
async function testSend(){
  msg.textContent='Sending…';
  const r=await fetch('/test',{method:'POST'}); msg.textContent=await r.text(); await load();
}
async function findNearby(){
  msg.textContent='Searching…';
  try{ await fetch('/discover',{method:'POST'}); await load(); }
  catch(e){ msg.textContent='Search started. Rejoin if the setup hotspot dropped.'; }
}
async function pick(mac){
  msg.textContent='Pairing '+mac+'…';
  const r=await fetch('/pair',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'rx_mac='+encodeURIComponent(mac)});
  msg.textContent=await r.text(); await load();
}
async function downloadConfig(){
  cfgStatus.textContent='Building…';
  try{
    const r=await fetch('/api/v1/config'+(cfgSecrets.checked?'?secrets=1':''));
    if(!r.ok) throw new Error(await r.text());
    const text=await r.text();
    const a=document.createElement('a');
    a.href=URL.createObjectURL(new Blob([text],{type:'application/json'}));
    a.download='lumosos-bell-config.json'; a.click();
    cfgStatus.textContent='Downloaded lumosos-bell-config.json';
  }catch(e){ cfgStatus.textContent='Download failed: '+e.message; }
}
async function uploadConfig(ev){
  const f=ev.target.files&&ev.target.files[0]; ev.target.value='';
  if(!f) return;
  try{
    const parsed=JSON.parse(await f.text());
    if(cfgClearIp.checked){ parsed.clear_static_ip=true; }
    const r=await fetch('/api/v1/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(parsed)});
    const t=await r.text();
    if(!r.ok) throw new Error(t);
    cfgStatus.textContent=t.indexOf('reboot')>=0?'Applied — rebooting…':t;
  }catch(e){ cfgStatus.textContent='Upload failed: '+e.message; }
}
async function uploadOta(){
  const f=firmware.files&&firmware.files[0];
  if(!f){ota.textContent='Pick a .bin first';return;}
  ota.textContent='Uploading…';
  const r=await fetch('/api/v1/ota',{method:'POST',body:f,headers:{'Content-Type':'application/octet-stream'}});
  ota.textContent=await r.text();
}
load(); scanWifi();
setInterval(load,10000);
</script>
</body></html>
)HTML";

std::string nvs_get(nvs_handle_t h, const char* key, const std::string& def = {}) {
    size_t len = 0;
    if (nvs_get_str(h, key, nullptr, &len) != ESP_OK || len == 0) {
        return def;
    }
    std::string out(len, '\0');
    if (nvs_get_str(h, key, out.data(), &len) != ESP_OK) {
        return def;
    }
    if (!out.empty() && out.back() == '\0') {
        out.pop_back();
    }
    return out;
}

void load_wifi_prefs() {
    nvs_handle_t h{};
    if (nvs_open(kWifiNs, NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    g_wifi.ssid = nvs_get(h, "ssid");
    g_wifi.password = nvs_get(h, "pass");
    g_wifi.hostname = nvs_get(h, "host", "LumosOS-Bell");
    std::uint8_t st = 0;
    nvs_get_u8(h, "stat", &st);
    g_wifi.use_static = st != 0;
    g_wifi.ip = nvs_get(h, "ip");
    g_wifi.gateway = nvs_get(h, "gw");
    g_wifi.netmask = nvs_get(h, "mask", "255.255.255.0");
    g_wifi.dns1 = nvs_get(h, "dns1");
    g_wifi.dns2 = nvs_get(h, "dns2");
    nvs_close(h);
}

void save_wifi_prefs() {
    nvs_handle_t h{};
    if (nvs_open(kWifiNs, NVS_READWRITE, &h) != ESP_OK) {
        log.error("nvs_open dbwifi failed");
        return;
    }
    nvs_set_str(h, "ssid", g_wifi.ssid.c_str());
    nvs_set_str(h, "pass", g_wifi.password.c_str());
    nvs_set_str(h, "host", g_wifi.hostname.c_str());
    nvs_set_u8(h, "stat", g_wifi.use_static ? 1 : 0);
    nvs_set_str(h, "ip", g_wifi.ip.c_str());
    nvs_set_str(h, "gw", g_wifi.gateway.c_str());
    nvs_set_str(h, "mask", g_wifi.netmask.c_str());
    nvs_set_str(h, "dns1", g_wifi.dns1.c_str());
    nvs_set_str(h, "dns2", g_wifi.dns2.c_str());
    nvs_commit(h);
    nvs_close(h);
}

std::string mdns_label(std::string host) {
    std::string out;
    for (unsigned char c : host) {
        if (std::isalnum(c) || c == '-') {
            out.push_back(static_cast<char>(std::tolower(c)));
        }
    }
    return out.empty() ? "lumosos-bell" : out;
}

void start_mdns() {
    static bool started = false;
    if (!started) {
        if (mdns_init() != ESP_OK) {
            log.warn("mdns_init failed");
            return;
        }
        started = true;
    }
    const auto host = mdns_label(g_wifi.hostname);
    mdns_hostname_set(host.c_str());
    mdns_instance_name_set(g_wifi.hostname.c_str());
    mdns_service_add(nullptr, "_http", "_tcp", 80, nullptr, 0);
    log.info("mDNS http://%s.local", host.c_str());
}

void apply_hostname() {
    if (g_sta_netif == nullptr) {
        return;
    }
    if (g_wifi.hostname.empty()) {
        g_wifi.hostname = "LumosOS-Bell";
    }
    if (g_wifi.hostname.size() > 32) {
        g_wifi.hostname.resize(32);
    }
    esp_netif_set_hostname(g_sta_netif, g_wifi.hostname.c_str());
}

esp_err_t apply_sta_ip_config() {
    if (g_sta_netif == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!g_wifi.use_static) {
        esp_err_t err = esp_netif_dhcpc_start(g_sta_netif);
        if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
            return err;
        }
        return ESP_OK;
    }
    if (g_wifi.ip.empty() || g_wifi.gateway.empty()) {
        return ESP_ERR_INVALID_ARG;
    }
    const std::string& mask = g_wifi.netmask.empty() ? "255.255.255.0" : g_wifi.netmask;
    const std::string& dns1 = g_wifi.dns1.empty() ? g_wifi.gateway : g_wifi.dns1;
    esp_netif_dhcpc_stop(g_sta_netif);
    esp_netif_ip_info_t ip_info{};
    if (esp_netif_str_to_ip4(g_wifi.ip.c_str(), &ip_info.ip) != ESP_OK ||
        esp_netif_str_to_ip4(g_wifi.gateway.c_str(), &ip_info.gw) != ESP_OK ||
        esp_netif_str_to_ip4(mask.c_str(), &ip_info.netmask) != ESP_OK) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_netif_set_ip_info(g_sta_netif, &ip_info);
    esp_netif_dns_info_t dns_main{};
    dns_main.ip.type = ESP_IPADDR_TYPE_V4;
    if (esp_netif_str_to_ip4(dns1.c_str(), &dns_main.ip.u_addr.ip4) != ESP_OK) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_netif_set_dns_info(g_sta_netif, ESP_NETIF_DNS_MAIN, &dns_main);
    if (!g_wifi.dns2.empty()) {
        esp_netif_dns_info_t dns_backup{};
        dns_backup.ip.type = ESP_IPADDR_TYPE_V4;
        if (esp_netif_str_to_ip4(g_wifi.dns2.c_str(), &dns_backup.ip.u_addr.ip4) == ESP_OK) {
            esp_netif_set_dns_info(g_sta_netif, ESP_NETIF_DNS_BACKUP, &dns_backup);
        }
    }
    log.info("STA static %s gw %s", g_wifi.ip.c_str(), g_wifi.gateway.c_str());
    return ESP_OK;
}

void start_ap_portal();

void connect_sta() {
    g_captive_dns.stop();
    g_want_sta = true;
    g_sta_fails = 0;
    apply_hostname();
    wifi_config_t sta{};
    std::strncpy(reinterpret_cast<char*>(sta.sta.ssid), g_wifi.ssid.c_str(), sizeof(sta.sta.ssid) - 1);
    std::strncpy(reinterpret_cast<char*>(sta.sta.password), g_wifi.password.c_str(),
                 sizeof(sta.sta.password) - 1);
    sta.sta.threshold.authmode = g_wifi.password.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta));
    (void)apply_sta_ip_config();
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_ps(WIFI_PS_NONE);
    esp_wifi_connect();
    log.info("Connecting STA to %s", g_wifi.ssid.c_str());
}

void start_ap_portal() {
    g_want_sta = false;
    if (g_tx != nullptr) {
        g_tx->set_sta_linked(false);
    }
    g_sta_ip.clear();
    wifi_config_t ap{};
    std::strncpy(reinterpret_cast<char*>(ap.ap.ssid), kApSsid, sizeof(ap.ap.ssid));
    ap.ap.ssid_len = static_cast<std::uint8_t>(std::strlen(kApSsid));
    ap.ap.channel = (g_tx != nullptr) ? g_tx->config().channel : 1;
    ap.ap.authmode = WIFI_AUTH_OPEN;
    ap.ap.max_connection = 4;
    ap.ap.beacon_interval = 100;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    esp_netif_ip_info_t ip{};
    if (g_ap_netif != nullptr && esp_netif_get_ip_info(g_ap_netif, &ip) == ESP_OK) {
        g_captive_dns.start(ip.ip.addr);
        log.info("SoftAP %s  " IPSTR, kApSsid, IP2STR(&ip.ip));
    }
}

void reboot_task(void*) {
    vTaskDelay(pdMS_TO_TICKS(800));
    esp_restart();
}

void wifi_event_handler(void*, esp_event_base_t base, std::int32_t id, void* data) {
    if (base == WIFI_EVENT) {
        if (id == WIFI_EVENT_STA_START && g_want_sta) {
            esp_wifi_connect();
        } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
            log.warn("STA disconnected");
            if (g_tx != nullptr) {
                g_tx->set_sta_linked(false);
            }
            g_sta_ip.clear();
            if (g_want_sta) {
                ++g_sta_fails;
                if (g_sta_fails >= 10) {
                    log.warn("STA failed repeatedly — setup AP");
                    start_ap_portal();
                } else {
                    esp_wifi_connect();
                }
            }
        } else if (id == WIFI_EVENT_AP_STACONNECTED) {
            const auto* ev = static_cast<wifi_event_ap_staconnected_t*>(data);
            log.info("AP client join " MACSTR, MAC2STR(ev->mac));
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        g_sta_fails = 0;
        const auto* ev = static_cast<ip_event_got_ip_t*>(data);
        char ip[16];
        esp_ip4addr_ntoa(&ev->ip_info.ip, ip, sizeof(ip));
        g_sta_ip = ip;
        wifi_ap_record_t ap{};
        g_sta_rssi = (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) ? ap.rssi : 0;
        log.info("Got IP %s", ip);
        start_mdns();
        if (g_tx != nullptr) {
            g_tx->set_sta_linked(true);
        }
    }
}

std::string form_value(const std::string& body, const char* key) {
    const std::string prefix = std::string(key) + "=";
    auto pos = body.find(prefix);
    if (pos == std::string::npos) {
        return {};
    }
    pos += prefix.size();
    auto end = body.find('&', pos);
    std::string raw = body.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    std::string out;
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '+') {
            out.push_back(' ');
        } else if (raw[i] == '%' && i + 2 < raw.size()) {
            unsigned v = 0;
            if (std::sscanf(raw.c_str() + i + 1, "%02x", &v) == 1) {
                out.push_back(static_cast<char>(v));
                i += 2;
            }
        } else {
            out.push_back(raw[i]);
        }
    }
    return out;
}

esp_err_t send_text(httpd_req_t* req, const char* body, const char* type = "text/plain") {
    httpd_resp_set_type(req, type);
    return httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
}

esp_err_t send_cjson(httpd_req_t* req, cJSON* root, int status = 200) {
    char* printed = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (printed == nullptr) {
        return send_text(req, "{\"error\":\"json\"}", "application/json");
    }
    if (status != 200) {
        httpd_resp_set_status(req, std::to_string(status).c_str());
    }
    const esp_err_t err = send_text(req, printed, "application/json");
    cJSON_free(printed);
    return err;
}

esp_err_t read_body(httpd_req_t* req, std::string& out) {
    const int total = req->content_len;
    if (total <= 0 || total > 8192) {
        return ESP_FAIL;
    }
    out.resize(static_cast<std::size_t>(total));
    int got = 0;
    while (got < total) {
        const int n = httpd_req_recv(req, out.data() + got, total - got);
        if (n <= 0) {
            return ESP_FAIL;
        }
        got += n;
    }
    return ESP_OK;
}

esp_err_t get_index(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, kIndexHtml, HTTPD_RESP_USE_STRLEN);
}

esp_err_t get_api(httpd_req_t* req) {
    cJSON* root = cJSON_CreateObject();
    if (g_tx != nullptr) {
        const auto st = g_tx->status();
        const auto& cfg = st.cfg;
        cJSON_AddStringToObject(root, "own_mac", st.own_mac.c_str());
        cJSON_AddStringToObject(root, "rx_mac",
                                cfg.rx_mac_valid ? lumos::format_mac(cfg.rx_mac).c_str() : "");
        cJSON_AddBoolToObject(root, "paired", st.paired);
        cJSON_AddNumberToObject(root, "channel", cfg.channel);
        cJSON_AddNumberToObject(root, "opto_pin", cfg.opto_pin);
        cJSON_AddBoolToObject(root, "active_low", cfg.active_low);
        cJSON_AddBoolToObject(root, "espnow_ready", st.espnow_ready);
        cJSON_AddNumberToObject(root, "last_seq", st.last_seq);
        cJSON_AddNumberToObject(root, "last_send_ms", st.last_send_ms);
        cJSON_AddBoolToObject(root, "pairing", st.pairing);
        cJSON_AddBoolToObject(root, "scanning", st.scanning);
        cJSON_AddNumberToObject(root, "pairing_ms", st.pairing_ms);
        cJSON* peers = cJSON_AddArrayToObject(root, "peers");
        for (int i = 0; i < st.peer_count; ++i) {
            cJSON* p = cJSON_CreateObject();
            cJSON_AddStringToObject(p, "mac", lumos::format_mac(st.peers[i].mac).c_str());
            cJSON_AddStringToObject(p, "name", st.peers[i].name);
            cJSON_AddNumberToObject(p, "channel", st.peers[i].channel);
            cJSON_AddNumberToObject(p, "rssi", st.peers[i].rssi);
            cJSON_AddItemToArray(peers, p);
        }
    }
    cJSON_AddStringToObject(root, "wifi_ssid", g_wifi.ssid.c_str());
    cJSON_AddStringToObject(root, "hostname", g_wifi.hostname.c_str());
    cJSON_AddBoolToObject(root, "wifi_use_static", g_wifi.use_static);
    cJSON_AddStringToObject(root, "wifi_ip", g_wifi.ip.c_str());
    cJSON_AddStringToObject(root, "wifi_gateway", g_wifi.gateway.c_str());
    cJSON_AddStringToObject(root, "wifi_netmask", g_wifi.netmask.c_str());
    cJSON_AddStringToObject(root, "wifi_dns1", g_wifi.dns1.c_str());
    cJSON_AddStringToObject(root, "wifi_dns2", g_wifi.dns2.c_str());
    cJSON_AddBoolToObject(root, "wifi_connected", !g_sta_ip.empty());
    cJSON_AddStringToObject(root, "sta_ip", g_sta_ip.c_str());
    cJSON_AddNumberToObject(root, "rssi", g_sta_rssi);
    cJSON_AddStringToObject(root, "ap_ip", "192.168.4.1");
    cJSON_AddBoolToObject(root, "sta_linked", g_tx != nullptr && g_tx->sta_linked());
    return send_cjson(req, root);
}

esp_err_t post_save(httpd_req_t* req) {
    if (g_tx == nullptr) {
        return send_text(req, "not ready");
    }
    char buf[512];
    const int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        return send_text(req, "bad body");
    }
    buf[len] = 0;
    const std::string body(buf);
    auto cfg = g_tx->config();
    const auto mac = form_value(body, "rx_mac");
    cfg.rx_mac_valid = lumos::parse_mac(mac, cfg.rx_mac);
    const auto ch = form_value(body, "channel");
    if (!ch.empty()) {
        cfg.channel = static_cast<std::uint8_t>(std::clamp(std::atoi(ch.c_str()), 1, 13));
    }
    const auto pin = form_value(body, "pin");
    if (!pin.empty()) {
        cfg.opto_pin = std::atoi(pin.c_str());
    }
    const auto al = form_value(body, "active_low");
    cfg.active_low = (al != "0" && al != "false");
    g_tx->apply_config(cfg);
    g_tx->save_nvs();
    return send_text(req, "Saved.");
}

esp_err_t post_test(httpd_req_t* req) {
    if (g_tx == nullptr) {
        return send_text(req, "not ready");
    }
    g_tx->test_send();
    return send_text(req, "Sent (if paired).");
}

esp_err_t post_discover(httpd_req_t* req) {
    if (g_tx == nullptr) {
        return send_text(req, "not ready");
    }
    g_tx->start_pairing();
    return send_text(req, "Scanning nearby LumosOS receivers…");
}

esp_err_t post_pair(httpd_req_t* req) {
    if (g_tx == nullptr) {
        return send_text(req, "not ready");
    }
    char buf[256];
    const int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        return send_text(req, "bad body");
    }
    buf[len] = 0;
    const auto mac = form_value(buf, "rx_mac");
    std::uint8_t parsed[6]{};
    if (!lumos::parse_mac(mac, parsed)) {
        return send_text(req, "mac required");
    }
    if (!g_tx->select_peer(parsed)) {
        return send_text(req, "pair failed");
    }
    return send_text(req, "Paired.");
}

esp_err_t get_wifi_scan(httpd_req_t* req) {
    wifi_mode_t mode = WIFI_MODE_NULL;
    esp_wifi_get_mode(&mode);
    if (mode == WIFI_MODE_AP) {
        esp_wifi_set_mode(WIFI_MODE_APSTA);
    }
    wifi_scan_config_t scan{};
    scan.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    if (esp_wifi_scan_start(&scan, true) != ESP_OK) {
        return send_text(req, "{\"error\":\"scan failed\"}", "application/json");
    }
    std::uint16_t n = 0;
    esp_wifi_scan_get_ap_num(&n);
    if (n > 40) {
        n = 40;
    }
    std::vector<wifi_ap_record_t> rec(n);
    if (n > 0) {
        esp_wifi_scan_get_ap_records(&n, rec.data());
    }
    cJSON* root = cJSON_CreateObject();
    cJSON* nets = cJSON_AddArrayToObject(root, "networks");
    std::set<std::string> seen;
    for (std::uint16_t i = 0; i < n; ++i) {
        const char* ssid = reinterpret_cast<const char*>(rec[i].ssid);
        if (ssid[0] == '\0') {
            continue;
        }
        std::string name(ssid);
        if (!seen.insert(name).second) {
            continue;
        }
        cJSON* o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "ssid", name.c_str());
        cJSON_AddNumberToObject(o, "rssi", rec[i].rssi);
        cJSON_AddNumberToObject(o, "channel", rec[i].primary);
        cJSON_AddBoolToObject(o, "secure", rec[i].authmode != WIFI_AUTH_OPEN);
        cJSON_AddItemToArray(nets, o);
    }
    return send_cjson(req, root);
}

const char* json_str(const cJSON* obj, const char* key) {
    const cJSON* v = cJSON_GetObjectItem(obj, key);
    return cJSON_IsString(v) && v->valuestring ? v->valuestring : nullptr;
}

esp_err_t post_wifi(httpd_req_t* req) {
    std::string body;
    if (read_body(req, body) != ESP_OK) {
        return send_text(req, "{\"error\":\"bad body\"}", "application/json");
    }
    cJSON* json = cJSON_Parse(body.c_str());
    if (json == nullptr) {
        return send_text(req, "{\"error\":\"invalid json\"}", "application/json");
    }
    if (cJSON_IsTrue(cJSON_GetObjectItem(json, "forget"))) {
        cJSON_Delete(json);
        g_wifi = WifiPrefs{};
        save_wifi_prefs();
        xTaskCreate(&reboot_task, "reboot", 2048, nullptr, 5, nullptr);
        return send_text(req, "{\"ok\":true,\"rebooting\":true}", "application/json");
    }
    if (const char* s = json_str(json, "ssid"); s && s[0]) {
        g_wifi.ssid = s;
    }
    if (const char* s = json_str(json, "password"); s && s[0]) {
        g_wifi.password = s;
    }
    if (const char* s = json_str(json, "hostname"); s && s[0]) {
        g_wifi.hostname = s;
    }
    if (const cJSON* v = cJSON_GetObjectItem(json, "use_static"); cJSON_IsBool(v)) {
        g_wifi.use_static = cJSON_IsTrue(v);
    }
    if (const char* s = json_str(json, "ip"); s) {
        g_wifi.ip = s;
    }
    if (const char* s = json_str(json, "gateway"); s) {
        g_wifi.gateway = s;
    }
    if (const char* s = json_str(json, "netmask"); s && s[0]) {
        g_wifi.netmask = s;
    }
    if (const char* s = json_str(json, "dns1"); s) {
        g_wifi.dns1 = s;
    }
    if (const char* s = json_str(json, "dns2"); s) {
        g_wifi.dns2 = s;
    }
    cJSON_Delete(json);
    if (g_wifi.ssid.empty()) {
        return send_text(req, "{\"error\":\"ssid required\"}", "application/json");
    }
    save_wifi_prefs();
    connect_sta();
    return send_text(req, "{\"ok\":true}", "application/json");
}

esp_err_t get_config(httpd_req_t* req) {
    const bool secrets = std::strstr(req->uri, "secrets=1") != nullptr;
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "product", "doorbell_tx");
    cJSON* device = cJSON_AddObjectToObject(root, "device");
    if (g_tx != nullptr) {
        const auto cfg = g_tx->config();
        cJSON_AddStringToObject(device, "rx_mac",
                                cfg.rx_mac_valid ? lumos::format_mac(cfg.rx_mac).c_str() : "");
        cJSON_AddNumberToObject(device, "channel", cfg.channel);
        cJSON_AddNumberToObject(device, "opto_pin", cfg.opto_pin);
        cJSON_AddBoolToObject(device, "active_low", cfg.active_low);
        cJSON_AddNumberToObject(device, "tx_id", static_cast<double>(cfg.tx_id));
    }
    cJSON_AddStringToObject(device, "wifi_ssid", g_wifi.ssid.c_str());
    cJSON_AddStringToObject(device, "hostname", g_wifi.hostname.c_str());
    if (secrets) {
        cJSON_AddStringToObject(device, "wifi_password", g_wifi.password.c_str());
    }
    cJSON_AddBoolToObject(device, "wifi_use_static", g_wifi.use_static);
    cJSON_AddStringToObject(device, "wifi_ip", g_wifi.ip.c_str());
    cJSON_AddStringToObject(device, "wifi_gateway", g_wifi.gateway.c_str());
    cJSON_AddStringToObject(device, "wifi_netmask", g_wifi.netmask.c_str());
    cJSON_AddStringToObject(device, "wifi_dns1", g_wifi.dns1.c_str());
    cJSON_AddStringToObject(device, "wifi_dns2", g_wifi.dns2.c_str());
    return send_cjson(req, root);
}

esp_err_t post_config(httpd_req_t* req) {
    std::string body;
    if (read_body(req, body) != ESP_OK) {
        return send_text(req, "{\"error\":\"bad body\"}", "application/json");
    }
    cJSON* json = cJSON_Parse(body.c_str());
    if (json == nullptr) {
        return send_text(req, "{\"error\":\"invalid json\"}", "application/json");
    }
    cJSON* device = cJSON_GetObjectItem(json, "device");
    if (!cJSON_IsObject(device)) {
        device = json;
    }
    const bool clear_ip = cJSON_IsTrue(cJSON_GetObjectItem(json, "clear_static_ip"));
    if (g_tx != nullptr) {
        auto cfg = g_tx->config();
        if (const char* s = json_str(device, "rx_mac"); s) {
            cfg.rx_mac_valid = lumos::parse_mac(s, cfg.rx_mac);
        }
        if (const cJSON* v = cJSON_GetObjectItem(device, "channel"); cJSON_IsNumber(v)) {
            cfg.channel = static_cast<std::uint8_t>(std::clamp(v->valueint, 1, 13));
        }
        if (const cJSON* v = cJSON_GetObjectItem(device, "opto_pin"); cJSON_IsNumber(v)) {
            cfg.opto_pin = v->valueint;
        }
        if (const cJSON* v = cJSON_GetObjectItem(device, "active_low"); cJSON_IsBool(v)) {
            cfg.active_low = cJSON_IsTrue(v);
        }
        g_tx->apply_config(cfg);
        g_tx->save_nvs();
    }
    if (const char* s = json_str(device, "wifi_ssid"); s) {
        g_wifi.ssid = s;
    }
    if (const char* s = json_str(device, "wifi_password"); s) {
        g_wifi.password = s;
    }
    if (const char* s = json_str(device, "hostname"); s && s[0]) {
        g_wifi.hostname = s;
    }
    if (const cJSON* v = cJSON_GetObjectItem(device, "wifi_use_static"); cJSON_IsBool(v)) {
        g_wifi.use_static = cJSON_IsTrue(v);
    }
    if (const char* s = json_str(device, "wifi_ip"); s) {
        g_wifi.ip = s;
    }
    if (const char* s = json_str(device, "wifi_gateway"); s) {
        g_wifi.gateway = s;
    }
    if (const char* s = json_str(device, "wifi_netmask"); s && s[0]) {
        g_wifi.netmask = s;
    }
    if (const char* s = json_str(device, "wifi_dns1"); s) {
        g_wifi.dns1 = s;
    }
    if (const char* s = json_str(device, "wifi_dns2"); s) {
        g_wifi.dns2 = s;
    }
    if (clear_ip) {
        g_wifi.use_static = false;
        g_wifi.ip.clear();
    }
    cJSON_Delete(json);
    save_wifi_prefs();
    xTaskCreate(&reboot_task, "reboot", 2048, nullptr, 5, nullptr);
    return send_text(req, "{\"ok\":true,\"reboot\":true}", "application/json");
}

esp_err_t http_404_redirect(httpd_req_t* req, httpd_err_code_t) {
    httpd_resp_set_status(req, "302 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);
}

void start_http() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 20;
    config.max_open_sockets = 7;
    config.stack_size = 8192;
    config.lru_purge_enable = true;
    httpd_handle_t server = nullptr;
    if (httpd_start(&server, &config) != ESP_OK) {
        log.error("httpd_start failed");
        return;
    }
    const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = get_index, .user_ctx = nullptr},
        {.uri = "/api", .method = HTTP_GET, .handler = get_api, .user_ctx = nullptr},
        {.uri = "/save", .method = HTTP_POST, .handler = post_save, .user_ctx = nullptr},
        {.uri = "/test", .method = HTTP_POST, .handler = post_test, .user_ctx = nullptr},
        {.uri = "/discover", .method = HTTP_POST, .handler = post_discover, .user_ctx = nullptr},
        {.uri = "/pair", .method = HTTP_POST, .handler = post_pair, .user_ctx = nullptr},
        {.uri = "/api/v1/wifi/scan", .method = HTTP_GET, .handler = get_wifi_scan, .user_ctx = nullptr},
        {.uri = "/api/v1/wifi", .method = HTTP_POST, .handler = post_wifi, .user_ctx = nullptr},
        {.uri = "/api/v1/config", .method = HTTP_GET, .handler = get_config, .user_ctx = nullptr},
        {.uri = "/api/v1/config", .method = HTTP_POST, .handler = post_config, .user_ctx = nullptr},
    };
    for (const auto& r : routes) {
        httpd_register_uri_handler(server, &r);
    }
    httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_redirect);
    if (!g_ota.start(server)) {
        log.error("OTA route failed");
    }
    log.info("HTTP on :80");
}

void start_wifi() {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    g_sta_netif = esp_netif_create_default_wifi_sta();
    g_ap_netif = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, nullptr));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    load_wifi_prefs();
    apply_hostname();
    if (!g_wifi.ssid.empty()) {
        connect_sta();
    } else {
        start_ap_portal();
    }
}

} // namespace

extern "C" void app_main() {
    log.info("Booting doorbell transmitter (ESP32, no LEDs)");

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    static lumos::DoorbellTransmitter tx;
    tx.load_nvs();
    start_wifi();
    g_tx = &tx;
    start_http();
    auto started = tx.start();
    if (!started) {
        log.error("transmitter start failed: %s", started.error().message.c_str());
    }
    if (!g_sta_ip.empty()) {
        tx.set_sta_linked(true);
    }
    log.info("ready  this_mac=%s  setup=http://192.168.4.1/  lan=http://lumosos-bell.local",
             tx.own_mac().c_str());
}
