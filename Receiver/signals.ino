/* ----------- Stuff for receiving signals and and sensor data ----------
 * License: 2-Clause BSD License
 * Copyright (c) 2023 codingABI
 */

// setup PIR sensor
void setupPIR() {
  pinMode(PIR_PIN,INPUT);
}

// setup BME280 sensor
void setupBME280() {
  DisplayMessage msg;
  // I2C for BME280 sensor
  Wire.begin(SDA_PIN,SCL_PIN);
  if (!bme.begin(0x76,&Wire)) {
    msg.id = ID_ALERT;
    snprintf(msg.strData,MAXMSGLENGTH+1,"BME280 not found");
    xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
    g_BME280ready = false;
  } else g_BME280ready = true;

}

// setup LoRA receiver
void setupLoRa() {
  DisplayMessage msg;
  // Lora 
  LoRa.setPins(LORA_NSS, LORA_RST, LORA_IRQ);
  if (!LoRa.begin(433E6)) { //433E6 433 MHz
    msg.id = ID_ALERT;
    snprintf(msg.strData,MAXMSGLENGTH+1,"LoRa not found");
    xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
    g_LORAready = false;
  } else {
    LoRa.setSyncWord(0xA5);
    LoRa.onReceive(callbackLoraReceived);
    // put the radio into receive mode
    LoRa.receive();
    g_LORAready = true;
  }
}

// setup ASK receiver
void setupASKReceiver() {
  pinMode(RCSWITCH_PIN, INPUT);
  g_433MHzRCSwitch.enableReceive(digitalPinToInterrupt(RCSWITCH_PIN));
}

