#include "ST7789.h"
#include "Counter.h"
#include "Display.h"

void app_main(void)
{
    SD_Init();
    LCD_Init();
    BK_Light(25);
    LVGL_Init();                           
    RGB_Init();
    Display();
    Counter_Init();

    while (1) {
	 vTaskDelay(pdMS_TO_TICKS(1000));  // Delay 1 second
    }
}
