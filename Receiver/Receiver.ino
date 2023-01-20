/*
 * Project: Receiver (Ding9)
 * Description:
 *   - receives sensor data by 433MHz ASK or LoRa signals
 *   - saves them to local csv files 
 *   - forwards some sensor data to Blynk
 *   - displays some sensor data on a touch display 
 *   - provides a webserver to show sensor data in a browser 
 *
 * License: 2-Clause BSD License
 * Copyright (c) 2023 codingABI
 * For details see: License.txt
 * 
 * External code parts of TFT_eSPI: 
 *   Copyright (c) 2022 Bodmer (https://github.com/Bodmer), FreeBSD License
 *   Copyright Limor Fried/Ladyada for Adafruit Industries, The MIT License (MIT)
 *   Copyright (c) 2012 Adafruit Industries, BSD License
 *   For details see externalCode.ino and License TFT-eSPI.txt
 *
 * Hardware: 
 * - ESP-WROOM-32 NodeMCU (Board manager: ESP32 Dev Model, Baud 115200)
 * - ILI9341 with XPT2046-Touch
 * - PIR sensor AM312 to wakeup display from screensaver
 * - Passive buzzer
 * - RXB6 433MHz receiver (At the beginning I used a MX-05v, but this receiver was too bad)
 * - BME280 sensor for pressure, temperature and humidity
 * - Lora SX1278 Ra-02
 * 
 * Beep codes (short = 1 kHz for 100ms & 100ms quiet time, default = 500 Hz for 200ms & 100ms quiet time, long = 250 Hz for 400ms & 100ms quiet time):
 *   default, short, default = LittleFS semaphore error
 *   short, default, default = LittleFS mount error
 *   default, default, default = SPI semaphore error
 *   1x short = Test sensor or a touch event 
 *   1x default = After device reset (in setup function)
 *   1x long = When an error message is displayed
 *   
 * History: 
 * 20230120, Initial public version
 */

#include "secrets.h"

#define DEBUG false  // true for Serial.print
#define SERIALDEBUG if (DEBUG) Serial

// Blynk defines (must be defined before #include <BlynkSimpleEsp32.h>)
#if DEBUG == true 
  #define BLYNK_PRINT Serial
#endif
#define BLYNK_NO_BUILTIN
#define BLYNK_NO_FANCY_LOGO

// Includes
#include <esp_log.h>
#include <core_version.h>
#include <ESPmDNS.h>
#include <RCSwitch.h>
#include <WiFi.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>
#include <esp_now.h>
#include <LoRa.h>
#include <TFT_eSPI.h> 
/* Dont forget to edit User_Setup.h from TFT_eSPI to define the pins:
// ###### EDIT THE PIN NUMBERS IN THE LINES FOLLOWING TO SUIT YOUR ESP32 SETUP   ######
#define TFT_DC 17
#define TFT_CS 22
#define TFT_RST 5
#define TFT_MOSI 23
#define TFT_MISO 19
#define TFT_CLK 18
#define TOUCH_CS 14
*/
#include <LittleFS.h>
#include <FS.h>
#include <rom/rtc.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "sensordata.h"
#include <BlynkSimpleEsp32.h>

#define RCSIGNATURE 0b00111000000000000000000000000000UL // Signature for 433MHz signals (only the first 5 bits are the signature)
#define SECS_PER_MIN 60
#define SECS_PER_HOUR 3600
#define SECS_PER_DAY 86400

#define EEPROMADDR 10 // Startaddress in EEPROM

#define MAXSSIDLENGTH 30 // Maximum length of Wifi SSID
#define MAXPASSWORDLENGTH 30 // Maximum length of Wifi password
  
// Maximum number of consecutive notifications to Blynk in case of battery warning of a sensor
#define MAXLOWBATTERYNOTIFICATIONS 3

// I/O-PINs
#define BEEPER_PIN 27
#define PIR_PIN 4
#define RCSWITCH_PIN GPIO_NUM_35
// I2C
#define SDA_PIN 32
#define SCL_PIN 33
// LoRa (SPI MOSI=23, MISO=19, CLK=18, same pins as the TFT)
#define LORA_NSS 26
#define LORA_RST 21
#define LORA_IRQ 34
// TFT (Pins definitions for TFT see User_Setup.h in TFT_eSPI)
#define TFT_LED 16
#define TIRQ_PIN 25

// Message IDs for display task
#define ID_ALERT 1 // Power on display and show message
#define ID_INFO 2 // Show message
#define ID_DISPLAYON 3 // Power on display
#define ID_REFESHBUTTONS 4 // Refresh button display
#define ID_RESET 5 // Reset graph, battery states, min/max values

// Time in MS, after the display will be blanked because of inactivity
#define SCREENSAVERMS 20000 

#define TIMEZONE "CET-1CEST,M3.5.0/02,M10.5.0/03" // Timezone for Germany
#define NTPSERVER "pool.ntp.org" // Configured NTP server

// Use only one cpu for our tasks to simplify interrupts and keeps CPU0 free for OS, Wifi ...
#if CONFIG_FREERTOS_UNICORE
  static const BaseType_t g_appCpu = 0;
#else
  static const BaseType_t g_appCpu = 1;
#endif

#define FORMAT_LittleFS_IF_FAILED false // set to true if you want to format LitteFS on a new device

// Global variables
RCSwitch g_433MHzRCSwitch = RCSwitch(); // 433MHz Receiver

