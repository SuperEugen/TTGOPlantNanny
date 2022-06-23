//***************************************************************************************************
//  TTGOPlantNanny: Cheap indoor plant watering system for an ESP32. No soil moisture sensors.
//                  Water is delivered by a timer controlled pump for each individual plant.
//                  No valves but a simple water pump for each plant. Up to four plants per system.
//                  System can be controlled via MQTT commands and reports remaining battery and
//                  water level in tank. Remaining water level is not measured with sensors
//                  but calculated according to cummulated pump time.
//                  I tried to stick to just the two built-in buttons for the user interface.
// 
//                  By Ingo Hoffmann.
//***************************************************************************************************
//
//  Hardware components:
//    TTGO T-Display:         ESP32 module with integrated 16bit color TFT with 240 x 135 pixel 
//                            based on ST7789V chip and two buttons
//    1 to 4 water pumps:     5v submersable electric water pumps
//    1 x 4 relais board      to switch the USB power of the pumps
//    4x AA battery holder:   supply for the electronic and the motors
//    water tank:             IKEA box, electronics, motors and battery fit into the 3D printed lid
//
//  Libraries used:
//    TFT_eSPI:               graphic library from Bodmer
//    Free_Fonts:             
//    Button2:                lib for the built-in buttons from LennartHennigs
//    esp_adc_cal:            lib for battery level measurement
//    WiFi:                   connect to wireless network
//    FS:                     SPIFFS
//    PubSubClient:           MQTT
//    ezTime:                 time
//
//  Dev history:
//    28.08.2019, IH:         First set-up, influenced from Esp32-Radio (Ed Smallenburg), 
//                            TTGO T-Display exmple, espaper-weatherstation (SQUIX), 
//                            TFT_eSPI example (Bodmer)
//    29.08.2019, IH:         TFT handled
//    30.08.2019, IH:         more TFT_eSPI understanding
//    31.08.2019, IH:         icons created
//    01.09.2019, IH:         better screen layout, wifi, mqtt and ntp integrated, battery measured
//    02.09.2019, IH:         storing preferences
//    03.09.2019, IH:         mqtt publishing and subscribing
//    04.09.2019, IH:         mqtt command decoding
//    05.09.2019, IH:         buttons integrated
//    06.09.2019, IH:         deep sleep
//    08.09.2019, IH:         time drift compensation
//    23.06.2022, IH:         revisit of project
//
//***************************************************************************************************

#define VERSION               "0.4"   // 23.06.22

// libraries
#include <Preferences.h>
#include <TFT_eSPI.h>
#include "Free_Fonts.h"
#include <Button2.h>
#include "esp_adc_cal.h"
#include "esp_wifi.h"
#include "WiFi.h"
#include <PubSubClient.h>
#include <ezTime.h>

// project files
#include "icons.h"
#include "secrets.h"
#include "settings.h"
#include "texts.h"

// defines
#define FS_NO_GLOBALS

//***************************************************************************************************
//  Global consts
//***************************************************************************************************
const uint8_t layoutButtonWidth = 64;
const uint8_t layoutInfoBarHeight = 24;
const uint8_t layoutStatusBarHeight = 15;

//***************************************************************************************************
//  Global data
//***************************************************************************************************
Preferences prefs;
TFT_eSPI tft = TFT_eSPI();
Button2 btnT(BTN_TOP);
Button2 btnB(BTN_BOTTOM);
WiFiClient wifiClient;
Timezone myTZ;
PubSubClient mqttClient(wifiClient);

bool wifiConnected = false;
bool mqttConnected = false;

bool btnTClicked = false;
bool btnBClicked = false;

unsigned long timeStamp;

bool justWatering = false;

//***************************************************************************************************
//  Data stored in preferences
//***************************************************************************************************
uint8_t wateringHour;
uint16_t containerSize;
uint16_t remainingWater;
uint16_t pumpThroughput;
uint16_t hoursBetweenWaterings[NUMBER_OF_PUMPS];
uint16_t hoursTillWatering[NUMBER_OF_PUMPS];
uint16_t secondsOfWatering[NUMBER_OF_PUMPS];

