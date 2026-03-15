cmake_minimum_required(VERSION 3.5)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWOLFSSL_USER_SETTINGS")

idf_component_register(
    SRCS "main.c"
    REQUIRES freertos esp_wifi nvs_flash esp_driver_gpio esp_driver_uart esp_driver_i2c esp_http_server esp_timer espressif__cjson
    INCLUDE_DIRS "."
)

target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu11)