bool g_ScreenSaverEnabled = true; // Screensaver on/off
volatile bool g_SoundDisabled = false; // Buzzer on/off
volatile bool g_PIREnabled = true; // PIR sensor on/off
bool g_wifiEnabled = false; // Current Wifi status
bool g_wifiSwitch = true; // Wifi switch on/off
bool g_demoModeEnabled = false; // Demo mode on/off
bool g_demoModeSwitch = false; // Demo mode switch on/off
bool g_mailAlert = false; // Alert from my mail box
enum pengingStates { none, alert, clear };
pengingStates g_pendingBlynkMailAlert = none; // pending events

time_t g_lastStorageTime = -SECS_PER_HOUR;
unsigned long g_lastNTPSyncMS = -SECS_PER_HOUR * 1000;
time_t g_lastNTPTime = 0; // Last NTP-Sync
time_t g_firstNTPTime = 0; // ~ Boot timestamp

unsigned long g_lastPIRChangeMS = 0;
unsigned long g_lastWIFICheckMS = -SECS_PER_MIN * 1000;
unsigned long g_lastBME280MS = -SECS_PER_MIN * 1000;

time_t g_nextWifiOn = 0; // Delayed Wifi start
unsigned long g_lastMotDCheckMS = 0;
bool g_motdShown = false; // Daily message at noon displayed?
bool g_timeSet = false; // Is time valid?
volatile bool v_touchedTFT = false; // Touch screen IRQ fired? 

Adafruit_BME280 bme; // BME280 I2C module
bool g_BME280ready = false; // BME280 found?

volatile bool v_loraReceived = false; // Lora IRQ?
bool g_LORAready = false;

// WLAN credentials
char g_wifiSSID[MAXSSIDLENGTH+1];
char g_wifiPassword[MAXPASSWORDLENGTH+1];

bool g_wifiAPMode = false; // AP-mode will be used, if no Wifi credentials are found in EEPROM or the the device is powered on and Wifi fails

WiFiServer g_server(80); // Webserver on port  80
byte g_buffer[1400]; // // Buffer for webserver to improve low speed due TCP_NODELAY & Nagle's Algorithm

TFT_eSPI g_tft = TFT_eSPI();
/* 
 * Definitions for scrolling area at the lower end of the screen
 * The scrolling area must be a integral multiple of TEXT_HEIGHT
 */
#define TOP_FIXED_AREA 272 // Linenumbers of the non scrolling area on top of screen
#define TEXT_HEIGHT 8 // Height of a text line in scrolled area

// Handles for tasks
TaskHandle_t g_handleTaskWebServer = NULL;
TaskHandle_t g_handleTaskDisplay = NULL;

// Semaphores
SemaphoreHandle_t g_semaphoreSPIBus; // for SPI-Bus
SemaphoreHandle_t g_semaphoreLittleFS; // for LittleFS
SemaphoreHandle_t g_semaphoreBeep; // for buzzer

// Structure for a message to the display task
#define MAXMSGLENGTH 40
typedef struct
{
   byte id;
   char strData[ MAXMSGLENGTH+1 ];
} DisplayMessage;

// Queues for the display
QueueHandle_t displayMsgQueue; // Messages
QueueHandle_t displayDatQueue; // Data sets

// List of last saved sensor data for internal purposes (e.g. debugging)
SensorDataBuffer dataHistoryBuffer;

// Newest sensor data
sensorData g_pendingSensorData;

// Timestamp for other received known 433 MHz signals
time_t g_last433MhzBathOn = 0;
time_t g_last433MhzBathOff = 0;
time_t g_last433MhzCafeOn = 0;
time_t g_last433MhzCafeOff = 0;
time_t g_last433MhzWifiOff = 0;
time_t g_last433MhzWifiOn = 0;

// ISR for Touch-Interrupt (Primarily to reliably exit screensaver after touch)
void IRAM_ATTR TouchISR() {
  v_touchedTFT = true;
}

// Callback for LoRa receiver
void callbackLoraReceived(int packetSize) {
  if (packetSize == 0) return;
  v_loraReceived = true;
}

// Read Wifi config from EEPROM
void getWifiConnecitonDataFromEEPROM() {  
  g_wifiSSID[0] = '\0';
  g_wifiPassword[0] = '\0';

  if ( (EEPROM.read(EEPROMADDR) == 15) && (EEPROM.read(EEPROMADDR+1) == 43) ) { // Check signature
    for (int i=0;i<=MAXSSIDLENGTH;i++) {
      g_wifiSSID[i] = EEPROM.read(EEPROMADDR+2+i)-g_eepromenc[i%(strlen(g_eepromenc))];
    }
    g_wifiSSID[MAXSSIDLENGTH] = '\0';
    for (int i=0;i<=MAXPASSWORDLENGTH;i++) {
      g_wifiPassword[i] = EEPROM.read(EEPROMADDR+2+MAXSSIDLENGTH+1+i)-g_eepromenc[i%(strlen(g_eepromenc))];
    }
    g_wifiPassword[MAXPASSWORDLENGTH] = '\0';
  }
}

// Write Wifi config to EEPROM
void storeWifiConnecitonDataToEEPROM() {  
  EEPROM.write(EEPROMADDR,15);
  EEPROM.write(EEPROMADDR+1,43);

  for (int i=0;i<=MAXSSIDLENGTH;i++) {
    EEPROM.write(EEPROMADDR+2+i,g_wifiSSID[i]+g_eepromenc[i%(strlen(g_eepromenc))]);
  }
  for (int i=0;i<=MAXPASSWORDLENGTH;i++) {
    EEPROM.write(EEPROMADDR+2+MAXSSIDLENGTH+1+i,g_wifiPassword[i]+g_eepromenc[i%(strlen(g_eepromenc))]);
  }
  EEPROM.commit();
}

