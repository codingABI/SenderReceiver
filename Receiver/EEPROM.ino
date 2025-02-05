/* ----------- Stuff for read/writes to EEPROM ----------
 * License: 2-Clause BSD License
 * Copyright (c) 2023-2025 codingABI
 */

// Timeout in millisecs for EEPROM semaphore
#define EEPROM_SEMAPHORETIMEOUTMS 10000

// First byte at start in EEPROM
#define EEPROM_SIGNATURE 15
// Second byte at start in EEPROM
#define EEPROM_VERSION 43
// Length for EEPROM_SIGNATURE and EEPROM_VERSION
#define EEPROM_HEADERSIZE 2
// Startaddress for WiFi in EEPROM
#define EEPROMADDR_WIFI 10
// Startaddress for line graph in EEPROM
#define EEPROMADDR_GRAPH (EEPROMADDR_WIFI+EEPROM_HEADERSIZE+MAXSSIDLENGTH+1+MAXPASSWORDLENGTH+1)
// Startaddress for mail alert flag
#define EEPROMADDR_MAILALERT (EEPROMADDR_GRAPH+EEPROM_HEADERSIZE+GRAPHDATAMAXITEMS*2+1)
// Startaddress for max uptime
#define EEPROMADDR_MAXUPTIME (EEPROMADDR_MAILALERT+EEPROM_HEADERSIZE+1)
// Startaddress for last data set
#define EEPROMADDR_LASTDATASET (EEPROMADDR_MAXUPTIME+EEPROM_HEADERSIZE+2)

// Read WiFi config from EEPROM
void getWiFiConnectionDataFromEEPROM(char strWifiSSID[],char strWifiPassword[]) {
  int addr = EEPROMADDR_WIFI;

  // Default values
  if (strWifiSSID != NULL) strWifiSSID[0] = '\0';
  if (strWifiPassword != NULL) strWifiPassword[0] = '\0';

  if (xSemaphoreTake( g_semaphoreEEPROM, EEPROM_SEMAPHORETIMEOUTMS / portTICK_PERIOD_MS) == pdTRUE) {
    if ( (EEPROM.read(addr) == EEPROM_SIGNATURE) && (EEPROM.read(addr+1) == EEPROM_VERSION) ) { // Check signature
      addr+=EEPROM_HEADERSIZE;
      if (strWifiSSID != NULL) {
        for (int i=0;i<=MAXSSIDLENGTH;i++) {
          strWifiSSID[i] = EEPROM.read(addr)-g_eepromenc[i%(strlen(g_eepromenc))];
          addr+=sizeof(byte);
        }
        strWifiSSID[MAXSSIDLENGTH] = '\0';
      }
      if (strWifiPassword != NULL) {
        for (int i=0;i<=MAXPASSWORDLENGTH;i++) {
          strWifiPassword[i] = EEPROM.read(addr)-g_eepromenc[i%(strlen(g_eepromenc))];
          addr+=sizeof(byte);
        }
        strWifiPassword[MAXPASSWORDLENGTH] = '\0';
      }
    }
    xSemaphoreGive( g_semaphoreEEPROM );
  } else {
    // Could not get semaphore in time
    SERIALDEBUG.println("Error: EEPROM Semaphore Timeout");
  }
}

// Write WiFi config to EEPROM
void setWiFiConnectionDataToEEPROM(char strWifiSSID[],char strWifiPassword[]) {
  int addr = EEPROMADDR_WIFI;

  if (strWifiSSID == NULL) return;
  if (strWifiPassword == NULL) return;

  if (xSemaphoreTake( g_semaphoreEEPROM, EEPROM_SEMAPHORETIMEOUTMS / portTICK_PERIOD_MS) == pdTRUE) {
    EEPROM.write(addr,EEPROM_SIGNATURE);
    addr+=sizeof(byte);
    EEPROM.write(addr,EEPROM_VERSION);
    addr+=sizeof(byte);

    for (int i=0;i<=MAXSSIDLENGTH;i++) {
      EEPROM.write(addr,strWifiSSID[i]+g_eepromenc[i%(strlen(g_eepromenc))]);
      addr+=sizeof(byte);
    }
    for (int i=0;i<=MAXPASSWORDLENGTH;i++) {
      EEPROM.write(addr,strWifiPassword[i]+g_eepromenc[i%(strlen(g_eepromenc))]);
      addr+=sizeof(byte);
    }
    EEPROM.commit();

    xSemaphoreGive( g_semaphoreEEPROM );
  } else {
    // Could not get semaphore in time
    SERIALDEBUG.println("Error: EEPROM Semaphore Timeout");
  }
}

