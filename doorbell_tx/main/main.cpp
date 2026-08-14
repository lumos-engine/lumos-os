#include "lumos/core/logger.hpp"
#include "lumos/doorbell/doorbell_mac.hpp"
#include "lumos/doorbell/doorbell_transmitter.hpp"

#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cstring>
#include <memory>
#include <string>

namespace {

lumos::Logger log{"doorbell_tx_main"};
lumos::DoorbellTransmitter* g_tx{nullptr};

constexpr const char* kApSsid = "LumosOS-Bell";

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
input,button{width:100%;padding:.65rem .75rem;border-radius:8px;border:1px solid var(--line);background:#0f141b;color:var(--text)}
button{background:var(--accent);color:#041018;border:none;font-weight:600;margin-top:.75rem;cursor:pointer}
button.secondary{background:transparent;color:var(--accent);border:1px solid var(--accent)}
.check{display:flex;align-items:center;gap:.5rem;color:var(--text)}
.check input{width:auto}
.hint{font-size:.8rem;color:var(--muted)}
pre{white-space:pre-wrap;background:#0f141b;padding:.75rem;border-radius:8px;font-size:.8rem}
</style></head>
<body>
<header><h1>Doorbell transmitter</h1><p>ESP-NOW · no LED stack</p></header>
<main>
<section>
<p class="hint">Join Wi-Fi <b>LumosOS-Bell</b>, then open this page. Channel must match the LED board's Wi-Fi channel. Paste this board's MAC into LumosOS /doorbell as paired TX.</p>
<pre id="status">Loading…</pre>
<label>LED / receiver MAC</label>
<input id="rxMac" placeholder="AA:BB:CC:DD:EE:FF"/>
<label>ESP-NOW / AP channel (1–13)</label>
<input id="channel" type="number" min="1" max="13" value="1"/>
<label>Optocoupler GPIO</label>
<input id="pin" type="number" min="4" max="39" value="4"/>
<label class="check"><input id="activeLow" type="checkbox" checked/> Active-LOW (typical optocoupler)</label>
<button type="button" onclick="save()">Save</button>
<button class="secondary" type="button" onclick="testSend()">Test send</button>
<pre id="msg"></pre>
</section>
</main>
<script>
async function load(){
  const r=await fetch('/api'); const d=await r.json();
  rxMac.value=d.rx_mac||'';
  channel.value=d.channel||1;
  pin.value=d.opto_pin||4;
  activeLow.checked=!!d.active_low;
  status.textContent=[
    'this_mac: '+(d.own_mac||'—')+'  (paste into LumosOS paired TX)',
    'espnow: '+!!d.espnow_ready,
    'paired: '+!!d.paired,
    'last_seq: '+(d.last_seq||0),
    'last_send_ms: '+(d.last_send_ms||0)
  ].join('\n');
}
async function save(){
  msg.textContent='Saving…';
  const body=new URLSearchParams({
    rx_mac:rxMac.value.trim(),
    channel:String(channel.value),
    pin:String(pin.value),
    active_low:activeLow.checked?'1':'0'
  });
  const r=await fetch('/save',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
  msg.textContent=await r.text();
  await load();
}
async function testSend(){
  msg.textContent='Sending…';
  const r=await fetch('/test',{method:'POST'});
  msg.textContent=await r.text();
  await load();
}
load();
</script>
</body></html>
)HTML";

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
        if (raw[i] == '+' ) {
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

esp_err_t get_index(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, kIndexHtml, HTTPD_RESP_USE_STRLEN);
}

esp_err_t get_api(httpd_req_t* req) {
    if (g_tx == nullptr) {
        return send_text(req, "{\"error\":\"not ready\"}", "application/json");
    }
    const auto cfg = g_tx->config();
    char json[384];
    std::snprintf(json, sizeof(json),
                  "{\"own_mac\":\"%s\",\"rx_mac\":\"%s\",\"paired\":%s,\"channel\":%u,"
                  "\"opto_pin\":%d,\"active_low\":%s,\"espnow_ready\":%s,"
                  "\"last_seq\":%u,\"last_send_ms\":%u}",
                  g_tx->own_mac().c_str(),
                  cfg.rx_mac_valid ? lumos::format_mac(cfg.rx_mac).c_str() : "",
                  cfg.rx_mac_valid ? "true" : "false",
                  static_cast<unsigned>(cfg.channel), cfg.opto_pin,
                  cfg.active_low ? "true" : "false",
                  g_tx->espnow_ready() ? "true" : "false",
                  static_cast<unsigned>(g_tx->last_seq()),
                  static_cast<unsigned>(g_tx->last_send_ms()));
    return send_text(req, json, "application/json");
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

void start_http() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;
    config.stack_size = 6144;
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
    };
    for (const auto& r : routes) {
        httpd_register_uri_handler(server, &r);
    }
}

void start_wifi_apsta(std::uint8_t channel) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    wifi_config_t ap{};
    std::strncpy(reinterpret_cast<char*>(ap.ap.ssid), kApSsid, sizeof(ap.ap.ssid));
    ap.ap.ssid_len = static_cast<std::uint8_t>(std::strlen(kApSsid));
    ap.ap.channel = channel;
    ap.ap.authmode = WIFI_AUTH_OPEN;
    ap.ap.max_connection = 2;
    ap.ap.beacon_interval = 100;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
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
    const auto ch = tx.config().channel;
    start_wifi_apsta(ch);
    auto started = tx.start();
    if (!started) {
        log.error("transmitter start failed: %s", started.error().message.c_str());
        return;
    }
    g_tx = &tx;
    start_http();
    log.info("AP %s  ch=%u  this_mac=%s  http://192.168.4.1/", kApSsid,
             static_cast<unsigned>(tx.config().channel), tx.own_mac().c_str());
}