// preferences must be 15 characters at max
#define PREF_WATERING_HOUR            "wh"
#define PREF_CONTAINER_SIZE           "cs"
#define PREF_REMAINING_WATER          "rw"
#define PREF_PUMP_THROUGHPUT          "pt"
#define PREF_HOURS_BETWEEN_WATERINGS  "hb"
#define PREF_HOURS_TILL_WATERING      "ht"
#define PREF_SEC_WATERING_AMOUNT      "wa"

//***************************************************************************************************
//  Screens
//***************************************************************************************************
//###################################################################################################
// todo
// name all screens, struct for follow up screen for top and bottom buttons, button icons and texts
//###################################################################################################
const int scrWatering =   0;
const int scrMain =       1;

int currentScreen;

//***************************************************************************************************
//  SETUP
//***************************************************************************************************
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.print("Starting plant-nanny by Ingo Hoffmann. Version: ");
  Serial.println(VERSION);

  // initialise tft
  tft.init();
  tft.setRotation(1);     // landscape

  // draw screen layout, show some info and system number
  tft.fillScreen(TFT_BLACK);
  clearInfoBar();
  clearMainArea();
  clearStatusBar();
  clearTopBtn();
  clearBottomBtn();
  showProgInfo();
  showSystemNumber();
  showContainerSize();

  // connect and show status
  loadPrefs();
  esp_wifi_start();
  connectAndShowWifiStatus();
  if(wifiConnected) {
    getNetworkTime();
    showTime();
    connectAndShowMQTTStatus();
    showAndPublishBatteryLevel();
    showAndPublishWaterLevel();
  
    // configure pins
    pinMode(PUMP_1, OUTPUT);
    pinMode(PUMP_2, OUTPUT);
    pinMode(PUMP_3, OUTPUT);
    pinMode(PUMP_4, OUTPUT);

    // initialise buttons
    buttonsInit();

    // prepare ui
    currentScreen = scrMain;
    timeStamp = millis();
  } else {
    Serial.print("No WiFi -> ");
    setTimerAndGoToSleep();
  }
}

//***************************************************************************************************
//  LOOP
//***************************************************************************************************
void loop() {
  if(minuteChanged() && wifiConnected) {
    showTime();
  }

  if(!mqttClient.connected()) {
    mqttReconnect();
  }
  mqttClient.loop();

  doTimedJobIfNecessary();

  if(justWatering) {
    Serial.print("just watering -> ");
    currentScreen = scrWatering;
    showScreen();
    setTimerAndGoToSleep();    
  } else {
    btnT.loop();
    btnB.loop();
    if(btnTClicked || btnBClicked) {
      timeStamp = millis();
    }

    updateUI();
    showScreen();

    if((millis() - timeStamp) > (INACTIVITY_THRESHOLD * 1000)) {
      Serial.print("No activity -> ");
      setTimerAndGoToSleep();
    }
  }
}

