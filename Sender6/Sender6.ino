 /*
 * Project: Sender6 (Ding18)
 * Description:
 * - Send a message, when my washing machine is finished (when no shaking is detected for a longer period at my over 20 year old Gorenje WA1141 machine)
 *
 * License: 2-Clause BSD License
 * Copyright (c) 2023 codingABI
 * For details see: License.txt
 * 
 * created by codingABI https://github.com/codingABI/SenderReceiver/#sender-6-433-mhz-lora
 * 
 * Hardware:
 * - Microcontroller ESP32 LOLIN32
 * - MPU6050 accelerometer and gyroscope
 * - SSD1306 OLED 128x32 pixel
 * - KY-040 rotary encoder
 * - SX1278 LoRa Ra-02
 * - 3.7V 330mAh Li-Ion battery
 * - Two resistors (47k, 100k) for a voltage divider
 * 
 * Current consumption (measured on battery, LED from gyro sensor was removed): 
 * a) 21 mA in menu selection
 * b) 4mA in minimal display mode while waiting for motions
 * c) 26 mA in maximal display mode while waiting for motions
 * d) 140 mA for the short time, while sending the LoRa signal 
 * e) In a), c) and d) +20mA, if Serial is enabled (because cpu clock must be higher)
 * => Runtime with 3.7V/330mAh battery ~10h
 * 
 * Buzzer-Codes
 * - 1xShort beep      = A button was pressed
 * - 1xStandard beep   = User input timed out
 * - 1xLaser beep      = Begin of normal device start/power on
 * - 1xLong beep       = Error (If critical, the device will be reset)
 * - 2xLong beep       = A previous WDT reset was detected
 * - 1xHigh short beep = LoRa-Signal sent, but no response from receiver
 * 
 * History: 
 * 20230510, Initial version
 * 20230519, Add language strings for EN
 * 20230521, Do not goto deep sleep, if motions are continuous
 */

#include <NewEncoder.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_SSD1306.h> 
#include <Wire.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <EEPROM.h>
#include <LoRa.h>
#include <esp_task_wdt.h>
#include <rom/rtc.h>

#define RCSIGNATURE 0b00111000000000000000000000000000UL // Signature for signals (only the first 5 bits are the signature)
#define ID 6
/* Signal (32-bit):
 * 5 bit: Signature
 * 3 bit: ID
 * 1 bit: Low battery
 * 6 bit: Vcc (0-63)
 * 9 bit: unused
 * 8 bit: type of message (0=STARTMESSAGE, 1=ENDMESSAGE, 2=TESTMESSAGE)   
 */

// Set display language to DE or EN
//#define DISPLAYLANGUAGE_DE
#define DISPLAYLANGUAGE_EN
#include "displayLanguage.h"

#define EEPROM_SIGNATURE 18 // First byte at startaddress in EEPROM
#define EEPROM_VERSION 2 // Second byte at startaddress in EEPROM
#define EEPROM_STARTADDR 0 // Startaddress in EEPROM
#define EEPROM_SIZE 64 // Size of EEPROM area

#define LOWBATWARNING 3.0f // Send low battery warning, when voltage <= this value

// I2C for OLED display and gyroscope 
#define SDA_PIN 32
#define SCL_PIN 33

// MPU irq (used for deep sleep wake up)
#define MPU_IRQ 36

// Pin for passive buzzer
#define BUZZER_PIN 25

// Reserved for future use 
#define SPARE_PIN 27

// Rotary encoder
#define ROTARY_DT_PIN 17 // rotary DT
#define ROTARY_CLK_PIN 5 // rotary CLK
#define ROTARY_SW_PIN GPIO_NUM_39 // rotary button
// I use pollings for button pin 39 and no interrupt handling, because operatins like analogRead or interrupts on pins 36 and 34 makes interrupt troubles 
// (perhaps related "When the power switch which controls the temperature sensor, SARADC1 sensor, SARADC2 sensor, AMP sensor, or HALL sensor, is turned on, the inputs of GPIO36 and GPIO39 will be pulled down for about 80 ns" or https://esp32.com/viewtopic.php?t=14087)

// Analog pin to measure the battery/loader voltage
#define VBAT_PIN 34 

// LoRa
#define MOSI_PIN 23 // SPI MOSI
#define MISO_PIN 19 // SPI MISO
#define SCK_PIN 18 // SPI SCK/CLK
#define LORA_RST_PIN 4
#define LORA_NSS_PIN 26
#define LORA_IRQ_PIN 35 // DIO0

// OLED display
#define OLED_RESET -1 // no reset pin
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
Adafruit_SSD1306 g_display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define OVERVIEWGAP 8 // Distance between overview dots
#define USERTIMEOUTMS 60000 // Set timeout in MS for user inputs
#define BUTTONDEBOUNCEMS 200 // Button debounce time in MS
#define WDTTIMEOUT 10 // WDT timeout in seconds
#define INVERTTIMEMS 500 // time range in MS to invert display after setting a value

// Menu items
enum menuItems { MENUSTART, MENUBATTERY, MENUTIMEOUT, MENUTHRESHOLD, MENUDISPLAY, MENUSOUND, MENUSERIAL, MENULORATEST, MENUINFOS, MENURESET, MENURESTART, MAXMENUITEMS};
// Display modes
enum displayModes { MODEMINIMAL, MODEMAXIMAL, MAXDISPLAYMODES};
// Pages for the information menu item 
enum infoPages { 
  PAGEAPP,
  PAGEMAC,
  PAGECHIP,
  PAGEIDF,
  PAGECOMPILEDATE,
  PAGECOMPILETIME,
  PAGEUPTIME,
  PAGEMPU,
  PAGEANALOG,
  PAGEFREEHEAP,
  PAGETOP,
  MAXINFOPAGES
};

/* 
 *  Simple sprite to show waiting state or detecting a motion
 *  made with https://www.piskelapp.com/p/create/sprite
 *  and converted by https://javl.github.io/image2cpp/
 */
