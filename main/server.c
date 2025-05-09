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
#define SCRATCH_BUFSIZE (8192)            // Буфер для чтения данных
#define MIN(a, b) ((a) < (b) ? (a) : (b)) // Добавляем макрос MIN
static const char *TAG = "http_server";

// Функция для определения MIME-типа по расширению файла
static const char *get_mime_type(const char *path) {
  if (strstr(path, ".html"))
    return "text/html";
  if (strstr(path, ".css"))
    return "text/css";
  if (strstr(path, ".js"))
    return "application/javascript";
  if (strstr(path, ".png"))
    return "image/png";
  if (strstr(path, ".jpg"))
    return "image/jpeg";
  return "application/octet-stream"; // По умолчанию
}

static void send_file(int client_socket, const char *path) {
  // char file_path[128];
  // FILE *fd = NULL;
  // struct stat file_stat;
  // snprintf(file_path, sizeof(file_path), "/spiffs%s", path);
  //
  // // Проверяем существование файла
  // if (stat(file_path, &file_stat) == -1) {
  //   ESP_LOGE(TAG, "Failed to stat file: %s", file_path);
  //   // Отправляем HTTP 404
  //   sprintf(header,
  //           "HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\n\r\nNot Found");
  //   write(client_socket, header, strlen(header));
  //   return;
  // }
  //
  // // Проверяем, что это обычный файл
  // if (!S_ISREG(file_stat.st_mode)) {
  //   ESP_LOGE(TAG, "Path is not a file: %s", file_path);
  //   // Отправляем HTTP 404
  //   sprintf(header,
  //           "HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\n\r\nNot Found");
  //   write(client_socket, header, strlen(header));
  //   return;
  // }
  //
  // // Открываем файл для чтения
  // file = fopen(file_path, "r");
  // if (!file) {
  //   ESP_LOGE(TAG, "Failed to open file: %s", file_path);
  //   // Отправляем HTTP 500
  //   sprintf(header, "HTTP/1.1 500 Internal Server Error\r\nContent-Length: "
  //                   "21\r\n\r\nInternal Server Error");
  //   write(client_socket, header, strlen(header));
  //   return;
  // }
  //
  // ESP_LOGI(TAG, "Sending file: %s (%ld bytes)", file_path,
  // file_stat.st_size);
  //
  // // Определяем тип содержимого по расширению файла
  // const char *content_type = "text/plain";
  // if (strstr(path, ".html")) {
  //   content_type = "text/html";
  // } else if (strstr(path, ".css")) {
  //   content_type = "text/css";
  // } else if (strstr(path, ".js")) {
  //   content_type = "application/javascript";
  // } else if (strstr(path, ".png")) {
  //   content_type = "image/png";
  // } else if (strstr(path, ".jpg") || strstr(path, ".jpeg")) {
  //   content_type = "image/jpeg";
  // } else if (strstr(path, ".ico")) {
  //   content_type = "image/x-icon";
  // } else if (strstr(path, ".json")) {
  //   content_type = "application/json";
  // }
  //
  // // Отправляем HTTP заголовок
  // sprintf(header,
  //         "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length:
  //         %ld\r\n\r\n", content_type, file_stat.st_size);
  // write(client_socket, header, strlen(header));
  //
  // // Отправляем содержимое файла
  // while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
  //   write(client_socket, buffer, bytes_read);
  // }
  //
  // // Закрываем файл
  // fclose(file);
  // ESP_LOGI(TAG, "File sent successfully: %s", file_path);
}

void read_file() {
  FILE *f = NULL;
  // Open renamed file for reading
  ESP_LOGI(TAG, "Reading file");
  f = fopen("/spiffs/foo.txt", "r");
  if (f == NULL) {
    ESP_LOGE(TAG, "Failed to open file for reading");
    return;
  }
  char line[64];
  fgets(line, sizeof(line), f);
  fclose(f);
  // strip newline
  char *pos = strchr(line, '\n');
  if (pos) {
    *pos = '\0';
  }
  ESP_LOGI(TAG, "Read from file: '%s'", line);
}
int check_webapp_uloaded() {
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
  const int is_webapp_uloaded = check_webapp_uloaded();
  if (!is_webapp_uloaded) {
    ESP_LOGW(TAG, "web application not yet uploaded");
    httpd_resp_send(req, default_html_response, strlen(default_html_response));
    return ESP_OK;
  }

  // Если файлы загружены, можно вернуть index.html
  FILE *f = fopen("/spiffs/index.html", "r");
  if (f == NULL) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  char buf[SCRATCH_BUFSIZE];
  size_t read_bytes;
  while ((read_bytes = fread(buf, 1, sizeof(buf), f)) > 0) {
    httpd_resp_send_chunk(req, buf, read_bytes);
  }
  fclose(f);
  httpd_resp_send_chunk(req, NULL, 0); // Завершение ответа
  return ESP_OK;
}

