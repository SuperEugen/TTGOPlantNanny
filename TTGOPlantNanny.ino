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
//                            TTGO T-Display exmple, epaper-weatherstation (SQUIX), 
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
//    05.07.2022, IH:         changed handling of start time
//    06.07.2022, IH:         added button icon for refill
//    14.07.2022, IH:         added more icons and screens
//    15.07.2022, IH:         some refactoring
//    17.07.2022, IH:         more clean-up
//    11.08.2022, IH:         calibration 
//    13.08.2022, IH:         menu system
//    22.08.2022, IH:         updated settings.h
//    28.08.2022, IH:         updated container size and empty container calc and water now button
//
//***************************************************************************************************

#define VERSION               "0.18"   // 28.08.22

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
int16_t containerSizeCat;                   // category 1, 2, 3, 4
int16_t remainingWaterML;                   // in milliliter
int16_t wateringFreqCat[NUMBER_OF_PUMPS];   // category 0, 1, 2, 3, 4
int16_t nextWateringH[NUMBER_OF_PUMPS];     // in hours
int16_t wateringAmountCat[NUMBER_OF_PUMPS]; // category 1, 2, 3, 4

//***************************************************************************************************
//  Screens
//***************************************************************************************************
const int scrMain =         1;
const int scrRefillYN =     2;
const int scrWaterNowYN =   3;

