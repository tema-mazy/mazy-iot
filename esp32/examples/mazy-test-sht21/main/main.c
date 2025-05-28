#define TAG_BME280 "BME280"

#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "driver/i2c.h"

#define I2C_MASTER_SCL_IO           GPIO_NUM_9           /*!< gpio number for I2C master clock IO9*/
#define I2C_MASTER_SDA_IO           GPIO_NUM_8           /*!< gpio number for I2C master data  IO8*/
#define I2C_MASTER_NUM              I2C_NUM_0            /*!< I2C port number for master bme280 */
#define I2C_MASTER_TX_BUF_DISABLE   0                    /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                    /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ          100000               /*!< I2C master clock frequency */
#define SHT21_SENSOR_ADDR    0x40      

void read_sensor() {
    uint8_t data[3];
    uint8_t cmd = 0xF5; // Humidity command
    i2c_master_write_to_device(I2C_MASTER_NUM, SHT21_SENSOR_ADDR, &cmd, 1, pdMS_TO_TICKS(1000));
    vTaskDelay(pdMS_TO_TICKS(50));
    i2c_master_read_from_device(I2C_MASTER_NUM, SHT21_SENSOR_ADDR, data, 3, pdMS_TO_TICKS(1000));

    uint16_t raw_humidity = (data[0] << 8) | (data[1] & 0xFC);
    float humidity = -6.0 + 125.0 * (raw_humidity / 65536.0);

    cmd = 0xF3; // Temperature command
    i2c_master_write_to_device(I2C_MASTER_NUM, SHT21_SENSOR_ADDR, &cmd, 1, pdMS_TO_TICKS(1000));
    vTaskDelay(pdMS_TO_TICKS(50));
    i2c_master_read_from_device(I2C_MASTER_NUM, SHT21_SENSOR_ADDR, data, 3, pdMS_TO_TICKS(1000));

    uint16_t raw_temperature = (data[0] << 8) | (data[1] & 0xFC);
    float temperature = -46.85 + 175.72 * (raw_temperature / 65536.0);

    printf("Temperature: %.2f °C Humidity: %.2f %%\n", temperature, humidity);
}

void i2c_init() {
    // Initialize I2C
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}



void app_main(void) {

    printf("TEST");
    i2c_init();
    while (1) {
        read_sensor();
        vTaskDelay(pdMS_TO_TICKS(2000));
    }   
}