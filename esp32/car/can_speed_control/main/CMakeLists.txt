idf_component_register(
  SRCS "main.c"
  INCLUDE_DIRS "."
  REQUIRES esp_driver_twai esp_driver_gpio esp_timer driver led_strip
            esp_wifi esp_http_server nvs_flash esp_netif esp_event app_update
)

