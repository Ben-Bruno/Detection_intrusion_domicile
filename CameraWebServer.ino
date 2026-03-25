#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h> // Pour la connexion sécurisée Telegram
#include <time.h>
#include <WebServer.h>        // Pour notre serveur web personnalisé

// --- INCLUDES SD (Spécifiques Freenove S3) ---
#include "FS.h"
#include "SD_MMC.h"

// --- INCLUDE ÉCRAN (Bonus) ---
#include <SPI.h>
#include <TFT_eSPI.h> // Si tu as l'écran activé dans User_Setup.h
TFT_eSPI tft = TFT_eSPI();

// ===============================================
// --- CONFIGURATION PERSONNELLE (À MODIFIER) ---
// ===============================================
const char* ssid = "Bbox-1C5097AA";

const char* password ="3yWLaXszVU97Fjw55d";


// Configuration Telegram (Étape 0)

const String botToken = "8754252712:AAGxelbzzvPYd4t_rSX4ioCgvyu9CJudsqg"; // ex: "7890123:AAHbK..."

const String chatId = "6093388674";          // ex: "123456789"

// Configuration matérielle (Freenove ESP32-S3)
#define CAMERA_MODEL_ESP32S3_EYE
#include "camera_pins.h"

// ===============================================

// Déclaration des serveurs
WebServer customServer(81); // Serveur pour la page web statique sur le port 81
void startCameraServer();   // Serveur de flux vidéo d'origine (Port 80)

// Prototypes des fonctions
void capture_et_sauvegarde(bool sendToTelegram);
bool sendPhotoTelegram(camera_fb_t * fb);
void tft_display_image(camera_fb_t * fb);

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- Démarrage du système de sécurité IoT ---");

  // --- INITIALISATION ÉCRAN (Bonus) ---
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Init Systeme...");

  // --- INITIALISATION SD (Freenove S3) ---
  Serial.println("Vérification de la carte SD...");
  SD_MMC.setPins(39, 38, 40); 
  if(!SD_MMC.begin("/sdcard", true)){
    Serial.println("⚠️ ERREUR SD échoué ou absente.");
    tft.println("SD: Erreur");
  } else {
    Serial.printf("✅ Carte SD montée : %llu MB\n", SD_MMC.cardSize() / (1024 * 1024));
    tft.println("SD: OK");
  }

  // --- INITIALISATION CAMÉRA ---
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0; config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM; config.pin_d1 = Y3_GPIO_NUM; config.pin_d2 = Y4_GPIO_NUM; config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM; config.pin_d5 = Y7_GPIO_NUM; config.pin_d6 = Y8_GPIO_NUM; config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM; config.pin_pclk = PCLK_GPIO_NUM; config.pin_vsync = VSYNC_GPIO_NUM; config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM; config.pin_sccb_scl = SIOC_GPIO_NUM; config.pin_pwdn = PWDN_GPIO_NUM; config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000; config.frame_size = FRAMESIZE_UXGA; config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY; config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12; config.fb_count = 1;

  if(psramFound()){ config.jpeg_quality = 10; config.fb_count = 2; config.grab_mode = CAMERA_GRAB_LATEST; } 
  else { config.frame_size = FRAMESIZE_SVGA; config.fb_location = CAMERA_FB_IN_DRAM; }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) { Serial.printf("Camera init failed: 0x%x", err); tft.println("Cam: Erreur"); return; }
  sensor_t * s = esp_camera_sensor_get();
  s->set_vflip(s, 1); s->set_brightness(s, 2); s->set_saturation(s, 4); s->set_framesize(s, FRAMESIZE_VGA);

  // --- INITIALISATION WIFI ---
  WiFi.begin(ssid, password);
  tft.print("WiFi: Connect...");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi connected");
  tft.println("OK");

  // --- INITIALISATION HEURE (NTP) ---
  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.nist.gov");
  delay(1000);

  // --- DEMARRAGE SERVEURS ---
  startCameraServer(); // Flux vidéo Port 80
  
  // Route pour notre page web statique
  customServer.on("/", HTTP_GET, [](){
    String html = "<html><head><title>Freenove Security</title>";
    html += "<style>body{font-family:sans-serif; text-align:center; padding:20px;}";
    html += ".btn{background:#2c3e50; color:white; padding:15px 25px; text-decoration:none; font-size:1.2em; border-radius:5px; margin:20px; display:inline-block; border:none; cursor:pointer;}";
    html += ".btn:hover{background:#34495e;} img{max-width:80%; border:3px solid #2c3e50; margin-top:20px;}</style></head><body>";
    html += "<h1>Freenove ESP32-S3 Security</h1>";
    html += "<p>Flux Video (Port 80) : <a href='http://" + WiFi.localIP().toString() + "'>Voir</a></p>";
    html += "<form action='/capture' method='POST'><button type='submit' class='btn'>📸 Prendre & Envoyer Photo Telegram</button></form>";
    // Affiche la dernière photo si elle existe (Chargement statique simple)
    html += "<h2>Derniere Capture</h2><img src='/last_photo.jpg?" + String(millis()) + "'>";
    html += "</body></html>";
    customServer.send(200, "text/html", html);
  });

  // Route pour déclencher la capture
  customServer.on("/capture", HTTP_POST, [](){
    Serial.println("Web demand: Capture & Send...");
    capture_et_sauvegarde(true); // true = envoyer à Telegram
    // Redirige vers la page d'accueil après capture
    customServer.sendHeader("Location", "/");
    customServer.send(303);
  });
  
  // Route pour servir la dernière photo affichée
  customServer.on("/last_photo.jpg", HTTP_GET, [](){
    if (SD_MMC.exists("/last_photo.jpg")) {
      File file = SD_MMC.open("/last_photo.jpg", FILE_READ);
      customServer.streamFile(file, "image/jpeg");
      file.close();
    } else {
      customServer.send(404, "text/plain", "Pas encore de photo");
    }
  });

  customServer.begin(); // Notre serveur Port 81

  Serial.print("Camera Stream Ready: http://"); Serial.println(WiFi.localIP());
  Serial.print("Web Interface Ready (Port 81): http://"); Serial.print(WiFi.localIP()); Serial.println(":81");
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(10, 10);
  tft.println("Systeme Prets!");
  tft.print("IP: "); tft.println(WiFi.localIP());
}