// Read mail alert from EEPROM
bool getMailAlertFromEEPROM() {
  bool result = false; // Default value

  int addr = EEPROMADDR_MAILALERT;
  if (xSemaphoreTake( g_semaphoreEEPROM, EEPROM_SEMAPHORETIMEOUTMS / portTICK_PERIOD_MS) == pdTRUE) {

    if ( (EEPROM.read(addr) == EEPROM_SIGNATURE) && (EEPROM.read(addr+1) == EEPROM_VERSION) ) { // Check signature
      addr+=EEPROM_HEADERSIZE;
      result = EEPROM.read(addr);
    }
    xSemaphoreGive( g_semaphoreEEPROM );
  } else {
    // Could not get semaphore in time
    SERIALDEBUG.println("Error: EEPROM Semaphore Timeout");
  }
  return result;
}

// Write mail alert to EEPROM
void setMailAlertToEEPROM(bool value) {
  int addr = EEPROMADDR_MAILALERT;
  if (xSemaphoreTake( g_semaphoreEEPROM, EEPROM_SEMAPHORETIMEOUTMS / portTICK_PERIOD_MS) == pdTRUE) {
    EEPROM.write(addr,EEPROM_SIGNATURE);
    addr+=sizeof(byte);
    EEPROM.write(addr,EEPROM_VERSION);
    addr+=sizeof(byte);
    EEPROM.write(addr,value);

    EEPROM.commit();

    xSemaphoreGive( g_semaphoreEEPROM );
  } else {
    // Could not get semaphore in time
    SERIALDEBUG.println("Error: EEPROM Semaphore Timeout");
  }
}

// Read line graph data from EEPROM
void getGraphDataFromEEPROM(byte graphData[], byte graphHour[],byte &graphLastPos) {
  int addr = EEPROMADDR_GRAPH;

  if (graphData == NULL) return;
  if (graphHour == NULL) return;
  
  if (xSemaphoreTake( g_semaphoreEEPROM, EEPROM_SEMAPHORETIMEOUTMS / portTICK_PERIOD_MS) == pdTRUE) {

    if ( (EEPROM.read(addr) == EEPROM_SIGNATURE) && (EEPROM.read(addr+1) == EEPROM_VERSION) ) { // Check signature
      addr+=EEPROM_HEADERSIZE;

      for (int i=0;i<GRAPHDATAMAXITEMS;i++) {
        graphData[i] = EEPROM.read(addr);
        addr+=sizeof(byte);
        graphHour[i] = EEPROM.read(addr);
        addr+=sizeof(byte);
      }
      graphLastPos = EEPROM.read(addr);
    }
    xSemaphoreGive( g_semaphoreEEPROM );
  } else {
    // Could not get semaphore in time
    SERIALDEBUG.println("Error: EEPROM Semaphore Timeout");
  }
}

// Write line graph data to EEPROM
void setGraphDataToEEPROM(byte graphData[], byte graphHour[],byte graphLastPos) {
  int addr = EEPROMADDR_GRAPH;

  if (graphData == NULL) return;
  if (graphHour == NULL) return;

  if (xSemaphoreTake( g_semaphoreEEPROM, EEPROM_SEMAPHORETIMEOUTMS / portTICK_PERIOD_MS) == pdTRUE) {
    EEPROM.write(addr,EEPROM_SIGNATURE);
    addr+=sizeof(byte);
    EEPROM.write(addr,EEPROM_VERSION);
    addr+=sizeof(byte);

    for (int i=0;i<GRAPHDATAMAXITEMS;i++) {
      EEPROM.write(addr,graphData[i]);
      addr+=sizeof(byte);
      EEPROM.write(addr,graphHour[i]);
      addr+=sizeof(byte);
    }
    EEPROM.write(addr,graphLastPos);

    EEPROM.commit();

    xSemaphoreGive( g_semaphoreEEPROM );
  } else {
    // Could not get semaphore in time
    SERIALDEBUG.println("Error: EEPROM Semaphore Timeout");
  }
}