int currentScreen;
int currentPump;
int currentPAction;
int currentPTime;
int currentPAmount;
int currentCSize;

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
  // info bar
  tft.fillRect(0, 0, 
               LAYOUT_LANDSCAPE_WIDTH - LAYOUT_BUTTON_WIDTH,  LAYOUT_INFO_BAR_HEIGHT, 
               COLOR_BG_INFO_BAR);
  // main area
  clearMainArea();
  // status bar
  tft.fillRect(0, LAYOUT_LANDSCAPE_HEIGHT - LAYOUT_STATUS_BAR_HEIGHT, 
               LAYOUT_LANDSCAPE_WIDTH - LAYOUT_BUTTON_WIDTH,  LAYOUT_STATUS_BAR_HEIGHT, 
               COLOR_BG_STATUS_BAR);
  // top button
  clearTButton();
  // bottom button
  clearBButton();
  // infos on info bar
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
    drawButtonIcons();
    currentPump = 1;
    currentPAction = 1;
    currentPTime = 1;
    currentPAmount = 1;
    currentCSize = 1;
    
    currentPump = 1;
    drawMainArea();
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
  if(minuteChanged() && wifiConnected) {
    showTime();
  }

  if(!mqttClient.connected()) {
    mqttReconnect();
  }
  mqttClient.loop();

  // is it full hour?
  if(minute() == 0) {
    if(remainingWaterML <= 0) {
      Serial.println("Water tank empty");
    } else {
      for(int i = 0; i < NUMBER_OF_PUMPS; i++) {
        nextWateringH[i] -= 1;
      }
      waterIfDue();
    }
    setTimerAndGoToSleep();
  }

  btnT.loop();
  btnB.loop();

  if(btnTClicked || btnBClicked) {
    timeStamp = millis();
    drawButtonIcons();
    calcNextScreen();
    btnTClicked = false;
    btnBClicked = false;
    delay(200);
    drawButtonIcons();
    drawMainArea();
  }
  
  mqttClient.loop();

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
  containerSizeCat = prefs.getUInt(PREF_CONTAINER_SIZE, DEFAULT_CONTAINER_SIZE);
  Serial.print("Load from prefs: containerSize      = ");
  Serial.println(String(containerSizeCat));
  remainingWaterML = prefs.getUInt(PREF_REMAINING_WATER, DEFAULT_CONTAINER_SIZE);
  Serial.print("Load from prefs: remainingWater     = ");
  Serial.println(String(remainingWaterML));
  // pump 1
  wateringFreqCat[0] = prefs.getUInt(PREF_P1_WATERING_FREQ, DEFAULT_WATERING_FREQ);
  Serial.print("Load from prefs: P1, wateringFreq   = ");
  Serial.println(String(wateringFreqCat[0]));
  nextWateringH[0] = prefs.getUInt(PREF_P1_NEXT_WATERING, DEFAULT_WATERING_FREQ);
  Serial.print("Load from prefs: P1, nextWatering   = ");
  Serial.println(String(nextWateringH[0]));
  wateringAmountCat[0] = prefs.getUInt(PREF_P1_WATERING_AMOUNT, DEFAULT_WATERING_AMOUNT);
  Serial.print("Load from prefs: P1, wateringAmount = ");
  Serial.println(String(wateringAmountCat[0]));
  // pump 2
  wateringFreqCat[1] = prefs.getUInt(PREF_P2_WATERING_FREQ, DEFAULT_WATERING_FREQ);
  Serial.print("Load from prefs: P2, wateringFreq   = ");
  Serial.println(String(wateringFreqCat[1]));
  nextWateringH[1] = prefs.getUInt(PREF_P2_NEXT_WATERING, DEFAULT_WATERING_FREQ);
  Serial.print("Load from prefs: P2, nextWatering   = ");
  Serial.println(String(nextWateringH[1]));
  wateringAmountCat[1] = prefs.getUInt(PREF_P2_WATERING_AMOUNT, DEFAULT_WATERING_AMOUNT);
  Serial.print("Load from prefs: P2, wateringAmount = ");
  Serial.println(String(wateringAmountCat[1]));
  // pump 3
  wateringFreqCat[2] = prefs.getUInt(PREF_P3_WATERING_FREQ, DEFAULT_WATERING_FREQ);
  Serial.print("Load from prefs: P3, wateringFreq   = ");
  Serial.println(String(wateringFreqCat[2]));
  nextWateringH[2] = prefs.getUInt(PREF_P3_NEXT_WATERING, DEFAULT_WATERING_FREQ);
  Serial.print("Load from prefs: P3, nextWatering   = ");
  Serial.println(String(nextWateringH[2]));
  wateringAmountCat[2] = prefs.getUInt(PREF_P3_WATERING_AMOUNT, DEFAULT_WATERING_AMOUNT);
  Serial.print("Load from prefs: P3, wateringAmount = ");
  Serial.println(String(wateringAmountCat[2]));
  // pump 4
  wateringFreqCat[3] = prefs.getUInt(PREF_P4_WATERING_FREQ, DEFAULT_WATERING_FREQ);
  Serial.print("Load from prefs: P4, wateringFreq   = ");
  Serial.println(String(wateringFreqCat[3]));
  nextWateringH[3] = prefs.getUInt(PREF_P4_NEXT_WATERING, DEFAULT_WATERING_FREQ);
  Serial.print("Load from prefs: P4, nextWatering   = ");
  Serial.println(String(nextWateringH[3]));
  wateringAmountCat[3] = prefs.getUInt(PREF_P4_WATERING_AMOUNT, DEFAULT_WATERING_AMOUNT);
  Serial.print("Load from prefs: P4, wateringAmount = ");
  Serial.println(String(wateringAmountCat[3]));
  prefs.end();
}

int translateContainerSize(int category) {
  int value;
  
  switch(category) {
    case 1:
      value = CONTAINER_SIZE_SMALL;
      break;
    case 2:
      value = CONTAINER_SIZE_TALL;
      break;
    case 3:
      value = CONTAINER_SIZE_FLAT;
      break;
    default:
      value = CONTAINER_SIZE_BIG;
      break;
  }
  return(value);
}

int translateWateringFreq(int category) {
  int value;
  
  switch(category) {
    case 0:
      value = WATERING_FREQ_OFF;
      break;
    case 1:
      value = WATERING_FREQ_VERY_SELDOM;
      break;
    case 2:
      value = WATERING_FREQ_SELDOM;
      break;
    case 3:
      value = WATERING_FREQ_NORMAL;
      break;
    default:
      value = WATERING_FREQ_OFTEN;
      break;
  }
  return(value);
}

