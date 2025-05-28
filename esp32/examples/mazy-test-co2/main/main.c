#include <stdio.h>
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define UART_NUM UART_NUM_1
#define TXD_PIN 5
#define RXD_PIN 4

void app_main(void) {
    const uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM, 256, 0, 0, NULL, 0);

    uint8_t cmd[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
    uint8_t response[9];

    while (1) {
        uart_write_bytes(UART_NUM, (const char *)cmd, 9);
        vTaskDelay(pdMS_TO_TICKS(500));

        int len = uart_read_bytes(UART_NUM, response, 9, pdMS_TO_TICKS(1000));
        if (len == 9 && response[0] == 0xFF && response[1] == 0x86) {
            int CO2 = (response[2] << 8) | response[3];
            printf("CO2 Concentration: %d ppm\n", CO2);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}