/*
 * Project: Sender3 (Ding11)
 * Description:
 * Magnetic reed switch sensor to detect if a window is open or tilted. 
 * Sends the window state, temperature, humidity and battery state 
 * every 30 minutes or triggered by magnetic reed switch change via 
 * a 433MHz-ASK to a receiver.
 *
 * License: 2-Clause BSD License
 * Copyright (c) 2023 codingABI
 * For details see: License.txt
 * 
 * created by codingABI https://github.com/codingABI/SenderReceiver
 * 
 * External code: 
 * getBandgap() from https://forum.arduino.cc/t/measuring-battery-voltage-conditionally/319327/5
 * For details see externalCode.ino
 *
 * Used external libraries from Arduino IDE Library Manager
 * - RCSwitch (by sui77,fingolfin) 
 * - DHT sensor library (by Adafruit)
 *
 * Hardware:
 * - Microcontroller ATmega328P (without crystal, in 8 MHz-RC mode. Board manager: "ATmega328 on a breadboard (8 MHz internal clock)" ) 
 * - DHT22 sensor
 * - 433MHz FS1000A sender
 * - 3x AA-Batteries without voltage regulation (runtime >8 months, dependent on usage of rechargeable or normal batteries)
 * - Control LED (blinks every 8 seconds) which can be enabled by a physical jumper
 * - Two magnetic reed switches "normally closed" to detect open windows
 *
 * History: 
 * 20230106, Initial version with PCB
 */
#include <avr/sleep.h> 
#include <avr/wdt.h> 
#include <RCSwitch.h>
#include <DHT.h>

// -------- Pin definitions --------
#define LED_PIN 6 // Led pin
#define SENDER_PIN 7 // 433 MHz sender
#define VCCOUTPUT_PIN 8 // Switchable Vcc for DHT sensor and 433-MHz sender
#define DHT_PIN 4 // DHT Sensor
#define SWITCH1_PIN 2 // for future use
#define SWITCH2_PIN 3 // for future use

#define RCSIGNATURE 0b00111000000000000000000000000000UL // Signature for 433MHz signals (only the first 5 bits are the signature)
/* Signal (32-bit):
 * 5 bit: Signature
 * 3 bit: ID
 * 1 bit: Low battery
 * 6 bit: Vcc (0-63)
 * 1 Bit: Switch1 "Reed1/Tilted" (Switch2==0 and Switch1==1=>Tilted)
 * 1 Bit: Switch2 "Reed2/Open" (Switch2==1=>Open)
 * 1 bit: unused
 * 7 bit: Humidity (127 marks invalid value)
 * 7 bit: Temperature with +50 degree shift to provide a range of -50 to +76 degree Celsius (77 marks an invalid value)
 */

// Invalid sensor values (to distinguish valid values)
#define NOVALIDHUMIDITYDATA_SENDER1 127
#define NOVALIDTEMPERATUREDATA_SENDER1 127-50

// -------- Global variables --------
#define NOVALUEKNOWN 255
byte g_lastValue = NOVALUEKNOWN; // Last sensor value
volatile bool v_pinChangeInterrupt = false;

// Sleep for X seconds
void sleep(int seconds) {
  for(int i=0; i < seconds/8; i++) { // Repeat 8 second intervals from WatchdogTimer to reach the needed seconds 
    set_sleep_mode(SLEEP_MODE_PWR_DOWN); // Set sleep mode to "Power-down"
    cli();
    sleep_enable(); // Set SE bit in SMCR register (Required for sleep_mode)
    sei();
    sleep_cpu(); // Goto sleep mode 
    sleep_disable(); // Disable SE bit from SMCR register

    enableWatchdogTimer(); // Reenable Watchdog (without the ISR will be startet once and the device resets)
    
    pinMode(LED_PIN,OUTPUT);
    digitalWrite(LED_PIN,HIGH);
    delay(50);
    pinMode(LED_PIN,INPUT);
    if (v_pinChangeInterrupt) break; // End loop when IRQ was triggered by switch
  }
}

// Enable WatchdogTimer
void enableWatchdogTimer() {
  noInterrupts();
  // Set bit 3+4 (WDE+WDCE bits) 
  // From Atmel datasheet: "...Within the next four clock cycles, write the WDE and 
  // watchdog prescaler bits (WDP) as desired, but with the WDCE bit cleared. 
  // This must be done in one operation..."
  WDTCSR = WDTCSR | B00011000;
  // Set Watchdog-Timer duration to 8 seconds
  WDTCSR = B00100001;
  // Enable Watchdog interrupt by WDIE bit and enable device reset via 1 in WDE bit.
  // From Atmel datasheet: "...The third mode, Interrupt and system reset mode, combines the other two modes by first giving an interrupt and then switch to system reset mode. This mode will for instance allow a safe shutdown by saving critical parameters before a system reset..." 
  WDTCSR = WDTCSR | B01001000;
  interrupts();
}