// Read runtime from EEPROM
unsigned int getMaxUptimeFromEEPROM() {
  unsigned int result = 0; // Default value

  int addr = EEPROMADDR_MAXUPTIME;
  if (xSemaphoreTake( g_semaphoreEEPROM, EEPROM_SEMAPHORETIMEOUTMS / portTICK_PERIOD_MS) == pdTRUE) {

    if ( (EEPROM.read(addr) == EEPROM_SIGNATURE) && (EEPROM.read(addr+1) == EEPROM_VERSION) ) { // Check signature
      addr+=EEPROM_HEADERSIZE;
      result = (EEPROM.read(addr) << 8);
      addr+=sizeof(byte);
      result += EEPROM.read(addr);
    }
    xSemaphoreGive( g_semaphoreEEPROM );
  } else {
    // Could not get semaphore in time
    SERIALDEBUG.println("Error: EEPROM Semaphore Timeout");
  }
  return result;
}

// Write runtime alert to EEPROM
void setMaxUptimeToEEPROM(unsigned int value) {
  int addr = EEPROMADDR_MAXUPTIME;
  if (xSemaphoreTake( g_semaphoreEEPROM, EEPROM_SEMAPHORETIMEOUTMS / portTICK_PERIOD_MS) == pdTRUE) {
    EEPROM.write(addr,EEPROM_SIGNATURE);
    addr+=sizeof(byte);
    EEPROM.write(addr,EEPROM_VERSION);
    addr+=sizeof(byte);
    EEPROM.write(addr,(value>>8) & 0xff);
    addr+=sizeof(byte);
    EEPROM.write(addr,value & 0xff);

    EEPROM.commit();

    xSemaphoreGive( g_semaphoreEEPROM );
  } else {
    // Could not get semaphore in time
    SERIALDEBUG.println("Error: EEPROM Semaphore Timeout");
  }
}

// Read last dataset from EEPROM
sensorData getLastDatasetFromEEPROM() {
  sensorData result; // Default values

  int addr = EEPROMADDR_LASTDATASET;
  if (xSemaphoreTake( g_semaphoreEEPROM, EEPROM_SEMAPHORETIMEOUTMS / portTICK_PERIOD_MS) == pdTRUE) {

    if ( (EEPROM.read(addr) == EEPROM_SIGNATURE) && (EEPROM.read(addr+1) == EEPROM_VERSION) ) { // Check signature
      addr+=EEPROM_HEADERSIZE;
      EEPROM.get(addr,result);
    }
    xSemaphoreGive( g_semaphoreEEPROM );
  } else {
    // Could not get semaphore in time
    SERIALDEBUG.println("Error: EEPROM Semaphore Timeout");
  }
  return result;
}

// Write last dataset to EEPROM
void setLastDatasetToEEPROM(sensorData value) {
  int addr = EEPROMADDR_LASTDATASET;
  if (xSemaphoreTake( g_semaphoreEEPROM, EEPROM_SEMAPHORETIMEOUTMS / portTICK_PERIOD_MS) == pdTRUE) {
    EEPROM.write(addr,EEPROM_SIGNATURE);
    addr+=sizeof(byte);
    EEPROM.write(addr,EEPROM_VERSION);
    addr+=sizeof(byte);
    EEPROM.put(addr,value);

    EEPROM.commit();

    xSemaphoreGive( g_semaphoreEEPROM );
  } else {
    // Could not get semaphore in time
    SERIALDEBUG.println("Error: EEPROM Semaphore Timeout");
  }
}
