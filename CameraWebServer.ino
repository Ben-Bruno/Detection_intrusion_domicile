
// ================================================================
// SecureCam IoT — Freenove ESP32-S3 Eye
// v5.0 — LVGL tactile, routes plates, couleurs corrigees, rapide
// ================================================================
#include "globals.h"
#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <WebServer.h>
#include "FS.h"
#include "SD_MMC.h"

// LVGL
#include "lvgl.h"
#include "display.h"   // fichiers TP2 display.h / display.cpp

// ================================================================
// BUZZER — calque sound_ui TP2
// ================================================================
#define PIN_BUZZER  12
#define BUZZ_CH      3
#define NOTE_D4    294
#define NOTE_D5    523
#define NOTE_D6   1047
#define NOTE_D7   2093

void buzzer_init() {
  ledcSetup(BUZZ_CH, 2000, 8);
  ledcAttachPin(PIN_BUZZER, BUZZ_CH);
}
// Equivalent de sound_set_buzzer du TP
void sound_set_buzzer(int freq, int ms) {
  ledcWriteTone(BUZZ_CH, freq);
  delay(ms);
  ledcWriteTone(BUZZ_CH, 0);
}
void melody_boot()  { sound_set_buzzer(NOTE_D4,80); delay(40);
                      sound_set_buzzer(NOTE_D5,80); delay(40);
                      sound_set_buzzer(NOTE_D6,150); }
void melody_click() { sound_set_buzzer(NOTE_D5, 60); }
void melody_alarm() { for(int i=0;i<3;i++){
                        sound_set_buzzer(NOTE_D7,180); delay(80); } }
void melody_ok()    { sound_set_buzzer(NOTE_D5,150); delay(60);
                      sound_set_buzzer(NOTE_D6,250); }

// ================================================================
// CAMERA
// ================================================================
#define CAMERA_MODEL_ESP32S3_EYE
#include "camera_pins.h"

// ================================================================
// CONFIG
// ================================================================
const char* ssid      = "Bbox-1C5097AA";
const char* password  = "3yWLaXszVU97Fjw55d";
const char* BOT_TOKEN = "8754252712:AAGxelbzzvPYd4t_rSX4ioCgvyu9CJudsqg";
const char* CHAT_ID   = "6093388674";

// ================================================================
// ETAT GLOBAL
// ================================================================
//volatile bool intrusion_detectee = false;  // partage avec app_httpd
#define PIR_PIN 13

enum AppMode { M_MENU, M_PHOTO, M_SURV };
volatile AppMode appMode    = M_MENU;
volatile bool    pirEnabled = false;

// ================================================================
// LVGL + Display
// ================================================================
Display screen;          // objet display.h du TP

// Objets LVGL globaux (evitent les re-creations)
static lv_obj_t* btn_photo  = nullptr;
static lv_obj_t* btn_surv   = nullptr;
static lv_obj_t* btn_retour = nullptr;
static lv_obj_t* img_obj    = nullptr;
static lv_obj_t* lbl_status = nullptr;

// Buffer image LVGL pour la photo camera
static uint8_t*  cam_img_buf  = nullptr;
static size_t    cam_img_len  = 0;
static bool      new_photo_ready = false;

// ================================================================
// SERVEUR WEB
// ================================================================
WebServer srv(81);
void startCameraServer();

// ================================================================
// PROTOTYPES
// ================================================================
void ui_show_menu();
void ui_show_photo();
void ui_show_surv();
void capture_and_send(bool sendTg, bool isIntrusion);
bool telegram_send(const uint8_t* buf, size_t len, bool isIntrusion);
String timeStr();
void setupRoutes();

// ================================================================
// CALLBACKS LVGL
// ================================================================
static void cb_btn_photo(lv_event_t* e) {
  if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  melody_click();
  appMode    = M_PHOTO;
  pirEnabled = false;
  ui_show_photo();
  // Lance une capture immediate
  capture_and_send(false, false);
}

static void cb_btn_surv(lv_event_t* e) {
  if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  melody_click();
  sound_set_buzzer(NOTE_D4,200); delay(100); sound_set_buzzer(NOTE_D4,200);
  appMode    = M_SURV;
  pirEnabled = true;
  ui_show_surv();
}

static void cb_btn_retour(lv_event_t* e) {
  if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  melody_click();
  appMode    = M_MENU;
  pirEnabled = false;
  ui_show_menu();
}