#define SPRITE_WIDTH 16
#define SPRITE_HEIGHT 16
#define MAXFRAMES 8
RTC_DATA_ATTR byte g_currentSpriteFrame = 0;
const byte g_sprite[MAXFRAMES][SPRITE_WIDTH*2] = {
{ 0x00, 0x00, 0x07, 0x00, 0x0f, 0x00, 0x18, 0x00, 0x30, 0x00, 0x60, 0x00, 0x60, 0x00, 0x60, 0x00, 
  0x60, 0x00, 0x60, 0x00, 0x60, 0x00, 0x30, 0x00, 0x10, 0x00, 0x08, 0x40, 0x05, 0x00, 0x00, 0x00},
{ 0x00, 0x00, 0x07, 0xe0, 0x0f, 0xf0, 0x18, 0x10, 0x30, 0x00, 0x60, 0x00, 0x60, 0x00, 0x60, 0x00, 
  0x20, 0x00, 0x20, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00},
{ 0x00, 0x00, 0x07, 0xe0, 0x0f, 0xf0, 0x18, 0x18, 0x20, 0x0c, 0x40, 0x06, 0x00, 0x06, 0x40, 0x06, 
  0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
{ 0x00, 0x00, 0x00, 0xe0, 0x03, 0xf0, 0x08, 0x18, 0x00, 0x0c, 0x20, 0x06, 0x00, 0x06, 0x00, 0x06, 
  0x00, 0x06, 0x00, 0x06, 0x00, 0x06, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
{ 0x00, 0x00, 0x00, 0xa0, 0x02, 0x10, 0x00, 0x08, 0x00, 0x0c, 0x00, 0x06, 0x00, 0x06, 0x00, 0x06, 
  0x00, 0x06, 0x00, 0x06, 0x00, 0x06, 0x00, 0x0c, 0x00, 0x18, 0x00, 0xf0, 0x00, 0xe0, 0x00, 0x00},
{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x04, 0x00, 0x04, 
  0x00, 0x06, 0x00, 0x06, 0x00, 0x06, 0x00, 0x0c, 0x08, 0x18, 0x0f, 0xf0, 0x07, 0xe0, 0x00, 0x00}, 
{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 
  0x60, 0x02, 0x60, 0x00, 0x60, 0x02, 0x30, 0x04, 0x18, 0x18, 0x0f, 0xf0, 0x07, 0xe0, 0x00, 0x00}, 
{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x60, 0x00, 0x60, 0x00, 0x60, 0x00, 
  0x60, 0x00, 0x60, 0x00, 0x60, 0x04, 0x30, 0x00, 0x18, 0x10, 0x0f, 0xc0, 0x07, 0x00, 0x00, 0x00}};

/* 
 *  Simple icons
 *  made with https://www.piskelapp.com/p/create/sprite
 *  and converted by https://javl.github.io/image2cpp/
 */
#define ICON_WIDTH 16
#define ICON_HEIGHT 16
enum icons { ICONAUDIOON, ICONAUDIOOFF, ICONOK, MAXICONS};
const byte g_icon[MAXICONS][ICON_WIDTH*2] = {
{ 0x00, 0x00, 0x00, 0x30, 0x00, 0x70, 0x00, 0xf0, 0x01, 0xf0, 0x0f, 0xb0, 0x1f, 0x30, 0x18, 0x30, 
  0x18, 0x30, 0x1f, 0x30, 0x0f, 0xb0, 0x01, 0xf0, 0x00, 0xf0, 0x00, 0x70, 0x00, 0x30, 0x00, 0x00}, 
{ 0xc0, 0x01, 0x60, 0x33, 0x30, 0x76, 0x18, 0xec, 0x0d, 0xd8, 0x06, 0xb0, 0x1b, 0x60, 0x19, 0xd0, 
  0x19, 0xd0, 0x1b, 0x60, 0x06, 0xb0, 0x0d, 0xd8, 0x18, 0xec, 0x30, 0x76, 0x60, 0x33, 0xc0, 0x01}, 
{ 0x00, 0x00, 0x00, 0x06, 0x00, 0x06, 0x00, 0x0c, 0x00, 0x0c, 0x00, 0x18, 0x00, 0x18, 0x00, 0x30, 
  0x00, 0x30, 0x00, 0x60, 0x18, 0x60, 0x0c, 0xc0, 0x06, 0xc0, 0x03, 0x80, 0x01, 0x80, 0x00, 0x00}
};

Adafruit_MPU6050 g_mpu; // MPU6050 accelerometer and gyroscope
byte g_motionThreshold; // Threshold for motion detection (1 = very sensible, 255 is minimal sensible)
unsigned long g_idleTimeTimeoutS; // Timeout in seconds after that the LoRa message will be sent, when no acceleration/motion is detected
byte g_displayMode; // 0 = Minimal/Default, 1 = Maximal/Debug
bool g_soundEnabled; // Enables the buzzer sound
bool g_serialEnabled; // Enables Serial... but need more power due higher cpu frequence
#define SERIALDEBUG if (g_serialEnabled) Serial
bool g_detectionActive = false; // Show detection of a acceleration/motion on screen
RTC_DATA_ATTR unsigned long g_lastIdleStartTimeS = 0; // Time of last idle time begin 
bool g_wakeUpByButton = false; // Deep sleep wake up by button?
bool g_wakeUpByMPU = false; // Deep sleep wake up by mpu?
volatile bool v_loraReceived = false; // Lora IRQ triggered?
float g_vBat; // Current battery/loader voltage
RTC_DATA_ATTR bool g_firstBoot = true; // Used to distinguish between a normal device startup and deep sleep wake ups 

// List of longest idle times
#define MAXTIMEHISTORY 4
RTC_DATA_ATTR unsigned long g_idleTimeSHistory[MAXTIMEHISTORY]; // List of longest time in seconds without an acceleration

enum beepTypes { DEFAULTBEEP, SHORTBEEP, LONGBEEP, HIGHSHORTBEEP, LASER };
enum messageTypes { STARTMESSAGE, ENDMESSAGE, TESTMESSAGE };

// Draw current frame of sprite 
void drawSprite(int x, int y, int speedDelayMS=50) {  
  static unsigned long lastSpriteChangeMS = 0;
  // Draw sprite pixels
  for (int i=0;i<SPRITE_HEIGHT;i++) { // every sprite line
    for (int j=0;j<2;j++) { // every 8 pixel per line (<SPRITE_WIDTH/8)
      byte value = g_sprite[g_currentSpriteFrame][(i<<1) + j];

      for (int k=0;k<8;k++) { // check bits from msb to lsb
        if (value & (0b10000000>>k)) {
          g_display.drawPixel(x+(j<<3)+k,y+i,WHITE);
        }
      }
    }
  }

  // Goto next frame, when needed
  if ((lastSpriteChangeMS == 0) || (millis()-lastSpriteChangeMS > speedDelayMS)) { 
    g_currentSpriteFrame++;
    if (g_currentSpriteFrame>=MAXFRAMES) g_currentSpriteFrame = 0;
    lastSpriteChangeMS = millis();
  }
}

// Draw current frame of sprite 
void drawIcon(int x, int y, byte icon) {  
  if (icon >= MAXICONS) {
    SERIALDEBUG.print("Unknown icon ");    
    SERIALDEBUG.println(icon);
    return;
  }
  // Draw sprite pixels
  for (int i=0;i<ICON_HEIGHT;i++) { // every sprite line
    for (int j=0;j<2;j++) { // every 8 pixel per line
      byte value = g_icon[icon][(i<<1) + j];

      for (int k=0;k<8;k++) { // check bits from msb to lsb
        if (value & (0b10000000>>k)) {
          g_display.drawPixel(x+(j<<3)+k,y+i,WHITE);
        }
      }
    }
  }
}

// Call back for LoRa in receiver mode
void callbackLoraReceived(int packetSize) {
  if (packetSize == 0) return;
  v_loraReceived = true;
}

// Get deep sleep capable runtime
int64_t getRunTimeS() {
  struct timeval current_time;
  gettimeofday(&current_time, NULL);
  return (current_time.tv_sec);
}

// Buzzer
void beep(int type=DEFAULTBEEP) {
  if (!g_soundEnabled) return;
  // User PWM to improve quality
  switch(type) {
    case DEFAULTBEEP: { // 500 Hz for 200ms
      ledcSetup(0, 500, 8);
      ledcAttachPin(BUZZER_PIN, 0);
      ledcWrite(0, 128);
      delay(200);
      ledcWrite(0, 0);
      ledcDetachPin(BUZZER_PIN);          
      break;
    }
    case SHORTBEEP: { // 1 kHz for 100ms
      ledcSetup(0, 1000, 8);
      ledcAttachPin(BUZZER_PIN, 0);
      ledcWrite(0, 128);
      delay(100);
      ledcWrite(0, 0);
      ledcDetachPin(BUZZER_PIN);          
      break;
    }
    case LONGBEEP: { // 250 Hz for 400ms
      ledcSetup(0, 250, 8);
      ledcAttachPin(BUZZER_PIN, 0);
      ledcWrite(0, 128);
      delay(400);
      ledcWrite(0, 0);
      ledcDetachPin(BUZZER_PIN);          
      break;
    }
    case HIGHSHORTBEEP: { // High and short beep 
      ledcSetup(0, 5000, 8);
      ledcAttachPin(BUZZER_PIN, 0);
      ledcWrite(0, 128);
      delay(100);
      ledcWrite(0, 0);
      ledcDetachPin(BUZZER_PIN);          
      break;
    }
    case LASER: { // Laser like sound
      int i = 5000; // Start frequency in Hz (goes down to 300 Hz)
      int j = 300; // Start duration in microseconds (goes up to 5000 microseconds)
      ledcSetup(0, i, 8);
      ledcAttachPin(BUZZER_PIN, 0);
      ledcWrite(0, 0);
      while (i>300) {
        i -=50;
        j +=50;
        ledcSetup(0, i, 8);
        ledcWrite(0, 128);
        delayMicroseconds(j);
        ledcWrite(0,0);
        delayMicroseconds(1000);
      }
      ledcWrite(0, 0);
      ledcDetachPin(BUZZER_PIN);          
      break;
    }
    default: {
      SERIALDEBUG.print("Unknown beep type ");    
      SERIALDEBUG.println(type);
    }
  }
  esp_task_wdt_reset();
}

// Center text on screen
void centerText(char *strData, int offsetY=0, int offsetX=0) {
  int16_t  x, y;
  uint16_t w, h;

  g_display.getTextBounds(strData, 0, 0, &x, &y, &w, &h);
  g_display.setCursor((SCREEN_WIDTH-w)/2+offsetX,(SCREEN_HEIGHT-y)/2+offsetY);
  g_display.print(strData);  
}

// Show current timeout value on the configuration screen
void showTimeout() {
  #define MAXSTRDATALENGTH 80
  char strData[MAXSTRDATALENGTH+1];

  g_display.setFont();
  g_display.setCursor(0,0);
  g_display.print(STR_IDLETIMEINSECONDS);
  g_display.setFont(&FreeSans12pt7b);
  snprintf(strData,MAXSTRDATALENGTH+1,"%lu",g_idleTimeTimeoutS); 
  g_display.setCursor(0,SCREEN_HEIGHT-2);
  g_display.print(strData);
}

// Show current sensor threshold value on the configuration screen
void showThreshold() {
  #define MAXSTRDATALENGTH 80
  char strData[MAXSTRDATALENGTH+1];

  g_display.setFont();
  g_display.setCursor(0,0);
  g_display.print(STR_SENSORTHRESHOLD);
  g_display.setFont(&FreeSans12pt7b);
  snprintf(strData,MAXSTRDATALENGTH+1,"%i",g_motionThreshold); 
  g_display.setCursor(0,SCREEN_HEIGHT-2);
  g_display.print(strData);
}

// Show device infos
void showInfos(byte page) {
  static int lastVbat;
  static unsigned long lastUpdateVbatMS=0;
  sensors_event_t a, g, temp;
  #define MAXSTRDATALENGTH 80
  char strData[MAXSTRDATALENGTH+1];

  if (page >= MAXINFOPAGES) {
    SERIALDEBUG.print("Unknown info page ");
    SERIALDEBUG.println(page);
    return;     
  }

  // Enable mpu, if needed
  if (page == PAGEMPU) g_mpu.enableSleep(false); else g_mpu.enableSleep(true);
    
  g_display.setFont();
  g_display.setCursor(0,0);

  switch (page) {
    case PAGEAPP: {
      g_display.print("Sender6 (Ding18)");
      g_display.setFont(&FreeSans9pt7b);
      g_display.setCursor(0,SCREEN_HEIGHT-8-1);
      g_display.print("(c) codingABI");
      break;
    }
    case PAGEMAC: {
      g_display.print("Mac address");
      g_display.setCursor(0,16);
      for (int i=0;i<6;i++) { // Build mac from chip id
        byte part = ((ESP.getEfuseMac() >> (i*8)) & 0xff);
        snprintf(strData,MAXSTRDATALENGTH+1,"%02X",part); 
        g_display.print(strData);
        if (i < 5) g_display.print(":");
      }
      break;
    }
    case PAGECHIP: {
      g_display.print("Chip model");
      g_display.setCursor(0,16);
      g_display.print(ESP.getChipModel());
      break;
    }
    case PAGEIDF: {
      g_display.print("IDF");
      g_display.setFont(&FreeSans9pt7b);
      g_display.setCursor(0,SCREEN_HEIGHT-7);
      g_display.print(ESP.getSdkVersion());
      break;
    }
    case PAGECOMPILEDATE: {
      g_display.print("Compile date");
      g_display.setFont(&FreeSans9pt7b);
      g_display.setCursor(0,SCREEN_HEIGHT-7);
      g_display.print(__DATE__);
      break;
    }
    case PAGECOMPILETIME: {
      g_display.print("Compile time");
      g_display.setFont(&FreeSans9pt7b);
      g_display.setCursor(0,SCREEN_HEIGHT-7);
      g_display.print(__TIME__);
      break;
    }
    case PAGEUPTIME: {
      g_display.print("Uptime");
      g_display.setFont(&FreeSans9pt7b);
      g_display.setCursor(0,SCREEN_HEIGHT-7);
      g_display.print(getRunTimeS());
      g_display.print("s");
      break;
    }
    case PAGEMPU: {
      g_display.setCursor(SCREEN_WIDTH-25,0);
      g_display.print("MPU");
      g_display.setCursor(0,0);
      g_mpu.getEvent(&a, &g, &temp);
      snprintf(strData,MAXSTRDATALENGTH+1,"X=%5.1f m/s%c",a.acceleration.x,char(252));
      g_display.println(strData);
      snprintf(strData,MAXSTRDATALENGTH+1,"Y=%5.1f m/s%c",a.acceleration.y,char(252));
      g_display.println(strData);
      snprintf(strData,MAXSTRDATALENGTH+1,"Z=%5.1f m/s%c",a.acceleration.z,char(252));
      g_display.println(strData);
      
      g_display.setCursor(SCREEN_WIDTH/2+16,16);
      snprintf(strData,MAXSTRDATALENGTH+1,"T=%2i C%c",(int) round(temp.temperature),char(247));      
      g_display.println(strData);
      break;
    }
    case PAGEANALOG: {
      if (millis() - lastUpdateVbatMS > 500) { // Prevent too fast readings from analog pin
        lastVbat = analogRead(VBAT_PIN);
        lastUpdateVbatMS = millis();
      }

      g_display.print("Analog pin ");
      g_display.print(VBAT_PIN);

      g_display.setFont(&FreeSans9pt7b);
      g_display.setCursor(0,SCREEN_HEIGHT-7);
      g_display.print(lastVbat);
      break;
    }
    case PAGEFREEHEAP: {
      g_display.print("Free heap bytes");
      g_display.setFont(&FreeSans9pt7b);
      g_display.setCursor(0,SCREEN_HEIGHT-7);
      g_display.print(ESP.getFreeHeap());
      break;
    }
    case PAGETOP: {
      g_display.print("Top idle times");
      g_display.setCursor(0,12);
      if (MAXTIMEHISTORY < 4) { // We need 4 history items for the info page
        SERIALDEBUG.println("MAXTIMEHISTORY less than 4");
        return;
      }
      #define DAYINSECS (3600*24)
      if (g_idleTimeSHistory[0] > 10*DAYINSECS) {
        snprintf(strData,MAXSTRDATALENGTH+1,"%i:%6lud  %i:%6lud",1,g_idleTimeSHistory[0]/DAYINSECS,3,g_idleTimeSHistory[2]/DAYINSECS); 
        g_display.println(strData);     
        snprintf(strData,MAXSTRDATALENGTH+1,"%i:%6lud  %i:%6lud",2,g_idleTimeSHistory[1]/DAYINSECS,4,g_idleTimeSHistory[3]/DAYINSECS); 
        g_display.print(strData);
      } else {
        snprintf(strData,MAXSTRDATALENGTH+1,"%i:%6lus  %i:%6lus",1,g_idleTimeSHistory[0],3,g_idleTimeSHistory[2]); 
        g_display.println(strData);     
        snprintf(strData,MAXSTRDATALENGTH+1,"%i:%6lus  %i:%6lus",2,g_idleTimeSHistory[1],4,g_idleTimeSHistory[3]); 
        g_display.print(strData);
      }            
      break;
    }
    default:SERIALDEBUG.println("Unknown info page");return; 
  }
}

// Init device settings
void initSettings() {
  g_motionThreshold = 5;
  g_idleTimeTimeoutS = 120;
  g_displayMode = 0;
  g_serialEnabled = false;
  g_soundEnabled = true;
}

// Save device settings to EEPROM
void saveSettings() {
  SERIALDEBUG.println("Write settings to EEPROM");

  int addr = EEPROM_STARTADDR;
  // Save values in EEPROM
  EEPROM.writeByte(addr,EEPROM_SIGNATURE);
  addr+=sizeof(byte);
  EEPROM.writeByte(addr,EEPROM_VERSION);
  addr+=sizeof(byte);
  EEPROM.writeByte(addr, g_motionThreshold);
  addr+=sizeof(byte);
  EEPROM.writeByte(addr, g_displayMode);
  addr+=sizeof(byte);
  EEPROM.writeByte(addr,(g_serialEnabled)?1:0);
  addr+=sizeof(byte);
  EEPROM.writeByte(addr,(g_soundEnabled)?1:0);
  addr+=sizeof(byte);
  EEPROM.writeULong(addr, g_idleTimeTimeoutS);
  EEPROM.commit();
}

// Send LoRa signal
bool sendLoRa(unsigned long data) {
  #define MAXSTRDATALENGTH 80
  char strData[MAXSTRDATALENGTH+1];
  unsigned long startWaitMS;
  bool responseOK;
  unsigned long received;
  char* strPtr;
  #define MAXRETRIES 3
  #define RESPONSETIMEOUTMS 2000

  SERIALDEBUG.print("Sending ");
  SERIALDEBUG.println(data,BIN);

  snprintf(strData,MAXSTRDATALENGTH+1,STR_LORAPACKET); 

  for (int i=0;i < MAXRETRIES;i++){
    LoRa.beginPacket();
    LoRa.print(data);
    LoRa.endPacket();
    // Wait for response
    LoRa.receive();
    startWaitMS = millis();
    responseOK = false;
    do {
      esp_task_wdt_reset();

      g_display.clearDisplay();
      g_display.setFont(&FreeSans9pt7b);
      centerText(strData,0,-SPRITE_WIDTH/2);
      drawSprite(SCREEN_WIDTH-SPRITE_WIDTH-1,(SCREEN_HEIGHT-SPRITE_HEIGHT)/2-1);
      g_display.display();

      if (v_loraReceived) {
        v_loraReceived = false;
        while (LoRa.available()) {  
          String LoRaData = LoRa.readString();
          received = strtoul (LoRaData.c_str(),&strPtr,10);
  
          if ((0xfffffffful ^ received)== data) { // XOR response
            SERIALDEBUG.println("Response received");
            responseOK = true;            
          }
        }
      }
    } while ((millis() - startWaitMS < RESPONSETIMEOUTMS) && !responseOK);
    if (!responseOK) {
      SERIALDEBUG.println("Response timeout");
      beep(HIGHSHORTBEEP);
    } else {
      break; // Exit for loop  
    }
  }
  LoRa.sleep();

  esp_task_wdt_reset();
  return(responseOK);
}

// Send LoRa testsignal
void sendLoRaTest() {
  #define MAXSTRDATALENGTH 80
  char strData[MAXSTRDATALENGTH+1];

  if (sendLoRa(RCSIGNATURE + 
    (((unsigned long) ID & 7) << 24) +
    ((unsigned long) (g_vBat <= LOWBATWARNING) << 23) +
    ((((unsigned long) round(g_vBat*10))&63) << 17) +
    TESTMESSAGE)) {
    snprintf(strData,MAXSTRDATALENGTH+1,STR_CONFIRMED); 
  } else {
    snprintf(strData,MAXSTRDATALENGTH+1,STR_SENT); 
  }
  
  g_display.clearDisplay();
  g_display.setFont(&FreeSans9pt7b);
  centerText(strData);
  g_display.display();
  delay(2000);
  esp_task_wdt_reset();
}

// Show battery/loader voltage without input timeout
void batteryView() {
  #define MAXSTRDATALENGTH 80
  char strData[MAXSTRDATALENGTH+1];
  bool exitLoop = false;

  do {
    esp_task_wdt_reset();

    updateVbat();
    snprintf(strData,MAXSTRDATALENGTH+1,"%.2fV",g_vBat); 

    g_display.clearDisplay();
    g_display.setFont();
    g_display.setCursor(0,0);
    g_display.print(STR_VBATLOADER);
    if (g_vBat <=LOWBATWARNING) {
      g_display.setCursor(SCREEN_WIDTH-1-strlen(STR_EMPTY)*6,SCREEN_HEIGHT-1-8); 
      g_display.print(STR_EMPTY);      
    }
    if ((g_vBat < 4.1f) && (g_vBat > 4.0f)) {
      g_display.setCursor(SCREEN_WIDTH-1-strlen(STR_FULL)*6,SCREEN_HEIGHT-1-8); 
      g_display.print(STR_FULL);     
    }
    g_display.setFont(&FreeSans12pt7b);
    g_display.setCursor(0,SCREEN_HEIGHT-1);
    g_display.print(strData);
    g_display.display();
    if (digitalRead(ROTARY_SW_PIN)==LOW) {
      g_display.clearDisplay();
      g_display.display();
      beep(SHORTBEEP);
      while (digitalRead(ROTARY_SW_PIN)==LOW) esp_task_wdt_reset(); // Wait for button release
      delay(BUTTONDEBOUNCEMS); // Debounce
      exitLoop = true;
    }
  } while (!exitLoop);
}

 // Change dualstate option
void changeSetting(NewEncoder *encoder, byte menuItem) {
  #define MAXSTRDATALENGTH 80
  char strData[MAXSTRDATALENGTH+1];
  enum options { OPTIONLEFT, OPTIONRIGHT, MAXOPTIONS };
  unsigned long lastInteractMS; // Last interaction time to detect timeout
  NewEncoder::EncoderState currentEncoderState;
  bool exitLoop = false;
  bool selection = false;
  int currentOption = 0;
  int currentMaxOptions;

  SERIALDEBUG.print("Selection mode for ");
  SERIALDEBUG.println(menuItem);

  if (menuItem == MENUINFOS) currentMaxOptions = MAXINFOPAGES; else currentMaxOptions = MAXOPTIONS;
  
  switch(menuItem) {
    case MENUDISPLAY:encoder->newSettings(-1, currentMaxOptions, g_displayMode, currentEncoderState);break;
    case MENUSERIAL:encoder->newSettings(-1, currentMaxOptions, (g_serialEnabled)?1:0, currentEncoderState);break;
    case MENURESET:encoder->newSettings(-1, currentMaxOptions, 0, currentEncoderState);break;
    case MENUSOUND:encoder->newSettings(-1, currentMaxOptions, (g_soundEnabled)?1:0, currentEncoderState);break;
    case MENUINFOS:encoder->newSettings(-1, currentMaxOptions, 0, currentEncoderState);break;
    default: SERIALDEBUG.println("Unknown menu item"); return;
  }

  lastInteractMS = millis();
  do {
    esp_task_wdt_reset();

    if(encoder->getState(currentEncoderState)) {
      lastInteractMS = millis(); 
    }
    
    currentOption = currentEncoderState.currentValue; 
    // Check for under- or overflow
    if (currentOption >= currentMaxOptions) currentOption = 0;
    if (currentOption < 0) currentOption = currentMaxOptions-1;
    encoder->getAndSet(currentOption,currentEncoderState,currentEncoderState);

    g_display.clearDisplay();
    g_display.setFont();
    g_display.setCursor(0,0);

    switch(menuItem) {
      case MENUDISPLAY:g_display.println(STR_DISPLAYMODE);break;
      case MENUSERIAL:g_display.println(STR_SERIALPRINTENABLED);break;
      case MENURESET:g_display.println(STR_FACTORYDEFAULT);break;
      case MENUSOUND:g_display.println(STR_SOUND);break;
      case MENUINFOS:showInfos(currentOption);break;
    }

    switch(menuItem) {
      case MENUDISPLAY: {
        switch(currentOption) {
          case OPTIONLEFT:snprintf(strData,MAXSTRDATALENGTH+1,STR_MINIMAL);break;
          case OPTIONRIGHT:snprintf(strData,MAXSTRDATALENGTH+1,STR_MAXIMAL);break;
        }
        g_display.setFont(&FreeSans9pt7b);
        centerText(strData,2);
        break;        
      }
      case MENUSOUND: {
        switch(currentOption) {
          case OPTIONLEFT: {
            drawIcon(SCREEN_WIDTH-50,(SCREEN_HEIGHT-ICON_HEIGHT)/2, ICONAUDIOOFF);
            snprintf(strData,MAXSTRDATALENGTH+1,STR_OFF);
            break;
          }
          case OPTIONRIGHT: {
            drawIcon(SCREEN_WIDTH-50,(SCREEN_HEIGHT-ICON_HEIGHT)/2, ICONAUDIOON);
            snprintf(strData,MAXSTRDATALENGTH+1,STR_ON);
            break;
          }
        }
        g_display.setFont(&FreeSans9pt7b);
        centerText(strData,0,-ICON_WIDTH/2);
        break;
      }
      case MENUSERIAL:
      case MENURESET: {
        switch(currentOption) {
          case OPTIONLEFT:snprintf(strData,MAXSTRDATALENGTH+1,STR_NO);break;
          case OPTIONRIGHT:snprintf(strData,MAXSTRDATALENGTH+1,STR_YES);break;
        }
        g_display.setFont(&FreeSans9pt7b);
        centerText(strData,2);
        break;
      }
      
    }
    // Overview dots
    for (int i=0;i<currentMaxOptions;i++) {
      if (i == currentOption) {
        g_display.drawFastHLine(SCREEN_WIDTH/2-1-(OVERVIEWGAP*(currentMaxOptions-1))/2 + OVERVIEWGAP*i,SCREEN_HEIGHT-2,2,WHITE);
      }
      g_display.drawFastHLine(SCREEN_WIDTH/2-1-(OVERVIEWGAP*(currentMaxOptions-1))/2 + OVERVIEWGAP*i,SCREEN_HEIGHT-1,2,WHITE);
    }

    // Timeout bar
    byte timeoutBar = SCREEN_HEIGHT - (SCREEN_HEIGHT*(millis() - lastInteractMS))/USERTIMEOUTMS;
    g_display.drawFastVLine(SCREEN_WIDTH-1, SCREEN_HEIGHT-timeoutBar, timeoutBar, WHITE);
    for (int i=0;i<4;i++) {
      g_display.drawPixel(SCREEN_WIDTH-1,i*(SCREEN_HEIGHT/4), BLACK);
    }
    g_display.display();
      
    if (millis()-lastInteractMS > USERTIMEOUTMS) {
      beep();
      exitLoop = true;
    }
    if (digitalRead(ROTARY_SW_PIN)==LOW) {
      if (menuItem == MENUSOUND) {
        g_soundEnabled = (currentOption == OPTIONRIGHT);
        beep(SHORTBEEP);
      }
      g_display.invertDisplay(true);
      delay(INVERTTIMEMS);
      g_display.invertDisplay(false);
      while (digitalRead(ROTARY_SW_PIN)==LOW) esp_task_wdt_reset(); // Wait for button release
      exitLoop = true;
    }
  } while (!exitLoop); // Until button was pressed or timeout

  switch(menuItem) {
    case MENUSOUND: {
      saveSettings();
      break;
    }
    case MENUDISPLAY: {
      g_displayMode = currentOption;
      saveSettings();
      break;
    }
    case MENUSERIAL: {
      g_serialEnabled = (currentOption == OPTIONRIGHT);
      setSerialMode();  
      saveSettings();
      break;
    }
    case MENURESET:{
      if (currentOption == OPTIONRIGHT) {
        initSettings();
        saveSettings();
        SERIALDEBUG.flush();
        ESP.restart();               
      }
      break;
    }
  }
}

// Device menu
void menu() {
  #define MENUITEMLENGHT 80
  char menuItem[MAXMENUITEMS][MENUITEMLENGHT] = {STR_START,STR_BATTERY,STR_IDLETIME,STR_THRESHOLD,STR_DISPLAY,STR_SOUND,STR_SERIAL,STR_LORATEST,STR_INFO,STR_RESET,STR_RESTART};
  int currentMenuItem = 0;
  int16_t  x, y;
  uint16_t w, h;
  bool exitLoop = false;
  unsigned long lastInteractMS = 0;
  
  NewEncoder::EncoderState currentEncoderState;
  NewEncoder encoder(ROTARY_CLK_PIN, ROTARY_DT_PIN, -1, MAXMENUITEMS, 0, FULL_PULSE);
  
  SERIALDEBUG.println("Start menu");

  g_mpu.enableSleep(true);

  if (!encoder.begin()) {
    SERIALDEBUG.println("Encoder Failed to Start. Check pin assignments and available interrupts. Aborting.");
    g_display.clearDisplay();
    g_display.setFont();
    g_display.setCursor(0,0);
    g_display.print(STR_COULDNOTSTARTROTARYENCODER);
    g_display.display();
    delay(5000);
    esp_task_wdt_reset();
    return;
  }

  lastInteractMS = millis();
  do {
    esp_task_wdt_reset();
    
    if (encoder.getState(currentEncoderState)) { // Rotary encoder rotation
      lastInteractMS = millis(); 
    }
    
    currentMenuItem = currentEncoderState.currentValue; 
    // Check for under- or overflow
    if (currentMenuItem >= MAXMENUITEMS) currentMenuItem = 0;
    if (currentMenuItem < 0) currentMenuItem = MAXMENUITEMS-1;
    encoder.getAndSet(currentMenuItem,currentEncoderState,currentEncoderState);

    if (digitalRead(ROTARY_SW_PIN)==LOW) {
      beep(SHORTBEEP);
      while (digitalRead(ROTARY_SW_PIN)==LOW) esp_task_wdt_reset(); // Wait for button release
      delay(BUTTONDEBOUNCEMS); // Debounce
      switch(currentMenuItem) {
        case MENUSTART: {
          exitLoop = true;
          break;
        }
        case MENUTHRESHOLD:
        case MENUTIMEOUT: {
          if (currentMenuItem == MENUTHRESHOLD) g_mpu.enableSleep(false);
          changeValue(&encoder, currentMenuItem);
          // Restore encoder settings for the menu
          encoder.newSettings(-1,MAXMENUITEMS, currentMenuItem,currentEncoderState); 
          if (currentMenuItem == MENUTHRESHOLD) g_mpu.enableSleep(true);
          break;
        }
        case MENUINFOS: // No perfect way to use changeSettings for displaying infos, but it works
        case MENUSOUND:
        case MENUDISPLAY:
        case MENURESET:
        case MENUSERIAL: {
          changeSetting(&encoder,currentMenuItem);
          // Restore encoder settings for the menu
          encoder.newSettings(-1,MAXMENUITEMS, currentMenuItem,currentEncoderState); 
          break;
        }
        case MENULORATEST: sendLoRaTest();break;
        case MENUBATTERY: {
          batteryView();
          // Restore encoder settings for the menu
          encoder.newSettings(-1,MAXMENUITEMS, currentMenuItem,currentEncoderState); 
          break;
        }
        case MENURESTART: resetIdleTimeHistory();exitLoop = true;break;
        default: exitLoop = true;
      }
      lastInteractMS = millis();
    }

    g_display.clearDisplay();
    g_display.setFont(&FreeSans9pt7b);

    centerText(menuItem[currentMenuItem], -2);

    // Timeout bar
    byte timeoutBar = SCREEN_HEIGHT - (SCREEN_HEIGHT*(millis() - lastInteractMS))/USERTIMEOUTMS;
    g_display.drawFastVLine(SCREEN_WIDTH-1, SCREEN_HEIGHT-timeoutBar, timeoutBar, WHITE);
    #define SEPARATORS 4
    for (int i=0;i<SEPARATORS;i++) {
      g_display.drawPixel(SCREEN_WIDTH-1,i*(SCREEN_HEIGHT/SEPARATORS), BLACK);
    }
    // Overview dots
    for (int i=0;i<MAXMENUITEMS;i++) {
      if (i == currentMenuItem) {
        g_display.drawFastHLine(SCREEN_WIDTH/2-1-(OVERVIEWGAP*(MAXMENUITEMS-1))/2 + OVERVIEWGAP*i,SCREEN_HEIGHT-2,2,WHITE);
      }
      g_display.drawFastHLine(SCREEN_WIDTH/2-1-(OVERVIEWGAP*(MAXMENUITEMS-1))/2 + OVERVIEWGAP*i,SCREEN_HEIGHT-1,2,WHITE);
    }
    // Batterywarning
    #define BATTERY_WIDTH 14
    #define BATTERY_HEIGHT 6
    if (((millis()/1000) & 1) & (g_vBat <= LOWBATWARNING)) {
      g_display.drawRect(1,0,BATTERY_WIDTH-1,BATTERY_HEIGHT,WHITE);
      g_display.drawFastVLine(0,1,BATTERY_HEIGHT-2,WHITE);
    }
    
    g_display.display();

    if (millis() - lastInteractMS > USERTIMEOUTMS) {
      beep();
      exitLoop = true;
    }
  } while (!exitLoop);
  encoder.end();

  // Enable MPU and send start signal
  g_mpu.enableSleep(false);
  // Send LoRa message
  sendLoRa(RCSIGNATURE + 
    (((unsigned long) ID & 7) << 24) +
    ((unsigned long) (g_vBat <= LOWBATWARNING) << 23) +
    ((((unsigned long) round(g_vBat*10))&63) << 17) +
    STARTMESSAGE);
}

// Check, if acceleration was detected (passive = without updating history and timestamp)
void checkMotion(bool passive=false) {
  if (g_mpu.getMotionInterruptStatus()) { // Detection
    if (!g_detectionActive) {
      unsigned long duration = getRunTimeS()-g_lastIdleStartTimeS;
      if (!passive) addToIdleTimeHistory(duration);
      SERIALDEBUG.print("Idle ends after ");
      SERIALDEBUG.print(duration);
      SERIALDEBUG.println("s");
    }
    g_detectionActive = true;
  } else { // Idle
    if (g_detectionActive) {
      if (!passive) g_lastIdleStartTimeS = getRunTimeS();
      SERIALDEBUG.println("Idle starts");
    }
    g_detectionActive = false;
  }
}

// Change value mode
void changeValue(NewEncoder *encoder, byte menuItem) {
  unsigned long lastInteractMS; // Last interaction time to detect timeout
  NewEncoder::EncoderState currentEncoderState;
  bool exitLoop = false;
  
  SERIALDEBUG.print("Change value mode for ");
  SERIALDEBUG.println(menuItem);

  switch(menuItem) {
    case MENUTIMEOUT:encoder->newSettings(10, 600, g_idleTimeTimeoutS, currentEncoderState);break;
    case MENUTHRESHOLD:encoder->newSettings(1, 255, g_motionThreshold, currentEncoderState);break;
    default: SERIALDEBUG.println("Unknown menu item"); return;
  }

  lastInteractMS = millis();
  do {
    esp_task_wdt_reset();
    
    g_display.clearDisplay();
    g_display.setFont();
    
    switch(menuItem) {
      case MENUTIMEOUT:showTimeout();break;
      case MENUTHRESHOLD:{
        checkMotion(true);
        showThreshold();
        #define BOXSIZE 32 // Detection rectangle width/height
        if (g_detectionActive) {
          g_display.fillRect(SCREEN_WIDTH-BOXSIZE-4, 0, BOXSIZE, BOXSIZE, WHITE);
        } else {
          g_display.drawRect(SCREEN_WIDTH-BOXSIZE-4, 0, BOXSIZE, BOXSIZE, WHITE);      
        }
        break;
      }
      default: SERIALDEBUG.println("Unknown menu item"); return;
    }

    // Timeout bar
    byte timeoutBar = SCREEN_HEIGHT - (SCREEN_HEIGHT*(millis() - lastInteractMS))/USERTIMEOUTMS;
    g_display.drawFastVLine(SCREEN_WIDTH-1, SCREEN_HEIGHT-timeoutBar, timeoutBar, WHITE);
    for (int i=0;i<4;i++) {
      g_display.drawPixel(SCREEN_WIDTH-1,i*(SCREEN_HEIGHT/4), BLACK);
    }
    g_display.display();

    if (encoder->getState(currentEncoderState)) {
      switch(menuItem) {
        case MENUTIMEOUT:g_idleTimeTimeoutS = currentEncoderState.currentValue;break;
        case MENUTHRESHOLD:{
          g_motionThreshold = currentEncoderState.currentValue;
          g_mpu.setMotionDetectionThreshold(g_motionThreshold);
          break;
        }
        default: SERIALDEBUG.println("Unknown menu item"); return;
      }      
      lastInteractMS = millis();
    }
    if (millis()-lastInteractMS > USERTIMEOUTMS) {
      beep();
      exitLoop = true;
    }
    if (digitalRead(ROTARY_SW_PIN)==LOW) {
      beep(SHORTBEEP);
      g_display.invertDisplay(true);
      delay(INVERTTIMEMS);
      g_display.invertDisplay(false);
      while (digitalRead(ROTARY_SW_PIN)==LOW) esp_task_wdt_reset(); // Wait for button release
      exitLoop = true;
    }
  } while (!exitLoop); // Until button was pressed or timeout

  if (menuItem != MENUINFOS) saveSettings;  
}

// Reset device in case of a problem
void reset() {
  SERIALDEBUG.println("Reset");
  SERIALDEBUG.flush();
  beep(LONGBEEP);
  ESP.restart();  
}

// Reset list of longest idle time periods
void resetIdleTimeHistory() {
  for (int i=0;i<MAXTIMEHISTORY;i++) {
    g_idleTimeSHistory[i]=0;   
  }
}

// Add entry to sorted list of longest idle time periods
void addToIdleTimeHistory(unsigned long newValue) {
  for (int i=0;i<MAXTIMEHISTORY;i++) {
    if (newValue > g_idleTimeSHistory[i]) {
      if (i < MAXTIMEHISTORY-1) {
        for (int j = MAXTIMEHISTORY-1;j>i;j--) {
          g_idleTimeSHistory[j] = g_idleTimeSHistory[j-1];
        }
      }
      g_idleTimeSHistory[i] = newValue;
      break;
    }
  }
}

// Show list of longest idle time periods
void showIdleTimeHistory() {
  SERIALDEBUG.println("History:");
  for (int i=0;i<MAXTIMEHISTORY;i++) {
    SERIALDEBUG.print(g_idleTimeSHistory[i]);
    SERIALDEBUG.println("s"); 
  }
}

// Read average voltage for battery, when powered by battery (When powered by USB this is the battery loader output voltage)
void updateVbat() {
  #define MAXSAMPLES 10
  static int samples[MAXSAMPLES];
  static int currentSample = -1;
  long sum = 0;
  int count = 0;
  static unsigned long lastReadMS = 0;

  if ((lastReadMS == 0) || (millis()-lastReadMS > 500)) {
    if (currentSample == -1) {
      for (int i=0;i<MAXSAMPLES;i++) samples[i] = -1;
      currentSample = 0;
    }
    samples[currentSample] = analogRead(VBAT_PIN);
    currentSample++;
    if (currentSample >= MAXSAMPLES) currentSample = 0;

    for (int i=0;i<MAXSAMPLES;i++) {
      if (samples[i] != -1) {
        sum += samples[i];
        count++; 
      }
    }
    g_vBat = round(map(sum/count,0,3400,0,4152)*10+5)/10000.0f;    
    lastReadMS = millis();
  }
}

// Enable/Disable Serial
void setSerialMode() {
  if (g_serialEnabled) {
    setCpuFrequencyMhz(80); // Serial does not work with cpu frequency <=40 MHz!
    Serial.begin(115200);
    SERIALDEBUG.println("Serial enabled");    
  } else {
    setCpuFrequencyMhz(40); // Reduce CPU frequency to reduce power consumption
    Serial.end();
  }
}

void setup() {  
  bool earlyButtonPressed = false; // Button pressed early on startup
  
  // Pin modes for rotary encoder (My KY-040 pins have builtin pullup resistors)
  pinMode(ROTARY_CLK_PIN,INPUT);
  pinMode(ROTARY_DT_PIN,INPUT);
  pinMode(ROTARY_SW_PIN,INPUT);

  // Passive buzzer
  pinMode(BUZZER_PIN,OUTPUT);

  // Analog input connected to a voltage divider (47k/100k resistors) for measure of the battery/loader voltage
  pinMode(VBAT_PIN,INPUT);

  // Init device settings
  initSettings();

  // Enable Serial, if enabled in initSettings
  setSerialMode(); 

  if (digitalRead(ROTARY_SW_PIN) == LOW) { // Check button
    SERIALDEBUG.println("Button pressed on startup");
    earlyButtonPressed = true;
  }

  // Read configuration from EEPROM, if valid signature and version found
  EEPROM.begin(EEPROM_SIZE);
  int addr = EEPROM_STARTADDR;
  if ((EEPROM.readByte(addr) == EEPROM_SIGNATURE) && (EEPROM.readByte(addr+sizeof(byte)) == EEPROM_VERSION) ) {
    SERIALDEBUG.println("Valid EEPROM data found");
    addr+=2*sizeof(byte);
    g_motionThreshold = EEPROM.readByte(addr);
    addr+=sizeof(byte);
    g_displayMode = EEPROM.readByte(addr);
    addr+=sizeof(byte);
    g_serialEnabled = (EEPROM.readByte(addr)==1);
    addr+=sizeof(byte);
    g_soundEnabled = (EEPROM.readByte(addr)==1);
    addr+=sizeof(byte);
    g_idleTimeTimeoutS = EEPROM.readULong(addr);
  } else { // No valid data found => Use default
    SERIALDEBUG.println("No valid EEPROM data found");
  }

  // Enable Serial, if enabled by EEPROM settings
  setSerialMode();
  
  if (esp_reset_reason() == ESP_RST_TASK_WDT) { // Previous WDT-Reset?
    beep(LONGBEEP);
    delay(200);
    beep(LONGBEEP);
    SERIALDEBUG.println("Previous WDT detected");
  } else {
    switch(esp_sleep_get_wakeup_cause()) {
      case ESP_SLEEP_WAKEUP_EXT0: { // Wakeup by button (often mpu wakeup triggers faster)
        SERIALDEBUG.println("EXT0 wakeup");
        g_wakeUpByButton = true;
        break;
      }
      case ESP_SLEEP_WAKEUP_EXT1: {
        SERIALDEBUG.print("EXT1 wakeup ");
        SERIALDEBUG.println(esp_sleep_get_ext1_wakeup_status(), BIN);
        if ((esp_sleep_get_ext1_wakeup_status() >> MPU_IRQ) & 1 == 1) {
          SERIALDEBUG.println("mpu wakeup");
          g_wakeUpByMPU = true;
          g_detectionActive = true;
          if (earlyButtonPressed) g_wakeUpByButton = true;
        }
        break;
      }
      case ESP_SLEEP_WAKEUP_TIMER: {
        SERIALDEBUG.println("Timer wakeup");
        if (earlyButtonPressed) g_wakeUpByButton = true;
        break;      
      }
      default:beep(LASER);break;
    }
  }

  // Enable WDT for current task
  esp_task_wdt_init(WDTTIMEOUT,true);
  esp_task_wdt_add(NULL);

  // I2C init
  Wire.begin(SDA_PIN,SCL_PIN);

  // OLED display init
  if(!g_display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    SERIALDEBUG.println("Display allocation failed");
    delay(5000);
    esp_task_wdt_reset();
    reset();
  }

  // Font color
  g_display.setTextColor(WHITE);

  // Reduce power consumption
  g_display.dim(true);


  if (g_firstBoot) { // Show init only on normal startup
    g_display.clearDisplay();
    g_display.print(STR_INIT);
    g_display.display();
  }

  // Init LoRa
  LoRa.setPins(LORA_NSS_PIN,LORA_RST_PIN,LORA_IRQ_PIN);
  if (!LoRa.begin(433E6)) { // 433 MHz
    SERIALDEBUG.println("Failed to find LoRa module");
    g_display.clearDisplay();
    g_display.setCursor(0,0);
    g_display.print(STR_COULDNOTFINDLORA);
    g_display.display();
    delay(5000);
    esp_task_wdt_reset();
    reset();
  } else {
    // Init LoRa and enable sleep mode (will be enabled when sending signals)
    LoRa.setSyncWord(0xA5);
    LoRa.onReceive(callbackLoraReceived);
    LoRa.sleep();
  }
  
  // Init MPU6050 accelerometer and gyroscope
  if (!g_mpu.begin(0x68, &Wire, 0)) {
    SERIALDEBUG.println("Failed to find MPU6050 chip");
    g_display.clearDisplay();
    g_display.setCursor(0,0);
    g_display.print(STR_COULDNOTFINDMPU);
    g_display.display();
    delay(5000);
    esp_task_wdt_reset();
    reset();
  }

  // Setup motion detection
  g_mpu.setHighPassFilter(MPU6050_HIGHPASS_UNUSED);
  g_mpu.setMotionDetectionThreshold(g_motionThreshold);
  g_mpu.setMotionDetectionDuration(1);
  g_mpu.setInterruptPinLatch(true);
  g_mpu.setInterruptPinPolarity(false);
  g_mpu.setMotionInterrupt(true);
  // Disable mpu to reduce current consumption to ~4mA when not waked up by mpu (mpu will be enabled afterwards, when needed)
  if (!g_wakeUpByMPU) g_mpu.enableSleep(true);  

  // Reset list of longest idle times only on power on and not on wake up
  if (g_firstBoot) resetIdleTimeHistory();

  esp_task_wdt_reset();

  // Update/Read current battery/loader voltage
  updateVbat();
}

void loop() {
  #define MAXSTRDATALENGTH 80
  char strData[MAXSTRDATALENGTH+1];
  int16_t x, y;
  uint16_t w, h;
  unsigned long idleDuration;

  esp_task_wdt_reset();
  
  // Goto menu, if rotary encoder button was pressed, waked up by button or power on
  if ((digitalRead(ROTARY_SW_PIN) == LOW) || g_wakeUpByButton || g_firstBoot) {
    // Blank screen
    g_display.clearDisplay();
    g_display.display();
    if (!g_firstBoot) beep(SHORTBEEP);
    while (digitalRead(ROTARY_SW_PIN)==LOW) esp_task_wdt_reset(); // Wait for button release
    delay(BUTTONDEBOUNCEMS); // Debounce
    g_firstBoot = false;
    g_wakeUpByButton = false;
    menu();
    SERIALDEBUG.println("Waiting for next end of idle time");
    g_lastIdleStartTimeS = getRunTimeS();
  }
  // Check, if motion was detected
  if (!g_wakeUpByMPU) checkMotion();

  g_display.clearDisplay();
  g_display.setFont();
  g_display.setCursor(0,0);

  // Update/Read current battery/loader voltage
  updateVbat();
  if (g_detectionActive) idleDuration = 0; else idleDuration = getRunTimeS()-g_lastIdleStartTimeS;

  if (idleDuration >= g_idleTimeTimeoutS) { // Idle timeout
    SERIALDEBUG.println("Timeout reached");

    // Send LoRa message
    bool confirmed = sendLoRa(RCSIGNATURE + 
      (((unsigned long) ID & 7) << 24) +
      ((unsigned long) (g_vBat <= LOWBATWARNING) << 23) +
      ((((unsigned long) round(g_vBat*10))&63) << 17) +
      ENDMESSAGE);
    showIdleTimeHistory();

    g_display.clearDisplay();
    if (confirmed) drawIcon(SCREEN_WIDTH-ICON_WIDTH-1-8,(SCREEN_HEIGHT-ICON_HEIGHT)/2, ICONOK);
    g_display.setFont(&FreeSans12pt7b);
    centerText(STR_FIN);
    drawIcon(SCREEN_WIDTH-ICON_WIDTH-1-8,(SCREEN_HEIGHT-ICON_HEIGHT)/2, ICONOK);
    g_display.display();

    SERIALDEBUG.print("Sleep for button press");
    SERIALDEBUG.flush();

    // Set mpu to sleep mode
    g_mpu.enableSleep(true);
    // Enable only deep sleep wake up by button
    esp_sleep_enable_ext0_wakeup(ROTARY_SW_PIN,0);
    // Deep sleep
    esp_deep_sleep_start();
  } else {
    g_display.setFont();
    if (g_displayMode > MODEMINIMAL) { // More than minimal display mode      
      snprintf(strData,MAXSTRDATALENGTH+1,"%.2fV",g_vBat); 
      g_display.print(strData);

      // Show longest idle times
      int historyItems = MAXTIMEHISTORY;
      if (historyItems > 3) historyItems = 3;
      for (int i=0;i<historyItems;i++) {
        if (g_idleTimeSHistory[i] >= 0) {
          g_display.setCursor(0,(i+1)*8);
          snprintf(strData,MAXSTRDATALENGTH+1,"%i:%4lus",i+1,g_idleTimeSHistory[i]); 
          g_display.print(strData);
        }
      }

      // Show idle duration
      snprintf(strData,MAXSTRDATALENGTH+1,"%i",idleDuration);
      
      g_display.setFont(&FreeSans18pt7b);
      g_display.getTextBounds(strData, 0, 0, &x, &y, &w, &h);
      g_display.setCursor(SCREEN_WIDTH-1-w-x,(SCREEN_HEIGHT+h)/2);    
      g_display.print(strData);
    };
  }

  // Draw sprite, when motion was detected 
  if (g_detectionActive) {
    if (g_displayMode == MODEMINIMAL) { // Delayed sprite and display clear in minimal mode to reduce power consumption
      unsigned long lastMS = millis();
      while (millis()-lastMS < 200) { // Delay 200 ms
        g_display.clearDisplay();
        drawSprite(SCREEN_WIDTH-SPRITE_WIDTH-1,(SCREEN_HEIGHT-SPRITE_HEIGHT)/2-1,100);
        if (digitalRead(ROTARY_SW_PIN) == LOW) return; // When button was pressed
        g_display.display();
      }
      g_detectionActive = false;
      g_wakeUpByMPU = false;
      return; // Exit loop to check motion and button again (and do not goto deep sleep, if motions are continuous) 
    }
  }
  
  g_display.display();

  if (g_displayMode == MODEMINIMAL) { // Goto deep sleep in minimal display mode
    // Deep sleep (Wake up possible by button, mpu or timeout)
    esp_sleep_enable_ext0_wakeup(ROTARY_SW_PIN,0);
    esp_sleep_enable_ext1_wakeup((1ULL << MPU_IRQ),ESP_EXT1_WAKEUP_ANY_HIGH);
    esp_sleep_enable_timer_wakeup((g_idleTimeTimeoutS-idleDuration)*1000*1000);

    g_mpu.getMotionInterruptStatus(); // Reset/read interrupt status 

    SERIALDEBUG.print("Sleep for ");
    SERIALDEBUG.print(g_idleTimeTimeoutS-idleDuration);
    SERIALDEBUG.print(" seconds");
    SERIALDEBUG.flush();
    
    g_display.ssd1306_command(SSD1306_DISPLAYOFF); // Seems to reduce power consumption by 1mA
    // Deep sleep
    esp_deep_sleep_start();    
  }
}
