#include "src/hardware/cameraManager.h"

#include <Arduino.h>
#include "esp_camera.h"
#include "esp_http_server.h"

/* =====================
   CAMERA PIN CONFIG
   ESP32-CAM-MJPEG2SD / OV3660
   ===================== */

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

/* =====================
   INTERNAL STATE
   ===================== */

static httpd_handle_t stream_httpd = NULL;
static bool cameraInitialized = false;

/* =====================
   CAMERA INIT
   ===================== */

static bool initCamera() {
  if (cameraInitialized) return true;

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;

  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_SVGA;   // OV3660
    config.jpeg_quality = 10;
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  if (esp_camera_init(&config) != ESP_OK) {
    return false;
  }

  cameraInitialized = true;
  return true;
}

/* =====================
   MJPEG STREAM HANDLER
   ===================== */

static esp_err_t stream_handler(httpd_req_t *req) {
  if (!initCamera()) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Camera init failed");
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");

  while (true) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) continue;

    char header[64];
    snprintf(header, sizeof(header),
             "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
             fb->len);

    httpd_resp_send_chunk(req, header, strlen(header));
    httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
    httpd_resp_send_chunk(req, "\r\n", 2);

    esp_camera_fb_return(fb);
    vTaskDelay(pdMS_TO_TICKS(30));
  }

  return ESP_OK;
}

/* =====================
   PUBLIC API
   ===================== */

void cameraRegisterStream() {
  if (stream_httpd) {
    Serial.println("[CAM] Stream server already running");
    return;
  }

  Serial.println("[CAM] Starting stream server on port 81");

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 81;
  config.ctrl_port = 32768;

  httpd_uri_t stream_uri = {
    .uri = "/stream",
    .method = HTTP_GET,
    .handler = stream_handler,
    .user_ctx = NULL
  };

  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    Serial.println("[CAM] Stream server started");
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  } else {
    Serial.println("[CAM] FAILED to start stream server");
  }
}