// ================================================================
// UI LVGL — MENU (2 boutons)
// ================================================================
void ui_show_menu() {
  lv_obj_clean(lv_scr_act());
  btn_photo  = nullptr;
  btn_surv   = nullptr;
  btn_retour = nullptr;

  // Fond sombre
  lv_obj_set_style_bg_color(lv_scr_act(),
    lv_color_hex(0x0d1117), LV_PART_MAIN);

  // Titre
  lv_obj_t* title = lv_label_create(lv_scr_act());
  lv_label_set_text(title, "SecureCam IoT");
  lv_obj_set_style_text_color(title, lv_color_hex(0x58a6ff), LV_PART_MAIN);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

  // Sous-titre IP
  lv_obj_t* ip_lbl = lv_label_create(lv_scr_act());
  lv_label_set_text(ip_lbl, WiFi.localIP().toString().c_str());
  lv_obj_set_style_text_color(ip_lbl, lv_color_hex(0x8b949e), LV_PART_MAIN);
  lv_obj_set_style_text_font(ip_lbl, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(ip_lbl, LV_ALIGN_TOP_MID, 0, 38);

  // Bouton MODE PHOTO (vert)
  btn_photo = lv_btn_create(lv_scr_act());
  lv_obj_set_size(btn_photo, 200, 70);
  lv_obj_align(btn_photo, LV_ALIGN_CENTER, 0, -55);
  lv_obj_set_style_bg_color(btn_photo, lv_color_hex(0x1a4a1a), LV_PART_MAIN);
  lv_obj_set_style_bg_color(btn_photo, lv_color_hex(0x2d7a2d),
                             LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_border_color(btn_photo,
    lv_color_hex(0x3fb950), LV_PART_MAIN);
  lv_obj_set_style_border_width(btn_photo, 2, LV_PART_MAIN);
  lv_obj_set_style_radius(btn_photo, 12, LV_PART_MAIN);
  lv_obj_t* lbl_p = lv_label_create(btn_photo);
  lv_label_set_text(lbl_p, "MODE PHOTO");
  lv_obj_set_style_text_color(lbl_p, lv_color_hex(0x3fb950), LV_PART_MAIN);
  lv_obj_center(lbl_p);
  lv_obj_add_event_cb(btn_photo, cb_btn_photo, LV_EVENT_CLICKED, NULL);

  // Bouton MODE SURVEILLANCE (rouge)
  btn_surv = lv_btn_create(lv_scr_act());
  lv_obj_set_size(btn_surv, 200, 70);
  lv_obj_align(btn_surv, LV_ALIGN_CENTER, 0, 45);
  lv_obj_set_style_bg_color(btn_surv, lv_color_hex(0x4a1a1a), LV_PART_MAIN);
  lv_obj_set_style_bg_color(btn_surv, lv_color_hex(0x8b2020),
                             LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_border_color(btn_surv,
    lv_color_hex(0xf85149), LV_PART_MAIN);
  lv_obj_set_style_border_width(btn_surv, 2, LV_PART_MAIN);
  lv_obj_set_style_radius(btn_surv, 12, LV_PART_MAIN);
  lv_obj_t* lbl_s = lv_label_create(btn_surv);
  lv_label_set_text(lbl_s, "SURVEILLANCE");
  lv_obj_set_style_text_color(lbl_s, lv_color_hex(0xf85149), LV_PART_MAIN);
  lv_obj_center(lbl_s);
  lv_obj_add_event_cb(btn_surv, cb_btn_surv, LV_EVENT_CLICKED, NULL);

  // Web hint
  lv_obj_t* hint = lv_label_create(lv_scr_act());
  lv_label_set_text(hint, "Dashboard: port 81");
  lv_obj_set_style_text_color(hint, lv_color_hex(0x444c56), LV_PART_MAIN);
  lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -8);
}

// ================================================================
// UI LVGL — MODE PHOTO
// ================================================================
void ui_show_photo() {
  lv_obj_clean(lv_scr_act());
  btn_retour = nullptr;
  img_obj    = nullptr;

  lv_obj_set_style_bg_color(lv_scr_act(),
    lv_color_hex(0x0d1117), LV_PART_MAIN);

  // Header
  lv_obj_t* hdr = lv_label_create(lv_scr_act());
  lv_label_set_text(hdr, "MODE PHOTO");
  lv_obj_set_style_text_color(hdr, lv_color_hex(0x3fb950), LV_PART_MAIN);
  lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 10);

  // Zone image (placeholder)
  lv_obj_t* frame = lv_obj_create(lv_scr_act());
  lv_obj_set_size(frame, 220, 165);
  lv_obj_align(frame, LV_ALIGN_CENTER, 0, -20);
  lv_obj_set_style_bg_color(frame, lv_color_hex(0x161b22), LV_PART_MAIN);
  lv_obj_set_style_border_color(frame,
    lv_color_hex(0x3fb950), LV_PART_MAIN);
  lv_obj_set_style_border_width(frame, 2, LV_PART_MAIN);
  lv_obj_set_style_radius(frame, 8, LV_PART_MAIN);

  lbl_status = lv_label_create(frame);
  lv_label_set_text(lbl_status, "Capture en cours...");
  lv_obj_set_style_text_color(lbl_status,
    lv_color_hex(0x444c56), LV_PART_MAIN);
  lv_obj_center(lbl_status);

  // Bouton retour
  btn_retour = lv_btn_create(lv_scr_act());
  lv_obj_set_size(btn_retour, 120, 38);
  lv_obj_align(btn_retour, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_obj_set_style_bg_color(btn_retour,
    lv_color_hex(0x21262d), LV_PART_MAIN);
  lv_obj_set_style_border_color(btn_retour,
    lv_color_hex(0x30363d), LV_PART_MAIN);
  lv_obj_set_style_border_width(btn_retour, 1, LV_PART_MAIN);
  lv_obj_set_style_radius(btn_retour, 8, LV_PART_MAIN);
  lv_obj_t* lr = lv_label_create(btn_retour);
  lv_label_set_text(lr, "< Retour");
  lv_obj_set_style_text_color(lr, lv_color_hex(0xc9d1d9), LV_PART_MAIN);
  lv_obj_center(lr);
  lv_obj_add_event_cb(btn_retour, cb_btn_retour, LV_EVENT_CLICKED, NULL);
}

// Appele depuis capture_and_send quand une nouvelle image est prete
void ui_update_photo_image() {
  if(!lbl_status) return;
  // On met a jour le label de statut
  lv_label_set_text(lbl_status, timeStr().c_str());
  lv_obj_set_style_text_color(lbl_status,
    lv_color_hex(0x3fb950), LV_PART_MAIN);
  // Note: affichage JPEG sur LVGL necessite lv_img avec source en RAM
  // On utilise ici la zone noire comme indicateur visuel simplifie
  // L'image complete est visible sur le dashboard web
}

// ================================================================
// UI LVGL — MODE SURVEILLANCE
// ================================================================
void ui_show_surv() {
  lv_obj_clean(lv_scr_act());
  btn_retour = nullptr;

  lv_obj_set_style_bg_color(lv_scr_act(),
    lv_color_hex(0x0d1117), LV_PART_MAIN);

  // Header rouge
  lv_obj_t* hdr = lv_label_create(lv_scr_act());
  lv_label_set_text(hdr, "SURVEILLANCE PIR");
  lv_obj_set_style_text_color(hdr, lv_color_hex(0xf85149), LV_PART_MAIN);
  lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 10);

  // Indicateur actif
  lv_obj_t* dot = lv_obj_create(lv_scr_act());
  lv_obj_set_size(dot, 14, 14);
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_bg_color(dot, lv_color_hex(0x3fb950), LV_PART_MAIN);
  lv_obj_set_style_border_width(dot, 0, LV_PART_MAIN);
  lv_obj_align(dot, LV_ALIGN_TOP_LEFT, 12, 38);

  lv_obj_t* act_lbl = lv_label_create(lv_scr_act());
  lv_label_set_text(act_lbl, "PIR actif");
  lv_obj_set_style_text_color(act_lbl,
    lv_color_hex(0x3fb950), LV_PART_MAIN);
  lv_obj_align(act_lbl, LV_ALIGN_TOP_LEFT, 32, 40);

  lv_obj_t* info = lv_label_create(lv_scr_act());
  lv_label_set_text(info,
    "Zone securisee.\n"
    "Intrusion :\n"
    "  - Alarme buzzer\n"
    "  - Photo -> Telegram\n"
    "Cooldown: 15s");
  lv_obj_set_style_text_color(info,
    lv_color_hex(0xd29922), LV_PART_MAIN);
  lv_obj_set_style_text_font(info, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(info, LV_ALIGN_CENTER, 0, -10);

  // Bouton retour
  btn_retour = lv_btn_create(lv_scr_act());
  lv_obj_set_size(btn_retour, 120, 38);
  lv_obj_align(btn_retour, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_obj_set_style_bg_color(btn_retour,
    lv_color_hex(0x21262d), LV_PART_MAIN);
  lv_obj_set_style_border_color(btn_retour,
    lv_color_hex(0x30363d), LV_PART_MAIN);
  lv_obj_set_style_border_width(btn_retour, 1, LV_PART_MAIN);
  lv_obj_set_style_radius(btn_retour, 8, LV_PART_MAIN);
  lv_obj_t* lr = lv_label_create(btn_retour);
  lv_label_set_text(lr, "< Retour");
  lv_obj_set_style_text_color(lr, lv_color_hex(0xc9d1d9), LV_PART_MAIN);
  lv_obj_center(lr);
  lv_obj_add_event_cb(btn_retour, cb_btn_retour, LV_EVENT_CLICKED, NULL);
}

// ================================================================
// UI LVGL — ALERTE INTRUSION
// ================================================================
void ui_show_alert() {
  lv_obj_clean(lv_scr_act());

  lv_obj_set_style_bg_color(lv_scr_act(),
    lv_color_hex(0xb71c1c), LV_PART_MAIN);

  lv_obj_t* t1 = lv_label_create(lv_scr_act());
  lv_label_set_text(t1, "!! ALERTE !!");
  lv_obj_set_style_text_color(t1, lv_color_hex(0xffffff), LV_PART_MAIN);
  lv_obj_set_style_text_font(t1, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_align(t1, LV_ALIGN_CENTER, 0, -40);

  lv_obj_t* t2 = lv_label_create(lv_scr_act());
  lv_label_set_text(t2, "INTRUSION DETECTEE\nPhoto -> Telegram");
  lv_obj_set_style_text_color(t2, lv_color_hex(0xffd54f), LV_PART_MAIN);
  lv_obj_set_style_text_font(t2, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(t2, LV_ALIGN_CENTER, 0, 20);

  lv_obj_t* t3 = lv_label_create(lv_scr_act());
  lv_label_set_text(t3, timeStr().c_str());
  lv_obj_set_style_text_color(t3, lv_color_hex(0xffcdd2), LV_PART_MAIN);
  lv_obj_set_style_text_font(t3, &lv_font_montserrat_12, LV_PART_MAIN);
  lv_obj_align(t3, LV_ALIGN_BOTTOM_MID, 0, -12);
}

// ================================================================
// SETUP
// ================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("=== SecureCam IoT v5.0 ===");

  buzzer_init();
  pinMode(PIR_PIN, INPUT);

  // ── LVGL + ecran ─────────────────────────────────────────────
  screen.init();   // initialise TFT + touch via display.h du TP

  lv_init();
  // Le display.h du TP enregistre deja les drivers LVGL
  // Si ce n'est pas le cas, il faut appeler lv_disp_drv_register ici

  // ── SD ───────────────────────────────────────────────────────
  SD_MMC.setPins(39, 38, 40);
  if (!SD_MMC.begin("/sdcard", true))
    Serial.println("SD absente");
  else
    Serial.printf("SD: %llu MB\n",
                  SD_MMC.cardSize() / (1024*1024));

  // ── Camera QVGA ──────────────────────────────────────────────
  camera_config_t cfg;
  cfg.ledc_channel  = LEDC_CHANNEL_0;
  cfg.ledc_timer    = LEDC_TIMER_0;
  cfg.pin_d0=Y2_GPIO_NUM; cfg.pin_d1=Y3_GPIO_NUM;
  cfg.pin_d2=Y4_GPIO_NUM; cfg.pin_d3=Y5_GPIO_NUM;
  cfg.pin_d4=Y6_GPIO_NUM; cfg.pin_d5=Y7_GPIO_NUM;
  cfg.pin_d6=Y8_GPIO_NUM; cfg.pin_d7=Y9_GPIO_NUM;
  cfg.pin_xclk=XCLK_GPIO_NUM; cfg.pin_pclk=PCLK_GPIO_NUM;
  cfg.pin_vsync=VSYNC_GPIO_NUM; cfg.pin_href=HREF_GPIO_NUM;
  cfg.pin_sccb_sda=SIOD_GPIO_NUM; cfg.pin_sccb_scl=SIOC_GPIO_NUM;
  cfg.pin_pwdn=PWDN_GPIO_NUM; cfg.pin_reset=RESET_GPIO_NUM;
  cfg.xclk_freq_hz = 20000000;
  cfg.pixel_format  = PIXFORMAT_JPEG;
  cfg.grab_mode     = CAMERA_GRAB_LATEST;
  cfg.frame_size    = FRAMESIZE_QVGA;
  cfg.jpeg_quality  = psramFound() ? 10 : 12;
  cfg.fb_count      = psramFound() ? 2 : 1;
  cfg.fb_location   = psramFound()
                        ? CAMERA_FB_IN_PSRAM
                        : CAMERA_FB_IN_DRAM;

  if (esp_camera_init(&cfg) != ESP_OK) {
    Serial.println("Camera ERREUR");
    return;
  }
  sensor_t* s = esp_camera_sensor_get();
  s->set_vflip(s, 1);
  s->set_brightness(s, 1);
  s->set_saturation(s, 0);

  // ── WiFi ─────────────────────────────────────────────────────
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(400);
  Serial.printf("WiFi: %s\n", WiFi.localIP().toString().c_str());

  // ── NTP ──────────────────────────────────────────────────────
  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org");
  delay(1200);

  // ── Serveurs ─────────────────────────────────────────────────
  startCameraServer();    // Port 80 — stream + face
  setupRoutes();
  srv.begin();            // Port 81 — dashboard

  Serial.printf("Stream : http://%s\n",
                WiFi.localIP().toString().c_str());
  Serial.printf("Dashboard : http://%s:81\n",
                WiFi.localIP().toString().c_str());

  melody_boot();
  delay(300);
  ui_show_menu();
}

// ================================================================
// LOOP — optimise : pas de delay() bloquant
// ================================================================
void loop() {
  // LVGL heartbeat (priorite haute)
  screen.routine();          // lv_task_handler() encapsule dans display.h

  // Serveur web
  srv.handleClient();

  // Surveillance PIR
  if (appMode == M_SURV && pirEnabled) {
    if (digitalRead(PIR_PIN) == HIGH) {
      pirEnabled = false;
      Serial.println("PIR: mouvement!");
      melody_alarm();
      ui_show_alert();
      lv_task_handler();     // force refresh ecran avant capture longue
      capture_and_send(true, true);
      delay(15000);
      pirEnabled = true;
      ui_show_surv();
    }
  }

  // Flag intrusion depuis reconnaissance faciale (app_httpd.cpp)
//if (intrusion_detectee) {
//  intrusion_detectee = false;
//  pirEnabled = false;
//  Serial.println("Intrusion faciale!");
//  melody_alarm();
//  ui_show_alert();
//  lv_task_handler();
//  capture_and_send(true, true);
//  if (appMode == M_SURV) {
//    delay(15000);
//    pirEnabled = true;
//    ui_show_surv();
//  }
//  }
}

// ================================================================
// CAPTURE + SD + TELEGRAM
// ================================================================
void capture_and_send(bool sendTg, bool isIntrusion) {
  Serial.println("Capture...");

  // Vider frame obsolete
  { camera_fb_t* fl = esp_camera_fb_get();
    if (fl) esp_camera_fb_return(fl); }
  delay(40);

  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) { Serial.println("Echec capture"); return; }

  // Mise a jour UI photo
  if (appMode == M_PHOTO) ui_update_photo_image();

  // Sauvegarde SD
  char path[48];
  struct tm ti;
  if (getLocalTime(&ti))
    strftime(path, sizeof(path),
             "/img_%Y%m%d_%H%M%S.jpg", &ti);
  else
    snprintf(path, sizeof(path),
             "/img_%lu.jpg", millis());

  // Ecriture par blocs de 4 Ko — plus rapide que write() octet par octet
  File f = SD_MMC.open(path, FILE_WRITE);
  if (f) {
    const size_t BLOCK = 4096;
    uint8_t* p = fb->buf;
    size_t   r = fb->len;
    while (r > 0) {
      size_t n = (r > BLOCK) ? BLOCK : r;
      f.write(p, n); p += n; r -= n;
    }
    f.close();
    Serial.printf("SD: %s\n", path);
  }
  // Copie last_photo.jpg
  File f2 = SD_MMC.open("/last_photo.jpg", FILE_WRITE);
  if (f2) { f2.write(fb->buf, fb->len); f2.close(); }

  // Telegram
  if (sendTg) {
    Serial.println("Telegram...");
    bool ok = telegram_send(fb->buf, fb->len, isIntrusion);
    Serial.println(ok ? "Telegram OK" : "Telegram echec");
  }

  esp_camera_fb_return(fb);
}

// ================================================================
// TELEGRAM — client sur heap, chunked 4 Ko
// ================================================================
bool telegram_send(const uint8_t* buf, size_t len, bool isIntrusion) {
  WiFiClientSecure* cl = new WiFiClientSecure();
  cl->setInsecure();
  cl->setTimeout(12);

  if (!cl->connect("api.telegram.org", 443)) {
    delete cl; return false;
  }

  char cap[200];
  if (isIntrusion)
    snprintf(cap, sizeof(cap),
      "ALERTE INTRUSION\n"
      "Mouvement detecte\n"
      "%s", timeStr().c_str());
  else
    snprintf(cap, sizeof(cap),
      "Capture manuelle\n%s", timeStr().c_str());

  const char* bnd = "ESP32S3Bnd";
  char hdr[600];
  int  hlen = snprintf(hdr, sizeof(hdr),
    "--%s\r\nContent-Disposition: form-data; name=\"chat_id\"\r\n\r\n%s"
    "\r\n--%s\r\nContent-Disposition: form-data; name=\"caption\"\r\n\r\n%s"
    "\r\n--%s\r\nContent-Disposition: form-data; name=\"photo\";"
    " filename=\"p.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n",
    bnd, CHAT_ID, bnd, cap, bnd);
  char ftr[64];
  int  flen = snprintf(ftr, sizeof(ftr), "\r\n--%s--\r\n", bnd);
  long total = hlen + (long)len + flen;

  cl->printf("POST /bot%s/sendPhoto HTTP/1.1\r\n", BOT_TOKEN);
  cl->printf("Host: api.telegram.org\r\n");
  cl->printf("Content-Type: multipart/form-data; boundary=%s\r\n", bnd);
  cl->printf("Content-Length: %ld\r\n\r\n", total);
  cl->write((const uint8_t*)hdr, hlen);

  const uint8_t* p = buf;
  size_t r = len;
  while (r > 0) {
    size_t n = (r > 4096) ? 4096 : r;
    cl->write(p, n); p += n; r -= n;
  }
  cl->write((const uint8_t*)ftr, flen);

  bool ok = false;
  unsigned long t0 = millis();
  while (cl->connected() && millis()-t0 < 9000)
    if (cl->available()) {
      String ln = cl->readStringUntil('\n');
      if (ln.indexOf("\"ok\":true") >= 0) { ok = true; break; }
    }
  cl->stop();
  delete cl;
  return ok;
}

// ================================================================
// ROUTES WEB PORT 81 — URI plates (pas de sous-chemins)
// ================================================================
// CSS en PROGMEM — evite stack overflow httpd
static const char CSS[] PROGMEM = R"CSS(
:root{--bg:#0d1117;--s:#161b22;--s2:#21262d;--bd:#30363d;
--ac:#58a6ff;--gr:#3fb950;--rd:#f85149;--yw:#d29922;
--tx:#c9d1d9;--mt:#8b949e}
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:system-ui,sans-serif;background:var(--bg);
color:var(--tx);min-height:100vh}
header{background:var(--s);border-bottom:1px solid var(--bd);
padding:12px 20px;display:flex;align-items:center;gap:12px;
position:sticky;top:0;z-index:10}
.logo{font-size:22px;font-weight:700;color:#fff}
.sub{font-size:11px;color:var(--mt);margin-top:2px}
.badge{margin-left:auto;background:var(--s2);border:1px solid var(--bd);
padding:3px 10px;border-radius:20px;font-size:11px}
.main{max-width:900px;margin:0 auto;padding:16px}
.row{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-bottom:14px}
@media(max-width:520px){.row{grid-template-columns:1fr}}
.card{background:var(--s);border:1px solid var(--bd);
border-radius:10px;padding:16px}
.card h2{font-size:13px;font-weight:600;color:#fff;margin-bottom:8px}
.card p{font-size:12px;color:var(--mt);margin-bottom:12px;line-height:1.5}
.btn{display:block;width:100%;padding:9px;border:none;
border-radius:7px;font-size:13px;font-weight:600;
cursor:pointer;text-align:center}
.btn:active{opacity:.75}
.gr{background:var(--gr);color:#000}
.rd{background:var(--rd);color:#fff}
.bl{background:var(--ac);color:#000}
.dk{background:var(--s2);color:var(--tx);border:1px solid var(--bd)}
.gal{display:grid;grid-template-columns:repeat(auto-fill,minmax(160px,1fr));gap:8px}
.gi{background:#000;border-radius:6px;overflow:hidden;
border:1px solid var(--bd);aspect-ratio:4/3;position:relative}
.gi img{width:100%;height:100%;object-fit:cover}
.gi .ts{position:absolute;bottom:0;left:0;right:0;
background:rgba(0,0,0,.65);color:#fff;font-size:10px;padding:2px 5px}
.no-img{color:var(--mt);font-size:12px;padding:10px}
.stat-row{display:grid;grid-template-columns:repeat(3,1fr);
gap:10px;margin-bottom:14px}
.stat{background:var(--s);border:1px solid var(--bd);
border-radius:8px;padding:12px}
.sv{font-size:18px;font-weight:700;color:#fff}
.sl{font-size:10px;color:var(--mt);margin-top:2px}
@keyframes blink{0%,100%{opacity:1}50%{opacity:.2}}
.blink{animation:blink 1.2s infinite}
footer{text-align:center;padding:14px;color:var(--mt);font-size:11px}
)CSS";

// Galerie 5 derniers fichiers
static String buildGallery() {
  String files[20];
  int cnt = 0;
  File root = SD_MMC.open("/");
  if (root) {
    File f = root.openNextFile();
    while (f && cnt < 20) {
      String nm = String(f.name());
      if (!f.isDirectory() &&
          nm.startsWith("/img_") &&
          nm.endsWith(".jpg"))
        files[cnt++] = nm;
      f = root.openNextFile();
    }
  }
  // Tri decroissant (ISO sort)
  for (int i=0;i<cnt-1;i++)
    for (int j=i+1;j<cnt;j++)
      if (files[j]>files[i])
        { String t=files[i]; files[i]=files[j]; files[j]=t; }

  int show = cnt < 5 ? cnt : 5;
  if (show == 0)
    return "<div class='no-img'>Aucune photo disponible</div>";

  String h;
  h.reserve(show * 120);
  for (int i=0;i<show;i++) {
    // /img_20260401_223200.jpg -> "01/04 22:32"
    String ts = (files[i].length() >= 20)
      ? files[i].substring(9,11) + "/" + files[i].substring(7,9)
        + " " + files[i].substring(12,14) + ":" + files[i].substring(14,16)
      : files[i];
    h += "<div class='gi'>"
         "<img src='/photo?f=" + files[i] + "' loading='lazy'>"
         "<div class='ts'>" + ts + "</div>"
         "</div>";
  }
  return h;
}

void setupRoutes() {

  // ── PAGE PRINCIPALE (GET /) ───────────────────────────────
  srv.on("/", HTTP_GET, []() {
    String ip   = WiFi.localIP().toString();
    const char* modeStr = (appMode==M_SURV) ? "Surveillance" : "Actif";
    bool surv   = (appMode == M_SURV);

    String out;
    out.reserve(2800);

    out  = "<!DOCTYPE html><html lang='fr'><head>"
           "<meta charset='UTF-8'>"
           "<meta name='viewport' content='width=device-width,initial-scale=1'>"
           "<title>SecureCam</title><style>";
    out += FPSTR(CSS);
    out += "</style></head><body><header>"
           "<div><div class='logo'>SecureCam IoT</div>"
           "<div class='sub'>ESP32-S3 &bull; ";
    out += ip;
    out += "</div></div>"
           "<div class='badge'>";
    out += modeStr;
    out += "</div></header><div class='main'>"

           // Stats
           "<div class='stat-row'>"
           "<div class='stat'><div class='sv'>";
    out += surv
      ? "<span class='blink' style='color:#f85149'>ON</span>"
      : "OFF";
    out += "</div><div class='sl'>PIR</div></div>"
           "<div class='stat'><div class='sv' style='font-size:13px'>";
    out += timeStr();
    out += "</div><div class='sl'>Heure</div></div>"
           "<div class='stat'><div class='sv' style='font-size:12px'>"
           "<a href='http://";
    out += ip;
    out += "' style='color:#58a6ff'>Flux live</a>"
           "</div><div class='sl'>Port 80</div></div>"
           "</div>"

           // Boutons modes — URI plates
           "<div class='row'>"
           "<div class='card'><h2>Mode Photo</h2>"
           "<p>Active la camera, capture et sauvegarde sur SD.</p>"
           "<form action='/photo' method='POST'>"
           "<button class='btn gr'>Activer Mode Photo</button></form></div>"

           "<div class='card'><h2>Surveillance PIR</h2>"
           "<p>Active le PIR. Intrus = alarme + Telegram.</p>"
           "<form action='/surv' method='POST'>"
           "<button class='btn rd'>Activer Surveillance</button></form></div>"

           "<div class='card'><h2>Capture manuelle</h2>"
           "<p>Photo immediate envoyee sur Telegram.</p>"
           "<form action='/cap' method='POST'>"
           "<button class='btn bl'>Capturer</button></form></div>"

           "<div class='card'><h2>Menu TFT</h2>"
           "<p>Revenir au menu sur l'ecran de la carte.</p>"
           "<form action='/menu' method='POST'>"
           "<button class='btn dk'>Retour menu</button></form></div>"
           "</div>"

           // Galerie
           "<div class='card'><h2>5 dernieres captures</h2>"
           "<div class='gal'>";
    out += buildGallery();
    out += "</div></div>"
           "</div><footer>SecureCam IoT 2026 ESP32-S3</footer>"
           "</body></html>";

    srv.send(200, "text/html", out);
  });

  // ── ACTIONS — URI PLATES (pas de slash interne) ───────────
  srv.on("/photo", HTTP_POST, []() {
    appMode = M_PHOTO; pirEnabled = false;
    ui_show_photo();
    melody_click();
    capture_and_send(false, false);
    srv.sendHeader("Location", "/"); srv.send(303);
  });

  srv.on("/surv", HTTP_POST, []() {
    appMode = M_SURV; pirEnabled = true;
    ui_show_surv();
    sound_set_buzzer(NOTE_D4,180); delay(80);
    sound_set_buzzer(NOTE_D4,180);
    srv.sendHeader("Location", "/"); srv.send(303);
  });

  srv.on("/menu", HTTP_POST, []() {
    appMode = M_MENU; pirEnabled = false;
    ui_show_menu();
    melody_click();
    srv.sendHeader("Location", "/"); srv.send(303);
  });

  srv.on("/cap", HTTP_POST, []() {
    capture_and_send(true, false);
    srv.sendHeader("Location", "/"); srv.send(303);
  });

  // ── PHOTO PAR NOM ─────────────────────────────────────────
  srv.on("/photo", HTTP_GET, []() {
    // GET /photo?f=/img_xxx.jpg
    if (!srv.hasArg("f")) {
      srv.send(400, "text/plain", "?f= requis"); return;
    }
    String p = srv.arg("f");
    if (!p.startsWith("/img_") || !p.endsWith(".jpg")) {
      srv.send(403, "text/plain", "Interdit"); return;
    }
    if (!SD_MMC.exists(p.c_str())) {
      srv.send(404, "text/plain", "Introuvable"); return;
    }
    File f = SD_MMC.open(p.c_str(), FILE_READ);
    srv.streamFile(f, "image/jpeg");
    f.close();
  });

  // ── DERNIERE PHOTO ────────────────────────────────────────
  srv.on("/last", HTTP_GET, []() {
    if (!SD_MMC.exists("/last_photo.jpg")) {
      srv.send(404, "text/plain", "Pas de photo"); return;
    }
    File f = SD_MMC.open("/last_photo.jpg", FILE_READ);
    srv.streamFile(f, "image/jpeg");
    f.close();
  });
}

// ================================================================
// UTILITAIRE
// ================================================================
String timeStr() {
  struct tm t;
  if (!getLocalTime(&t)) return String(millis()/1000) + "s";
  char b[20];
  strftime(b, sizeof(b), "%d/%m %H:%M:%S", &t);
  return String(b);
}
