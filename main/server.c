#include "driver/rmt_encoder.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_system.h"
#include "esp_vfs.h" // Для работы с файлами
#include "globals.h"
#include "http_parser.h"
#include "led_strip_wrapper.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "mdns.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define BUFFER_SIZE 1024
#define MAX_BODY_SIZE 1024
#define SCRATCH_BUFSIZE (8192)  // Буфер для чтения данных
#define UPLOAD_BUFFER_SIZE 4096 // Уменьшаем буфер до 4KB
#define MIN(a, b)                                                              \
  ((a) < (b) ? (a) : (b)) // Добавляем макрос MIN
                          //
static const char *TAG = "http_server";
static char *cached_index_html = NULL;
static size_t cached_index_len = 0;

esp_err_t init_mdns() {
  esp_err_t err = mdns_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "mDNS Init failed: %s", esp_err_to_name(err));
    return err;
  }

  // Установить имя хоста
  err = mdns_hostname_set("smart-lamp");
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "mDNS Set hostname failed: %s", esp_err_to_name(err));
    return err;
  }

  // Установить имя экземпляра (для обнаружения)
  err = mdns_instance_name_set("ESP32 Web Server");
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "mDNS Set instance name failed: %s", esp_err_to_name(err));
    return err;
  }

  // Добавить сервис HTTP
  err = mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "mDNS Add service failed: %s", esp_err_to_name(err));
    return err;
  }

  ESP_LOGI(TAG, "mDNS started: http://smart-lamp.local");
  return ESP_OK;
}

esp_err_t cache_index_html() {
  FILE *f = fopen("/spiffs/index.html", "rb");
  if (!f) {
    ESP_LOGE(TAG, "Failed to open index.html");
    return ESP_FAIL;
  }

  // Получаем размер файла
  fseek(f, 0, SEEK_END);
  cached_index_len = ftell(f);
  fseek(f, 0, SEEK_SET);

  // Выделяем память (обычная куча, не DMA)
  cached_index_html = malloc(cached_index_len + 1);
  if (!cached_index_html) {
    fclose(f);
    ESP_LOGE(TAG, "Failed to allocate cache buffer");
    return ESP_FAIL;
  }

  size_t read_bytes = fread(cached_index_html, 1, cached_index_len, f);
  fclose(f);

  if (read_bytes != cached_index_len) {
    free(cached_index_html);
    cached_index_html = NULL;
    ESP_LOGE(TAG, "Failed to read index.html");
    return ESP_FAIL;
  }

  cached_index_html[cached_index_len] = '\0';
  ESP_LOGI(TAG, "Cached index.html (%d bytes)", (int)cached_index_len);
  return ESP_OK;
}

int check_webapp_uploaded() {
  FILE *f = NULL;
  // Open renamed file for reading
  ESP_LOGI(TAG, "Check index.html exists");
  f = fopen("/spiffs/index.html", "r");
  if (f == NULL) {
    return 0;
  }
  fclose(f);
  return 1;
}

const char *default_html_response =
    "<!DOCTYPE html>\n"
    "<html lang=\"en\">\n"
    "<head>\n"
    "    <meta charset=\"UTF-8\">\n"
    "    <meta name=\"viewport\" content=\"width=device-width, "
    "initial-scale=1.0\">\n"
    "    <title>Error</title>\n"
    "</head>\n"
    "<body>\n"
    "    <h1>Web Interface Unavailable</h1>\n"
    "    <p>Please upload the required files first.</p>\n"
    "</body>\n"
    "</html>";

esp_err_t get_handler(httpd_req_t *req) {
  const int is_webapp_uploaded = check_webapp_uploaded();
  if (!is_webapp_uploaded) {
    ESP_LOGW(TAG, "Web application not yet uploaded");
    return httpd_resp_send(req, default_html_response,
                           strlen(default_html_response));
  }
  if (!cached_index_html) {
    cache_index_html(); // Trying to cache existing file
  }
  if (!cached_index_html) {
    ESP_LOGE(TAG, "Cache not initialized");
    return httpd_resp_send_500(req);
  }

  return httpd_resp_send(req, cached_index_html, cached_index_len);
}