int translateWateringAmount(int category) {
  int value;
  
  switch(category) {
    case 1:
      value = PUMP_25ML;
      break;
    case 2:
      value = PUMP_50ML;
      break;
    case 3:
      value = PUMP_75ML;
      break;
    default:
      value = PUMP_100ML;
      break;
  }
  return(value);
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
  ypos = LAYOUT_LANDSCAPE_HEIGHT - LAYOUT_STATUS_BAR_HEIGHT + 2;
  strcpy(temp, TEXT_VERSION);
  strcat(temp, " ");
  strcat(temp, VERSION);
  tft.setTextFont(0);
  tft.drawString(temp, xpos, ypos, GFXFF);
}

void clearMainArea() {
//***************************************************************************************************
//  
//***************************************************************************************************
  tft.fillRect(0, LAYOUT_INFO_BAR_HEIGHT, 
               LAYOUT_LANDSCAPE_WIDTH - LAYOUT_BUTTON_WIDTH, LAYOUT_LANDSCAPE_HEIGHT - LAYOUT_INFO_BAR_HEIGHT - LAYOUT_STATUS_BAR_HEIGHT, 
               COLOR_BG_MAIN_AREA);
}

void clearTButton() {
//***************************************************************************************************
//  
//***************************************************************************************************
  tft.fillRect(LAYOUT_LANDSCAPE_WIDTH - LAYOUT_BUTTON_WIDTH, 0,
               LAYOUT_BUTTON_WIDTH, LAYOUT_LANDSCAPE_HEIGHT / 2, 
               COLOR_BG_TOP_BTN);
}

void clearBButton() {
//***************************************************************************************************
//  
//***************************************************************************************************
  tft.fillRect(LAYOUT_LANDSCAPE_WIDTH - LAYOUT_BUTTON_WIDTH, LAYOUT_LANDSCAPE_HEIGHT / 2,
               LAYOUT_BUTTON_WIDTH, LAYOUT_LANDSCAPE_HEIGHT - (LAYOUT_LANDSCAPE_HEIGHT / 2), 
               COLOR_BG_BOTTOM_BTN);
}

void drawButtonIcons() {
//***************************************************************************************************
//  icon and color depending on clicked status and current screen
//***************************************************************************************************
  int xpos = LAYOUT_LANDSCAPE_WIDTH - LAYOUT_BUTTON_WIDTH + ((LAYOUT_BUTTON_WIDTH - iconWidthBig) / 2);
  int ypos;
  uint16_t fgcolor;

  clearTButton();
  clearBButton();
  if(currentScreen == scrMain) {
    // iconRefill and iconConfig
    if(btnTClicked) {
      fgcolor = COLOR_BUTTON_PRESSED;
    } else {
      fgcolor = TFT_SKYBLUE;
    }
    ypos = ((LAYOUT_LANDSCAPE_HEIGHT / 2) - iconHeightBig) / 2;
    tft.drawXBitmap(xpos, ypos, iconRefill, iconWidthBig, iconHeightBig, fgcolor);
    if(btnBClicked) {
      fgcolor = COLOR_BUTTON_PRESSED;
    } else {
      fgcolor = TFT_LIGHTGREY;
    }
    ypos = (((LAYOUT_LANDSCAPE_HEIGHT / 2) - iconHeightBig) / 2) + (LAYOUT_LANDSCAPE_HEIGHT / 2);
    tft.drawXBitmap(xpos, ypos, iconConfig, iconWidthBig, iconHeightBig, fgcolor);
  } else {
    // iconOK and iconNext
    if(btnTClicked) {
      fgcolor = COLOR_BUTTON_PRESSED;
    } else {
      fgcolor = TFT_GREEN;
    }
    ypos = ((LAYOUT_LANDSCAPE_HEIGHT / 2) - iconHeightBig) / 2;
    tft.drawXBitmap(xpos, ypos, iconOK, iconWidthBig, iconHeightBig, fgcolor);
    if(btnBClicked) {
      fgcolor = COLOR_BUTTON_PRESSED;
    } else {
      fgcolor = TFT_MAGENTA;
    }
    ypos = (((LAYOUT_LANDSCAPE_HEIGHT / 2) - iconHeightBig) / 2) + (LAYOUT_LANDSCAPE_HEIGHT / 2);
    tft.drawXBitmap(xpos, ypos, iconNext, iconWidthBig, iconHeightBig, fgcolor);
  }
}

