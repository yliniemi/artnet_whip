#ifndef SETTINGS_H
#define SETTINGS_H

#define HOSTNAME "LEDwall"        // replace this with the name for this particular device. everyone deserves a unique name
#define LED_WIDTH 32              // maximum with the height 144 is 125
#define LED_HEIGHT 144
#define MAX_CURRENT 4000          // maximum current in milliwatts. this is here so that your powersupply won't overheat or shutdown
#define USING_SERIALOTA           // uncomment this if you are not using SerialOTA
#define UNIVERSE_SIZE 144         // my setup is 170 leds per universe no matter if the last universe is not full.
// #define USING_LED_BUFFER
#define FASTLED_ESP32_I2S

#define CONFIG_FILE_NAME "/config.jsn"

#define PIN_0 13
#define PIN_1 5
#define PIN_2 27
#define PIN_3 18
#define PIN_4 19
#define PIN_5 23
#define PIN_6 32
#define PIN_7 33

#endif