esp_err_t upload_handler(httpd_req_t *req) {
  char *buf = malloc(UPLOAD_BUFFER_SIZE);
  if (!buf) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  char filename[128] = {0};
  FILE *fd = NULL;
  const char *fail_resp = "{\"result\": false}";
  const char *success_resp = "{\"result\": true}";
  char boundary[70] = {0};
  bool file_complete = false;
  size_t total_written = 0;

  // Получаем boundary из Content-Type
  char content_type[128];
  if (httpd_req_get_hdr_value_str(req, "Content-Type", content_type,
                                  sizeof(content_type)) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get Content-Type header");
    free(buf);
    httpd_resp_send(req, fail_resp, strlen(fail_resp));
    return ESP_FAIL;
  }

  char *boundary_start = strstr(content_type, "boundary=");
  if (!boundary_start) {
    ESP_LOGE(TAG, "Boundary not found in Content-Type");
    free(buf);
    httpd_resp_send(req, fail_resp, strlen(fail_resp));
    return ESP_FAIL;
  }
  strncpy(boundary, boundary_start + 9, sizeof(boundary) - 1);
  ESP_LOGI(TAG, "Boundary: %s", boundary);

  // Обработка данных
  int remaining = req->content_len;
  bool is_first_chunk = true;
  char *data_start = NULL;
  size_t data_len = 0;

  while (remaining > 0 && !file_complete) {
    int received = httpd_req_recv(req, buf, MIN(remaining, UPLOAD_BUFFER_SIZE));
    if (received <= 0) {
      ESP_LOGE(TAG, "Receive failed or connection closed");
      if (fd)
        fclose(fd);
      free(buf);
      httpd_resp_send(req, fail_resp, strlen(fail_resp));
      return ESP_FAIL;
    }

    if (is_first_chunk) {
      // Парсинг имени файла из заголовков
      char *filename_start = strstr(buf, "filename=\"");
      if (!filename_start) {
        ESP_LOGE(TAG, "Filename not found in headers");
        free(buf);
        httpd_resp_send(req, fail_resp, strlen(fail_resp));
        return ESP_FAIL;
      }
      filename_start += 10;
      char *filename_end = strchr(filename_start, '"');
      if (!filename_end) {
        ESP_LOGE(TAG, "Malformed filename header");
        free(buf);
        httpd_resp_send(req, fail_resp, strlen(fail_resp));
        return ESP_FAIL;
      }
      strncpy(filename, filename_start, filename_end - filename_start);

      // Открываем файл в SPIFFS
      char filepath[256];
      snprintf(filepath, sizeof(filepath), "/spiffs/%s", filename);
      fd = fopen(filepath, "wb");
      if (!fd) {
        ESP_LOGE(TAG, "Failed to open file %s", filepath);
        free(buf);
        httpd_resp_send(req, fail_resp, strlen(fail_resp));
        return ESP_FAIL;
      }

      // Находим начало данных файла (после \r\n\r\n)
      data_start = strstr(buf, "\r\n\r\n");
      if (data_start) {
        data_start += 4;
        data_len = buf + received - data_start;
      } else {
        data_start = buf;
        data_len = received;
      }
      is_first_chunk = false;
    } else {
      data_start = buf;
      data_len = received;
    }

    // Ищем boundary в текущем чанке
    char *boundary_pos =
        memmem(data_start, data_len, boundary, strlen(boundary));
    if (boundary_pos) {
      // Нашли конец файла
      size_t to_write = boundary_pos - data_start - 2; // Учитываем \r\n
      fwrite(data_start, 1, to_write, fd);
      total_written += to_write;
      file_complete = true;
      ESP_LOGI(TAG, "File end detected at %d bytes", (int)to_write);
    } else {
      // Записываем весь чанк (если boundary не найден)
      fwrite(data_start, 1, data_len, fd);
      total_written += data_len;
    }

    remaining -= received;
  }

  if (fd)
    fclose(fd);
  free(buf);

  if (file_complete) {
    ESP_LOGI(TAG, "File %s uploaded successfully, size: %d bytes", filename,
             (int)total_written);
    cache_index_html();
    httpd_resp_send(req, success_resp, strlen(success_resp));
    return ESP_OK;
  } else {
    ESP_LOGE(TAG, "File upload incomplete");
    httpd_resp_send(req, fail_resp, strlen(fail_resp));
    return ESP_FAIL;
  }
}

