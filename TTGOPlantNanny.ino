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
//    water container:        IKEA box, electronics, motors and battery fit into the 3D printed lid
//
//  Libraries used:
//    Preferences:            ESP32 lib for storing values permanently in non-volstile ram
//    TFT_eSPI:               graphic library from Bodmer
//    Free_Fonts:             
//    Button2:                lib for the built-in buttons from LennartHennigs
//    esp_adc_cal:            lib for battery level measurement
//    esp_wifi:               lib to set wifi into deep sleep
//    WiFi:                   connect to wireless network
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
//    24.06.2022, IH:         revisit of project, clean up, added showContainerSize
//    28.06.2022, IH:         more power supply testing
//    30.06.2022, IH:         some code reformatting
//    01.07.2022, IH:         button clicked visualisation, version number shown in status bar
//    04.07.2022, IH:         problems with pref storage
//
//***************************************************************************************************

#define VERSION               "0.8"   // 04.07.22

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
#include "secrets.h"          // secrets-dummy.h is NOT used
#include "settings.h"
#include "texts.h"

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

//***************************************************************************************************
//  Data stored in preferences
//***************************************************************************************************
uint8_t wateringHour;
uint16_t containerSize;
uint16_t remainingWater;
uint16_t pumpThroughput;
uint16_t wateringFreq[NUMBER_OF_PUMPS];
uint16_t nextWatering[NUMBER_OF_PUMPS];
uint16_t wateringAmount[NUMBER_OF_PUMPS];

//***************************************************************************************************
//  Screens
//***************************************************************************************************
const int scrMain =       1;

int currentScreen;

void setup() {
//***************************************************************************************************
//  run once every wake up from deep sleep
//***************************************************************************************************
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
  loadPrefs();
  showContainerSize();

  // connect and show status
  esp_wifi_start();
  connectAndShowWifiStatus();
  if(wifiConnected) {
    getNetworkTime();
    showTime();
    connectAndShowMQTTStatus();
    showAndPublishBatteryVoltage();
    showAndPublishWaterLevel();
  
    // configure pins
    pinMode(PUMP_1, OUTPUT);
    pinMode(PUMP_2, OUTPUT);
    pinMode(PUMP_3, OUTPUT);
    pinMode(PUMP_4, OUTPUT);

    // immediately turn relais off
    digitalWrite(PUMP_1, HIGH);
    digitalWrite(PUMP_2, HIGH);
    digitalWrite(PUMP_3, HIGH);
    digitalWrite(PUMP_4, HIGH);

    // initialise buttons
    buttonsInit();

    // prepare ui
    currentScreen = scrMain;
    showScreen();
    timeStamp = millis();
  } else {
    Serial.print("No WiFi -> ");
    setTimerAndGoToSleep();
  }
}

void loop() {
//***************************************************************************************************
//  LOOP
//***************************************************************************************************
  int lastScreen = 0;
  
  if(minuteChanged() && wifiConnected) {
    showTime();
  }

  if(!mqttClient.connected()) {
    mqttReconnect();
  }
  mqttClient.loop();

  doTimedJobIfNecessary();  // and go to sleep after job is done

  btnT.loop();
  btnB.loop();
  if(btnTClicked) {
    timeStamp = millis();
    showBtnTClicked();
  }
  if(btnBClicked) {
    timeStamp = millis();
    showBtnBClicked();
  }

  if((millis() - timeStamp) > (INACTIVITY_THRESHOLD * 1000)) {
    Serial.print("No activity -> ");
    setTimerAndGoToSleep();
  }
}

