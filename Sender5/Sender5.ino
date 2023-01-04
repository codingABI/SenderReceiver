/*
 * Project: Sender5 (Ding15)
 * Description:
 * Sensor for the slot of a mailbox. When the slot is opened, a magnetic reed switch 
 * triggers and sends a LoRa signal to the receiver. Additionally once per day the 
 * current battery voltage and the magnetic reed switch state will also be sent to the receiver.
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
 * Hardware:
 * - Microcontroller ATmega328PU (without crystal, in 8 MHz-RC mode. Board manager: "ATmega328 on a breadboard (8 MHz internal clock)" ) 
 * - HT7333 voltage regulator
 * - Lora SX1278 Ra-02
 * - 18650 Battery with integrated protection against deep discharge
 * - Magnetic reed-switch "normally closed" with a pullup resistor
 * - Control LED (blinks every 8 seconds) which can be enabled/disabled on demand with physical jumper SW2A
 *
 * Current consumption (measured on SW1A while LED switch SW2A was opened) 
 * - 28uA in deep sleep (5uA is HT7333 idle current)
 * - 5mA after wake up
 * - 120mA while sending via LoRa
 *
 * History: 
 * 20221226, Initial version
 */

#define DEBUG false  // true for Serial.print
#define SERIALDEBUG if (DEBUG) Serial

#include <SPI.h>
#include <LoRa.h>
#include <avr/sleep.h> 
#include <avr/wdt.h> 
 
#define LED_PIN 5 // Alive LED (Switched on for a short time, during sending the signal or switched on for 8 seconds when Lora init fails)
#define SENSW_PIN 3 // Magnetic reed-switch "normally closed" with external pullup resistor
#define NSS_PIN 10 // Lora NSS
#define RST_PIN 9 // Lora RST
#define DIO0_PIN 2 // Lora IRQ
#define ID 5 // Nbr for the individual sender

#define RCSIGNATURE 0b00111000000000000000000000000000UL // Signature for signals (only the first 5 bits are the signature)
/* Signal (32-bit):
 * 5 bit: Signature
 * 3 bit: ID
 * 1 bit: Low battery
 * 6 bit: Vcc (0-63)
 * 1 bit: Mail box sensor switch
 * 1 bit: Pin change interrupt (1 if signal was triggered by the mailbox slot; 0 if signal is the daily status signal)
 * 15 bit: unused
 */

// -------- Global variables --------
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
    delay(100);
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
  SERIALDEBUG.begin(115200);

  SERIALDEBUG.print("Init Sender ID ");
  SERIALDEBUG.println(ID);
  
  // Alive LED
  pinMode(LED_PIN,OUTPUT);

  enableWatchdogTimer(); // Watchdog timer
  // Pin mode and Pin change interrupt for the switch
  pinMode(SENSW_PIN,INPUT);
  pciSetup(SENSW_PIN);

  // Init LoRa
  LoRa.setPins(NSS_PIN,RST_PIN,DIO0_PIN);
  
  SERIALDEBUG.println("Init LoRa Sender");

  if (!LoRa.begin(433E6)) { // 433 MHz
    digitalWrite(LED_PIN,HIGH);
    SERIALDEBUG.println("Starting LoRa failed");
    // Starting LoRa failed => wait for watchdog reset
    while (1);
  }
  LoRa.setSyncWord(0xA5); // ranges from 0-0xFF, default 0x34, see API docs

  delay(1000);
}

void loop() {
  int id = 5;
  int Switch;
  int Vcc;
  int lowBattery;
  static int msgCount = 0;
  byte oldADCSRA;
  unsigned long sendData;

  if (v_pinChangeInterrupt) { // Wait after PinChangeInterrupt to get switch state more stabile
    SERIALDEBUG.println("Delay after PinChangeInterrupt"); 
    delay(200);
    wdt_reset(); 
  } 
  
  Switch = digitalRead(SENSW_PIN); // Read switch state

  SERIALDEBUG.print("Switch state ");
  SERIALDEBUG.println(Switch);
  
  Vcc = getBandgap(); // Vcc in 10mV units
  /* Enable low battery warning when below threshold Vcc 300 = 3.0V 
   * We measure the voltage after the voltage regulator and not the real battery voltage,
   * but this should be enough to detect, when the battery is low 
   */
  if (Vcc < 300 ) lowBattery=1; else lowBattery=0; 
  SERIALDEBUG.print("Battery ");
  SERIALDEBUG.println(Vcc);

  wdt_reset();

  digitalWrite(LED_PIN,HIGH);

  // Send data
  sendData = RCSIGNATURE +
    (((unsigned long) id & 7) <<24) + 
    ((unsigned long) lowBattery << 23) +
    (((((unsigned long) Vcc+5)/10) & 63) << 17) +
    ((unsigned long) Switch << 16) +
    ((unsigned long) v_pinChangeInterrupt << 15);

  SERIALDEBUG.print("Data to send ");
  SERIALDEBUG.println(sendData);

  // Send data
  SERIALDEBUG.println("Start LoRa sending");

  LoRa.beginPacket();  
  LoRa.print(sendData);
  LoRa.endPacket();
  
  SERIALDEBUG.println("End LoRa sending");

  digitalWrite(LED_PIN,LOW);
  
  // Goto to sleep
  SERIALDEBUG.println("Going to sleep");
  SERIALDEBUG.flush();
  
  LoRa.sleep();
  wdt_reset();

  // Save ADC status
  oldADCSRA = ADCSRA;
  // Disable ADC
  ADCSRA = 0;  
  // Disable TWI (Two Wire Interface = I2C) 
  TWCR = 0;
   
  v_pinChangeInterrupt = false;

  // Sleep mode
  sleep(3600*24); // Sleep for 24 hours 
  
  // Restore ADC
  ADCSRA = oldADCSRA;

  SERIALDEBUG.println("Wake up");
}