void calcNextScreen() {
//***************************************************************************************************
//  depending on current screen and button clicked
//***************************************************************************************************
  switch(currentScreen) {
    case scrMain:
      if(btnTClicked) {
        currentScreen = scrRefillYN;
      }
      if(btnBClicked) {
        currentScreen = scrWaterNowYN;
      }
      break;
    case scrRefillYN:
      if(btnTClicked) {
        remainingWaterML = translateContainerSize(containerSizeCat);
        showAndPublishWaterLevel();
        currentScreen = scrMain;
      }
      if(btnBClicked) {
        currentScreen = scrMain;
      }
      break;
    case scrWaterNowYN:
      if(btnTClicked) {
        // water now all plants with set amount, reset next watering to new cycle for all pumps
        for(int i = 0; i < NUMBER_OF_PUMPS; i++) {
          nextWateringH[i] = 0;
        }
        waterIfDue();
        currentScreen = scrMain;
      }
      if(btnBClicked) {
        currentScreen = scrMain;
      }
      break;
  }
}

void drawMainArea() {
//***************************************************************************************************
//  show graphics and texts depending on current screen
//***************************************************************************************************
  int xpos = 10;  // left margin
  int ypos = (LAYOUT_LANDSCAPE_HEIGHT - LAYOUT_BUTTON_WIDTH - LAYOUT_STATUS_BAR_HEIGHT) / 2 ;  // top margin
  uint16_t color;
  char temp[30];  // for string concatination
  int16_t simRemainingWaterML;
  int simHours;
  uint16_t simNextWateringH[NUMBER_OF_PUMPS];
  int i;
  int simDays;
  String tempString;

  clearMainArea();
  tft.setTextColor(COLOR_FG_MAIN_AREA, COLOR_BG_MAIN_AREA);
  tft.setTextDatum(TL_DATUM);
  tft.setFreeFont(FSS9);

  switch(currentScreen) {
    case scrMain:
      // simulate watering
      simRemainingWaterML = remainingWaterML;
      simHours = 0;
      for(i=0; i < NUMBER_OF_PUMPS; i++) {
        simNextWateringH[i] = nextWateringH[i];
      }
      while(simRemainingWaterML > 0) {
        for(i = 0; i < NUMBER_OF_PUMPS; i++) {
          // recalc simNextWatering
          simNextWateringH[i] -= 1;
          // check if pump is due
          if(simNextWateringH[i] <= 0) {
            switch(wateringAmountCat[i]) {
              case 1:
                simRemainingWaterML -= 25;
                break;
              case 2:
                simRemainingWaterML -= 50;
                break;
              case 3:
                simRemainingWaterML -= 75;
                break;
              default:
                simRemainingWaterML -= 100;
                break;
            }
            // adjust simNextWatering
            simNextWateringH[i] = translateWateringFreq(wateringFreqCat[i]);            
          }
        }
        simHours += 1;
      }
      tft.drawString(TEXT_REMAINING, xpos, ypos, GFXFF);
      simDays = simHours / 24;
      tempString = String(simDays);
      if(simDays != 1) {
        tempString = TEXT_DAYS;
      } else {
        tempString = TEXT_DAY;
      }
      // draw main area
      tft.drawString(String(simDays) + " " + tempString, xpos, ypos + 20, GFXFF);
      break;
    case scrRefillYN:
      // draw main area
      tft.drawString("RefillYN", xpos, ypos + 20, GFXFF);
      break;
    case scrWaterNowYN:
      // draw main area
      tft.drawString("WaterNowYN", xpos, ypos + 20, GFXFF);
      break;
  }
}