void loadPrefs() {
//***************************************************************************************************
//  name space is "nanny", these name value pairs store data during deep sleep
//***************************************************************************************************
  prefs.begin("nanny", false);
  wateringHour = prefs.getUInt(PREF_WATERING_HOUR, DEFAULT_WATERING_HOUR);
  Serial.print("Load from prefs: wateringHour       = ");
  Serial.println(String(wateringHour));
  containerSize = prefs.getUInt(PREF_CONTAINER_SIZE, CONTAINER_SIZE_SMALL);
  Serial.print("Load from prefs: containerSize      = ");
  Serial.println(String(containerSize));
  remainingWater = prefs.getUInt(PREF_REMAINING_WATER, CONTAINER_SIZE_SMALL);
  Serial.print("Load from prefs: remainingWater     = ");
  Serial.println(String(remainingWater));
  pumpThroughput = prefs.getUInt(PREF_PUMP_THROUGHPUT, PUMP_BLACK);
  Serial.print("Load from prefs: pumpThroughput     = ");
  Serial.println(String(pumpThroughput));
  // pump 1
  wateringFreq[0] = prefs.getUInt(PREF_P1_WATERING_FREQ, DEFAULT_WATERING_FREQ);
  Serial.print("Load from prefs: P1, wateringFreq   = ");
  Serial.println(String(wateringFreq[0]));
  nextWatering[0] = prefs.getUInt(PREF_P1_NEXT_WATERING, DEFAULT_WATERING_FREQ);
  Serial.print("Load from prefs: P1, nextWatering   = ");
  Serial.println(String(nextWatering[0]));
  wateringAmount[0] = prefs.getUInt(PREF_P1_WATERING_AMOUNT, DEFAULT_WATERING_AMOUNT);
  Serial.print("Load from prefs: P1, wateringAmount = ");
  Serial.println(String(wateringAmount[0]));
  // pump 2
  wateringFreq[1] = prefs.getUInt(PREF_P2_WATERING_FREQ, DEFAULT_WATERING_FREQ);
  Serial.print("Load from prefs: P2, wateringFreq   = ");
  Serial.println(String(wateringFreq[1]));
  nextWatering[1] = prefs.getUInt(PREF_P2_NEXT_WATERING, DEFAULT_WATERING_FREQ);
  Serial.print("Load from prefs: P2, nextWatering   = ");
  Serial.println(String(nextWatering[1]));
  wateringAmount[1] = prefs.getUInt(PREF_P2_WATERING_AMOUNT, DEFAULT_WATERING_AMOUNT);
  Serial.print("Load from prefs: P2, wateringAmount = ");
  Serial.println(String(wateringAmount[1]));
  // pump 3
  wateringFreq[2] = prefs.getUInt(PREF_P3_WATERING_FREQ, DEFAULT_WATERING_FREQ);
  Serial.print("Load from prefs: P3, wateringFreq   = ");
  Serial.println(String(wateringFreq[2]));
  nextWatering[2] = prefs.getUInt(PREF_P3_NEXT_WATERING, DEFAULT_WATERING_FREQ);
  Serial.print("Load from prefs: P3, nextWatering   = ");
  Serial.println(String(nextWatering[2]));
  wateringAmount[2] = prefs.getUInt(PREF_P3_WATERING_AMOUNT, DEFAULT_WATERING_AMOUNT);
  Serial.print("Load from prefs: P3, wateringAmount = ");
  Serial.println(String(wateringAmount[2]));
  // pump 4
  wateringFreq[3] = prefs.getUInt(PREF_P4_WATERING_FREQ, DEFAULT_WATERING_FREQ);
  Serial.print("Load from prefs: P4, wateringFreq   = ");
  Serial.println(String(wateringFreq[3]));
  nextWatering[3] = prefs.getUInt(PREF_P4_NEXT_WATERING, DEFAULT_WATERING_FREQ);
  Serial.print("Load from prefs: P4, nextWatering   = ");
  Serial.println(String(nextWatering[3]));
  wateringAmount[3] = prefs.getUInt(PREF_P4_WATERING_AMOUNT, DEFAULT_WATERING_AMOUNT);
  Serial.print("Load from prefs: P4, wateringAmount = ");
  Serial.println(String(wateringAmount[3]));
  prefs.end();
}

void clearInfoBar() {
//***************************************************************************************************
//  time, nanny number, wifi, mqtt, water, battery
//***************************************************************************************************
  tft.fillRect(0, 0, 
               TFT_HEIGHT - LAYOUT_BUTTON_WIDTH,  LAYOUT_INFO_BAR_HEIGHT, 
               COLOR_BG_INFO_BAR);
}

