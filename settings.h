//***************************************************************************************************
//  settings:     Adapt to your liking.
//***************************************************************************************************

// system number
#define NANNY_NUMBER              '2'

// see texts.h for available options
#define LANG                      'G'   // debug in english, tft as defined here

#define NUMBER_OF_PUMPS           4

// Pin connections pump relais
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

// water container sizes in milli liter (IKEA 365+)
// full is to the lid
// 0 is when pump starts to run dry
#define CONTAINER_SIZE_SMALL      2000  // mqtt value 1
#define CONTAINER_SIZE_TALL       3000  // mqtt value 2
#define CONTAINER_SIZE_FLAT       5200  // mqtt value 3
#define CONTAINER_SIZE_BIG        8000  // mqtt value 4

// water pump througput for defined amounts in milliseconds
#define PUMP_25ML                 1100  // mqtt value 1
#define PUMP_50ML                 1800  // mqtt value 2
#define PUMP_75ML                 2400  // mqtt value 3
#define PUMP_100ML                2800  // mqtt value 4

// watering frequencies in hours
#define WATERING_FREQ_OFF         0     // mqtt value 0
#define WATERING_FREQ_VERY_SELDOM 168   // mqtt value 1
#define WATERING_FREQ_SELDOM      72    // mqtt value 2
#define WATERING_FREQ_NORMAL      48    // mqtt value 3
#define WATERING_FREQ_OFTEN       24    // mqtt value 4

// preference names
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
#define DEFAULT_CONTAINER_SIZE    2     // tall
#define DEFAULT_WATERING_FREQ     3     // every third day
#define DEFAULT_WATERING_AMOUNT   2     // 50ml

// how many seconds of inactivity to go to sleep
#define INACTIVITY_THRESHOLD      15

// drift compensation for esp_sleep_enable_timer_wakeup for one hour in seconds
#define TIMER_DRIFT_COMPENSATION  0

// MQTT settings
const char* mqttClientID =        "PlantNanny";
const char* mqttMainTopic =       "plant-nanny";          // followed by /NANNY_NUMBER
const char* mqttTopicWaterLevel = "water-level";
const char* mqttTopicBatVoltage = "battery-value";
const char* mqttCmndFreq =        "command-freq";         // per pump command, sets watering-frequency
const char* mqttCmndNext =        "command-next";         // per pump setting, sets next watering hour
const char* mqttCmndAmount =      "command-amount";       // per pump command, sets watering-amount
const char* mqttCmndContainer =   "command-container";    // sets new container size
const char* mqttCmndWater =       "command-water";        // resets remaining water

//***************************************************************************************************
//  user interface
//  TFT_WIDTH is the short side of the TTGO display (135 x 240 pixel)
//***************************************************************************************************
#define LAYOUT_LANDSCAPE_WIDTH    TFT_HEIGHT              // redefinition to make things easier
#define LAYOUT_LANDSCAPE_HEIGHT   TFT_WIDTH               // redefinition to make things easier
#define LAYOUT_BUTTON_WIDTH       40
#define LAYOUT_INFO_BAR_HEIGHT    22
#define LAYOUT_STATUS_BAR_HEIGHT  13
#define LAYOUT_X_POS_ADJUST       8                         // icons in status bar shifted left

//  two buttons
#define TOP_BUTTON                1
#define BOTTOM_BUTTON             2

//  all color values are 16bit i.e. RGB565
//  predefined colors (defined in TFT_eSPI.h)
//  TFT_BLACK       0x0000                                  /*   0,   0,   0 */
//  TFT_NAVY        0x000F                                  /*   0,   0, 128 */
//  TFT_DARKGREEN   0x03E0                                  /*   0, 128,   0 */
//  TFT_DARKCYAN    0x03EF                                  /*   0, 128, 128 */
//  TFT_MAROON      0x7800                                  /* 128,   0,   0 */
//  TFT_PURPLE      0x780F                                  /* 128,   0, 128 */
//  TFT_OLIVE       0x7BE0                                  /* 128, 128,   0 */
//  TFT_LIGHTGREY   0xD69A                                  /* 211, 211, 211 */
//  TFT_DARKGREY    0x7BEF                                  /* 128, 128, 128 */
//  TFT_BLUE        0x001F                                  /*   0,   0, 255 */
//  TFT_GREEN       0x07E0                                  /*   0, 255,   0 */
//  TFT_CYAN        0x07FF                                  /*   0, 255, 255 */
//  TFT_RED         0xF800                                  /* 255,   0,   0 */
//  TFT_MAGENTA     0xF81F                                  /* 255,   0, 255 */
//  TFT_YELLOW      0xFFE0                                  /* 255, 255,   0 */
//  TFT_WHITE       0xFFFF                                  /* 255, 255, 255 */
//  TFT_ORANGE      0xFDA0                                  /* 255, 180,   0 */
//  TFT_GREENYELLOW 0xB7E0                                  /* 180, 255,   0 */
//  TFT_PINK        0xFE19                                  /* 255, 192, 203 */    
//  TFT_BROWN       0x9A60                                  /* 150,  75,   0 */
//  TFT_GOLD        0xFEA0                                  /* 255, 215,   0 */
//  TFT_SILVER      0xC618                                  /* 192, 192, 192 */
//  TFT_SKYBLUE     0x867D                                  /* 135, 206, 235 */
//  TFT_VIOLET      0x915C                                  /* 180,  46, 226 */

// background colors
#define COLOR_BG_INFO_BAR         TFT_DARKGREEN
#define COLOR_BG_STATUS_BAR       TFT_DARKGREEN
#define COLOR_BG_TOP_BTN          TFT_OLIVE
#define COLOR_BG_BOTTOM_BTN       TFT_MAROON
#define COLOR_BG_MAIN_AREA        TFT_GREEN
#define COLOR_BUTTON_PRESSED      TFT_BLACK
// foreground colors
#define COLOR_FG_INFO_BAR         TFT_WHITE
#define COLOR_FG_STATUS_BAR       TFT_WHITE
#define COLOR_FG_MAIN_AREA        TFT_BLACK

// circle for system number
#define COLOR_CIRCLE              TFT_NAVY
