#ifndef CONFIG
#define CONFIG

/**
 * The name of this device (as defined in the AWS IOT console).
 * Also used to set the hostname on the network
 */
#define DEVICE_NAME "boiler"

/**
 *  on board led
 */ 
#define LED  19

/**
 *  Button
 */ 
#define BUTTONPIN 39
#define DEBOUNCETIME 10


/**
 * ADC input pin that is used to read out the sensor
 */
#define ADC_INPUT 36
#define ADC_BITS  12


/**
 * WiFi credentials
 */
#define WIFI_NETWORK "HAS"
#define WIFI_PASSWORD "smarthome33"

/**
 * Timeout for the WiFi connection. 
 */
#define WIFI_TIMEOUT 10000

/**
 * How long should we wait after a failed WiFi connection
 * before trying to set one up again.
 */
#define WIFI_RECOVER_TIME_MS 60000

/**
 * Wether or not you want to enable Home Assistant integration
 */
#define MQTT_ENABLED false
#define MQTT_ADDRESS "pi.local"
#define MQTT_PORT 1883
#define MQTT_USER ""
#define MQTT_PASSWORD ""
#define MQTT_CONNECT_DELAY 200
#define MQTT_CONNECT_TIMEOUT 20000 // 20 seconds
#define MQTT_PATH "devices/"
#define MQTT_LASTWILL { "i will survive!" }

/**
 * Syncing time with an NTP server
 */
#define NTP_TIME_SYNC_ENABLED true
#define NTP_SERVER "193.193.193.107"
#define NTP_TZ  2
#define NTP_DST 1
#define NTP_UPDATE_INTERVAL_MS 60000


// Check which core Arduino is running on. 
// This is done because updating the display and WiFI only works from the Arduino core.
#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

#endif
