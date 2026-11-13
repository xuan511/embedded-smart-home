// SPDX-License-Identifier: MIT
#include "home_network.h"
#include "home_gateway.h"
#include "sdkconfig.h"
#include <stdio.h>
#include <string.h>
#include "esp_console.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static home_network_state_t state;
static portMUX_TYPE state_lock = portMUX_INITIALIZER_UNLOCKED;
void home_network_snapshot(home_network_state_t *out)
{
    portENTER_CRITICAL(&state_lock); *out = state; portEXIT_CRITICAL(&state_lock);
}

#if CONFIG_HOME_NETWORK
#include "home_service.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "mqtt_client.h"

static const char *TAG = "network";
static esp_mqtt_client_handle_t mqtt;
static char topic_state[80], topic_availability[80];
static bool mqtt_started;
typedef struct { uint32_t id; size_t size; char body[HOME_API_BODY_MAX + 1]; } cached_request_t;
static cached_request_t cache[8]; // RAM only; HTTP server runs handlers serially.
static unsigned cache_next;

static bool authorized(httpd_req_t *req)
{
    char actual[96], expected[96];
    size_t length = httpd_req_get_hdr_value_len(req, "Authorization");
    if (!length || length >= sizeof(actual) ||
        httpd_req_get_hdr_value_str(req, "Authorization", actual, sizeof(actual)) != ESP_OK) return false;
    snprintf(expected, sizeof(expected), "Bearer %s", CONFIG_HOME_API_TOKEN);
    return !strcmp(actual, expected);
}

static esp_err_t response(httpd_req_t *req, const char *status, const char *body)
{
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_sendstr(req, body);
    // Close after errors, including rejected requests whose body is unread.
    // Never let leftover body bytes become another keep-alive HTTP request.
    return status[0] >= '4' ? ESP_FAIL : err;
}

static esp_err_t get_state(httpd_req_t *req)
{
    if (!authorized(req)) return response(req, "401 Unauthorized", "{\"error\":\"unauthorized\"}");
    home_snapshot_t view; home_network_state_t net;
    char body[HOME_API_STATE_MAX];
    home_snapshot(&view); home_network_snapshot(&net);
    if (!home_api_encode(&view, &net, body, sizeof(body)))
        return response(req, "500 Internal Server Error", "{\"error\":\"state_encoding\"}");
    return response(req, "200 OK", body);
}

static esp_err_t post_control(httpd_req_t *req)
{
    if (!authorized(req)) return response(req, "401 Unauthorized", "{\"error\":\"unauthorized\"}");
    if (!req->content_len || req->content_len > HOME_API_BODY_MAX)
        return response(req, "413 Content Too Large", "{\"error\":\"body_size\"}");
    char body[HOME_API_BODY_MAX + 1];
    size_t used = 0;
    while (used < req->content_len) {
        int n = httpd_req_recv(req, body + used, req->content_len - used);
        if (n <= 0) return ESP_FAIL; // End the connection; never execute a partial body.
        used += (size_t)n;
    }
    body[used] = 0;
    home_snapshot_t view; home_snapshot(&view);
    home_request_t command;
    if (!home_api_decode(body, used, &view.state.config, &command))
        return response(req, "400 Bad Request", "{\"error\":\"invalid_command\"}");
    char reply[96];
    for (unsigned i = 0; i < 8; ++i) if (cache[i].id == command.request_id) {
        if (cache[i].size != used || memcmp(cache[i].body, body, used))
            return response(req, "409 Conflict", "{\"error\":\"id_reused\"}");
        snprintf(reply, sizeof(reply), "{\"id\":%lu,\"queued\":true,\"duplicate\":true}", (unsigned long)command.request_id);
        return response(req, "200 OK", reply);
    }
    if (view.pending) return response(req, "409 Conflict", "{\"error\":\"busy\"}");
    if (command.action != HOME_CONFIG && !view.simulated && (!view.online || !view.synced))
        return response(req, "503 Service Unavailable", "{\"error\":\"stm32_not_ready\"}");
    if (!home_submit(&command))
        return response(req, "503 Service Unavailable", "{\"error\":\"queue_full\"}");
    cached_request_t *entry = &cache[cache_next++ % 8];
    entry->id = command.request_id; entry->size = used; memcpy(entry->body, body, used);
    snprintf(reply, sizeof(reply), "{\"id\":%lu,\"queued\":true,\"duplicate\":false}", (unsigned long)command.request_id);
    return response(req, "202 Accepted", reply); // Local queue acceptance is NOT a hardware ACK.
}