// parse received signal
void checkSignal(unsigned long received) {
  DisplayMessage msg;
  int id, lowBattery;
  static unsigned long lastUnknownReceived = 0;
  static bool duplicateWarned = false;
  int receivedInt;
  float receivedFloat;
  time_t now;
  struct tm * ptrTimeinfo;
  #define MAXSTRDATALENGTH 127
  char strData[MAXSTRDATALENGTH+1];
  bool unknownReceived;
  static byte sensor1LowBatteryThreshold = MAXLOWBATTERYNOTIFICATIONS;
  static byte sensor3LowBatteryThreshold = MAXLOWBATTERYNOTIFICATIONS;
  static byte sensor5LowBatteryThreshold = MAXLOWBATTERYNOTIFICATIONS;
  
  time(&now);
  unknownReceived = true;

  SERIALDEBUG.println(received);
  if (received==1316116) { // Test signal
    unknownReceived = false;
    SERIALDEBUG.println("TestSensor received");
    msg.id=ID_INFO;
    snprintf(msg.strData,MAXMSGLENGTH+1,"TestSensor received");
    xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
    g_mailAlert = true;
    msg.id = ID_REFESHBUTTONS;
    msg.strData[0]='\0';
    xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
    beep();
  }

  if (received==1315924) { // Wifi off signal
    unknownReceived = false;
    if (now - g_last433MhzWifiOff > SECS_PER_MIN) { // Accept only one signal per minute
      SERIALDEBUG.println("WLAN OFF received");
      msg.id=ID_INFO;
      snprintf(msg.strData,MAXMSGLENGTH+1,"WLAN OFF received");
      xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
      g_last433MhzWifiOff = now;

      if (WiFi.status() == WL_CONNECTED) {
        g_server.end();
        WiFiOff();
        g_nextWifiOn = 0;
      }
    }
  }

  if (received==1381716) { // Bath off signal
    unknownReceived = false;
    if (now - g_last433MhzBathOff > SECS_PER_MIN) { // Accept only one signal per minute
      SERIALDEBUG.println("Bath OFF received");
      g_last433MhzBathOff = now;
      msg.id=ID_INFO;
      snprintf(msg.strData,MAXMSGLENGTH+1,"Bath OFF received");
      xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );       
    }
  }

  if (received==1381717) { // Bath on signal
    unknownReceived = false;
    if (now - g_last433MhzBathOn > SECS_PER_MIN) { // Accept only one signal per minute
      SERIALDEBUG.println("Bath ON received");
      g_last433MhzBathOn = now;
      msg.id=ID_INFO;
      snprintf(msg.strData,MAXMSGLENGTH+1,"Bath ON received");
      xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );       
    }
  }

  if (received==1315153) { // Cafe on signal
    unknownReceived = false;
    if (now - g_last433MhzCafeOn > SECS_PER_MIN) { // Accept only one signal per minute
      SERIALDEBUG.println("Cafe ON received");
      g_last433MhzCafeOn = now;
      msg.id=ID_INFO;
      snprintf(msg.strData,MAXMSGLENGTH+1,"Cafe ON received");
      xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );               
    }
  }
  if (received==1315156) { // Cafe off signal
    unknownReceived = false;
    if (now - g_last433MhzCafeOff > SECS_PER_MIN) { // Accept only one signal per minute
      SERIALDEBUG.println("Cafe OFF received");
      g_last433MhzCafeOff = now;
      msg.id=ID_INFO;
      snprintf(msg.strData,MAXMSGLENGTH+1,"Cafe OFF received");
      xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );       
    }
  }

  if (received==1315921) { // Wifi on signal
    unknownReceived = false;
    if (now - g_last433MhzWifiOn > SECS_PER_MIN) { // Accept only one signal per minute
      SERIALDEBUG.println("WLAN ON received");
      g_nextWifiOn = now + SECS_PER_MIN * 5; // Power on wifi in 5 minutes to give access point enough time for booting
      g_last433MhzWifiOn = now;

      ptrTimeinfo = localtime(&g_nextWifiOn);
      msg.id=ID_INFO;
      snprintf(msg.strData,MAXMSGLENGTH+1,"WLAN start at %02d:%02d",ptrTimeinfo->tm_hour,ptrTimeinfo->tm_min);
      xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
    }
  }

  if ((received & (0b11111 << 27)) == RCSIGNATURE) { // ASK Sensor with my signature received
    unknownReceived = false;
    SERIALDEBUG.println("433Mhz received");
    id = ((received >> 24) & 7);
    lowBattery = (received >> 23) & 1;

    // UTC time
    ptrTimeinfo = gmtime(&now);

    if (id == 1) { // Stairs
      /* Signal (32-bit):
       * 5 bit: Signature
       * 3 bit: ID
       * 1 bit: Low battery
       * 6 bit: Vcc (0-63)
       * 3 bit: unused
       * 7 bit: Humidity (127 marks invalid value)
       * 7 bit: Temperature with +50 degree shift to provide a range of -50 to +76 degree Celsius (77 marks an invalid value)
       */
      if (now - g_pendingSensorData.sensor1LastDataTime > SECS_PER_MIN) { // Accept only one signal per minute
        SERIALDEBUG.println("Sensor 1 received");

        g_pendingSensorData.sensor1LowBattery = lowBattery;
        Blynk.virtualWrite(V2, g_pendingSensorData.sensor1LowBattery);
        if (g_pendingSensorData.sensor1LowBattery == 1) {
          if (sensor1LowBatteryThreshold > 0) { // Battery warning
            Blynk.logEvent("lowbattery","Sensor Treppenhaus");
            sensor1LowBatteryThreshold --;
          }
        } else sensor1LowBatteryThreshold = MAXLOWBATTERYNOTIFICATIONS;

        receivedInt = (int) (received & 0b1111111) - 50;
        if (receivedInt != 0b1111111 - 50) {
          g_pendingSensorData.sensor1Temperature = receivedInt;
          Blynk.virtualWrite(V0, g_pendingSensorData.sensor1Temperature);
        }
        
        receivedInt = ((received >> 7) & 0b1111111);
        if (receivedInt != 0b1111111) {
          g_pendingSensorData.sensor1Humidity = receivedInt;
          Blynk.virtualWrite(V1, g_pendingSensorData.sensor1Humidity);
        }
        
        receivedInt = ((received >> 17) & 0b111111);
        if (receivedInt != 0b111111) g_pendingSensorData.sensor1Vcc = receivedInt;

        g_pendingSensorData.sensor1LastDataTime = now;
        
        snprintf(strData,MAXSTRDATALENGTH+1,"UTC %02d.%02d.%04d;%02d:%02d;%d;%d;%d;%d",
          ptrTimeinfo->tm_mday,
          ptrTimeinfo->tm_mon + 1,
          ptrTimeinfo->tm_year + 1900,
          ptrTimeinfo->tm_hour,
          ptrTimeinfo->tm_min,
          g_pendingSensorData.sensor1LowBattery,
          g_pendingSensorData.sensor1Temperature,
          g_pendingSensorData.sensor1Humidity,
          g_pendingSensorData.sensor1Vcc
          );

        SERIALDEBUG.println(strData);
        msg.id=ID_INFO;
        snprintf(msg.strData,MAXMSGLENGTH+1,"Sensor 1 %i,%i,%i,%i",g_pendingSensorData.sensor1LowBattery,g_pendingSensorData.sensor1Temperature,g_pendingSensorData.sensor1Humidity,g_pendingSensorData.sensor1Vcc);
        xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
      } else SERIALDEBUG.println("Sensor 1 duplicate received");
    }

    if (id == 3) { // Window
      /* Signal (32-bit):
       * 5 bit: Signature
       * 3 bit: ID
       * 1 bit: Low battery
       * 6 bit: Vcc (0-63)
       * 1 Bit: Switch1
       * 1 Bit: Switch2
       * 1 bit: unused
       * 7 bit: Humidity (127 marks invalid value)
       * 7 bit: Temperature with +50 degree shift to provide a range of -50 to +76 degree Celsius (77 marks an invalid value)
       */
      if (now - g_pendingSensorData.sensor3LastDataTime > SECS_PER_MIN) { // Accept only one signal per minute
        SERIALDEBUG.println("Sensor 3 received");

        g_pendingSensorData.sensor3LowBattery = lowBattery;
        Blynk.virtualWrite(V9, g_pendingSensorData.sensor3LowBattery);
        if (g_pendingSensorData.sensor3LowBattery == 1) {
          if (sensor3LowBatteryThreshold > 0) { // Battery warning
            Blynk.logEvent("lowbattery","Sensor Fenster");
            sensor3LowBatteryThreshold --;
          }
        } else sensor3LowBatteryThreshold = MAXLOWBATTERYNOTIFICATIONS;

        g_pendingSensorData.sensor3Switch1 = ((received >> 16) & 1);
        Blynk.virtualWrite(V7, g_pendingSensorData.sensor3Switch1);

        g_pendingSensorData.sensor3Switch2 = ((received >> 15) & 1);
        Blynk.virtualWrite(V8, g_pendingSensorData.sensor3Switch2);

        receivedInt = ((received >> 17) & 0b111111);
        if (receivedInt != 0b111111) g_pendingSensorData.sensor3Vcc = receivedInt;

        receivedInt = (int) (received & 0b1111111) - 50;
        if (receivedInt != 0b1111111 - 50) {
          g_pendingSensorData.sensor3Temperature = receivedInt;
          Blynk.virtualWrite(V13, g_pendingSensorData.sensor3Temperature);
        }
        
        receivedInt = ((received >> 7) & 0b1111111);
        if (receivedInt != 0b1111111) {
          g_pendingSensorData.sensor3Humidity = receivedInt;
          Blynk.virtualWrite(V14, g_pendingSensorData.sensor3Humidity);
        }
      
        g_pendingSensorData.sensor3LastDataTime = now;

        snprintf(strData,MAXSTRDATALENGTH+1,"UTC %02d.%02d.%04d;%02d:%02d;%d;%d;%d;%d;%d;%d",
          ptrTimeinfo->tm_mday,
          ptrTimeinfo->tm_mon + 1,
          ptrTimeinfo->tm_year + 1900,
          ptrTimeinfo->tm_hour,
          ptrTimeinfo->tm_min,
          g_pendingSensorData.sensor3LowBattery,
          g_pendingSensorData.sensor3Switch1,
          g_pendingSensorData.sensor3Switch2,
          g_pendingSensorData.sensor3Temperature,
          g_pendingSensorData.sensor3Humidity,
          g_pendingSensorData.sensor3Vcc
          );

        SERIALDEBUG.println(strData);
        msg.id=ID_INFO;
        snprintf(msg.strData,MAXMSGLENGTH+1,"Sensor 3 %i,%i,%i,%i,%i,%i",g_pendingSensorData.sensor3LowBattery,g_pendingSensorData.sensor3Switch1,g_pendingSensorData.sensor3Switch2,g_pendingSensorData.sensor3Temperature,g_pendingSensorData.sensor3Humidity,g_pendingSensorData.sensor3Vcc);
        xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
      } else SERIALDEBUG.println("Sensor 3 duplicate received");
    }
    
    if (id == 4) { // SolarPoweredRev2
      /* Signal (32-bit):
       * 5 bit: Signature
       * 3 bit: ID
       * 1 bit: Low battery
       * 6 bit: Vcc (0-63)
       * 7 bit: unused
       * 10 bit: Runtime
       */
      if (now - g_pendingSensorData.sensor4LastDataTime > SECS_PER_MIN) { // Accept only one signal per minute
        SERIALDEBUG.println("Sensor 4 received");

        g_pendingSensorData.sensor4LowBattery = lowBattery;

        receivedInt = ((received >> 17) & 0b111111);
        if (receivedInt != 0b111111) g_pendingSensorData.sensor4Vcc = receivedInt;

        g_pendingSensorData.sensor4Runtime = (int) received & 1023;

        g_pendingSensorData.sensor4LastDataTime = now;
        snprintf(strData,MAXSTRDATALENGTH+1,"UTC %02d.%02d.%04d;%02d:%02d;%d;%d;%d",
          ptrTimeinfo->tm_mday,
          ptrTimeinfo->tm_mon + 1,
          ptrTimeinfo->tm_year + 1900,
          ptrTimeinfo->tm_hour,
          ptrTimeinfo->tm_min,
          g_pendingSensorData.sensor4LowBattery,
          g_pendingSensorData.sensor4Vcc,
          g_pendingSensorData.sensor4Runtime
          );

        SERIALDEBUG.println(strData);
        msg.id=ID_INFO;
        snprintf(msg.strData,MAXMSGLENGTH+1,"Sensor 4 %i,%i,%i",g_pendingSensorData.sensor4LowBattery,g_pendingSensorData.sensor4Vcc,g_pendingSensorData.sensor4Runtime);
        xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );   

        Blynk.virtualWrite(V4, g_pendingSensorData.sensor4LowBattery);
        Blynk.virtualWrite(V10, (float) (g_pendingSensorData.sensor4Vcc/10.0f));
        Blynk.virtualWrite(V11, g_pendingSensorData.sensor4Runtime);        
      } else SERIALDEBUG.println("Sensor 4 duplicate received");
    }

    if (id == 5) { // Mail box
      /* Signal (32-bit):
       * 5 bit: Signature
       * 3 bit: ID
       * 1 bit: Low battery
       * 6 bit: Vcc (0-63)
       * 1 bit: Mail box sensor switch
       * 1 bit: Pin change event (1 if signal was triggered by the mailbox slot; 0 if signal is the daily status signal)
       * 15 bit: unused
       */
      if (now - g_pendingSensorData.sensor1LastDataTime > SECS_PER_MIN) { // Accept only one signal per minute
        SERIALDEBUG.println("Sensor 5 received");
        
        g_pendingSensorData.sensor5LowBattery = lowBattery;
        Blynk.virtualWrite(V12, g_pendingSensorData.sensor5LowBattery);
        if (g_pendingSensorData.sensor5LowBattery == 1) {
          if (sensor5LowBatteryThreshold > 0) { // Low battery warning
            Blynk.logEvent("lowbattery","Sensor Briefkasten");
            sensor5LowBatteryThreshold --;
          }
        } else sensor5LowBatteryThreshold = MAXLOWBATTERYNOTIFICATIONS;
  
        receivedInt = ((received >> 17) & 0b111111);
        if (receivedInt != 0b111111) g_pendingSensorData.sensor5Vcc = receivedInt;
  
        g_pendingSensorData.sensor5Switch1 = ((received >> 16) & 1);
        g_pendingSensorData.sensor5PCI1 = ((received >> 15) & 1);
        if ((!g_mailAlert) && ((g_pendingSensorData.sensor5PCI1 == 1) || (g_pendingSensorData.sensor5Switch1 == 1))) { // If alert is not already enabled => enable alert
          g_pendingBlynkMailAlert = alert; // Wifi is not alway available => Send to when connected
          g_mailAlert = true;
          msg.id = ID_REFESHBUTTONS;
          msg.strData[0]='\0';
          xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
          beep();
        }
  
        g_pendingSensorData.sensor5LastDataTime = now;
  
        snprintf(strData,MAXSTRDATALENGTH+1,"UTC %02d.%02d.%04d;%02d:%02d;%d;%d;%d;%d",
          ptrTimeinfo->tm_mday,
          ptrTimeinfo->tm_mon + 1,
          ptrTimeinfo->tm_year + 1900,
          ptrTimeinfo->tm_hour,
          ptrTimeinfo->tm_min,
          g_pendingSensorData.sensor5LowBattery,
          g_pendingSensorData.sensor5Switch1,
          g_pendingSensorData.sensor5PCI1,
          g_pendingSensorData.sensor5Vcc
          );
  
        SERIALDEBUG.println(strData);
        msg.id=ID_INFO;
        snprintf(msg.strData,MAXMSGLENGTH+1,"Sensor 5 %i,%i,%i,%i",g_pendingSensorData.sensor5LowBattery,g_pendingSensorData.sensor5Switch1,g_pendingSensorData.sensor5PCI1,g_pendingSensorData.sensor5Vcc);
        xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
      } else SERIALDEBUG.println("Sensor 5 duplicate received");
    }
  } 

  if (unknownReceived) { // Foreign signal
    if ((lastUnknownReceived == 0) || (lastUnknownReceived != received)) { // Spam filter
      msg.id=ID_INFO;
      snprintf(msg.strData,MAXMSGLENGTH+1,"Received %lu",received);
      xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
      lastUnknownReceived = received;
      duplicateWarned = false;
    } else {
      if (!duplicateWarned && (lastUnknownReceived == received)) {
        msg.id=ID_INFO;
        snprintf(msg.strData,MAXMSGLENGTH+1,"Suppressing duplicates...");
        xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );  
        duplicateWarned = true;       
      }
    }
  } else lastUnknownReceived = 0;
}