void loop() {
  customServer.handleClient(); // Gère les requêtes sur le port 81
  delay(1); 
}

// ===============================================
// --- FONCTIONS CLÉS (NIVEAU INGÉNIEUR) ---
// ===============================================

void capture_et_sauvegarde(bool sendToTelegram) {
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(10, 10);
  tft.println("📸 Capture...");
  Serial.println("📸 Début de la capture d'image...");
  
  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb) { Serial.println("❌ Échec de la capture."); tft.println("Cam Erreur"); return; }

  // --- BONUS : AFFICHER SUR L'ÉCRAN ---
  tft_display_image(fb);

  // Générer les noms de fichiers
  String path_dated, path_static;
  struct tm timeinfo;
  
  if (!getLocalTime(&timeinfo)) {
    path_dated = "/intrusion_" + String(millis()) + ".jpg"; 
  } else {
    char timeStringBuff[50];
    strftime(timeStringBuff, sizeof(timeStringBuff), "/intrusion_%Y%m%d_%H%M%S.jpg", &timeinfo);
    path_dated = String(timeStringBuff);
  }
  path_static = "/last_photo.jpg"; // Pour l'affichage web statique

  // Sauvegarder sur la carte SD (version datée)
  File file = SD_MMC.open(path_dated.c_str(), FILE_WRITE);
  if (!file) { Serial.println("❌ Échec écriture SD."); } 
  else { file.write(fb->buf, fb->len); Serial.printf("✅ Sauvegardé : %s\n", path_dated.c_str()); }
  file.close();

  // Sauvegarder sur la carte SD (version statique pour le web)
  File fileStatic = SD_MMC.open(path_static.c_str(), FILE_WRITE);
  if (fileStatic) { fileStatic.write(fb->buf, fb->len); fileStatic.close(); }

  // --- ENVOI TELEGRAM ---
  if (sendToTelegram) {
    tft.println("Envoi Telegram...");
    Serial.println("📤 Envoi Telegram...");
    if(sendPhotoTelegram(fb)) {
      Serial.println("✅ Telegram OK!");
      tft.println("Telegram OK!");
    } else {
      Serial.println("❌ Telegram Échec!");
      tft.println("Telegram Echoué");
    }
  }

  // Libérer la mémoire
  esp_camera_fb_return(fb);
}

bool sendPhotoTelegram(camera_fb_t * fb) {
  WiFiClientSecure client;
  client.setInsecure(); // Simplest pour un prototype, mentionner la sécurité en soutenance

  Serial.print("Connexion api.telegram.org...");
  if (!client.connect("api.telegram.org", 443)) {
    Serial.println(" Échec.");
    return false;
  }
  Serial.println(" OK.");

  // Construction de la requête multipart/form-data
  String boundary = "----ESP32FreenoveBoundary";
  String start_str = "--" + boundary + "\r\nContent-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + chatId +
                     "\r\n--" + boundary + "\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"photo.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
  String end_str = "\r\n--" + boundary + "--\r\n";

  // Calculer la longueur totale du corps
  long total_len = start_str.length() + fb->len + end_str.length();

  Serial.println("Envoi requête POST...");
  client.print("POST /bot" + botToken + "/sendPhoto HTTP/1.1\r\n");
  client.print("Host: api.telegram.org\r\n");
  client.print("Content-Type: multipart/form-data; boundary=" + boundary + "\r\n");
  client.print("Content-Length: " + String(total_len) + "\r\n\r\n");

  // Envoyer les données par morceaux pour économiser la RAM
  client.print(start_str);
  client.write(fb->buf, fb->len);
  client.print(end_str);

  // Lire la réponse
  long timeout = millis();
  while (client.connected() && millis() - timeout < 5000) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      if (line == "\r") break; // Fin des en-têtes
    }
  }
  client.stop();
  return true;
}

// Fonction Bonus TFT - Nécessite TJpg_Decoder si TFT_eSPI ne decode pas le JPEG nativement
// Pour la démo, on dessine juste un rectangle car TFT_eSPI n'affiche pas les JPEG directement
// Pour un vrai affichage JPEG, il faut inclure TJpg_Decoder.h et sa logique
void tft_display_image(camera_fb_t * fb) {
  // Option simple : Dessiner une représentation basique
  tft.fillRect(40, 60, 240, 160, TFT_NAVY); 
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(60, 100);
  tft.println("PHOTO CAPTUREE");
  tft.setCursor(60, 130);
  tft.print("Taille: "); tft.print(fb->len); tft.println(" o");
}