static void mqtt_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base;
    const esp_mqtt_event_t *event = data;
    if (id == MQTT_EVENT_CONNECTED) {
        portENTER_CRITICAL(&state_lock);
        state.mqtt = true; state.mqtt_failure = HOME_MQTT_FAILURE_NONE;
        portEXIT_CRITICAL(&state_lock);
        esp_mqtt_client_publish(mqtt, topic_availability, "online", 0, 1, true);
    } else if (id == MQTT_EVENT_DISCONNECTED) {
        portENTER_CRITICAL(&state_lock);
        state.mqtt = false; ++state.mqtt_disconnect_events;
        portEXIT_CRITICAL(&state_lock);
    } else if (id == MQTT_EVENT_ERROR) {
        // ESP-MQTT 1.0.0: inspect error_type before reading its related fields.
        // Copy numbers during the callback; never retain error_handle pointers.
        int type = 0, code = 0, esp_error = 0, socket_error = 0;
        home_mqtt_failure_t failure = HOME_MQTT_FAILURE_OTHER;
        if (event && event->error_handle) {
            const esp_mqtt_error_codes_t *error = event->error_handle;
            type = error->error_type;
            if (type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                failure = HOME_MQTT_FAILURE_TRANSPORT;
                esp_error = error->esp_tls_last_esp_err;
                socket_error = error->esp_transport_sock_errno;
            } else if (type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                code = error->connect_return_code;
                failure = code == MQTT_CONNECTION_REFUSE_BAD_USERNAME || code == MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED
                    ? HOME_MQTT_FAILURE_AUTH : HOME_MQTT_FAILURE_REFUSED;
            }
        }
        portENTER_CRITICAL(&state_lock);
        state.mqtt = false; state.mqtt_failure = failure;
        state.last_mqtt_type = type; state.last_mqtt_code = code;
        state.last_mqtt_esp_error = esp_error; state.last_mqtt_socket_errno = socket_error;
        portEXIT_CRITICAL(&state_lock);
        ESP_LOGW(TAG, "MQTT error type=%d code=%d esp=%d errno=%d; net status for next checks",
            type, code, esp_error, socket_error);
    }
}

static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) esp_wifi_connect();
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *event = data;
        int reason = event ? event->reason : 0;
        portENTER_CRITICAL(&state_lock);
        state.wifi = state.mqtt = false; strcpy(state.ip, "0.0.0.0");
        state.last_wifi_reason = reason; ++state.wifi_disconnect_events;
        portEXIT_CRITICAL(&state_lock);
        ESP_LOGW(TAG, "Wi-Fi disconnected reason=%d; check SSID/authentication/AP/signal; retrying", reason);
        esp_wifi_connect(); // ESP driver schedules connection; never block controller/UI.
    }
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = data;
        char ip[16]; snprintf(ip, sizeof(ip), IPSTR, IP2STR(&event->ip_info.ip));
        portENTER_CRITICAL(&state_lock); state.wifi = true; strcpy(state.ip, ip); portEXIT_CRITICAL(&state_lock);
        ESP_LOGI(TAG, "LAN IP: %s; phone API port 80 (Bearer token required)", ip);
        if (mqtt && !mqtt_started) {
            mqtt_started = esp_mqtt_client_start(mqtt) == ESP_OK;
        }
    }
}

