#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_system.h"
#include "esp_vfs.h" // Для работы с файлами
#include "http_parser.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 1024
#define SCRATCH_BUFSIZE (8192)  // Буфер для чтения данных
#define UPLOAD_BUFFER_SIZE 4096 // Уменьшаем буфер до 4KB
#define MIN(a, b)                                                              \
  ((a) < (b) ? (a) : (b)) // Добавляем макрос MIN
                          //
static const char *TAG = "http_server";

int check_webapp_uploaded() {
  FILE *f = NULL;
  // Open renamed file for reading
  ESP_LOGI(TAG, "Reading file");
  f = fopen("/spiffs/index.html", "r");
  if (f == NULL) {
    return 0;
  }
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
  FILE *f = NULL;
  uint8_t *buf = NULL;
  esp_err_t ret = ESP_OK;

  const int is_webapp_uploaded = check_webapp_uploaded();
  if (!is_webapp_uploaded) {
    ESP_LOGW(TAG, "Web application not yet uploaded");
    return httpd_resp_send(req, default_html_response,
                           strlen(default_html_response));
  }

  f = fopen("/spiffs/index.html", "rb");
  if (!f) {
    ESP_LOGE(TAG, "Failed to open index.html");
    ret = httpd_resp_send_500(req);
    goto cleanup;
  }

  buf = heap_caps_malloc(SCRATCH_BUFSIZE, MALLOC_CAP_DMA);
  if (!buf) {
    ESP_LOGE(TAG, "Failed to allocate buffer");
    ret = httpd_resp_send_500(req);
    goto cleanup;
  }

  size_t read_bytes;
  while ((read_bytes = fread(buf, 1, SCRATCH_BUFSIZE, f)) > 0) {
    if (httpd_resp_send_chunk(req, (char *)buf, read_bytes) != ESP_OK) {
      ret = ESP_FAIL;
      break;
    }
  }

  if (ferror(f)) {
    ESP_LOGE(TAG, "File read error");
    ret = ESP_FAIL;
  }

cleanup:
  if (f)
    fclose(f);
  if (buf)
    heap_caps_free(buf);

  if (ret == ESP_OK) {
    httpd_resp_send_chunk(req, NULL, 0);
  } else {
    httpd_resp_send_500(req);
  }

  return ret;
}
// esp_err_t get_handler(httpd_req_t *req) {
//   const int is_webapp_uploaded = check_webapp_uploaded();
//   if (!is_webapp_uploaded) {
//     ESP_LOGW(TAG, "Web application not yet uploaded");
//     return httpd_resp_send(req, default_html_response,
//                            strlen(default_html_response));
//   }
//
//   FILE *f = fopen("/spiffs/index.html", "rb"); // Открываем в бинарном режиме
//   if (!f) {
//     ESP_LOGE(TAG, "Failed to open index.html");
//     return httpd_resp_send_500(req);
//   }
//
//   // Выделяем выровненный буфер в куче
//   uint8_t *buf = heap_caps_malloc(SCRATCH_BUFSIZE, MALLOC_CAP_DMA);
//   if (!buf) {
//     fclose(f);
//     ESP_LOGE(TAG, "Failed to allocate buffer");
//     return httpd_resp_send_500(req);
//   }
//
//   esp_err_t ret = ESP_OK;
//   size_t read_bytes;
//
//   while ((read_bytes = fread(buf, 1, SCRATCH_BUFSIZE, f)) > 0) {
//     if (httpd_req_get_hdr_value_len(req, "Connection") < 0) {
//       ret = ESP_FAIL;
//       break;
//     }
//     if (httpd_resp_send_chunk(req, (char *)buf, read_bytes) != ESP_OK) {
//       ret = ESP_FAIL;
//       break;
//     }
//   }
//
//   // Проверяем ошибки чтения
//   if (ferror(f)) {
//     ESP_LOGE(TAG, "File read error");
//     ret = ESP_FAIL;
//   }
//
//   fclose(f);
//   heap_caps_free(buf);
//
//   // Завершаем ответ
//   if (ret == ESP_OK) {
//     httpd_resp_send_chunk(req, NULL, 0);
//   } else {
//     httpd_resp_send_500(req);
//   }
//
//   return ret;
// }

