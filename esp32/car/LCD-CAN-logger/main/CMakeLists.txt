idf_component_register(
                         SRCS "main.c" 
                              "LCD_Driver/Vernon_ST7789T/Vernon_ST7789T.c" 
                              "LCD_Driver/ST7789.c"
                              "LVGL_Driver/LVGL_Driver.c"
                              "Display/Display.c"
                              "RGB/RGB.c"
                              "SD_Card/SD_SPI.c"
                              "Counter/Counter.c"

                         INCLUDE_DIRS 
                              "./LCD_Driver/Vernon_ST7789T" 
                              "./LCD_Driver" 
                              "./LVGL_Driver" 
                              "./Display" 
                              "./SD_Card" 
                              "./Counter"
                              "./RGB" 
                              "."
                       )
