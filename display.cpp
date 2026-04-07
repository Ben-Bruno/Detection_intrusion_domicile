#include "display.h"
#include <Arduino.h>
#include <TFT_eSPI.h>     // Driver TFT Freenove ESP32-S3 Eye
#include "lvgl.h"

// --- CONFIG LVGL ---
static const uint16_t screenWidth  = 240;
static const uint16_t screenHeight = 240;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[screenWidth * 40];
static lv_color_t buf2[screenWidth * 40];

TFT_eSPI tft = TFT_eSPI();   // Freenove S3 Eye TFT

// --- FLUSH CALLBACK LVGL ---
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp);
}

Display::Display() {}

void Display::init()
{
    // Init TFT
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);

    // Init LVGL
    lv_init();

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, screenWidth * 40);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);

    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;

    lv_disp_drv_register(&disp_drv);
}

void Display::routine()
{
    lv_timer_handler();   // LVGL heartbeat
}