void clearMainArea() {
//***************************************************************************************************
//  available for any information
//***************************************************************************************************
  tft.fillRect(0, LAYOUT_INFO_BAR_HEIGHT, 
               TFT_HEIGHT - LAYOUT_BUTTON_WIDTH, TFT_WIDTH - LAYOUT_INFO_BAR_HEIGHT - LAYOUT_STATUS_BAR_HEIGHT, 
               COLOR_BG_MAIN_AREA);
}

void clearStatusBar() {
//***************************************************************************************************
//  
//***************************************************************************************************
  tft.fillRect(0, TFT_WIDTH - LAYOUT_STATUS_BAR_HEIGHT, 
               TFT_HEIGHT - LAYOUT_BUTTON_WIDTH,  LAYOUT_STATUS_BAR_HEIGHT, 
               COLOR_BG_STATUS_BAR);
}

void clearTopBtn() {
//***************************************************************************************************
//  icon and text
//***************************************************************************************************
  tft.fillRect(TFT_HEIGHT - LAYOUT_BUTTON_WIDTH, 0,
               LAYOUT_BUTTON_WIDTH, TFT_WIDTH / 2, 
               COLOR_BG_TOP_BTN);
}

void clearBottomBtn() {
//***************************************************************************************************
//  icon and text
//***************************************************************************************************
  tft.fillRect(TFT_HEIGHT - LAYOUT_BUTTON_WIDTH, TFT_WIDTH / 2,
               LAYOUT_BUTTON_WIDTH, TFT_WIDTH - (TFT_WIDTH / 2), 
               COLOR_BG_BOTTOM_BTN);
}

void showProgInfo() {
//***************************************************************************************************
//  program name, developer and version during start-up
//***************************************************************************************************
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
  
  tft.setTextColor(COLOR_FG_STATUS_BAR, COLOR_BG_STATUS_BAR);
  tft.setTextDatum(TL_DATUM);
  xpos = 8;
  ypos = TFT_WIDTH - LAYOUT_STATUS_BAR_HEIGHT + 4;
  strcpy(temp, TEXT_VERSION);
  strcat(temp, " ");
  strcat(temp, VERSION);
  tft.setTextFont(0);
  tft.drawString(temp, xpos, ypos, GFXFF);
}

void doTimedJobIfNecessary() {
//***************************************************************************************************
//  update PREF_HOURS_TILL_WATERING for each pump and check is it watering time and is any pump due
//***************************************************************************************************
  const int timerWindow = 30;   // to adjust for inaccuracies of timer

  // is it full hour?
  if((minute() == 0) && (second() < timerWindow)) {
    Serial.println("new hour");
    // recaclulate hoursTillWatering for every pump

    // check all pumps if watering is due

    Serial.print("just watering -> ");

    digitalWrite(PUMP_1, LOW);
    delay(1000);
    digitalWrite(PUMP_1, HIGH);
    delay(1000);
    digitalWrite(PUMP_2, LOW);
    delay(2000);
    digitalWrite(PUMP_2, HIGH);
    delay(1000);
    digitalWrite(PUMP_3, LOW);
    delay(3000);
    digitalWrite(PUMP_3, HIGH);
    delay(1000);
    digitalWrite(PUMP_4, LOW);
    delay(4000);
    digitalWrite(PUMP_4, HIGH);

    setTimerAndGoToSleep();    
  }  
}

void showScreen() {
//***************************************************************************************************
//  show graphics and texts for each screen
//***************************************************************************************************
  int xpos = 10;  // left margin
  int ypos = (TFT_WIDTH - LAYOUT_BUTTON_WIDTH - LAYOUT_STATUS_BAR_HEIGHT) / 2 ;  // top margin
  char temp[30];  // for string concatination

  clearMainArea();
  tft.setTextColor(COLOR_FG_MAIN_AREA, COLOR_BG_MAIN_AREA);
  tft.setTextDatum(TL_DATUM);
  tft.setFreeFont(FSS12);
  switch(currentScreen) {
    case scrMain:
      tft.setTextColor(COLOR_FG_MAIN_AREA, COLOR_BG_MAIN_AREA);
      tft.setTextDatum(TL_DATUM);
      tft.setFreeFont(FSS12);
      tft.drawString("Main Screen", xpos, ypos, GFXFF);
  }
}