// Check for ASK 433Mhz receiver signals
void checkForASKSignals() {
  unsigned long received;
  // Process 433Mhz receiver signals
  if (g_433MHzRCSwitch.available()) {
    received = g_433MHzRCSwitch.getReceivedValue();
    checkSignal(received);
    g_433MHzRCSwitch.resetAvailable();
  }
}

// Check for LoRa signals
void checkForLoRaSignals() {
  unsigned long received;
  char* strPtr;

  if (v_loraReceived) {
    v_loraReceived = false;
    while(LoRa.available()) {
      if (xSemaphoreTake( g_semaphoreSPIBus, 10000 / portTICK_PERIOD_MS) == pdTRUE) { // Take SPI semaphore to prevent problems with the TFT
  
        String LoRaData = LoRa.readString();
        received = strtol (LoRaData.c_str(),&strPtr,10);

        checkSignal(received);

        xSemaphoreGive( g_semaphoreSPIBus );
      } else {
        SERIALDEBUG.println("Error: SPI Semaphore Timeout");
        beep();
        beep();
        beep();
      }
      vTaskDelay(1 / portTICK_PERIOD_MS); // Block SPI not too long (SPI is needed for TFT and to process the displayMsgQueue)
    }
  }
}

// Create fake data in demo mode
void processDemoMode() {
  static int demoCounter = 0;
  static unsigned long lastDemoMS = 0;
  DisplayMessage msg;
  sensorData newSensorData, emptySensorData;  
  time_t now;
  
  if (g_demoModeEnabled && (millis() - lastDemoMS > 2000)) { // Only in demo mode and every 2 seconds
    // create "fake data"
    demoCounter++;
    g_pendingSensorData.sensor1LowBattery = random(0,2);
    g_pendingSensorData.sensor1Temperature = random(10,30);
    g_pendingSensorData.sensor1Humidity = sin((2*PI/360.0f)*((demoCounter *20) % 360) )*40.0f+41.0f; // random(30,70);      
    g_pendingSensorData.sensor3LowBattery = random(0,2);
    g_pendingSensorData.sensor3Switch1 = random(0,2);
    g_pendingSensorData.sensor3Switch2 = random(0,2);
    g_pendingSensorData.sensor3Temperature = random(10,30);
    g_pendingSensorData.sensor3Humidity = random(30,70);      
    g_pendingSensorData.sensor5LowBattery = random(0,2);
    
    newSensorData = g_pendingSensorData;
    newSensorData.time = now;
      
    // Send "fake data" to display
    xQueueSend( displayDatQueue, ( void * ) &newSensorData, portMAX_DELAY );

    // Reset pending sensor data
    g_pendingSensorData = emptySensorData;

    lastDemoMS = millis();
  }
}