// callback when blynk established a connection
BLYNK_CONNECTED() {
  DisplayMessage msg;
  
  SERIALDEBUG.println("Connected to Blynk");
  msg.id = ID_INFO;
  snprintf(msg.strData,MAXMSGLENGTH+1,"Connected to Blynk");
  xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
}

// Start Wifi in AP-mode to configure Wifi
void startAPMode() {
  #define RANDOMPASSWORDLENGTH 8
  char randomPassword[RANDOMPASSWORDLENGTH+1];
  const char *passwordChars="abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ123456789";
  DisplayMessage msg;

  // Generate password
  for (int i=0;i<RANDOMPASSWORDLENGTH;i++) {
    randomPassword[i]=passwordChars[random(strlen(passwordChars))];
  }
  randomPassword[RANDOMPASSWORDLENGTH] = '\0';

  // start AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(HOSTNAME,randomPassword);
  delay(1000);

  // Show AP connection data on display
  IPAddress IP = WiFi.softAPIP();
  msg.id = ID_INFO;
  snprintf(msg.strData,MAXMSGLENGTH+1,"Please connect to AP:");
  xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
  snprintf(msg.strData,MAXMSGLENGTH+1,"SSID %s", HOSTNAME);
  xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
  snprintf(msg.strData,MAXMSGLENGTH+1,"Password %s", randomPassword);
  xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
  snprintf(msg.strData,MAXMSGLENGTH+1,"URL http://%d.%d.%d.%d/cfg",IP[0],IP[1],IP[2],IP[3]);
  xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );

  g_wifiAPMode = true; // Enable /cfg-Site on webserver

  // Start webserver
  g_server.begin();

  // Loop because device will be reset by webserver
  for (;;) {
    if ((digitalRead(PIR_PIN) == HIGH) && (g_PIREnabled)) {
      if (millis() - g_lastPIRChangeMS > 1000) {
        msg.id = ID_DISPLAYON;
        msg.strData[0]='\0';
        xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
        g_lastPIRChangeMS = millis();
      } 
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// Beep with passive buzzer
void beep(int type=0) {
  // User PWM to improve quality
  #define SHORTBEEP 1
  #define LONGBEEP 2
  if (!g_SoundDisabled) {
    if (xSemaphoreTake( g_semaphoreBeep, 10000 / portTICK_PERIOD_MS) == pdTRUE) {
      switch(type) {
        case 0: // 500 Hz for 200ms
          ledcSetup(0, 500, 8);
          ledcAttachPin(BEEPER_PIN, 0);
          ledcWrite(0, 128);
          delay(200);
          ledcWrite(0, 0);
          ledcDetachPin(BEEPER_PIN);          
          break;
        case SHORTBEEP: // 1 kHz for 100ms
          ledcSetup(0, 1000, 8);
          ledcAttachPin(BEEPER_PIN, 0);
          ledcWrite(0, 128);
          delay(100);
          ledcWrite(0, 0);
          ledcDetachPin(BEEPER_PIN);          
          break;
        case LONGBEEP: // 250 Hz for 400ms
          ledcSetup(0, 250, 8);
          ledcAttachPin(BEEPER_PIN, 0);
          ledcWrite(0, 128);
          delay(400);
          ledcWrite(0, 0);
          ledcDetachPin(BEEPER_PIN);          
          break;
      }
    }
    delay(100);
    xSemaphoreGive( g_semaphoreBeep );

  } else {
    SERIALDEBUG.println("Error: beep Semaphore Timeout");
  }
}

// Append line to file (write header, if file is not existing)
byte appendToFile(char strFile[], char strData[], char strHeader[]) {
  bool newFile = false;
  int RC=0;
  if (xSemaphoreTake( g_semaphoreLittleFS, 10000 / portTICK_PERIOD_MS) == pdTRUE) {

    if (!LittleFS.exists(strFile)) newFile = true;
    
    fs::File file = LittleFS.open(strFile, "a");
    if (!file) {
      RC=2;
    } else {

      if (newFile) {
        if (!file.println(strHeader)) RC=3;
      }
    
      if ((RC==0) && (!file.println(strData))) RC=4;

      file.close();
    }
    xSemaphoreGive( g_semaphoreLittleFS );
  } else {
    SERIALDEBUG.println("Error: LittleFS Semaphore Timeout");
    beep();
    beep(SHORTBEEP);
    beep();
    RC=6;
  }
  return RC;
}

// Reset ESP
void reset() {  
  SERIALDEBUG.println("Reset...");

  delay(2000);
  ESP.restart();
}

// callback function for reveived ESPNOW packages
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) { 
  #define MAXMACLENGTH 17
  char strMac[MAXMACLENGTH+1];
  DisplayMessage msg;

  snprintf(strMac, MAXMACLENGTH+1, "%02x:%02x:%02x:%02x:%02x:%02x",mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);  
  msg.id= ID_INFO;
  snprintf(msg.strData,MAXMSGLENGTH+1,"ESPNOW from %s",strMac);
  xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
}

// Enable Wifi
void WiFiOn() {
  byte wifiRetry=0;
  byte mac[6];
  #define MAXMACLENGTH 17
  char strMac[MAXMACLENGTH+1];
  DisplayMessage msg;
  IPAddress ip;

  getWifiConnecitonDataFromEEPROM();
  
  if (strlen(g_wifiSSID)==0) { // No Wifi config found in EEPROM
    SERIALDEBUG.println("Alert: no ssid configured");
    msg.id = ID_ALERT;
    snprintf(msg.strData,MAXMSGLENGTH+1,"No Wifi SSID configured");
    xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
    startAPMode();
    // This line will never be reached, because the configuration waits for a reset by the webserver (exception would be a semaphore timeout for display or button)
  } else {
    msg.id= ID_INFO;
    snprintf(msg.strData,MAXMSGLENGTH+1,"Startup WLAN to %s",g_wifiSSID);
    xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );

    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);  // Fix to make setHostname working without delay

    WiFi.persistent(false);

    WiFi.begin(g_wifiSSID, g_wifiPassword);
    WiFi.setHostname(HOSTNAME);

    SERIALDEBUG.print("WIFI to ");
    SERIALDEBUG.print(g_wifiSSID);

    while ((WiFi.status() != WL_CONNECTED) && (wifiRetry <= 20)) {
      wifiRetry++;
      delay(500);
      SERIALDEBUG.print(".");
    }

    if (WiFi.status() != WL_CONNECTED) { // Connection failed
      if (g_demoModeSwitch) return; // Demo modus switch was touched during Wifi connection => do nothing 
      if ((!g_timeSet)) { // If time is not valid
        if (rtc_get_reset_reason(0)==12) { // SW_CPU_RESET => Reset
          SERIALDEBUG.println("Alert: Wifi timeout => Reset");
          msg.id = ID_ALERT;
          snprintf(msg.strData,MAXMSGLENGTH+1,"Wifi timeout => reset");
          xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
          vTaskDelay(2000 / portTICK_PERIOD_MS);
          reset();
        } else {
           // powered on and no Wifi is working => Goto AP-mode to configure Wifi
          msg.id = ID_ALERT;
          snprintf(msg.strData,MAXMSGLENGTH+1,"Wifi timeout => reconfig");
          xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
          startAPMode();
          // This line will never be reached, because the configuration waits for a reset by the webserver (exception would be a semaphore timeout for display or button)
        }
      } else {
        msg.id = ID_ALERT;
        snprintf(msg.strData,MAXMSGLENGTH+1,"Wifi timeout => continue");
        xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );        
      }
    } else {
      // Init ESP-NOW
      if (esp_now_init() != ESP_OK) {
        msg.id = ID_ALERT;
        snprintf(msg.strData,MAXMSGLENGTH+1,"ESP-NOW failed");
        xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );        
      } else {        
        // Once ESPNow is successfully Init, we will register for recv CB to
        // get recv packer info
        esp_now_register_recv_cb(OnDataRecv);
      }
      // Show ip config
      SERIALDEBUG.println();
      SERIALDEBUG.print("Hostname: ");
      SERIALDEBUG.println(HOSTNAME);
      SERIALDEBUG.print("NTP-Server: ");
      SERIALDEBUG.println(NTPSERVER);
      SERIALDEBUG.print("IP: ");
      SERIALDEBUG.println(WiFi.localIP());
      SERIALDEBUG.print("Subnetmask: ");
      SERIALDEBUG.println(WiFi.subnetMask());
      SERIALDEBUG.print("Gateway: ");
      SERIALDEBUG.println(WiFi.gatewayIP());
      SERIALDEBUG.print("DNS: ");
      SERIALDEBUG.println(WiFi.dnsIP());
      WiFi.macAddress(mac);
      SERIALDEBUG.print("MAC: ");
      snprintf(strMac,MAXMACLENGTH+1,"%02X:%02X:%02X:%02X:%02X:%02X",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
      SERIALDEBUG.println(strMac);
      g_wifiEnabled = true;
      g_wifiSwitch = true;

      msg.id = ID_REFESHBUTTONS;
      msg.strData[0]='\0';
      xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );

      msg.id = ID_INFO;
      snprintf(msg.strData,MAXMSGLENGTH+1,"Hostname %s",HOSTNAME);
      xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
  
      ip = WiFi.localIP();
      snprintf(msg.strData,MAXMSGLENGTH+1,"IP %i.%i.%i.%i",ip[0],ip[1],ip[2],ip[3]);
      xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
  
      snprintf(msg.strData,MAXMSGLENGTH+1,"MAC %s",strMac);
      xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
  
      ArduinoOTA
      .onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
          type = "sketch";
        else // U_SPIFFS
          type = "filesystem";
  
        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
        SERIALDEBUG.println("Start updating " + type);
      })
      .onEnd([]() {
        SERIALDEBUG.println("\nEnd");
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        SERIALDEBUG.printf("Progress: %u%%\r", (progress / (total / 100)));
      })
      .onError([](ota_error_t error) {
        SERIALDEBUG.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) SERIALDEBUG.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) SERIALDEBUG.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) SERIALDEBUG.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) SERIALDEBUG.println("Receive Failed");
        else if (error == OTA_END_ERROR) SERIALDEBUG.println("End Failed");
      });
  
      ArduinoOTA.setPassword(OTAPASSWORD);
  
      ArduinoOTA.begin();
  
      if(!MDNS.begin(HOSTNAME)) { // mDNS after OTA, to avoid problems
        SERIALDEBUG.println("Error starting mDNS");
      } else {
        SERIALDEBUG.print("MDNS: ");
        SERIALDEBUG.print(HOSTNAME);       
        SERIALDEBUG.println(".local");    
      }
      Blynk.connect();
    }
  }
}