esp_err_t upload_handler(httpd_req_t *req) {
  char buf[SCRATCH_BUFSIZE];
  int remaining = req->content_len;
  char filename[128];
  bool is_first_chunk = true;
  FILE *fd = NULL;
  const char *fail_resp = "{\"result\": false }";
  char boundary[70] = {0};
  bool file_complete = false;

  // Получаем boundary из заголовка Content-Type
  char content_type[128];
  if (httpd_req_get_hdr_value_str(req, "Content-Type", content_type,
                                  sizeof(content_type))) {
    httpd_resp_send(req, fail_resp, strlen(fail_resp));
    return ESP_FAIL;
  }

  char *boundary_start = strstr(content_type, "boundary=");
  if (!boundary_start) {
    httpd_resp_send(req, fail_resp, strlen(fail_resp));
    return ESP_FAIL;
  }
  boundary_start += 9; // длина "boundary="
  strncpy(boundary, boundary_start, sizeof(boundary) - 1);
  boundary[sizeof(boundary) - 1] = '\0';

  while (remaining > 0 && !file_complete) {
    int received = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)));
    if (received <= 0) {
      if (fd)
        fclose(fd);
      httpd_resp_send(req, fail_resp, strlen(fail_resp));
      return ESP_FAIL;
    }

    if (is_first_chunk) {
      // Извлекаем имя файла
      char *start = strstr(buf, "filename=\"");
      if (!start) {
        httpd_resp_send(req, fail_resp, strlen(fail_resp));
        return ESP_FAIL;
      }
      start += strlen("filename=\"");
      char *end = strchr(start, '"');
      if (!end) {
        httpd_resp_send(req, fail_resp, strlen(fail_resp));
        return ESP_FAIL;
      }
      strncpy(filename, start, end - start);
      filename[end - start] = '\0';

      // Открываем файл для записи
      char filepath[256];
      snprintf(filepath, sizeof(filepath), "/spiffs/%s", filename);
      fd = fopen(filepath, "w");
      if (!fd) {
        httpd_resp_send(req, fail_resp, strlen(fail_resp));
        return ESP_FAIL;
      }

      // Находим начало данных файла
      char *data_start = strstr(buf, "\r\n\r\n");
      if (data_start) {
        data_start += 4;
        size_t data_len = buf + received - data_start;

        // Проверяем, не содержится ли boundary в первом чанке
        char *boundary_pos =
            memmem(data_start, data_len, boundary, strlen(boundary));
        if (boundary_pos) {
          // Записываем только данные до boundary
          fwrite(data_start, 1, boundary_pos - data_start - 2,
                 fd); // -2 для \r\n перед boundary
          file_complete = true;
        } else {
          fwrite(data_start, 1, data_len, fd);
        }
      }
      is_first_chunk = false;
    } else {
      // Проверяем наличие boundary в чанке
      char *boundary_pos = memmem(buf, received, boundary, strlen(boundary));
      if (boundary_pos) {
        // Записываем только данные до boundary
        fwrite(buf, 1, boundary_pos - buf - 2,
               fd); // -2 для \r\n перед boundary
        file_complete = true;
      } else {
        fwrite(buf, 1, received, fd);
      }
    }
    remaining -= received;
  }

  if (fd)
    fclose(fd);

  const char *success_resp = "{\"result\": true }";
  httpd_resp_send(req, success_resp, strlen(success_resp));
  return ESP_OK;
}

httpd_uri_t uri_get = {
    .uri = "/", .method = HTTP_GET, .handler = get_handler, .user_ctx = NULL};

httpd_uri_t uri_post_upload = {.uri = "/upload",
                               .method = HTTP_POST,
                               .handler = upload_handler,
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
    httpd_register_uri_handler(server, &uri_post_upload);
  }
  /* If server failed to start, handle will be NULL */
  return server;
}

// fs operations examples >>>>
// ESP_LOGI(TAG, "Read before:");
// read_file();
// ESP_LOGI(TAG, "Opening file");
// FILE *f = fopen("/spiffs/hello.txt", "w");
// if (f == NULL) {
//   ESP_LOGE(TAG, "Failed to open file for writing");
//   return;
// }
// fprintf(f, "Hello World2!\n");
// fclose(f);
// ESP_LOGI(TAG, "File written");
//
// // Check if destination file exists before renaming
// struct stat st;
// if (stat("/spiffs/foo.txt", &st) == 0) {
//   // Delete it if it exists
//   unlink("/spiffs/foo.txt");
// }
//
// // Rename original file
// ESP_LOGI(TAG, "Renaming file");
// if (rename("/spiffs/hello.txt", "/spiffs/foo.txt") != 0) {
//   ESP_LOGE(TAG, "Rename failed");
//   return;
// }
// read_file();
// esp_vfs_spiffs_unregister(NULL);
// ESP_LOGI(TAG, "SPIFFS unmounted");