// Get temperature, humidity and pressure from BME sensor
void readBME280() {
  DisplayMessage msg;
  int receivedInt;
  float receivedFloat;
  time_t now;

  // Read BME280 once per minute (in demo mode every second)
  if (((millis() - g_lastBME280MS > SECS_PER_MIN * 1000) && g_BME280ready && g_timeSet)||
    ((millis() - g_lastBME280MS >= 1) && g_demoModeEnabled)){
    time(&now);

    receivedFloat = bme.readTemperature();
    if (isnan(receivedFloat)) { 
      SERIALDEBUG.print("Error: temp. data from BME280 NAN");      

      msg.id=ID_ALERT;
      snprintf(msg.strData,MAXMSGLENGTH+1,"BME-temp. NAN");
      xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
    } else {
      // In comparision to a DS18B20 is my BME 1 degree to high => -1
      receivedInt = (int) round(receivedFloat-1);
      if ((receivedInt >= -40) && (receivedInt <=85)) { 
        g_pendingSensorData.sensor2Temperature = receivedInt;
        g_pendingSensorData.sensor2LastDataTime = now;
      } else {
        SERIALDEBUG.print("Error: Invalid temp. data from BME280 ");      
        SERIALDEBUG.println(receivedInt);
  
        msg.id=ID_ALERT;
        snprintf(msg.strData,MAXMSGLENGTH+1,"Invalid BME-temp. %d",receivedInt);
        xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
        msg.id=ID_ALERT;
        snprintf(msg.strData,MAXMSGLENGTH+1,"%f",receivedFloat);
        xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
      }
    }
    
    receivedInt =  (int) round(bme.readHumidity());
    if ((receivedInt > 0) && (receivedInt <=100)) { 
      g_pendingSensorData.sensor2Humidity = receivedInt;
      g_pendingSensorData.sensor2LastDataTime = now;
    } else {
      SERIALDEBUG.print("Error: Invalid humidity data from BME280 ");      
      SERIALDEBUG.println(receivedInt);      

      msg.id=ID_ALERT;
      snprintf(msg.strData,MAXMSGLENGTH+1,"Invalid BME-hum. %d",receivedInt);
      xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );

    }
    // Pressure for sea level based on my location
    receivedInt = (int) round (bme.seaLevelForAltitude(440,bme.readPressure() / 100.0F)); 
    if ((receivedInt >= 300) && (receivedInt <=1100)) { 
      g_pendingSensorData.sensor2Pressure = receivedInt;
      g_pendingSensorData.sensor2LastDataTime = now;
    } else {
      SERIALDEBUG.print("Error: Invalid pressure data from BME280 ");      
      SERIALDEBUG.println(receivedInt);
      
      msg.id=ID_ALERT;
      snprintf(msg.strData,MAXMSGLENGTH+1,"Invalid BME-press. %d",receivedInt);
      xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
    }
    g_lastBME280MS = millis();
  }
}

// Read PIR sensor
void readPIR(){
  DisplayMessage msg;

  if ((digitalRead(PIR_PIN) == HIGH) && (g_PIREnabled)) {
    if (millis() - g_lastPIRChangeMS > 1000) {
      msg.id = ID_DISPLAYON;
      msg.strData[0]='\0';
      xQueueSend( displayMsgQueue, ( void * ) &msg, portMAX_DELAY );
      g_lastPIRChangeMS = millis();
    } 
  }
}