void showBtnTClicked() {
//***************************************************************************************************
//  
//***************************************************************************************************
  tft.fillRect(TFT_HEIGHT - LAYOUT_BUTTON_WIDTH, 0,
               LAYOUT_BUTTON_WIDTH, TFT_WIDTH / 2, 
               COLOR_BG_TOP_BTN_PRESS);
  delay(200);
  tft.fillRect(TFT_HEIGHT - LAYOUT_BUTTON_WIDTH, 0,
               LAYOUT_BUTTON_WIDTH, TFT_WIDTH / 2, 
               COLOR_BG_TOP_BTN);

  btnTClicked = false;
}

void showBtnBClicked() {
//***************************************************************************************************
//  
//***************************************************************************************************
  tft.fillRect(TFT_HEIGHT - LAYOUT_BUTTON_WIDTH, TFT_WIDTH / 2,
               LAYOUT_BUTTON_WIDTH, TFT_WIDTH - (TFT_WIDTH / 2), 
               COLOR_BG_BOTTOM_BTN_PRESS);
  delay(200);
  tft.fillRect(TFT_HEIGHT - LAYOUT_BUTTON_WIDTH, TFT_WIDTH / 2,
               LAYOUT_BUTTON_WIDTH, TFT_WIDTH - (TFT_WIDTH / 2), 
               COLOR_BG_BOTTOM_BTN);
  
  btnBClicked = false;
}

void getNetworkTime() {
//***************************************************************************************************
//  get network time according to time zone, needs wifi connection
//***************************************************************************************************
  waitForSync();                    // Wait for ezTime to get its time synchronized
  myTZ.setLocation(F(timezone));
  setInterval(uint16_t(86400));      // 86400 = 60 * 60 * 24, set NTP polling interval to daily
  setDebug(NONE);                   // NONE = set ezTime to quiet
}

void showTime() {
//***************************************************************************************************
//  left aligned on info bar at the top
//***************************************************************************************************
  int xpos = 8;  // left margin
  int ypos = 8;   // top margin

  tft.setTextColor(COLOR_FG_INFO_BAR, COLOR_BG_INFO_BAR);
  tft.setTextDatum(TL_DATUM);
  
  tft.setTextFont(0);
  tft.drawString(String(myTZ.dateTime("H:i")), xpos, ypos, GFXFF);  
}

void showSystemNumber() {
//***************************************************************************************************
//  taken from settings, to distinguish more than one plant nanny, used in mqtt messages
//***************************************************************************************************
  const int posFromRight = 5;
  const int radius = 9;
  
  int xpos = TFT_HEIGHT - LAYOUT_BUTTON_WIDTH - (posFromRight * LAYOUT_INFO_BAR_HEIGHT) + radius - LAYOUT_X_POS_ADJUST;
  int ypos = LAYOUT_INFO_BAR_HEIGHT / 2;

  tft.fillCircle(xpos, ypos, radius, COLOR_CIRCLE);
  tft.setTextColor(COLOR_FG_INFO_BAR, COLOR_CIRCLE);
  tft.setTextDatum(MC_DATUM);
  
  tft.setTextFont(0);
  tft.drawString(String(NANNY_NUMBER), xpos, ypos, GFXFF);  
}

