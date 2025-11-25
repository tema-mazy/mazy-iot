#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_intr_alloc.h"

#define PCNT_GPIO       0        // Pulse input GPIO

#define SENSOR_GPIO GPIO_NUM_6
#define BLINK_GPIO GPIO_NUM_8 //internal LED

uint16_t CPS = 0;
volatile uint16_t pulse_count = 0;

static void IRAM_ATTR gpio_isr_handler(void* arg) {
    pulse_count++;
}

// Blinker task function
void blink_task(void *pvParameter)
{

    gpio_reset_pin(BLINK_GPIO); // Reset to default
    // Configure the Blink GPIO pin for output mode
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    ESP_LOGI("GPIO", "GPIO initialized ");

    while (1) {
        gpio_set_level(BLINK_GPIO, 1);
        gpio_set_level(SENSOR_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(2000));

        gpio_set_level(SENSOR_GPIO, 0);
        gpio_set_level(BLINK_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
// Pulse rate monitor task
void pulse_monitor_task(void *arg)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_POSEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = 1ULL << PCNT_GPIO,
        .pull_up_en = 1,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(PCNT_GPIO, gpio_isr_handler, NULL));

    ESP_LOGI("PULSE", "PCNT initialized ISR on GPIO %d", PCNT_GPIO);

    while (1) {
        CPS = pulse_count;
	pulse_count = 0;

        ESP_LOGI("PULSE", "Pulses per second: %d", CPS);
        vTaskDelay(pdMS_TO_TICKS(1000));  // Delay 1 second
    }
}

void app_main(void)
{
    xTaskCreate(pulse_monitor_task,"CNT task"    , 4096, NULL, 1, NULL);
    xTaskCreate(blink_task        ,"Blink task"  , 2048, NULL, 1, NULL);

    while (1) {
	 vTaskDelay(pdMS_TO_TICKS(1000));  // Delay 1 second
    }
}
