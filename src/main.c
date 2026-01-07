#include <stdint.h>
#include <memory.h>
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "panel.h"
#include "lvgl.h"
#include "demos/benchmark/lv_demo_benchmark.h"
#if !defined(CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0) || CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0!=0
#define RENDER_USE_SLEEP
#endif
static lv_display_t* lvgl_display = NULL;
static void lvgl_on_flush( lv_display_t *disp, const lv_area_t *area, uint8_t * px_map) {
    panel_lcd_flush(area->x1,area->y1,area->x2,area->y2,px_map);
#ifdef LCD_SYNC_TRANSFER
    lv_display_flush_ready(lvgl_display);
#endif
}
#ifndef LCD_SYNC_TRANSFER
void panel_lcd_flush_complete(void) {
    lv_display_flush_ready(lvgl_display);
}
#endif

static uint32_t lvgl_get_ticks(void)
{
    
    return (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());
}

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
#if (defined(LCD_X_ALIGN) && LCD_X_ALIGN > 1) || (defined(LCD_Y_ALIGN) && LCD_Y_ALIGN > 1)
static void lvgl_align_cb(lv_event_t *e)
{
    lv_area_t *area = lv_event_get_param(e);
#if (defined(LCD_X_ALIGN) && LCD_X_ALIGN > 1)
    /* Round the width to the nearest multiple of 8 */
    area->x1 = (area->x1 & ~(LCD_X_ALIGN-1));
    area->x2 = (area->x2 | (LCD_X_ALIGN-1));
#endif
#if (defined(LCD_Y_ALIGN) && LCD_Y_ALIGN > 1)
    /* Round the height to the nearest multiple of 8 */
    area->y1 = (area->y1 & ~(LCD_Y_ALIGN-1));
    area->y2 = (area->y2 | (LCD_Y_ALIGN-1));
#endif
}
#endif

static void lvgl_task(void* state) {
#ifndef RENDER_USE_SLEEP
    esp_task_wdt_add(NULL);
#endif
    lv_demo_benchmark();
#ifdef RENDER_USE_SLEEP    
    TickType_t wdt_ts = 0;
#endif
    while(1) {
#ifdef RENDER_USE_SLEEP
        // let the idle task feed the WDT to prevent a reboot
        TickType_t ts = xTaskGetTickCount();
        if(ts>wdt_ts+100) {
            wdt_ts = ts;
            vTaskDelay(5);
        }
#else
        esp_task_wdt_reset();
#endif
       
       //if(!panel_lcd_vsync_flush_count()) {
        lv_timer_handler();
       //}
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
#ifdef SD_BUS
    panel_sd_init(false,0,0);
#endif
    lv_init();
    lv_tick_set_cb(lvgl_get_ticks); 
    
    lvgl_display = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
    assert(lvgl_display);
        
    lv_display_set_buffers(lvgl_display,
        panel_lcd_transfer_buffer(),
#ifndef LCD_SYNC_TRANSFER
        panel_lcd_transfer_buffer2(), 
#else
        NULL,
#endif
        LCD_TRANSFER_SIZE, LCD_FULLSCREEN_TRANSFER==1?LV_DISP_RENDER_MODE_FULL:LV_DISP_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(lvgl_display, lvgl_on_flush);
#if (defined(LCD_X_ALIGN) && LCD_X_ALIGN > 1) || (defined(LCD_Y_ALIGN) && LCD_Y_ALIGN > 1)
    lv_display_add_event_cb(lvgl_display, lvgl_align_cb, LV_EVENT_INVALIDATE_AREA, NULL);
#endif
    // set the effective FPS cap to 100
    lv_timer_t* refr_timer = lv_display_get_refr_timer(lvgl_display);
    lv_timer_set_period(refr_timer,5);
#ifdef TOUCH_BUS
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, lvgl_on_touch_read);
#endif
    // Use a task for LVGL rendering so we can set the stack size
    TaskHandle_t task_handle;
    xTaskCreate(lvgl_task,"lvgl_task",LV_DRAW_THREAD_STACK_SIZE,NULL,uxTaskPriorityGet(NULL),&task_handle);
}