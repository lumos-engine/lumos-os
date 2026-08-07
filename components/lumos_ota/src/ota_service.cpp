#include "lumos/ota/ota_service.hpp"
#include "lumos/core/logger.hpp"

#include "esp_app_format.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace lumos {
namespace {
Logger log{"ota"};
}

esp_err_t OtaService::post_ota(httpd_req_t* req) {
    const esp_partition_t* update_partition = esp_ota_get_next_update_partition(nullptr);
    if (update_partition == nullptr) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no ota partition");
        return ESP_FAIL;
    }

    esp_ota_handle_t ota_handle = 0;
    esp_err_t err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "ota begin failed");
        return err;
    }

    char buf[1024];
    int remaining = req->content_len;
    log.info("OTA upload starting (%d bytes) -> %s", remaining, update_partition->label);

    while (remaining > 0) {
        const int to_read = remaining > static_cast<int>(sizeof(buf))
                                ? static_cast<int>(sizeof(buf))
                                : remaining;
        const int received = httpd_req_recv(req, buf, to_read);
        if (received <= 0) {
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recv failed");
            return ESP_FAIL;
        }
        err = esp_ota_write(ota_handle, buf, received);
        if (err != ESP_OK) {
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "ota write failed");
            return err;
        }
        remaining -= received;
    }

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "ota end failed");
        return err;
    }
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "set boot failed");
        return err;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true,\"rebooting\":true}");
    log.info("OTA success — rebooting");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK;
}

Result<void> OtaService::start(httpd_handle_t server) {
    httpd_uri_t uri = {
        .uri = "/api/v1/ota",
        .method = HTTP_POST,
        .handler = post_ota,
        .user_ctx = nullptr,
    };
    if (httpd_register_uri_handler(server, &uri) != ESP_OK) {
        return Result<void>::fail(ErrorCode::IoError, "failed to register OTA route");
    }
    return Result<void>::ok();
}

} // namespace lumos