//***************************************************************************************************
//  loadPrefs
//  name space is "nanny", these name value pairs store data during deep sleep
//***************************************************************************************************
void loadPrefs() {
  prefs.begin("nanny", false);
  wateringHour = prefs.getUInt(PREF_WATERING_HOUR, DEFAULT_WATERING_HOUR);
  Serial.print("Load from prefs: wateringHour = ");
  Serial.println(String(wateringHour));
  containerSize = prefs.getUInt(PREF_CONTAINER_SIZE, CONTAINER_SIZE_SMALL);
  Serial.print("Load from prefs: containerSize = ");
  Serial.println(String(containerSize));
  remainingWater = prefs.getUInt(PREF_REMAINING_WATER, CONTAINER_SIZE_SMALL);
  Serial.print("Load from prefs: remainingWater = ");
  Serial.println(String(remainingWater));
  pumpThroughput = prefs.getUInt(PREF_PUMP_THROUGHPUT, PUMP_BLACK);
  Serial.print("Load from prefs: pumpThroughput = ");
  Serial.println(String(pumpThroughput));
  for(int i = 0; i < NUMBER_OF_PUMPS; i++) {
    // 49 = ASCII '1'
    hoursBetweenWaterings[i] = prefs.getUInt(PREF_HOURS_BETWEEN_WATERINGS + char(49 + i), 
                                             DEFAULT_WATERING_FREQ);
    Serial.print("Load from prefs: hoursBetweenWaterings[");
    Serial.print(String(i));
    Serial.print("] = ");
    Serial.print(String(hoursBetweenWaterings[i]));
    // 49 = ASCII '1'
    hoursTillWatering[i] = prefs.getUInt(PREF_HOURS_TILL_WATERING + char(49 + i), 
                                         DEFAULT_WATERING_FREQ);
    Serial.print(", hoursTillWatering[");
    Serial.print(String(i));
    Serial.print("] = ");
    Serial.println(String(hoursTillWatering[i]));
    // 49 = ASCII '1'
    secondsOfWatering[i] = prefs.getUInt(PREF_SEC_WATERING_AMOUNT + char(49 + i), 
                                         DEFAULT_WATERING_AMOUNT);
    Serial.print(", secondsOfWatering[");
    Serial.print(String(i));
    Serial.print("] = ");
    Serial.println(String(secondsOfWatering[i]));
  }
  prefs.end();
}

//***************************************************************************************************
//  clearInfoBar
//  time, nanny number, wifi, mqtt, water, battery
//***************************************************************************************************
void clearInfoBar() {
  // swap TFT_WIDTH and TFT_HEIGHT for landscape use
  tft.fillRect(0, 0, 
               TFT_HEIGHT - layoutButtonWidth,  layoutInfoBarHeight, 
               COLOR_BG_INFO_BAR);
}

//***************************************************************************************************
//  clearMainArea
//  available for any information
//***************************************************************************************************
void clearMainArea() {
  // swap TFT_WIDTH and TFT_HEIGHT for landscape use
  tft.fillRect(0, layoutInfoBarHeight, 
               TFT_HEIGHT - layoutButtonWidth, TFT_WIDTH - layoutInfoBarHeight - layoutStatusBarHeight, 
               COLOR_BG_MAIN_AREA);
}

//***************************************************************************************************
//  clearStatusBar
//  page indicator
//***************************************************************************************************
void clearStatusBar() {
  // swap TFT_WIDTH and TFT_HEIGHT for landscape use
  tft.fillRect(0, TFT_WIDTH - layoutStatusBarHeight, 
               TFT_HEIGHT - layoutButtonWidth,  layoutStatusBarHeight, 
               COLOR_BG_STATUS_BAR);
}

//***************************************************************************************************
//  clearTopBtn
//  icon and text
//***************************************************************************************************
void clearTopBtn() {
  // swap TFT_WIDTH and TFT_HEIGHT for landscape use
  tft.fillRect(TFT_HEIGHT - layoutButtonWidth, 0,
               layoutButtonWidth, TFT_WIDTH / 2, 
               COLOR_BG_TOP_BTN);
}

//***************************************************************************************************
//  clearBottomBtn
//  icon and text
//***************************************************************************************************
void clearBottomBtn() {
  // swap TFT_WIDTH and TFT_HEIGHT for landscape use
  tft.fillRect(TFT_HEIGHT - layoutButtonWidth, TFT_WIDTH / 2,
               layoutButtonWidth, TFT_WIDTH - (TFT_WIDTH / 2), 
               COLOR_BG_BOTTOM_BTN);
}

