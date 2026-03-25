#include "esp_camera.h"
#include <WiFi.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <time.h>
#include "FS.h"
#include "SD_MMC.h"
//TFT_eSPI tft = TFT_eSPI();
//Freenove_ESP32_WS2812 strip = Freenove_ESP32_WS2812(1, LEDS_PIN, 0, TYPE_GRB);

//
// WARNING!!! PSRAM IC required for UXGA resolution and high JPEG quality
//            Ensure ESP32 Wrover Module or other board with PSRAM is selected
//            Partial images will be transmitted if image exceeds buffer size
//
//            You must select partition scheme from the board menu that has at least 3MB APP space.
//            Face Recognition is DISABLED for ESP32 and ESP32-S2, because it takes up from 15 
//            seconds to process single frame. Face Detection is ENABLED if PSRAM is enabled as well

// ===================
// Select camera model
// ===================
//#define CAMERA_MODEL_WROVER_KIT // Has PSRAM
//#define CAMERA_MODEL_ESP_EYE // Has PSRAM
#define CAMERA_MODEL_ESP32S3_EYE
//#define CAMERA_MODEL_ESP32S3_EYE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_PSRAM // Has PSRAM
//#define CAMERA_MODEL_M5STACK_V2_PSRAM // M5Camera version B Has PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_ESP32CAM // No PSRAM
//#define CAMERA_MODEL_M5STACK_UNITCAM // No PSRAM
//#define CAMERA_MODEL_AI_THINKER // Has PSRAM
//#define CAMERA_MODEL_TTGO_T_JOURNAL // No PSRAM
//#define CAMERA_MODEL_XIAO_ESP32S3 // Has PSRAM
// ** Espressif Internal Boards **
//#define CAMERA_MODEL_ESP32_CAM_BOARD
//#define CAMERA_MODEL_ESP32S2_CAM_BOARD
//#define CAMERA_MODEL_ESP32S3_CAM_LCD
//#define CAMERA_MODEL_DFRobot_FireBeetle2_ESP32S3 // Has PSRAM
//#define CAMERA_MODEL_DFRobot_Romeo_ESP32S3 // Has PSRAM
#include "camera_pins.h"

// ===========================
// Enter your WiFi credentials
// ===========================
const char* ssid = "Bbox-1C5097AA";
const char* password = "3yWLaXszVU97Fjw55d";

void startCameraServer();
void setupLedFlash(int pin);
void capture_et_sauvegarde();

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- Démarrage du système ---");
  //strip.begin();
  // Initialisation sécurisée de la carte SD
  Serial.println("Vérification de la carte SD...");
  
  // 1. Définition des broches spécifiques pour la Freenove ESP32-S3
  SD_MMC.setPins(39, 38, 40); // Ordre : CLK, CMD, D0
  
  // 2. Initialisation en mode 1-bit (le paramètre 'true')
  if(!SD_MMC.begin("/sdcard", true)){
    Serial.println("⚠️ ERREUR : Montage de la carte SD échoué ou carte absente.");
    Serial.println("👉 Le système continuera sans sauvegarde locale.");
  } else {
    uint8_t cardType = SD_MMC.cardType();
    if(cardType == CARD_NONE){
      Serial.println("⚠️ ERREUR : Aucune carte SD détectée.");
    } else {
      Serial.printf("✅ Carte SD montée avec succès. Taille : %llu MB\n", SD_MMC.cardSize() / (1024 * 1024));
    }
  }
 

  
  
  Serial.setDebugOutput(true);
  Serial.println();

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
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
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG; // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;
  
  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if(config.pixel_format == PIXFORMAT_JPEG){
    if(psramFound()){
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1); // flip it back
    s->set_brightness(s, 2); // up the brightness just a bit
    s->set_saturation(s, 4); // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  if(config.pixel_format == PIXFORMAT_JPEG){
    s->set_framesize(s, FRAMESIZE_QVGA);
  }

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif

// Setup LED FLash if LED pin is defined in camera_pins.h
#if defined(LED_GPIO_NUM)
  setupLedFlash(LED_GPIO_NUM);
#endif

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("Synchronisation de l'heure NTP...");
  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.nist.gov");
  delay(2000);
  Serial.println(" OK!");
  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println(" to connect");

  delay(3000);
  capture_et_sauvegarde();
}


void loop() {
  // Do nothing. Everything is done in another task by the web server
  delay(10000);
}


void capture_et_sauvegarde() {
  Serial.println("📸 Début de la capture d'image...");
  
  // 1. Prendre la photo
  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("❌ Échec de la capture de la caméra.");
    return;
  }

  // 2. Générer le nom du fichier (Horodaté)
  String path;
  struct tm timeinfo;
  
  if (!getLocalTime(&timeinfo)) {
    Serial.println("⚠️ Impossible de récupérer l'heure, utilisation du temps système.");
    // Plan B de secours si le serveur NTP n'a pas répondu
    path = "/intrusion_" + String(millis()) + ".jpg"; 
  } else {
    // Formatage : /intrusion_YYYYMMDD_HHMMSS.jpg
    char timeStringBuff[50];
    strftime(timeStringBuff, sizeof(timeStringBuff), "/intrusion_%Y%m%d_%H%M%S.jpg", &timeinfo);
    path = String(timeStringBuff);
  }
  
  Serial.printf("Chemin du fichier : %s\n", path.c_str());
  // 3. Sauvegarder sur la carte SD
  File file = SD_MMC.open(path.c_str(), FILE_WRITE);
  if (!file) {
    Serial.println("❌ Échec de l'ouverture du fichier en écriture. SD absente ?");
  } else {
    file.write(fb->buf, fb->len); // Écriture des données
    Serial.printf("✅ Image sauvegardée avec succès ! Taille : %u octets\n", fb->len);
  }
  file.close();

  // 4. Libérer la mémoire (TRÈS IMPORTANT pour éviter les fuites de mémoire)
  esp_camera_fb_return(fb);
}
