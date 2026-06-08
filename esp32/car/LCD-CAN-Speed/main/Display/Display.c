#include "Display.h"

lv_obj_t * label = NULL;
static lv_style_t style;

void lvgl_loop_task(void *arg)
{
    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void monitor_out_task(void *arg)
{
    char buf[32];

    while (1) {
        if (!can_connected) {
            lv_label_set_text(label, "NO CAN");
        } else {
            snprintf(buf, sizeof(buf), "%d", CPS);
            lv_label_set_text(label, buf);
        }
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}

void Display(void){
    lv_init();
    lv_style_init(&style);
    
    lv_style_set_text_font(&style, &lv_font_montserrat_40);
    lv_style_set_text_color(&style, lv_color_hex(0x0080FF));  // Blue color
    
    lv_style_set_radius(&style, 5);
    lv_style_set_bg_opa(&style, LV_OPA_COVER);
    lv_style_set_bg_color(&style, lv_palette_lighten(LV_PALETTE_GREY, 2));
    lv_style_set_border_width(&style, 2);
    lv_style_set_border_color(&style, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_pad_all(&style, 10);

    lv_style_set_text_color(&style, lv_palette_main(LV_PALETTE_BLUE));

    /*Create an object with the new style*/
    label = lv_label_create(lv_scr_act());
    lv_obj_add_style(label, &style, 0);
    lv_label_set_text(label, "0");
    lv_obj_center(label);
    
    xTaskCreate(lvgl_loop_task, "lvgl_loop", 4096, NULL, 2, NULL);
    xTaskCreate(monitor_out_task, "monitor_out", 4096, NULL, 5, NULL);
}



