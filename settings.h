//***************************************************************************************************
//  settings:     Adapt to your liking.
//***************************************************************************************************

// see texts.h for available options
#define LANG                      'G'   // debug in english, tft as defined here

// Pin connections pump relais
#define NUMBER_OF_PUMPS           4

#define PUMP_1                    21
#define PUMP_2                    22
#define PUMP_3                    17
#define PUMP_4                    2

// Pin connections for built-in hardware
#define ADC_EN                    14
#define ADC_PIN                   34
#define ADC_VREF                  1100  // for battery level measurement
#define BTN_TOP                   35
#define BTN_BOTTOM                0
#define TFT_BL                    4     // backlight is PWM controllable

// Battery measurement
#define ADC_READS                 20    // build average after n reads of ADC
#define BATTERY_VERY_LOW          3.0
#define BATTERY_LOW               3.5

#ifndef TFT_DISPOFF
#define TFT_DISPOFF               0x28
#endif

#ifndef TFT_SLPIN
#define TFT_SLPIN                 0x10
#endif

// wifi settings
const char *ssid =                SECRET_WIFI_SSID;
const char *wifiPassword =        SECRET_WIFI_PASSWORD;

// mqtt settings
const char* mqttBroker =          SECRET_MQTT_BROKER;
const int mqttPort =              SECRET_MQTT_PORT;
const char* mqttUser =            SECRET_MQTT_USER;
const char* mqttPassword =        SECRET_MQTT_PASSWORD;

// timezone
const char* timezone =            "Europe/Berlin";

// system number
#define NANNY_NUMBER              '1'

// water container sizes in milli liter (IKEA 365+)
#define CONTAINER_SIZE_SMALL      2000
#define CONTAINER_SIZE_TALL       4200
#define CONTAINER_SIZE_FLAT       5200
#define CONTAINER_SIZE_BIG        10600

// water pump througput in milli liter / second
#define PUMP_BLACK                69  // i.e. 250l/h

// watering frequencies
#define WATERING_FREQ_OFF         0
#define WATERING_FREQ_VERY_SELDOM 72    // every third day
#define WATERING_FREQ_SELDOM      48    // every other day
#define WATERING_FREQ_NORMAL      24    // daily
#define WATERING_FREQ_OFTEN       12    // twice a day

// watering amount
#define WATERING_AMOUNT_A_LOT     4.0   // 4 seconds
#define WATERING_AMOUNT_MORE      3.0   // 3 seconds
#define WATERING_AMOUNT_NORMAL    2.0   // 2 seconds
#define WATERING_AMOUNT_A_LITTLE  1.0   // 1 seconds

// preference names
#define PREF_WATERING_HOUR        "wh"
#define PREF_CONTAINER_SIZE       "cs"
#define PREF_REMAINING_WATER      "rw"
#define PREF_PUMP_THROUGHPUT      "pt"
#define PREF_P1_WATERING_FREQ     "p1wf"
#define PREF_P2_WATERING_FREQ     "p2wf"
#define PREF_P3_WATERING_FREQ     "p3wf"
#define PREF_P4_WATERING_FREQ     "p4wf"
#define PREF_P1_NEXT_WATERING     "p1nw"
#define PREF_P2_NEXT_WATERING     "p2nw"
#define PREF_P3_NEXT_WATERING     "p3nw"
#define PREF_P4_NEXT_WATERING     "p4nw"
#define PREF_P1_WATERING_AMOUNT   "p1wa"
#define PREF_P2_WATERING_AMOUNT   "p2wa"
#define PREF_P3_WATERING_AMOUNT   "p3wa"
#define PREF_P4_WATERING_AMOUNT   "p4wa"

// default values
#define DEFAULT_WATERING_HOUR     8     // 08:00
#define DEFAULT_WATERING_FREQ     24    // every day
#define DEFAULT_WATERING_AMOUNT   2.0   // 2 seconds

// how many seconds of inactivity to go to sleep
#define INACTIVITY_THRESHOLD      15

// drift compensation for esp_sleep_enable_timer_wakeup for one hour in seconds
#define TIMER_DRIFT_COMPENSATION  27

// MQTT settings
const char* mqttClientID =        "S-I-Plant-Nanny-";
const char* mqttMainTopic =       "plant-nanny";          // followed by /NANNY_NUMBER
const char* mqttTopicContainer =  "container-size";
const char* mqttTopicThroughput = "pump-throughput";
const char* mqttTopicWaterLevel = "water-level";
const char* mqttTopicBatVoltage = "battery-value";
const char* mqttTopicTime =       "watering-hour";
const char* mqttTopicWait =       "watering-wait";        // per pump setting
const char* mqttTopicFrequency =  "watering-frequency";   // per pump setting
const char* mqttTopicAmount =     "watering-amount";      // per pump setting
const char* mqttCommandHour =     "command-hour";         // sets watering-hour
const char* mqttCommandFreq =     "command-freq";         // per pump command, sets watering-frequency
const char* mqttCommandAmount =   "command-amount";       // per pump command, sets watering-amount

//***************************************************************************************************
//  user interface
//***************************************************************************************************
#define LAYOUT_BUTTON_WIDTH       64
#define LAYOUT_INFO_BAR_HEIGHT    24
#define LAYOUT_STATUS_BAR_HEIGHT  15
#define LAYOUT_X_POS_ADJUST       8                         // icons in status bar shifted left

// all color values are 16bit i.e. RGB565
// background colors
#define COLOR_BG_INFO_BAR         0x03E0                    // RGB =   0, 127,   0
#define COLOR_BG_STATUS_BAR       0x03E0                    // RGB =   0, 127,   0
#define COLOR_BG_TOP_BTN          0x7BE0                    // RGB = 127, 127,   0
#define COLOR_BG_BOTTOM_BTN       0x7800                    // RGB = 127,   0,   0
#define COLOR_BG_TOP_BTN_PRESS    0x5AE4                    // RGB =  95,  95,  32
#define COLOR_BG_BOTTOM_BTN_PRESS 0x5904                    // RGB =  95,  32,  32
#define COLOR_BG_MAIN_AREA        0x07E0                    // RGB =   0, 255,   0

// foreground colors
#define COLOR_FG_INFO_BAR         0xFFFF                    // RGB = 255, 255, 255
#define COLOR_FG_STATUS_BAR       0xFFFF                    // RGB = 255, 255, 255
#define COLOR_FG_TOP_BTN          0xB7E0                    // RGB = 180, 255,   0
#define COLOR_FG_BOTTOM_BTN       0xB7E0                    // RGB = 180, 255,   0
#define COLOR_FG_MAIN_AREA        0x0000                    // RGB =   0,   0,   0

// circle for system number
#define COLOR_CIRCLE              0x000F                    // RGB =   0,   0, 127