//***************************************************************************************************
//  showProgInfo
//  program name, developer and version during start-up
//***************************************************************************************************
void showProgInfo() {
  int xpos = 10;  // left margin
  int ypos = 40;  // top margin
  char temp[30];  // for string concatination

  tft.setTextColor(COLOR_FG_MAIN_AREA, COLOR_BG_MAIN_AREA);
  tft.setTextDatum(TL_DATUM);
  
  tft.setFreeFont(FSS12);
  tft.drawString("Plant Nanny", xpos, ypos, GFXFF);
  ypos += tft.fontHeight(GFXFF);
  tft.setFreeFont(FSS9);
  strcpy(temp, TEXT_BY);
  strcat(temp, " Ingo Hoffmann");
  tft.drawString(temp, xpos, ypos, GFXFF);
  ypos += tft.fontHeight(GFXFF);
  strcpy(temp, TEXT_VERSION);
  strcat(temp, " ");
  strcat(temp, VERSION);
  tft.drawString(temp, xpos, ypos, GFXFF);
}

//***************************************************************************************************
//  showContainerSize
//  what size of container is used
//***************************************************************************************************
void showContainerSize() {

}

//***************************************************************************************************
//  doTimedJobIfNecessary
//  update PREF_HOURS_TILL_WATERING for each pump and check is it watering time and is any pump due
//***************************************************************************************************
void doTimedJobIfNecessary() {
//###################################################################################################
// todo
// checke for watering time and watering time + 12
//###################################################################################################
  const int timerWindow = 30;

  if((minute() == 0) && (second() < timerWindow)) {
//###################################################################################################
// todo
//###################################################################################################
    Serial.println("new hour");
    justWatering = true;
    delay(5000);
  }  
}

//***************************************************************************************************
//  updateUI
//  calculate what screen and buttons to show according to the buttons pressed, if any
//***************************************************************************************************
void updateUI() {
//###################################################################################################
// todo
// use struct
//###################################################################################################

  btnTClicked = false;
  btnBClicked = false;
}

//***************************************************************************************************
//  showScreen
//  show graphics and texts for each screen
//***************************************************************************************************
void showScreen() {
  // swap TFT_WIDTH and TFT_HEIGHT for landscape use
  int xpos = 10;  // left margin
  int ypos = (TFT_WIDTH - layoutInfoBarHeight - layoutStatusBarHeight) / 2 ;  // top margin

  clearMainArea();

  tft.setTextColor(COLOR_FG_MAIN_AREA, COLOR_BG_MAIN_AREA);
  tft.setTextDatum(TL_DATUM);
  tft.setFreeFont(FSS12);
  switch(currentScreen) {
    case scrWatering:
      tft.drawString(TEXT_WATERING, xpos, ypos, GFXFF);
    case scrMain:
      ;
    default:
      ;
  }
//###################################################################################################
// todo
// what to do when data entry?
//###################################################################################################

//  pageIndicator(1, 4);
  
}

//***************************************************************************************************
//  getNetworkTime
//  get network time according to time zone, needs wifi connection
//***************************************************************************************************
void getNetworkTime() {
  waitForSync();                    // Wait for ezTime to get its time synchronized
  myTZ.setLocation(F(timezone));
  setInterval(uint16_t(86400));      // 86400 = 60 * 60 * 24, set NTP polling interval to daily
  setDebug(NONE);                   // NONE = set ezTime to quiet
}

//***************************************************************************************************
//  showTime
//  left aligned on info bar at the top
//***************************************************************************************************
void showTime() {
  int xpos = 10;  // left margin
  int ypos = 8;   // top margin

  tft.setTextColor(COLOR_FG_INFO_BAR, COLOR_BG_INFO_BAR);
  tft.setTextDatum(TL_DATUM);
  
  tft.setTextFont(0);
  tft.drawString(String(myTZ.dateTime("H:i")), xpos, ypos, GFXFF);  
}