// Interrupt service routine for the Watchdog-Timer
ISR(WDT_vect) {
}

ISR (PCINT2_vect) // handle pin change interrupt for D0 to D7 here
{
  v_pinChangeInterrupt = true;
}

void pciSetup(byte pin) {
  *digitalPinToPCMSK(pin) |= bit (digitalPinToPCMSKbit(pin)); // Enable pin
  PCIFR |= bit(digitalPinToPCICRbit(pin)); // Clear pending interrupts
  PCICR |= bit(digitalPinToPCICRbit(pin)); // Enable interrupt for group
}

void setup() {
  enableWatchdogTimer(); // Watchdog timer
  pinMode(SWITCH1_PIN,INPUT);
  pinMode(SWITCH2_PIN,INPUT);
  // Enable pin change interrupt for the magnetic reed switches
  pciSetup(SWITCH1_PIN);
  pciSetup(SWITCH2_PIN);
}

void loop(){
  float temperature, humidity, currentValue;
  int id = 3;
  int Vcc;
  int Switch1, Switch2;
  RCSwitch mySwitch = RCSwitch();
  int oldADCSRA;
  byte maxLoops;
  int lowBattery;
  bool acceptableValues;
  DHT dht(DHT_PIN, DHT22);

  if (v_pinChangeInterrupt) { delay(1000);wdt_reset(); } // Wait after PinChangeEvent to get switch state more stabile
  // Read switch states
  Switch1 = digitalRead(SWITCH1_PIN);
  Switch2 = digitalRead(SWITCH2_PIN);
 
  // Enable Vcc for DHC and sender
  pinMode(VCCOUTPUT_PIN,OUTPUT);
  digitalWrite(VCCOUTPUT_PIN,HIGH);

  dht.begin();

  // 433MHz sequence
  pinMode(SENDER_PIN,OUTPUT);
  // Enable transmit   
  mySwitch.enableTransmit(SENDER_PIN);
  // Reduce packets
  mySwitch.setRepeatTransmit(5);

  Vcc = getBandgap();
  humidity=NAN;
  temperature=NAN;
  acceptableValues = false;
  maxLoops=10;
  do { // Repeat measurements while "Not a number" or temperature difference is higher then 10 degree since last measurement
    delay(2500); // Wait 2 secs for DHT

    currentValue = dht.readHumidity();
    if (!isnan(currentValue)) humidity = currentValue;
    
    currentValue = dht.readTemperature();
    if (!isnan(currentValue)) temperature = currentValue;

    maxLoops--;
    wdt_reset();

    if (!isnan(humidity) && !isnan(temperature) && ((g_lastValue == NOVALUEKNOWN) || (abs(temperature - g_lastValue) <= 10))) acceptableValues = true;  
  } while ((acceptableValues == false) && (maxLoops > 0));

  g_lastValue = temperature;
  // Enable battery warning
  if (Vcc < 320 ) lowBattery=1; else lowBattery=0;
  
  // Mark not a number as invalid data
  if (isnan(humidity)) humidity = NOVALIDHUMIDITYDATA_SENDER1;
  if (isnan(temperature)) temperature = NOVALIDTEMPERATUREDATA_SENDER1;

  // Send data
  mySwitch.send(RCSIGNATURE +
    (((unsigned long) id & 7) <<24) + 
    ((unsigned long) lowBattery << 23) +
    (((((unsigned long) Vcc+5)/10) & 63) << 17) +
    ((round(temperature) + 50) & 127) + (((round(humidity) & 127) << 7)) +
    (((unsigned long) Switch1) <<16) + (((unsigned long) Switch2) <<15) , 32);

  mySwitch.disableTransmit();

  wdt_reset();

  // Save ADC status
  oldADCSRA = ADCSRA;
  // Disable ADC
  ADCSRA = 0;  
  // Disable TWI (Two Wire Interface) = I2C 
  TWCR = 0;

  pinMode(SENDER_PIN,INPUT);
  digitalWrite(VCCOUTPUT_PIN,LOW);
  pinMode(VCCOUTPUT_PIN,INPUT);
  pinMode(DHT_PIN,INPUT);

  v_pinChangeInterrupt = false;

  // Sleep mode
  sleep(1795); // Sleep for ~30 minutes (5 Seconds less then sensor 1 to avoid collisions)
  
  // Restore ADC
  ADCSRA = oldADCSRA;
}