void connectAndShowWifiStatus() {
//***************************************************************************************************
//  stacked to the right of the info bar at the top
//***************************************************************************************************
  const int posFromRight = 4;

  int xpos = TFT_HEIGHT - LAYOUT_BUTTON_WIDTH - (posFromRight * LAYOUT_INFO_BAR_HEIGHT) - LAYOUT_X_POS_ADJUST;
  int ypos = (LAYOUT_INFO_BAR_HEIGHT - iconHeight) / 2;

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

void connectAndShowMQTTStatus() {
//***************************************************************************************************
//  stacked to the right on info bar at the top
//***************************************************************************************************
  const int posFromRight = 3;

  int xpos = TFT_HEIGHT - LAYOUT_BUTTON_WIDTH - (posFromRight * LAYOUT_INFO_BAR_HEIGHT) - LAYOUT_X_POS_ADJUST;
  int ypos = (LAYOUT_INFO_BAR_HEIGHT - iconHeight) / 2;

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

void showAndPublishBatteryVoltage() {
//***************************************************************************************************
//  
//***************************************************************************************************
  const int posFromRight = 2;

  int xpos = TFT_HEIGHT - LAYOUT_BUTTON_WIDTH - (posFromRight * LAYOUT_INFO_BAR_HEIGHT) - LAYOUT_X_POS_ADJUST;
  int ypos = (LAYOUT_INFO_BAR_HEIGHT - iconHeight) / 2;

  uint16_t color;

  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize((adc_unit_t)ADC_UNIT_1, 
                                                          (adc_atten_t)ADC1_CHANNEL_6, 
                                                          (adc_bits_width_t)ADC_WIDTH_BIT_12, 
                                                          ADC_VREF, 
                                                          &adc_chars);

  uint16_t v = analogRead(ADC_PIN);
  float batteryVoltage = ((float)v / 4095.0) * 2.0 * 3.3 * (ADC_VREF / 1100.0);

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

void showAndPublishWaterLevel() {
//***************************************************************************************************
//  
//***************************************************************************************************
  const int posFromRight = 1;

  int xpos = TFT_HEIGHT - LAYOUT_BUTTON_WIDTH - (posFromRight * LAYOUT_INFO_BAR_HEIGHT) - LAYOUT_X_POS_ADJUST;
  int ypos = (LAYOUT_INFO_BAR_HEIGHT - iconHeight) / 2;

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
  tft.drawXBitmap(xpos, ypos, iconContainer, iconWidth, iconHeight, color, COLOR_BG_INFO_BAR);
  
  mqttPublishValue(mqttTopicWaterLevel, String(remainingWater));
}

void showContainerSize() {
//***************************************************************************************************
//  
//***************************************************************************************************
  const int posFromRight = 0;

  int xpos = TFT_HEIGHT - LAYOUT_BUTTON_WIDTH - (posFromRight * LAYOUT_INFO_BAR_HEIGHT) - LAYOUT_X_POS_ADJUST;
  int ypos = ((LAYOUT_INFO_BAR_HEIGHT - iconHeight) / 2) + 8;

  String containerChar;

  if(containerSize == CONTAINER_SIZE_SMALL){
    containerChar = "S";
  } else if (containerSize == CONTAINER_SIZE_TALL){
    containerChar ="T";
  } else if (containerSize == CONTAINER_SIZE_FLAT){
    containerChar ="F";
  } else {
    containerChar ="B";
  }
  
  tft.setTextColor(COLOR_FG_INFO_BAR, COLOR_BG_INFO_BAR);
  tft.setTextFont(0);
  tft.drawString(containerChar, xpos, ypos, FONT2);  
}

void mqttSubscribeToTopics() {
//***************************************************************************************************
//  to avoid feedback loops we subscribe to command messages and send status messages
//***************************************************************************************************
  String baseCommand;
  String pumpCommand;
  String temp;
  String temp1;
  String temp2;

  baseCommand.concat(mqttMainTopic);
  baseCommand.concat("/");
  baseCommand.concat(NANNY_NUMBER);
  baseCommand.concat("/");
  pumpCommand = baseCommand;
  baseCommand.concat(mqttCommandHour);
  mqttClient.subscribe(baseCommand.c_str());
  Serial.print("MQTT subscribed to: ");
  Serial.println(baseCommand.c_str());

  for(int i = 1; i <= NUMBER_OF_PUMPS; i++) {
    temp = pumpCommand;
    temp.concat(char(48 + i));    // 48 = ASCII '0'
    temp.concat("/");
    temp1 =temp;
    temp1.concat(mqttCommandFreq);
    mqttClient.subscribe(temp1.c_str());
    Serial.print("MQTT subscribed to: ");
    Serial.println(temp1.c_str());
    temp2 = temp;
    temp2.concat(mqttCommandAmount);
    mqttClient.subscribe(temp2.c_str());
    Serial.print("MQTT subscribed to: ");
    Serial.println(temp2.c_str());
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
//***************************************************************************************************
//  there are general and pump specific commands
//***************************************************************************************************
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

  Serial.print("MQTT callback: ");
  Serial.print(topicString);
  Serial.print(" = ");
  Serial.println(stringValue);

  if(shortenedTopic == mqttCommandHour) {
    prefs.begin("nanny", false);
    prefs.putUInt(PREF_WATERING_HOUR, stringValue.toInt());
    prefs.end();  
  } else {
    pump = shortenedTopic.toInt();    // returns 0 if no number is found
    if(pump <= 0 || pump > NUMBER_OF_PUMPS) {
      Serial.print("MQTT callback: command not recognized = ");
      Serial.println(shortenedTopic);
    } else {
      shortenedPumpTopic = shortenedTopic.substring(2);   // single digit pump number and '/'
      if(shortenedPumpTopic == mqttCommandFreq) {
        prefs.begin("nanny", false);
        switch(pump) {
          case 1:
            prefs.putUInt(PREF_P1_WATERING_FREQ, stringValue.toInt());        
            break;
          case 2:
            prefs.putUInt(PREF_P2_WATERING_FREQ, stringValue.toInt());        
            break;
          case 3:
            prefs.putUInt(PREF_P3_WATERING_FREQ, stringValue.toInt());        
            break;
          case 4:
            prefs.putUInt(PREF_P4_WATERING_FREQ, stringValue.toInt());        
            break;
        }
        prefs.end();  
      } else if(shortenedPumpTopic == mqttCommandAmount){
        prefs.begin("nanny", false);
        switch(pump) {
          case 1:
            prefs.putUInt(PREF_P1_WATERING_AMOUNT, stringValue.toInt());        
            break;
          case 2:
            prefs.putUInt(PREF_P2_WATERING_AMOUNT, stringValue.toInt());        
            break;
          case 3:
            prefs.putUInt(PREF_P3_WATERING_AMOUNT, stringValue.toInt());        
            break;
          case 4:
            prefs.putUInt(PREF_P4_WATERING_AMOUNT, stringValue.toInt());        
            break;
        }
        prefs.end();  
      } else {
        Serial.print("MQTT callback: pump ");
        Serial.print(pump);
        Serial.print(", command not recognized = ");
        Serial.println(shortenedPumpTopic);        
      }
    }
  }
}

void mqttReconnect() {
//***************************************************************************************************
//  
//***************************************************************************************************
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

void mqttPublishValue(String topic, String value) {
//***************************************************************************************************
//  
//***************************************************************************************************
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

void buttonsInit() {
//***************************************************************************************************
//  
//***************************************************************************************************
  btnT.setPressedHandler([](Button2 & b) {
    Serial.println("Top button clicked");
    btnTClicked = true;
  });

  btnB.setPressedHandler([](Button2 & b) {
    Serial.println("Bottom button clicked");
    btnBClicked = true;
  });  
}

void setTimerAndGoToSleep() {
//***************************************************************************************************
//  set alarm clock to the next full hour (if no internet connection exists then just 1 hour)
//***************************************************************************************************
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
  // wakeup on timer
  esp_sleep_enable_timer_wakeup((sleepingSeconds + TIMER_DRIFT_COMPENSATION) * uSToSecondsFactor);
  // also wakeup on top button
  esp_sleep_enable_ext1_wakeup(GPIO_SEL_35, ESP_EXT1_WAKEUP_ALL_LOW);
  esp_deep_sleep_start();
}
