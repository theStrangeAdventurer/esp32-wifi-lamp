#include "esp_log.h"
#include "esp_system.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "http_server";

// HTTP-ответ для GET /
static const char *http_get_response =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Connection: close\r\n"
    "\r\n"
    "<!DOCTYPE HTML>"
    "<html>"
    "<head><title>ESP8266 Server</title></head>"
    "<body><h1>Hello from ESP8266!</h1></body>"
    "</html>";

void start_server() {
  struct sockaddr_in server_addr;
  int server_socket, client_socket;
  char rx_buffer[512]; // Увеличим буфер для чтения тела POST-запроса

  // Создаем сокет
  server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (server_socket < 0) {
    ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
    return;
  }
  ESP_LOGI(TAG, "Socket created");

  // Настраиваем параметры сервера
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(80); // Порт 80 (HTTP)

  // Привязываем сокет
  if (bind(server_socket, (struct sockaddr *)&server_addr,
           sizeof(server_addr)) < 0) {
    ESP_LOGE(TAG, "Socket bind failed: errno %d", errno);
    close(server_socket);
    return;
  }
  ESP_LOGI(TAG, "Socket bound, port 80");

  // Слушаем входящие соединения
  if (listen(server_socket, 5) < 0) {
    ESP_LOGE(TAG, "Socket listen failed: errno %d", errno);
    close(server_socket);
    return;
  }
  ESP_LOGI(TAG, "Socket listening");

  while (1) {
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // Принимаем входящее соединение
    client_socket = accept(server_socket, (struct sockaddr *)&client_addr,
                           &client_addr_len);
    if (client_socket < 0) {
      ESP_LOGE(TAG, "Socket accept failed: errno %d", errno);
      continue;
    }
    ESP_LOGI(TAG, "Client connected");

    // Читаем запрос клиента
    int len = recv(client_socket, rx_buffer, sizeof(rx_buffer) - 1, 0);
    if (len < 0) {
      ESP_LOGE(TAG, "Receive failed: errno %d", errno);
      close(client_socket);
      continue;
    }
    rx_buffer[len] = 0; // Добавляем нулевой терминатор
    ESP_LOGI(TAG, "Received: %s", rx_buffer);

    // Парсим первую строку запроса (метод и путь)
    char method[16], path[64];
    if (sscanf(rx_buffer, "%s %s", method, path) != 2) {
      ESP_LOGE(TAG, "Invalid HTTP request");
      close(client_socket);
      continue;
    }

    // Обрабатываем запросы
    if (strcmp(method, "GET") == 0 && strcmp(path, "/") == 0) {
      // GET / - отдаем HTML-страницу
      if (send(client_socket, http_get_response, strlen(http_get_response), 0) <
          0) {
        ESP_LOGE(TAG, "Send failed: errno %d", errno);
      } else {
        ESP_LOGI(TAG, "GET / response sent");
      }
    } else if (strcmp(method, "POST") == 0 &&
               strcmp(path, "/api/config") == 0) {
      // POST /api/config - ищем "value" в теле запроса
      char *body_start =
          strstr(rx_buffer, "\r\n\r\n"); // Ищем начало тела после заголовков
      char *value = NULL;
      if (body_start) {
        body_start += 4; // Пропускаем \r\n\r\n
        // Предполагаем, что тело в формате value=some_value
        char *value_start = strstr(body_start, "value=");
        if (value_start) {
          value_start += 6; // Пропускаем "value="
          char *value_end = strchr(value_start, '&');
          if (!value_end)
            value_end = value_start + strlen(value_start);
          int value_len = value_end - value_start;
          value = (char *)malloc(value_len + 1);
          if (value) {
            strncpy(value, value_start, value_len);
            value[value_len] = 0; // Нулевой терминатор
          }
        }
      }

      // Формируем JSON-ответ
      char json_response[1024];
      if (value) {
        snprintf(json_response, sizeof(json_response),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: application/json\r\n"
                 "Connection: close\r\n"
                 "\r\n"
                 "{\"result\": \"%s\"}",
                 value);
        free(value);
      } else {
        snprintf(json_response, sizeof(json_response),
                 "HTTP/1.1 400 Bad Request\r\n"
                 "Content-Type: application/json\r\n"
                 "Connection: close\r\n"
                 "\r\n"
                 "{\"error\": \"value not found\"}");
      }

      // Отправляем JSON-ответ
      if (send(client_socket, json_response, strlen(json_response), 0) < 0) {
        ESP_LOGE(TAG, "Send failed: errno %d", errno);
      } else {
        ESP_LOGI(TAG, "POST /api/config response sent");
      }
    } else {
      // Неизвестный запрос - 404
      const char *not_found_response = "HTTP/1.1 404 Not Found\r\n"
                                       "Content-Type: text/plain\r\n"
                                       "Connection: close\r\n"
                                       "\r\n"
                                       "404 Not Found";
      if (send(client_socket, not_found_response, strlen(not_found_response),
               0) < 0) {
        ESP_LOGE(TAG, "Send failed: errno %d", errno);
      } else {
        ESP_LOGI(TAG, "404 response sent");
      }
    }

    // Закрываем соединение с клиентом
    close(client_socket);
    ESP_LOGI(TAG, "Client disconnected");
  }

  // Закрываем серверный сокет (недостижимо из-за бесконечного цикла)
  close(server_socket);
}