void waterIfDue() {
//***************************************************************************************************
//  update PREF_PX_NEXT_WATERING for each pump and check if it's watering time and if any pump is due
//***************************************************************************************************
  for(int i = 0; i < NUMBER_OF_PUMPS; i++) {
    if(nextWateringH[i] <= 0) {
      // set pump on
      switch(i) {
        case 0:
          digitalWrite(PUMP_1, LOW);
          break;
        case 1:
          digitalWrite(PUMP_2, LOW);
          break;
        case 2:
          digitalWrite(PUMP_3, LOW);
          break;
        case 3:
          digitalWrite(PUMP_4, LOW);
          break;
      }
      // wait till amount is reached
      delay(translateWateringAmount(wateringAmountCat[i]));
      // set pump off and write pref
      switch(i) {
        case 0:
          digitalWrite(PUMP_1, HIGH);
          break;
        case 1:
          digitalWrite(PUMP_2, HIGH);
          break;
        case 2:
          digitalWrite(PUMP_3, HIGH);
          break;
        case 3:
          digitalWrite(PUMP_4, HIGH);
          break;
      }
      // adjust remainingWater
      switch(wateringAmountCat[i]) {
        case 1:
          remainingWaterML -= 25;
          break;
        case 2:
          remainingWaterML -= 50;
          break;
        case 3:
          remainingWaterML -= 75;
          break;
        default:
          remainingWaterML -= 100;
          break;
      }
      // adjust nextWatering
      nextWateringH[i] = translateWateringFreq(wateringFreqCat[i]);
    }
    // wait a little between pumps
    delay(100);
  }
  showAndPublishWaterLevel();;  
  prefs.begin("nanny", false);
  prefs.putUInt(PREF_P1_NEXT_WATERING, nextWateringH[0]);
  prefs.putUInt(PREF_P2_NEXT_WATERING, nextWateringH[1]);
  prefs.putUInt(PREF_P3_NEXT_WATERING, nextWateringH[2]);
  prefs.putUInt(PREF_P4_NEXT_WATERING, nextWateringH[3]);
  prefs.end();
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
  
  int xpos = LAYOUT_LANDSCAPE_WIDTH - LAYOUT_BUTTON_WIDTH - (posFromRight * LAYOUT_INFO_BAR_HEIGHT) + radius - LAYOUT_X_POS_ADJUST;
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

  int xpos = LAYOUT_LANDSCAPE_WIDTH - LAYOUT_BUTTON_WIDTH - (posFromRight * LAYOUT_INFO_BAR_HEIGHT) - LAYOUT_X_POS_ADJUST;
  int ypos = (LAYOUT_INFO_BAR_HEIGHT - iconHeightSmall) / 2;

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
    wifiConnected = true;
    color = TFT_GREEN;
  } else {
    Serial.println(F("WiFi not connected"));
    wifiConnected = false;
    color = TFT_RED;
  }
  
  tft.drawXBitmap(xpos, ypos, iconWifi, iconWidthSmall, iconHeightSmall, color, COLOR_BG_INFO_BAR);    
}