// Disable Wifi
void WiFiOff() {
  DisplayMessage msg;

  msg.id=ID_INFO;
  snprintf(msg.strData,MAXMSGLENGTH+1,"Shutdown WLAN");
  xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );

  Blynk.disconnect();
  ArduinoOTA.end();
  SERIALDEBUG.println("Shutdown WIFI");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  g_wifiEnabled = false;
  g_wifiSwitch = false;

  msg.id = ID_REFESHBUTTONS;
  msg.strData[0]='\0';
  xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
}

// Daily status message
void showMotD() {
  DisplayMessage msg;
  time_t now;
  struct tm * ptrTimeinfo;

  if (g_timeSet) {
    time(&now);
    ptrTimeinfo = localtime ( &now );

    if (ptrTimeinfo->tm_hour == 12) { // Only at noon
      if (g_motdShown == false) {
        msg.id = ID_INFO;
        snprintf(msg.strData,MAXMSGLENGTH+1,"Uptime %d days",(now-g_firstNTPTime)/SECS_PER_DAY);
        xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
        if (LittleFS.totalBytes() != 0) {
          snprintf(msg.strData,MAXMSGLENGTH+1,"%d%% disk used",100 *LittleFS.usedBytes()/LittleFS.totalBytes());
          xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
        }
        g_motdShown = true;
      }
    } else g_motdShown = false;
  }
}