//***************************************************************************************************
//  showSystemNumber
//  taken from settings, to distinguish more than one plant nanny, used in mqtt messages
//***************************************************************************************************
void showSystemNumber() {
  // swap TFT_WIDTH and TFT_HEIGHT for landscape use
  const int posFromRight = 5;
  const int radius = 9;
  
  int xpos = TFT_HEIGHT - layoutButtonWidth - (posFromRight * layoutInfoBarHeight) + radius;
  int ypos = layoutInfoBarHeight / 2;

  tft.fillCircle(xpos, ypos, radius, COLOR_CIRCLE);
  tft.setTextColor(COLOR_FG_INFO_BAR, COLOR_CIRCLE);
  tft.setTextDatum(MC_DATUM);
  
  tft.setTextFont(0);
  tft.drawString(String(NANNY_NUMBER), xpos, ypos, GFXFF);  
}

//***************************************************************************************************
//  connectAndShowWifiStatus
//  stacked to the right on info bar at the top
//***************************************************************************************************
void connectAndShowWifiStatus() {
  // swap TFT_WIDTH and TFT_HEIGHT for landscape use
  const int posFromRight = 4;

  int xpos = TFT_HEIGHT - layoutButtonWidth - (posFromRight * layoutInfoBarHeight);
  int ypos = (layoutInfoBarHeight - iconHeight) / 2;

  int wifiRetries = 0;
  uint16_t color;

  // connecting to WiFi network
  WiFi.mode(WIFI_STA);
  WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASSWORD);
  while ((WiFi.status() != WL_CONNECTED) && wifiRetries < 8) {
    delay(800);
    wifiRetries++;
  }
  if(WiFi.status() == WL_CONNECTED) {
    Serial.println(F("WiFi connected"));
    wifiConnected = true;
    color = TFT_GREEN;
  } else {
    Serial.println(F("WiFi not connected"));
    wifiConnected = false;
    color = TFT_RED;
  }
  
  tft.drawXBitmap(xpos, ypos, iconWifi, iconWidth, iconHeight, color, COLOR_BG_INFO_BAR);    
}

//***************************************************************************************************
//  connectAndShowMQTTStatus
//  stacked to the right on info bar at the top
//***************************************************************************************************
void connectAndShowMQTTStatus() {
  // swap TFT_WIDTH and TFT_HEIGHT for landscape use
  const int posFromRight = 3;

  int xpos = TFT_HEIGHT - layoutButtonWidth - (posFromRight * layoutInfoBarHeight);
  int ypos = (layoutInfoBarHeight - iconHeight) / 2;

  uint16_t color;

  mqttClient.setServer(SECRET_MQTT_BROKER, SECRET_MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  if (mqttClient.connect(mqttClientID, SECRET_MQTT_USER, SECRET_MQTT_PASSWORD)) {
    Serial.println(F("MQTT connected"));
    mqttConnected = true;
    color = TFT_GREEN;
    mqttSubscribeToTopics();
  } else {
    Serial.println(F("MQTT not connected"));
    mqttConnected = false;
    color = TFT_RED;
  }

  tft.drawXBitmap(xpos, ypos, iconMQTT, iconWidth, iconHeight, color, COLOR_BG_INFO_BAR);
}

//***************************************************************************************************
//  mqttSubscribeToTopics
//  to avoid feedback loops we subscribe to command messages and send status messages
//***************************************************************************************************
void mqttSubscribeToTopics() {
  String baseCommand;
  String pumpCommand;
  String temp;

  baseCommand.concat(mqttMainTopic);
  baseCommand.concat("/");
  baseCommand.concat(NANNY_NUMBER);
  baseCommand.concat("/");
  pumpCommand = baseCommand;
  baseCommand.concat(mqttCommandHour);
  mqttClient.subscribe(baseCommand.c_str());

  for(int i = 1; i <= NUMBER_OF_PUMPS; i++) {
    temp = pumpCommand;
    temp.concat(char(48 + i));    // 48 = ASCII '0'
    temp.concat("/");
    temp.concat(mqttCommandFreq);
    mqttClient.subscribe(temp.c_str());
  }
}

//***************************************************************************************************
//  mqttCallback
//  there are general and pump specific commands
//***************************************************************************************************
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String topicPrefix;
  int topicPrefixLength;
  String topicString;
  String shortenedTopic;
  int pump;
  String shortenedPumpTopic;
  
  topicPrefix = mqttMainTopic;
  topicPrefix.concat("/");
  topicPrefix.concat(NANNY_NUMBER);
  topicPrefix.concat("/");
  topicPrefixLength = topicPrefix.length(); 
  topicString = topic;
  shortenedTopic = topicString.substring(topicPrefixLength);

  payload[length] = '\0';
  String stringValue = String((char*) payload);

  if(shortenedTopic == mqttCommandHour) {
    Serial.println("MQTT: new watering time received");
//###################################################################################################
// todo
// update the pref
//###################################################################################################
    
  } else {
    pump = shortenedTopic.toInt();    // returns 0 if no number is found
    if(pump <= 0 || pump > NUMBER_OF_PUMPS) {
      Serial.print("MQTT command not recognized: ");
      Serial.println(shortenedTopic);
    } else {
      shortenedPumpTopic = shortenedTopic.substring(2);   // single digit pump number and '/'
      if(shortenedPumpTopic == mqttCommandFreq) {
        Serial.print("MQTT pump ");
        Serial.print(pump);
        Serial.println(": new watering frequency received");
//###################################################################################################
// todo
// update pump specific pref
//###################################################################################################
        
      } else {
        Serial.print("MQTT pump ");
        Serial.print(pump);
        Serial.print(" command not recognized: ");
        Serial.println(shortenedPumpTopic);        
      }
    }
  }
}

