//***************************************************************************************************
//  settings:     Adapt to your liking.
//***************************************************************************************************

// see texts.h for available options
#define LANG                      'G'                      // debug in english, tft as defined here

// Pin connections
#define PUMP_1                    22
#define PUMP_2                    17
#define PUMP_3                    2
#define PUMP_4                    15

#define ADC_PIN                   34

// Battery measurement
#define ADC_READS                 20
#define ADC_RED_VAL               2000
#define ADC_YELLOW_VAL            2500
#define ADC_GREEN_VAL             3000

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
#define PUMP_BLACK                10

// default values
#define DEFAULT_WATERING_HOUR     8       // 08:00
#define DEFAULT_WATERING_FREQ     24      // every day

// how many seconds of inactivity to go to sleep
#define INACTIVITY_THRESHOLD      15

// drift compensation for esp_sleep_enable_timer_wakeup for one hour in seconds
#define TIMER_DRIFT_COMPENSATION  27

// MQTT settings
const char* mqttClientID =        "S-I-Plant-Nanny-";
const char* mqttMainTopic =       "plant-nanny";          // followed by /NANNY_NUMBER
const char* mqttTopicContainer =  "container-size";
const char* mqttTopicThroughput = "pump-throughput";
const char* mqttTopicWater =      "water-level";
const char* mqttTopicValue =      "battery-value";
const char* mqttTopicTime =       "watering-hour";
const char* mqttTopicWait =       "watering-wait";        // per pump setting
const char* mqttTopicFrequency =  "watering-frequency";   // per pump setting
const char* mqttCommandHour =     "command-hour";         // sets watering-hour
const char* mqttCommandFreq =     "command-freq";         // per pump command, sets watering-frequency

// background colors
#define COLOR_BG_INFO_BAR         0x03E0                    //   0, 128,   0
#define COLOR_BG_STATUS_BAR       0x03E0                    //   0, 128,   0
#define COLOR_BG_TOP_BTN          0x7BE0                    // 128, 128,   0
#define COLOR_BG_BOTTOM_BTN       0x7800                    // 128,   0,   0
#define COLOR_BG_MAIN_AREA        0x07E0                    //   0, 255,   0

// foreground colors
#define COLOR_FG_INFO_BAR         0xFFFF                    // 255, 255, 255
#define COLOR_FG_STATUS_BAR       0xFFFF                    // 255, 255, 255
#define COLOR_FG_TOP_BTN          0xB7E0                    // 180, 255,   0
#define COLOR_FG_BOTTOM_BTN       0xB7E0                    // 180, 255,   0
#define COLOR_FG_MAIN_AREA        0x0000                    //   0,   0,   0

// circle for system number
#define COLOR_CIRCLE              0x000F                    //   0,   0, 128
