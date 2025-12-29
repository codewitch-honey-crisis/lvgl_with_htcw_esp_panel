#include <stdint.h>
#include <memory.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "panel.h"
#include "lvgl.h"
#include "demos/benchmark/lv_demo_benchmark.h"
static lv_display_t* lvgl_display = NULL;
static void lvgl_on_flush( lv_display_t *disp, const lv_area_t *area, uint8_t * px_map) {
    panel_lcd_flush(area->x1,area->y1,area->x2,area->y2,px_map);
}
void panel_lcd_flush_complete() {
    lv_display_flush_ready(lvgl_display);
}
static uint32_t lvgl_get_ticks(void)
{
    return (uint32_t)xTaskGetTickCount();
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
    lv_demo_benchmark();
    TickType_t wdt_ts = xTaskGetTickCount();
    while(1) {
        // feed the WDT to prevent a reboot
        TickType_t ticks = xTaskGetTickCount();
        if(ticks>=wdt_ts+200) {
            wdt_ts = ticks;
            vTaskDelay(5);
        }
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
#ifdef TOUCH_BUS
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER); /*Touchpad should have POINTER type*/
    lv_indev_set_read_cb(indev, lvgl_on_touch_read);
#endif
    // Use a task for LVGL rendering so we can set the stack size
    TaskHandle_t task_handle;
    xTaskCreate(lvgl_task,"lvgl_task",LV_DRAW_THREAD_STACK_SIZE,NULL,uxTaskPriorityGet(NULL),&task_handle);
    
}