//***************************************************************************************************
//  mqttReconnect
//  
//***************************************************************************************************
void mqttReconnect() {
  int mqttRetries = 0;

  // try 5 times to reconnect
  while (!mqttClient.connected() && mqttRetries < 5) {
    // Attempt to connect
    if (mqttClient.connect(mqttClientID + NANNY_NUMBER, SECRET_MQTT_USER, SECRET_MQTT_PASSWORD)) {
      // resubscribe
      mqttSubscribeToTopics();
    } else {
      Serial.print("MQTT failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
      mqttRetries++;
    }
  }
}

//***************************************************************************************************
//  mqttPublishValue
//  
//***************************************************************************************************
void mqttPublishValue(String topic, String value) {
  String completeTopic;

  if(!mqttClient.connected()) {
    mqttReconnect();
  }
  mqttClient.loop();
  completeTopic = mqttMainTopic;
  completeTopic.concat('/');
  completeTopic.concat(NANNY_NUMBER);
  completeTopic.concat('/');
  completeTopic.concat(topic);
  mqttClient.publish(completeTopic.c_str(), value.c_str());
  Serial.print("MQTT publishing: ");
  Serial.print(completeTopic.c_str());
  Serial.print(" = ");
  Serial.println(value.c_str());
}

//***************************************************************************************************
//  showAndPublishBatteryLevel
//  
//***************************************************************************************************
void showAndPublishBatteryLevel() {
  // swap TFT_WIDTH and TFT_HEIGHT for landscape use
  const int posFromRight = 2;

  int xpos = TFT_HEIGHT - layoutButtonWidth - (posFromRight * layoutInfoBarHeight);
  int ypos = (layoutInfoBarHeight - iconHeight) / 2;

  uint16_t color;

  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize((adc_unit_t)ADC_UNIT_1, 
                                                          (adc_atten_t)ADC1_CHANNEL_6, 
                                                          (adc_bits_width_t)ADC_WIDTH_BIT_12, 
                                                          ADC_VREF, 
                                                          &adc_chars);

  uint16_t v = analogRead(ADC_PIN);
  float batteryVoltage = ((float)v / 4095.0) * 2.0 * 3.3 * (ADC_VREF / 1100.0);

  String voltage = "Bat.: " + String(batteryVoltage) + "V";
  Serial.println(voltage);

  if(batteryVoltage < BATTERY_VERY_LOW) {
    color = TFT_RED;
  } else if(batteryVoltage < BATTERY_LOW) {
    color = TFT_YELLOW;
  } else {
    color = TFT_GREEN;
  }
  
  tft.drawXBitmap(xpos, ypos, iconBattery, iconWidth, iconHeight, color, COLOR_BG_INFO_BAR);

  mqttPublishValue(mqttTopicBatVoltage, String(batteryVoltage));
}

//***************************************************************************************************
//  showAndPublishWaterLevel
//  
//***************************************************************************************************
void showAndPublishWaterLevel() {
  // swap TFT_WIDTH and TFT_HEIGHT for landscape use
  const int posFromRight = 1;

  int xpos = TFT_HEIGHT - layoutButtonWidth - (posFromRight * layoutInfoBarHeight);
  int ypos = (layoutInfoBarHeight - iconHeight) / 2;

  uint16_t color;

  int percent;

  percent = containerSize / remainingWater * 100;
  if(percent < 10) {
    color = TFT_RED;
  } else if(percent < 30) {
    color = TFT_YELLOW;
  } else {
    color = TFT_GREEN;
  }
  Serial.print("Container = ");
  Serial.print(percent);
  Serial.println("%");
  tft.drawXBitmap(xpos, ypos, iconContainer, iconWidth, iconHeight, color, COLOR_BG_INFO_BAR);
}

//***************************************************************************************************
//  buttonsInit
//  
//***************************************************************************************************
void buttonsInit() {
  btnT.setPressedHandler([](Button2 & b) {
    Serial.println("Top button clicked");
    btnTClicked = true;
  });

  btnB.setPressedHandler([](Button2 & b) {
    Serial.println("Bottom button clicked");
    btnBClicked = true;
  });  
}

//***************************************************************************************************
//  setTimerAndGoToSleep
//  set alarm clock to the next full hour (if no internet connection exists then just 1 hour)
//***************************************************************************************************
void setTimerAndGoToSleep() {
  const unsigned long uSToSecondsFactor = 1000000;
  unsigned long sleepingSeconds = 60 * 60;          // one hour

  if(wifiConnected) {
    sleepingSeconds = (59 - minute()) * 60 + (59 - second());
    Serial.print("going to sleep for ");
    Serial.print(sleepingSeconds + TIMER_DRIFT_COMPENSATION);
    Serial.println(" seconds");
  } else {
    Serial.println("going to sleep for one hour");
  }

  tft.writecommand(TFT_DISPOFF);
  tft.writecommand(TFT_SLPIN);
  int err = esp_wifi_stop();
  esp_sleep_enable_timer_wakeup((sleepingSeconds + TIMER_DRIFT_COMPENSATION) * uSToSecondsFactor);
  esp_sleep_enable_ext1_wakeup(GPIO_SEL_35, ESP_EXT1_WAKEUP_ALL_LOW);
  esp_deep_sleep_start();
}

//***************************************************************************************************
//  pageIndicator
//  small dots on status bar indicating current page; 9 pages is maximum
//***************************************************************************************************
void pageIndicator(uint8_t current, uint8_t pages) {
  // swap TFT_WIDTH and TFT_HEIGHT for landscape use
  const int radius = 5;
  const int xSeparation = 20;

  int xpos = (TFT_HEIGHT - layoutButtonWidth) / 2 - ((pages - 1) * xSeparation) / 2;
  int ypos = TFT_WIDTH - (layoutStatusBarHeight / 2);

  for(int i = 0; i < pages; i++) {
    if(i == (current - 1)) {
      tft.fillCircle(xpos + (i * xSeparation), ypos, radius, COLOR_FG_STATUS_BAR);
    } else {
      tft.drawCircle(xpos + (i * xSeparation), ypos, radius, COLOR_FG_STATUS_BAR);
    }
  }
}