esp_err_t get_control_handler(httpd_req_t *req) {
  char resp[128];

  snprintf(resp, sizeof(resp), "{ \"data\": { \"brightness\": %d } }",
           scale_0_255_to_0_100_fast(lamp_state.brightness));
  httpd_resp_send(req, resp, strlen(resp));
  return ESP_OK;
}

esp_err_t control_handler(httpd_req_t *req) {

  char buf[MAX_BODY_SIZE];
  int ret, remaining = req->content_len;

  if (remaining >= MAX_BODY_SIZE) {
    ESP_LOGE(TAG, "Request body too large (%d bytes), max allowed: %d bytes",
             remaining, MAX_BODY_SIZE);
    char *large_resp = "Request body too large";
    httpd_resp_send(req, large_resp, strlen(large_resp));
    return ESP_FAIL;
  }
  // Читаем тело запроса
  while (remaining > 0) {
    ret = httpd_req_recv(req, buf, MIN(remaining, MAX_BODY_SIZE));
    if (ret <= 0) {
      ESP_LOGE(TAG, "Error receiving request body");
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }
    buf[ret] = '\0'; // Добавляем завершающий нуль
    remaining -= ret;
  }

  ESP_LOGI(TAG, "Received body: %s", buf);

  char *brightness_str = strstr(buf, "brightness=");
  if (!brightness_str) {
    char *bad_brightness = "Field 'brightness' not found in request body";
    ESP_LOGE(TAG, "Field 'brightness' not found in request body");
    httpd_resp_send(req, bad_brightness, strlen(bad_brightness));
    return ESP_FAIL;
  }

  // Смещаемся к значению после "brightness="
  brightness_str += strlen("brightness=");
  u_int8_t brightness = atoi(brightness_str); // Преобразуем строку в int

  // Логируем значение brightness
  ESP_LOGI(TAG, "Brightness value: %d", brightness);

  set_brightness_value(brightness);

  const char *resp = "{\"result\": true }";
  httpd_resp_send(req, resp, strlen(resp));

  return ESP_OK;
}

static esp_err_t favicon_handler(httpd_req_t *req) {
  // Отправляем HTTP 204 No Content (нет данных)
  httpd_resp_set_status(req, "204 No Content");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

httpd_uri_t uri_get = {
    .uri = "/", .method = HTTP_GET, .handler = get_handler, .user_ctx = NULL};

httpd_uri_t uri_post_upload = {.uri = "/upload",
                               .method = HTTP_POST,
                               .handler = upload_handler,
                               .user_ctx = NULL};

httpd_uri_t uri_post_control = {.uri = "/api/control",
                                .method = HTTP_POST,
                                .handler = control_handler,
                                .user_ctx = NULL};

httpd_uri_t uri_get_control = {.uri = "/api/control",
                               .method = HTTP_GET,
                               .handler = get_control_handler,
                               .user_ctx = NULL};

httpd_uri_t uri_favicon = {.uri = "/favicon.ico",
                           .method = HTTP_GET,
                           .handler = favicon_handler,
                           .user_ctx = NULL};

httpd_handle_t start_server() {
  init_mdns();
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  httpd_handle_t server = NULL;

  if (httpd_start(&server, &config) == ESP_OK) {
    httpd_register_uri_handler(server, &uri_get);
    httpd_register_uri_handler(server, &uri_favicon);
    httpd_register_uri_handler(server, &uri_post_upload);
    httpd_register_uri_handler(server, &uri_post_control);
    httpd_register_uri_handler(server, &uri_get_control);
  }
  return server;
}