void connectAndShowMQTTStatus() {
//***************************************************************************************************
//  stacked to the right on info bar at the top
//***************************************************************************************************
  const int posFromRight = 3;

  int xpos = LAYOUT_LANDSCAPE_WIDTH - LAYOUT_BUTTON_WIDTH - (posFromRight * LAYOUT_INFO_BAR_HEIGHT) - LAYOUT_X_POS_ADJUST;
  int ypos = (LAYOUT_INFO_BAR_HEIGHT - iconHeightSmall) / 2;

  uint16_t color;

  mqttClient.setServer(SECRET_MQTT_BROKER, SECRET_MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  if (mqttClient.connect(mqttClientID + NANNY_NUMBER, SECRET_MQTT_USER, SECRET_MQTT_PASSWORD)) {
    mqttConnected = true;
    color = TFT_GREEN;
    mqttSubscribeToTopics();
  } else {
    Serial.println(F("MQTT not connected"));
    mqttConnected = false;
    color = TFT_RED;
  }

  tft.drawXBitmap(xpos, ypos, iconMQTT, iconWidthSmall, iconHeightSmall, color, COLOR_BG_INFO_BAR);
}

void showAndPublishBatteryVoltage() {
//***************************************************************************************************
//  
//***************************************************************************************************
  const int posFromRight = 2;

  int xpos = LAYOUT_LANDSCAPE_WIDTH - LAYOUT_BUTTON_WIDTH - (posFromRight * LAYOUT_INFO_BAR_HEIGHT) - LAYOUT_X_POS_ADJUST;
  int ypos = (LAYOUT_INFO_BAR_HEIGHT - iconHeightSmall) / 2;

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
  
  tft.drawXBitmap(xpos, ypos, iconBattery, iconWidthSmall, iconHeightSmall, color, COLOR_BG_INFO_BAR);

  mqttPublishValue(mqttTopicBatVoltage, String(batteryVoltage));
}

void showAndPublishWaterLevel() {
//***************************************************************************************************
//  
//***************************************************************************************************
  const int posFromRight = 1;

  int xpos = LAYOUT_LANDSCAPE_WIDTH - LAYOUT_BUTTON_WIDTH - (posFromRight * LAYOUT_INFO_BAR_HEIGHT) - LAYOUT_X_POS_ADJUST;
  int ypos = (LAYOUT_INFO_BAR_HEIGHT - iconHeightSmall) / 2;

  uint16_t color;

  int percent;

  percent = remainingWaterML / translateContainerSize(containerSizeCat) * 100;
  if(percent < 10) {
    color = TFT_RED;
  } else if(percent < 30) {
    color = TFT_YELLOW;
  } else {
    color = TFT_GREEN;
  }
  tft.drawXBitmap(xpos, ypos, iconContainer, iconWidthSmall, iconHeightSmall, color, COLOR_BG_INFO_BAR);
  
  mqttPublishValue(mqttTopicWaterLevel, String(remainingWaterML));

  prefs.begin("nanny", false);
  prefs.putUInt(PREF_REMAINING_WATER, remainingWaterML);
  prefs.end();  
}

void showContainerSize() {
//***************************************************************************************************
//  
//***************************************************************************************************
  const int posFromRight = 0;

  int xpos = LAYOUT_LANDSCAPE_WIDTH - LAYOUT_BUTTON_WIDTH - (posFromRight * LAYOUT_INFO_BAR_HEIGHT) - LAYOUT_X_POS_ADJUST;
  int ypos = ((LAYOUT_INFO_BAR_HEIGHT - iconHeightSmall) / 2) + 8;

  String containerChar;

  switch(containerSizeCat) {
    case 1:
      containerChar = "S";
      break;
    case 2:
      containerChar = "T";
      break;
    case 3:
      containerChar = "F";
      break;
    default:
      containerChar = "B";
      break;
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
  String temp1;
  String temp2;

  baseCommand.concat(mqttMainTopic);
  baseCommand.concat("/");
  baseCommand.concat(NANNY_NUMBER);
  baseCommand.concat("/");

  temp1 = baseCommand;
  temp1.concat(mqttCmndContainer);
  mqttClient.subscribe(temp1.c_str());
  temp1 = baseCommand;
  temp1.concat(mqttCmndWater);
  mqttClient.subscribe(temp1.c_str());
  
  for(int i = 1; i <= NUMBER_OF_PUMPS; i++) {
    temp1 = baseCommand;
    temp1.concat(char(48 + i));    // 48 = ASCII '0'
    temp1.concat("/");
    temp2 =temp1;
    temp2.concat(mqttCmndFreq);
    mqttClient.subscribe(temp2.c_str());
    temp2 = temp1;
    temp2.concat(mqttCmndNext);
    mqttClient.subscribe(temp2.c_str());
    temp2 = temp1;
    temp2.concat(mqttCmndAmount);
    mqttClient.subscribe(temp2.c_str());
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
  int intValue;
  
  topicPrefix = mqttMainTopic;
  topicPrefix.concat("/");
  topicPrefix.concat(NANNY_NUMBER);
  topicPrefix.concat("/");
  topicPrefixLength = topicPrefix.length(); 
  topicString = topic;
  shortenedTopic = topicString.substring(topicPrefixLength);

  payload[length] = '\0';
  String stringValue = String((char*) payload);
  intValue = stringValue.toInt();

  Serial.print("MQTT callback:   ");
  Serial.print(topicString);
  Serial.print(" = ");
  Serial.println(stringValue);

  if(shortenedTopic == mqttCmndContainer) {
    prefs.begin("nanny", false);
    prefs.putUInt(PREF_CONTAINER_SIZE, intValue);
    prefs.end();
    containerSizeCat = intValue;
  } else {
    if(shortenedTopic == mqttCmndWater) {
      prefs.begin("nanny", false);
      prefs.putUInt(PREF_REMAINING_WATER, intValue);
      prefs.end();
      remainingWaterML = intValue;
    } else {
      pump = shortenedTopic.toInt();    // returns 0 if no number is found
      if(pump <= 0 || pump > NUMBER_OF_PUMPS) {
        Serial.print("MQTT callback:   command not recognized = ");
        Serial.println(shortenedTopic);
      } else {
        shortenedPumpTopic = shortenedTopic.substring(2);   // single digit pump number and '/'
        if(shortenedPumpTopic == mqttCmndFreq) {
          prefs.begin("nanny", false);
          switch(pump) {
            case 1:
              prefs.putUInt(PREF_P1_WATERING_FREQ, intValue);
              break;
            case 2:
              prefs.putUInt(PREF_P2_WATERING_FREQ, intValue);        
              break;
            case 3:
              prefs.putUInt(PREF_P3_WATERING_FREQ, intValue);        
              break;
            case 4:
              prefs.putUInt(PREF_P4_WATERING_FREQ, intValue);        
              break;
          }
          prefs.end();
          wateringFreqCat[pump] = intValue;
        } else if(shortenedPumpTopic == mqttCmndNext) {
          prefs.begin("nanny", false);
          switch(pump) {
            case 1:
              prefs.putUInt(PREF_P1_NEXT_WATERING, intValue);        
              break;
            case 2:
              prefs.putUInt(PREF_P2_NEXT_WATERING, intValue);        
              break;
            case 3:
              prefs.putUInt(PREF_P3_NEXT_WATERING, intValue);        
              break;
            case 4:
              prefs.putUInt(PREF_P4_NEXT_WATERING, intValue);        
              break;
          }
          prefs.end();
          nextWateringH[pump] = intValue;
        } else if(shortenedPumpTopic == mqttCmndAmount) {
          prefs.begin("nanny", false);
          switch(pump) {
            case 1:
              prefs.putUInt(PREF_P1_WATERING_AMOUNT, intValue);        
              break;
            case 2:
              prefs.putUInt(PREF_P2_WATERING_AMOUNT, intValue);        
              break;
            case 3:
              prefs.putUInt(PREF_P3_WATERING_AMOUNT, intValue);        
              break;
            case 4:
              prefs.putUInt(PREF_P4_WATERING_AMOUNT, intValue);        
              break;
          }
          prefs.end();
          wateringAmountCat[pump] = intValue;
        } else {
          Serial.print("MQTT callback:   pump ");
          Serial.print(pump);
          Serial.print(", command not recognized = ");
          Serial.println(shortenedPumpTopic);        
        }
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
    btnTClicked = true;
  });

  btnB.setPressedHandler([](Button2 & b) {
    btnBClicked = true;
  });  
}

void setTimerAndGoToSleep() {
//***************************************************************************************************
//  set alarm clock to the next full hour (if no internet connection exists then just 1 hour)
//***************************************************************************************************
  unsigned long sleepingSeconds;

  if(wifiConnected) {
    sleepingSeconds = (59 - minute()) * 60 + (59 - second());
    sleepingSeconds += TIMER_DRIFT_COMPENSATION;
    Serial.print("going to sleep for ");
    Serial.print(sleepingSeconds);
    Serial.println(" seconds");
  } else {
    sleepingSeconds = 60 * 60;
    Serial.println("going to sleep for one hour");
  }

  tft.writecommand(TFT_DISPOFF);
  tft.writecommand(TFT_SLPIN);
  int err = esp_wifi_stop();
  // wakeup on timer
  esp_sleep_enable_timer_wakeup((sleepingSeconds) * 1000000ul);
  // also wakeup on top button
  esp_sleep_enable_ext1_wakeup(GPIO_SEL_35, ESP_EXT1_WAKEUP_ALL_LOW);
  esp_deep_sleep_start();
}