void setup() {
  sensorData emptySensorData;
  DisplayMessage msg;

  SERIALDEBUG.begin(115200);

  SERIALDEBUG.print("ESP32 SDK: ");
  SERIALDEBUG.println(ESP.getSdkVersion());
  SERIALDEBUG.print("Compile time: ");
  SERIALDEBUG.println(__DATE__ " " __TIME__);

  EEPROM.begin(512);

  g_semaphoreBeep = xSemaphoreCreateBinary();
  configASSERT( g_semaphoreBeep );
  xSemaphoreGive( g_semaphoreBeep );

  pinMode(BEEPER_PIN,OUTPUT);
  beep();

  pinMode(TIRQ_PIN,INPUT);
  pinMode(TFT_LED,OUTPUT);
  digitalWrite(TFT_LED,HIGH);

  setupPIR();

  setupASKReceiver();
  
  // Set timezone to prevent wrong time after OTA resets until first NTP sync
  setenv("TZ", TIMEZONE, 1);
  tzset();

  displayMsgQueue = xQueueCreate(50,sizeof( DisplayMessage ) );
  configASSERT( displayMsgQueue );

  displayDatQueue = xQueueCreate(5,sizeof( sensorData ) );
  configASSERT( displayDatQueue );

  // Reset reason
  for (int i=0;i<ESP.getChipCores();i++) {
    switch (rtc_get_reset_reason(i))
    {
      case 1 : snprintf(msg.strData,MAXMSGLENGTH+1,"CPU%d POWERON_RESET",i);break;          /**<1, Vbat power on reset*/ // Normaler Einschaltvorgang durch Stromzufuhr
      case 3 : snprintf(msg.strData,MAXMSGLENGTH+1,"CPU%d SW_RESET",i);break;               /**<3, Software reset digital core*/
      case 4 : snprintf(msg.strData,MAXMSGLENGTH+1,"CPU%d OWDT_RESET",i);break;             /**<4, Legacy watch dog reset digital core*/
      case 5 : snprintf(msg.strData,MAXMSGLENGTH+1,"CPU%d DEEPSLEEP_RESET",i);break;        /**<5, Deep Sleep reset digital core*/
      case 6 : snprintf(msg.strData,MAXMSGLENGTH+1,"CPU%d SDIO_RESET",i);break;             /**<6, Reset by SLC module, reset digital core*/
      case 7 : snprintf(msg.strData,MAXMSGLENGTH+1,"CPU%d TG0WDT_SYS_RESET",i);break;       /**<7, Timer Group0 Watch dog reset digital core*/
      case 8 : snprintf(msg.strData,MAXMSGLENGTH+1,"CPU%d TG1WDT_SYS_RESET",i);break;       /**<8, Timer Group1 Watch dog reset digital core*/
      case 9 : snprintf(msg.strData,MAXMSGLENGTH+1,"CPU%d RTCWDT_SYS_RESET",i);break;       /**<9, RTC Watch dog Reset digital core*/
      case 10 : snprintf(msg.strData,MAXMSGLENGTH+1,"CPU%d INTRUSION_RESET",i);break;       /**<10, Intrusion tested to reset CPU*/
      case 11 : snprintf(msg.strData,MAXMSGLENGTH+1,"CPU%d TGWDT_CPU_RESET",i);break;       /**<11, Time Group reset CPU*/
      case 12 : snprintf(msg.strData,MAXMSGLENGTH+1,"CPU%d SW_CPU_RESET",i);break;          /**<12, Software reset CPU*/ // z.B. nach einem Flashvorgang
      case 13 : snprintf(msg.strData,MAXMSGLENGTH+1,"CPU%d RTCWDT_CPU_RESET",i);break;      /**<13, RTC Watch dog Reset CPU*/
      case 14 : snprintf(msg.strData,MAXMSGLENGTH+1,"CPU%d EXT_CPU_RESET",i);break;         /**<14, for APP CPU, reseted by PRO CPU*/
      case 15 : snprintf(msg.strData,MAXMSGLENGTH+1,"CPU%d RTCWDT_BROWN_OUT_RESET",i);break;/**<15, Reset when the vdd voltage is not stable*/
      case 16 : snprintf(msg.strData,MAXMSGLENGTH+1,"CPU%d RTCWDT_RTC_RESET",i);break;      /**<16, RTC Watch dog reset digital core and rtc module*/
      default : snprintf(msg.strData,MAXMSGLENGTH+1,"CPU%d NO_MEAN",i);
    }
    msg.id = ID_INFO;
    xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
  }

  setupBME280();
  
  setupLoRa();

  if (!LittleFS.begin(FORMAT_LittleFS_IF_FAILED)) {
    SERIALDEBUG.println("LittleFS mount failed");
    beep(SHORTBEEP);
    beep();
    beep(SHORTBEEP);
    reset();
  } else {
    SERIALDEBUG.println("LittleFS OK");

    msg.id = ID_INFO;
    snprintf(msg.strData,MAXMSGLENGTH+1,"Disk %dk from %dk used",LittleFS.usedBytes() / 1024, LittleFS.totalBytes() / 1024);
    xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
  }
  g_semaphoreLittleFS = xSemaphoreCreateBinary();
  configASSERT( g_semaphoreLittleFS );
  xSemaphoreGive( g_semaphoreLittleFS );
    
  // TFT
  g_tft.init();

  // Blynk
  Blynk.config(BLYNK_AUTH_TOKEN);

  g_semaphoreSPIBus = xSemaphoreCreateBinary();
  configASSERT( g_semaphoreSPIBus );
  xSemaphoreGive( g_semaphoreSPIBus );
  
  xTaskCreatePinnedToCore(
    taskWebServer ,    // Function that should be called
    "taskWebServer",   // Name of the task (for debugging)
    3250,            // Stack size (bytes)
    NULL,            // Parameter to pass
    1,               // Task priority (Prio 0 would reduce webserver performance to ~50%)
    &g_handleTaskWebServer             // Task handle
    , g_appCpu
  );
  configASSERT( g_handleTaskWebServer );

  xTaskCreatePinnedToCore(
    taskDisplay ,    // Function that should be called
    "taskDisplay",   // Name of the task (for debugging)
    4400,            // Stack size (bytes)
    NULL,            // Parameter to pass
    0,               // Task priority 
    &g_handleTaskDisplay             // Task handle
    , g_appCpu
  );
  configASSERT( g_handleTaskDisplay );

  // Define empty sensor set data
  emptySensorData.time = 0;
  emptySensorData.sensor1LowBattery = NOVALIDLOWBATTERY;
  emptySensorData.sensor1Temperature = NOVALIDTEMPERATUREDATA;
  emptySensorData.sensor1Humidity= NOVALIDHUMIDITYDATA;
  emptySensorData.sensor1Vcc = NOVALIDVCCDATA;
  emptySensorData.sensor2Temperature = NOVALIDTEMPERATUREDATA;
  emptySensorData.sensor2Humidity= NOVALIDHUMIDITYDATA;
  emptySensorData.sensor2Pressure = NOVALIDPRESSUREDATA;
  emptySensorData.sensor3LowBattery = NOVALIDLOWBATTERY;
  emptySensorData.sensor3Switch1 = NOVALIDSWITCHDATA;
  emptySensorData.sensor3Switch2 = NOVALIDSWITCHDATA;
  emptySensorData.sensor3Vcc = NOVALIDVCCDATA;
  emptySensorData.sensor3Temperature = NOVALIDTEMPERATUREDATA;
  emptySensorData.sensor3Humidity= NOVALIDHUMIDITYDATA;
  emptySensorData.sensor4LowBattery = NOVALIDLOWBATTERY;
  emptySensorData.sensor4Vcc = NOVALIDVCCDATA;
  emptySensorData.sensor4Runtime = NOVALIDRUNTIME;
  emptySensorData.sensor5LowBattery = NOVALIDLOWBATTERY;
  emptySensorData.sensor5Vcc = NOVALIDVCCDATA;
  emptySensorData.sensor5Switch1 = NOVALIDSWITCHDATA;
  emptySensorData.sensor5PCI1 = NOVALIDPCI;

  // Send empty data to display
  xQueueSend( displayDatQueue, ( void * ) &emptySensorData, portMAX_DELAY );
  
  msg.id = ID_INFO;
  snprintf(msg.strData,MAXMSGLENGTH+1,"Setup finished");
  xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
}

