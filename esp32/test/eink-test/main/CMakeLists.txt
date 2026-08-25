idf_component_register(SRCS "main.c" "epaper.c" "epaper_gfx.c" "dashdata.c" "screens.c" "fonts.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_driver_spi esp_driver_gpio esp_wifi
                                esp_http_client esp-tls nvs_flash esp_timer
                                app_update mbedtls)
