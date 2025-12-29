#include <stdint.h>
#include <memory.h>
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "panel.h"
#include "lvgl.h"
#include "demos/benchmark/lv_demo_benchmark.h"
#if !defined(CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0) || CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0!=0
#define RENDER_USE_SLEEP
#endif
static lv_display_t* lvgl_display = NULL;
static void lvgl_on_flush( lv_display_t *disp, const lv_area_t *area, uint8_t * px_map) {
    panel_lcd_flush(area->x1,area->y1,area->x2,area->y2,px_map);
}
void panel_lcd_flush_complete() {
    lv_display_flush_ready(lvgl_display);
}
static uint32_t lvgl_get_ticks(void)
{
    
    return (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());
}
#if LV_USE_LOG != 0
void lvgl_log( lv_log_level_t level, const char * buf )
{
    LV_UNUSED(level);
    puts(buf);
    fflush(stdout);
}
#endif
#ifdef TOUCH_BUS
void lvgl_on_touch_read( lv_indev_t * indev, lv_indev_data_t * data ) {
    panel_touch_update();
    size_t count = 1;
    uint16_t x,y,s;
    panel_touch_read(&count,&x,&y,&s);
    if(!count) {
        data->state = LV_INDEV_STATE_RELEASED;
    } else {
        data->state = LV_INDEV_STATE_PRESSED;

        data->point.x = x;
        data->point.y = y;
    }
}
#endif
static void lvgl_task(void* state) {
#ifndef RENDER_USE_SLEEP
    esp_task_wdt_add(NULL);
#endif
    lv_demo_benchmark();
#ifdef RENDER_USE_SLEEP    
    TickType_t wdt_ts = xTaskGetTickCount();
#endif
    while(1) {
#ifdef RENDER_USE_SLEEP
        // let the idle task feed the WDT to prevent a reboot
        TickType_t ts = xTaskGetTickCount();
        if(ts>wdt_ts+200) {
            wdt_ts = ts;
            vTaskDelay(5);
        }
#else
        esp_task_wdt_reset();
#endif
        lv_timer_handler();
    }
}
void app_main() {
#ifdef POWER
    panel_power_init();
#endif
    panel_lcd_init();
#ifdef TOUCH_BUS
    panel_touch_init();
#endif
#ifdef BUTTON
    panel_button_init();
#endif
    lv_init();
    lv_tick_set_cb(lvgl_get_ticks);
#if LV_USE_LOG !=0
    lv_log_register_print_cb(lvgl_log);
#endif
    lvgl_display = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
    lv_display_set_flush_cb(lvgl_display, lvgl_on_flush);
    lv_display_set_buffers(lvgl_display,panel_lcd_transfer_buffer(),panel_lcd_transfer_buffer2(), LCD_TRANSFER_SIZE, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_timer_t* refr_timer = lv_display_get_refr_timer(lvgl_display);
    lv_timer_set_period(refr_timer,5);
#ifdef TOUCH_BUS
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER); //Touchpad should have POINTER type 
    lv_indev_set_read_cb(indev, lvgl_on_touch_read);
#endif
    // Use a task for LVGL rendering so we can set the stack size
    TaskHandle_t task_handle;
    xTaskCreate(lvgl_task,"lvgl_task",LV_DRAW_THREAD_STACK_SIZE,NULL,uxTaskPriorityGet(NULL),&task_handle);
    
}