static void upload_task(void *arg)
{
    (void)arg;
    for (;;) {
        home_network_state_t net; home_network_snapshot(&net);
        if (net.wifi && net.mqtt) {
            home_snapshot_t view; char body[HOME_API_STATE_MAX]; home_snapshot(&view);
            if (home_api_encode(&view, &net, body, sizeof(body)))
                // QoS0/non-retained: no backlog of old sensor values after reconnect.
                esp_mqtt_client_publish(mqtt, topic_state, body, 0, 0, false);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static esp_err_t start_network(void)
{
    if (!strlen(CONFIG_HOME_WIFI_SSID)) { ESP_LOGI(TAG, "Wi-Fi not configured; local LCD/USB only"); return ESP_OK; }
    size_t token_length = strlen(CONFIG_HOME_API_TOKEN);
    if (token_length < 16 || token_length > 64) {
        ESP_LOGE(TAG, "Set a 16..64 character API token in menuconfig; network disabled"); return ESP_ERR_INVALID_ARG;
    }
    for (size_t i = 0; i < token_length; ++i) {
        char c = CONFIG_HOME_API_TOKEN[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_'))
            return ESP_ERR_INVALID_ARG;
    }
    const char *device = CONFIG_HOME_DEVICE_ID;
    if (!device[0] || strlen(device) > 32 || strlen(CONFIG_HOME_WIFI_SSID) > 32 || strlen(CONFIG_HOME_WIFI_PASSWORD) > 63)
        return ESP_ERR_INVALID_ARG;
    for (const char *p = device; *p; ++p)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '-' || *p == '_'))
            return ESP_ERR_INVALID_ARG;
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif");
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    if (!esp_netif_create_default_wifi_sta()) return ESP_ERR_NO_MEM;
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init), TAG, "wifi init");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, NULL), TAG, "wifi events");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event, NULL), TAG, "ip events");
    wifi_config_t wifi = {0};
    memcpy(wifi.sta.ssid, CONFIG_HOME_WIFI_SSID, strlen(CONFIG_HOME_WIFI_SSID));
    memcpy(wifi.sta.password, CONFIG_HOME_WIFI_PASSWORD, strlen(CONFIG_HOME_WIFI_PASSWORD));
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "wifi storage");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "wifi mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi), TAG, "wifi credentials");
    if (strlen(CONFIG_HOME_MQTT_URI)) {
        if (strncmp(CONFIG_HOME_MQTT_URI, "mqtt://", 7)) return ESP_ERR_INVALID_ARG;
        snprintf(topic_state, sizeof(topic_state), "home/%s/state", device);
        snprintf(topic_availability, sizeof(topic_availability), "home/%s/availability", device);
        const esp_mqtt_client_config_t config = {
            .broker.address.uri = CONFIG_HOME_MQTT_URI,
            .credentials.client_id = device,
            .credentials.username = strlen(CONFIG_HOME_MQTT_USERNAME) ? CONFIG_HOME_MQTT_USERNAME : NULL,
            .credentials.authentication.password = CONFIG_HOME_MQTT_PASSWORD,
            .session.keepalive = 15,
            .session.last_will = {.topic = topic_availability, .msg = "offline", .qos = 1, .retain = true},
            .network.timeout_ms = 3000,
        };
        mqtt = esp_mqtt_client_init(&config);
        if (!mqtt) return ESP_ERR_NO_MEM;
        ESP_RETURN_ON_ERROR(esp_mqtt_client_register_event(mqtt, ESP_EVENT_ANY_ID, mqtt_event, NULL), TAG, "mqtt events");
    }
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192; config.max_open_sockets = 4; config.lru_purge_enable = true;
    config.recv_wait_timeout = 2; config.send_wait_timeout = 2;
    httpd_handle_t server = NULL;
    ESP_RETURN_ON_ERROR(httpd_start(&server, &config), TAG, "HTTP server");
    const httpd_uri_t get = {.uri = "/api/state", .method = HTTP_GET, .handler = get_state};
    const httpd_uri_t post = {.uri = "/api/control", .method = HTTP_POST, .handler = post_control};
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &get), TAG, "state endpoint");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &post), TAG, "control endpoint");
    portENTER_CRITICAL(&state_lock); state.enabled = state.api = true; strcpy(state.ip, "0.0.0.0"); portEXIT_CRITICAL(&state_lock);
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start");
    if (mqtt && xTaskCreate(upload_task, "home_upload", 6144, NULL, 3, NULL) != pdPASS) return ESP_ERR_NO_MEM;
    return ESP_OK;
}
#else
static esp_err_t start_network(void) { return ESP_OK; }
#endif

esp_err_t home_network_start(void)
{
#if CONFIG_HOME_NETWORK
    portENTER_CRITICAL(&state_lock);
    state.wifi_configured = strlen(CONFIG_HOME_WIFI_SSID) != 0;
    state.mqtt_configured = strlen(CONFIG_HOME_MQTT_URI) != 0;
    portEXIT_CRITICAL(&state_lock);
#endif
    esp_err_t err = start_network();
    portENTER_CRITICAL(&state_lock); state.start_error = err; portEXIT_CRITICAL(&state_lock);
    return err;
}

static int net_command(int argc, char **argv)
{
    if (argc != 2 || strcmp(argv[1], "status")) { puts("net status (credentials: menuconfig > Smart Home > Network)"); return 1; }
    home_network_state_t net; home_network_snapshot(&net);
    printf("Network enabled=%d WiFi=%d IP=%s API=%d MQTT=%d; token/credentials never printed\n",
        net.enabled, net.wifi, net.ip, net.api, net.mqtt);
    printf("Hint=%s start_error=%s | last WiFi reason=%d events=%lu\n",
        home_network_hint(&net), esp_err_to_name(net.start_error), net.last_wifi_reason,
        (unsigned long)net.wifi_disconnect_events);
    printf("Last MQTT type=%d code=%d esp=%d errno=%d events=%lu (historical; current status above)\n",
        net.last_mqtt_type, net.last_mqtt_code, net.last_mqtt_esp_error,
        net.last_mqtt_socket_errno, (unsigned long)net.mqtt_disconnect_events);
    return 0;
}
esp_err_t home_network_console_register(void)
{
    const esp_console_cmd_t cmd = {.command = "net", .help = "net status", .func = net_command};
    return esp_console_cmd_register(&cmd);
}
