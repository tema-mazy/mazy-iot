/*
 * multiple_accessories.ino
 *
 *  Created on: 2020-05-16
 *      Author: Mixiaoxiao (Wang Bin)
 *
 */

#include <Arduino.h>
#include <arduino_homekit_server.h>
#include "wifi_info.h"

#define LOG_D(fmt, ...)   printf_P(PSTR(fmt "\n") , ##__VA_ARGS__);

void setup() {
	Serial.begin(9600);
	wifi_connect(); // in wifi_info.h
	//homekit_storage_reset(); // to remove the previous HomeKit pairing storage when you first run this new HomeKit example
	my_homekit_setup();
}

void loop() {
	my_homekit_loop();
	delay(10);
}

//==============================
// HomeKit setup and loop
//==============================

extern "C" homekit_server_config_t config;
extern "C" homekit_characteristic_t cha_temperature;
extern "C" homekit_characteristic_t cha_humidity;
extern "C" homekit_characteristic_t cha_pressure;
extern "C" homekit_characteristic_t cha_co2;
extern "C" homekit_characteristic_t cha_co2_alert;


void my_homekit_setup() {
	arduino_homekit_setup(&config);
}

static uint32_t next_heap_millis = 0;
static uint32_t next_report_millis = 0;

void my_homekit_loop() {
	arduino_homekit_loop();
	const uint32_t t = millis();
	if (t > next_report_millis) {
		// report sensor values every 10 seconds
		next_report_millis = t + 10 * 1000;
		my_homekit_report();
	}
	if (t > next_heap_millis) {
		// Show heap info every 5 seconds
		next_heap_millis = t + 5 * 1000;
		LOG_D("Free heap: %d, HomeKit clients: %d",
				ESP.getFreeHeap(), arduino_homekit_connected_clients_count());

	}
}

void my_homekit_report() {
	// FIXME, read your real sensors here.
	float t = random_value(10, 30);
	float h = random_value(30, 70);
	float c = random_value(1, 10000);
	float p = random_value(900, 1800);
	bool ca = c > 2000;

	cha_temperature.value.float_value = t;
	homekit_characteristic_notify(&cha_temperature, cha_temperature.value);

	cha_humidity.value.float_value = h;
	homekit_characteristic_notify(&cha_humidity, cha_humidity.value);

	cha_pressure.value.float_value = p;
	homekit_characteristic_notify(&cha_pressure, cha_pressure.value);

	cha_co2.value.float_value = c;
	homekit_characteristic_notify(&cha_co2, cha_co2.value);

	cha_co2_alert.value.bool_value = ca;
	homekit_characteristic_notify(&cha_co2_alert, cha_co2_alert.value);

	LOG_D("t %.1f, h %.1f, c %.1f, a %u, ", t, h, c, (uint8_t)ca);
}

int random_value(int min, int max) {
	return min + random(max - min);
}