esp_err_t upload_handler(httpd_req_t *req) {
  char *buf = malloc(UPLOAD_BUFFER_SIZE); // Выделяем буфер в куче
  if (!buf) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  char filename[128] = {0};
  FILE *fd = NULL;
  const char *fail_resp = "{\"result\": false }";
  char boundary[70] = {0};
  bool file_complete = false;
  size_t total_written = 0;

  // Получаем boundary
  char content_type[128];
  if (httpd_req_get_hdr_value_str(req, "Content-Type", content_type,
                                  sizeof(content_type))) {
    free(buf);
    httpd_resp_send(req, fail_resp, strlen(fail_resp));
    return ESP_FAIL;
  }

  char *boundary_start = strstr(content_type, "boundary=");
  if (!boundary_start) {
    free(buf);
    httpd_resp_send(req, fail_resp, strlen(fail_resp));
    return ESP_FAIL;
  }
  strncpy(boundary, boundary_start + 9, sizeof(boundary) - 1);

  // Обработка данных
  int remaining = req->content_len;
  bool is_first_chunk = true;
  char *boundary_search_start = buf;
  size_t boundary_search_len = 0;

  while (remaining > 0 && !file_complete) {
    int received = httpd_req_recv(req, buf, MIN(remaining, UPLOAD_BUFFER_SIZE));
    if (received <= 0) {
      if (fd)
        fclose(fd);
      free(buf);
      httpd_resp_send(req, fail_resp, strlen(fail_resp));
      return ESP_FAIL;
    }

    if (is_first_chunk) {
      // Парсинг имени файла из первого чанка
      char *start = strstr(buf, "filename=\"");
      if (!start) {
        free(buf);
        httpd_resp_send(req, fail_resp, strlen(fail_resp));
        return ESP_FAIL;
      }
      start += 10;
      char *end = strchr(start, '"');
      if (!end) {
        free(buf);
        httpd_resp_send(req, fail_resp, strlen(fail_resp));
        return ESP_FAIL;
      }
      strncpy(filename, start, end - start);
      // Открытие файла
      char filepath[256];
      snprintf(filepath, sizeof(filepath), "/spiffs/%s", filename);
      fd = fopen(filepath, "wb");
      if (!fd) {
        free(buf);
        httpd_resp_send(req, fail_resp, strlen(fail_resp));
        return ESP_FAIL;
      }

      // Поиск начала данных
      char *data_start = strstr(buf, "\r\n\r\n");
      if (data_start) {
        data_start += 4;
        boundary_search_start = data_start;
        boundary_search_len = buf + received - data_start;
      }
      is_first_chunk = false;
    } else {
      boundary_search_start = buf;
      boundary_search_len = received;
    }

    // Поиск boundary
    char *boundary_pos = memmem(boundary_search_start, boundary_search_len,
                                boundary, strlen(boundary));
    if (boundary_pos) {
      size_t to_write =
          boundary_pos - boundary_search_start - 2; // Учитываем \r\n
      fwrite(boundary_search_start, 1, to_write, fd);
      total_written += to_write;
      file_complete = true;
    } else {
      // Записываем весь чанк (кроме последних N байт, где может начинаться
      // boundary)
      size_t safe_write = boundary_search_len - strlen(boundary) - 4;
      fwrite(boundary_search_start, 1, safe_write, fd);
      total_written += safe_write;

      // Сохраняем хвост для следующей итерации
      memmove(buf, boundary_search_start + safe_write,
              boundary_search_len - safe_write);
      boundary_search_len = boundary_search_len - safe_write;
    }

    remaining -= received;
  }

  if (fd)
    fclose(fd);
  free(buf);

  ESP_LOGI(TAG, "File %s uploaded, size: %d bytes", filename,
           (int)total_written);
  char *resp = "{\"result\": true}";
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

httpd_uri_t uri_favicon = {.uri = "/favicon.ico",
                           .method = HTTP_GET,
                           .handler = favicon_handler,
                           .user_ctx = NULL};

httpd_handle_t start_server() {
  /* Generate default configuration */
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  /* Empty handle to esp_http_server */
  httpd_handle_t server = NULL;

  /* Start the httpd server */
  if (httpd_start(&server, &config) == ESP_OK) {
    /* Register URI handlers */
    httpd_register_uri_handler(server, &uri_get);
    httpd_register_uri_handler(server, &uri_favicon);
    httpd_register_uri_handler(server, &uri_post_upload);
  }
  /* If server failed to start, handle will be NULL */
  return server;
}