void loop() {
  DisplayMessage msg;
  sensorData newSensorData, emptySensorData;
  #define MAXSTRDATALENGTH 127
  char strTempData[MAXSTRDATALENGTH+1];
  char strData[MAXSTRDATALENGTH+1];
  #define MAXFILENAMELENGHT 13
  char strFile[MAXFILENAMELENGHT+1];
  byte RC;
  struct tm timeinfo;
  time_t now;
  struct tm * ptrTimeinfo;
  byte maxLoops;
  
  ArduinoOTA.handle();

  // Turn demo mode on/off, if changed by gui button
  if ((g_demoModeSwitch && !g_demoModeEnabled) || (!g_demoModeSwitch && g_demoModeEnabled)) {
    g_demoModeEnabled = !g_demoModeEnabled; // Invert mode
    msg.id = ID_INFO;
    if (g_demoModeEnabled) {
      snprintf(msg.strData,MAXMSGLENGTH+1,"Enter demo mode");
    } else {
      snprintf(msg.strData,MAXMSGLENGTH+1,"Exit demo mode");
    }
    xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );

    msg.strData[0]='\0';
    // Reset graph, battery states, min/max values on display
    msg.id = ID_RESET;
    xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
    // Send empty data to display
    xQueueSend( displayDatQueue, ( void * ) &newSensorData, portMAX_DELAY );
  }
  // Read PIR sensor
  readPIR();

  // Daily status message at noon
  if (millis() - g_lastMotDCheckMS >  SECS_PER_MIN * 1000) {
    showMotD();
    g_lastMotDCheckMS = millis();
  }
  if (!g_demoModeEnabled) {
    if(Blynk.connected()){ // Blynk, if ony if connected, because the connection phase can block loop for several seconds
      Blynk.run();
      if (Blynk.connected()) {
        switch (g_pendingBlynkMailAlert) {
          case none:break;
          case clear:Blynk.resolveAllEvents("mailalert");break; // resolveAllEvents is only allowed every 15 minutes
          case alert:
            if (g_pendingSensorData.sensor5Vcc != NOVALIDVCCDATA) 
              snprintf(strData,MAXSTRDATALENGTH+1,"Du hast Post... (Vcc=%.1fV)",g_pendingSensorData.sensor5Vcc/10.0f);             
              else snprintf(strData,MAXSTRDATALENGTH+1,"Du hast Post...");
            Blynk.logEvent("mailalert",strData); // Send to Blynk
            break;
        }
        g_pendingBlynkMailAlert = none;
      }
    } else {
      if ((WiFi.status() == WL_CONNECTED) && g_timeSet) { // Connection to Blynk lost, but WLAN connected and time available => restart WLAN and thus Blynk
        time(&now);
        msg.id = ID_INFO;
        snprintf(msg.strData,MAXMSGLENGTH+1,"Blynk lost=>Reset WLAN");
        xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
        SERIALDEBUG.println("Error: connection to Blynk lost=>Reset WLAN");
        g_server.end();
        WiFiOff();
        WiFiOn(); 
        g_server.begin();
      }
    }
  
    // Turn Wifi on/off if changed by gui button
    if (!g_wifiSwitch && g_wifiEnabled) {
      g_server.end();
      WiFiOff();    
    }
    if (g_wifiSwitch && !g_wifiEnabled) {
      WiFiOn(); 
      g_server.begin();    
    }

    // WIFI check once per minute
    if (millis() - g_lastWIFICheckMS >  SECS_PER_MIN * 1000) {
  
      // When WIFI disconnected unexpectedly
      if (g_wifiEnabled && (WiFi.status() != WL_CONNECTED)) {
        msg.id = ID_INFO;
        snprintf(msg.strData,MAXMSGLENGTH+1,"WLAN lost=>Reset WLAN");
        xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
        SERIALDEBUG.println("Error: WLAN connection lost=>Reset WLAN");
        g_server.end();
        WiFiOff();
        WiFiOn(); 
        g_server.begin();
      }
  
      time(&now);
      // Switch on Wifi, if no time is known or if corresponding switch-on signal has been received
      ptrTimeinfo = localtime ( &now );
      if (!g_timeSet || ((g_nextWifiOn != 0) && (now - SECS_PER_MIN >= g_nextWifiOn))) {
        if (WiFi.status() != WL_CONNECTED) {
          WiFiOn(); 
          g_server.begin();
        }
        g_nextWifiOn = 0;
      }
      g_lastWIFICheckMS = millis();
    }
  
     // NTP sync once per hour
    if (millis() - g_lastNTPSyncMS > SECS_PER_HOUR * 1000) {
      time(&now);
      // Local time
      ptrTimeinfo = localtime ( &now );
      if (!g_timeSet || ((ptrTimeinfo->tm_hour>=8) && (ptrTimeinfo->tm_hour<=23))) { // Only during the day, when WIFI is switched on or when no time is known yet
        if (WiFi.status() == WL_CONNECTED) {
          SERIALDEBUG.println("NTP-Sync");
  
          // Set NTP to SNTP_OPMODE_POLL and set timezone. Start NTP sync
          configTime(0, 0, NTPSERVER);
          setenv("TZ", TIMEZONE, 1);
          tzset();
  
          if(!getLocalTime(&timeinfo)){ // Convert UTC-time to local time (timezone related). Timeout is 5s by default and occurs if the time-year is <= 2016 (according to esp32-hal-time.c)
            
            if (!g_timeSet){ // If no valid time => Reset
              msg.id = ID_ALERT;
              snprintf(msg.strData,MAXMSGLENGTH+1,"No NTP");
              xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
              SERIALDEBUG.println("Alert: Failed to obtain NTP time and no time known => Reset");
              vTaskDelay(2000 / portTICK_PERIOD_MS);
              reset();
            } else {
              msg.id = ID_INFO;
              snprintf(msg.strData,MAXMSGLENGTH+1,"No NTP");
              xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
              SERIALDEBUG.println("Error: Failed to obtain NTP time");          
            }
          } else {
            time(&g_lastNTPTime);
            SERIALDEBUG.println(&timeinfo, "NTP-Time(Local TimeZone): %A, %B %d %Y %H:%M:%S");
            if (!g_timeSet) g_firstNTPTime = g_lastNTPTime;
            g_timeSet = true;
          }
        } else {
          SERIALDEBUG.println("Warning: no NTP without WIFI");
        }
        g_lastNTPSyncMS = millis();
      }
    }
  }

  readBME280();

  processDemoMode();
    
  if (g_timeSet || g_demoModeEnabled)  { // Only when time is valid or demo mode enabled
    time(&now);
    ptrTimeinfo = localtime(&now); // Local time
    
    // Save sensor data once per hour at minute 0 to csv file, when data are available
    if ((now - g_lastStorageTime > SECS_PER_HOUR - SECS_PER_MIN) && (ptrTimeinfo->tm_min==0) && 
      !( (g_pendingSensorData.sensor1LastDataTime == 0) && (g_pendingSensorData.sensor2LastDataTime == 0) 
      && (g_pendingSensorData.sensor3LastDataTime == 0) ) && !g_demoModeEnabled)  {
      SERIALDEBUG.println("Save data to storage");

      newSensorData = g_pendingSensorData;
      newSensorData.time = now;
        
      // UTC time to create file name for csv
      ptrTimeinfo = gmtime ( &now );
      snprintf(strFile,MAXFILENAMELENGHT+1,"/sn%04d%02d.csv", ptrTimeinfo->tm_year + 1900,ptrTimeinfo->tm_mon + 1);

      // Timestamp string
      snprintf(strData,MAXSTRDATALENGTH+1,"%04d-%02d-%02dT%02d:%02d:00.000Z",
        ptrTimeinfo->tm_year + 1900,
        ptrTimeinfo->tm_mon + 1,
        ptrTimeinfo->tm_mday,
        ptrTimeinfo->tm_hour,
        ptrTimeinfo->tm_min
      );
      // Append sensor 1 data to string
      if ((newSensorData.sensor1LowBattery != NOVALIDLOWBATTERY) 
        && (newSensorData.sensor1Temperature != NOVALIDTEMPERATUREDATA)
        && (newSensorData.sensor1Humidity != NOVALIDHUMIDITYDATA)) {
        snprintf(strTempData,MAXSTRDATALENGTH+1,"%s;%d;%d;%d",strData,
          newSensorData.sensor1LowBattery,
          newSensorData.sensor1Temperature,
          newSensorData.sensor1Humidity
          );
      } else {
        snprintf(strTempData,MAXSTRDATALENGTH+1,"%s;;;",strData);
      }
      snprintf(strData,MAXSTRDATALENGTH+1,strTempData);
      // Append sensor 2 data to string
      if ((newSensorData.sensor2Temperature != NOVALIDTEMPERATUREDATA)
        && (newSensorData.sensor2Humidity != NOVALIDHUMIDITYDATA) 
        && (newSensorData.sensor2Pressure != NOVALIDPRESSUREDATA)){
        snprintf(strTempData,MAXSTRDATALENGTH+1,"%s;%d;%d;%d",strData,
          newSensorData.sensor2Temperature,
          newSensorData.sensor2Humidity,
          newSensorData.sensor2Pressure
          );
        // Send sensor 2 to Blynk (because sensor 2 is builtin and has always data)
        Blynk.virtualWrite(V5, newSensorData.sensor2Temperature);
        Blynk.virtualWrite(V6, newSensorData.sensor2Humidity);
        Blynk.virtualWrite(V3, newSensorData.sensor2Pressure);
      } else {
        snprintf(strTempData,MAXSTRDATALENGTH+1,"%s;;;",strData);
      }
      snprintf(strData,MAXSTRDATALENGTH+1,strTempData);
      // Append sensor 3  data to string     
      if ((newSensorData.sensor3LowBattery != NOVALIDLOWBATTERY)
        && (newSensorData.sensor3Temperature != NOVALIDTEMPERATUREDATA)
        && (newSensorData.sensor3Humidity != NOVALIDHUMIDITYDATA)) {
        snprintf(strTempData,MAXSTRDATALENGTH+1,"%s;%d;%d;%d;%d;%d",strData,
          newSensorData.sensor3LowBattery,
          newSensorData.sensor3Switch1,
          newSensorData.sensor3Switch2,
          newSensorData.sensor3Temperature,
          newSensorData.sensor3Humidity
          );
      } else {
        snprintf(strTempData,MAXSTRDATALENGTH+1,"%s;;;;;",strData);
      }
      snprintf(strData,MAXSTRDATALENGTH+1,strTempData);

      SERIALDEBUG.println(strData);
      // Append string to csv file
      RC=appendToFile(strFile,strData,"Time;Lowbat_stairs;T_stairs;H_stairs;T_room;H_room;P_room;Lowbat_window;S1_window;S2_window;T_window;H_window;Lowbat_mail");
      if (RC != 0) {
        SERIALDEBUG.print("Error: file write error: ");
        SERIALDEBUG.println(RC);
        msg.id=ID_ALERT;
        snprintf(msg.strData,MAXMSGLENGTH+1,"file write error %i",RC);
        xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
      } else {
        // Send data to display, if save to csv was successful
        xQueueSend( displayDatQueue, ( void * ) &newSensorData, portMAX_DELAY );
      }          

      // Save data to HistoryBuffer for debugging...
      if (dataHistoryBuffer.isFull()) { dataHistoryBuffer.removeFirst(); }
      if (!dataHistoryBuffer.addLast(newSensorData)){
        msg.id=ID_INFO;
        snprintf(msg.strData,MAXMSGLENGTH+1,"History add false");
        xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );        
      }

      // Prevent rarely sensor data from hourly reset
      emptySensorData.sensor4LastDataTime = g_pendingSensorData.sensor4LastDataTime;
      emptySensorData.sensor4LowBattery = g_pendingSensorData.sensor4LowBattery;
      emptySensorData.sensor4Vcc = g_pendingSensorData.sensor4Vcc;
      emptySensorData.sensor4Runtime = g_pendingSensorData.sensor4Runtime;

      emptySensorData.sensor5LastDataTime = g_pendingSensorData.sensor5LastDataTime;
      emptySensorData.sensor5LowBattery = g_pendingSensorData.sensor5LowBattery;
      emptySensorData.sensor5Vcc = g_pendingSensorData.sensor5Vcc;
      emptySensorData.sensor5Switch1 = g_pendingSensorData.sensor5Switch1;
      emptySensorData.sensor5PCI1 = g_pendingSensorData.sensor5PCI1;

      // Reset pending sensor data
      g_pendingSensorData = emptySensorData;
      time(&g_lastStorageTime);
    }

    // ASK 433 MHz signals
    checkForASKSignals();
    
    // LoRa signals
    checkForLoRaSignals();
        
    vTaskDelay(10 / portTICK_PERIOD_MS); // Give >Prio0-tasks a chance
  }
}
