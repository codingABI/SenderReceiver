/* ----------- Stuff for the webserver ----------
 * License: 2-Clause BSD License
 * Copyright (c) 2023 codingABI
 */
 
// Inline background pattern for webpages (My first svg background pattern :-) )
#define BACKGROUNDPATTERN "        background-image: url(\"data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='48' height='24'%3E%3Cpath stroke='none' fill='%23252525' d='M23,4l-8 -4l-8 4l16 8l16 -8l-8 -4z M7,12 l-8 4v8l16 -8z M39,12 l8 4l2 -1v8l-2 1-16 -8z'/%3E%3Cpath stroke='none' fill='%23171717' d='M15,0l8 4v-5h-8zM23,12v13h-6l-2 -1v-8l-8 -4v-8zM31 0l8 4v8l8 4l2 -1v-18h-18zM31 16v8l2 1h14v-1z'/%3E%3Cpath stroke='%23000000' stroke-width='1' stroke-linecap='round' stroke-linejoin='round' fill='none' d='M23,-1v5l-8 -4v-1m0 1l-8 4l16 8l16 -8l-8 -4v-1m0 1l-8 4m-16 0v8l8 4v8m-2 1l2 -1l2 1m-2 -9l-16 8v1m0 -1l-2 -1m2 -7v-16l-2 -1m2 1l2 -1m-4 16l2 1l8 -4m16 0v13m6 0l2 -1l2 1m-2 -1v-8l8 -4v-8m10 19l-2 1v1m0 -1l-16 -8m8 -4l8 4l2 -1l-2 1v-16l-2 -1m2 1l2 -1'/%3E%3C/svg%3E\")"

// Task for a simple webserver
void taskWebServer(void * parameter) {
  #define RECEIVELIMIT 80
  char header[RECEIVELIMIT];
  char payload[RECEIVELIMIT];
  #define MAXFILENAMELENGTH 59
  char strFile[MAXFILENAMELENGTH+1];
  char* ptrString;
  unsigned int clientConnectTimeMS = 0;
  bool payloadTooLarge;

  SERIALDEBUG.print("taskWebServer, CPU:");
  SERIALDEBUG.print(xPortGetCoreID());
  SERIALDEBUG.print(", Prio:");
  SERIALDEBUG.print(uxTaskPriorityGet(NULL));
  SERIALDEBUG.print(", MinStackFreeBytes:");
  SERIALDEBUG.print(uxTaskGetStackHighWaterMark(NULL));
  SERIALDEBUG.println();

  for (;;) {

    // Check for client requests
    WiFiClient client = g_server.available();
    if (client) {
      SERIALDEBUG.print("new client ");
      SERIALDEBUG.print("IP: ");
      SERIALDEBUG.println(client.remoteIP());

      clientConnectTimeMS = millis();
      boolean currentLineIsBlank = true;
      header[0] = '\0';
  
      while (client.connected()) {
        if ((millis() - clientConnectTimeMS) > 100) {
          // Disconnect hanging clients (Connected, but does not send requests)
          client.stop();
          SERIALDEBUG.println("timeout => client disconnected");
          vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        if (client.available()) {
          // Read header (Only the first chars, because only GET or POST request are allowed)
          clientConnectTimeMS = millis();
          char c = client.read();
          SERIALDEBUG.write(c);
          if (strlen(header) < RECEIVELIMIT-1) { 
            header[strlen(header)+1] = '\0';
            header[strlen(header)] = c;
          }
          if (c == '\n' && currentLineIsBlank) { // After a blank line the header is complete
            strFile[0] = '\0';
            if (g_wifiAPMode && strncmp(header,"POST /cfg HTTP/1.1",18) == 0) { // POST-requests for configuration changes are only allowed in AP mode             
              payload[0] = '\0';
              payloadTooLarge = false;
              while(client.available()) { // Read payload
                if (strlen(payload) < RECEIVELIMIT-1) { 
                  payload[strlen(payload)+1] = '\0';
                  payload[strlen(payload)] = client.read();
                } else payloadTooLarge = true;
              }
              if (payloadTooLarge) {
                client.println("HTTP/1.1 413 Payload Too Large\r\n");
              } else {
                SERIALDEBUG.print("New WIFI config received");
                parse(payload,"a",g_wifiSSID,MAXSSIDLENGTH,false);
                urlDecode(g_wifiSSID);
                parse(payload,"b",g_wifiPassword,MAXPASSWORDLENGTH,false);
                urlDecode(g_wifiPassword);
                SendConfigChangeToClient(&client);
              }
              break;              
            } else if (strncmp(header,"GET /",5) == 0) { // "common" GET-request...
              // get GET and file name from header
              ptrString = strstr(header, " HTTP/1.1");
              if (ptrString != NULL) {
                ptrString[0] = '\0';
                if (strlcpy(strFile, header + 4, sizeof(strFile)) < sizeof(strFile)) {
                } else {
                  SERIALDEBUG.print("Warning: filename too long:");
                  SERIALDEBUG.println(header + 4);
                  strFile[0] = '\0';
                  client.println("HTTP/1.1 431 Request Header Fields Too Large\r\n");
                }
              } else {
                SERIALDEBUG.println("Warning: No HTTP/1.1 in header found");
                strFile[0] = '\0';                
              }
            }
            if (strcmp(strFile, "/favicon.ico") == 0) {
              SendFileToClient(&client, strFile, "Cache-Control: public, max-age=31536000\r\n"
                "X-Content-Type-Options: nosniff\r\n"         
                "Content-Type: image/x-icon\r\n");              
              break;
            }

            if (strcmp(strFile, "/test.csv") == 0) { // Test file for performance tests
              SendTestFileToClient(&client, strFile, "Content-Type: Content-Type: text/csv; charset=utf-8");              
              break;
            }
            if (strcmp(strFile, "/chartbackground.png") == 0) {
              SendFileToClient(&client, strFile, "Content-Type: image/png");              
              break;
            }
            if ((strcmp(strFile, "/") == 0) || strcmp(strFile, "/index.html") == 0) {
              SendFileToClient(&client, "/index.html", "Content-Type: text/html; charset=utf-8");              
              break;
            }
            if (strcmp(strFile + strlen(strFile) - 4 , ".csv") == 0) {
              SendFileToClient(&client, strFile, "Content-Type: text/csv; charset=utf-8");              
              break;
            }
            if (strcmp(strFile, "/info.html") == 0) {
              SendInfoToClient(&client);
              break;
            }
            
            if (g_wifiAPMode && strcmp(strFile, "/cfg") == 0) { // Wifi config page is only allowed in AP mode
              SendConfigPageToClient(&client);
              break;
            }

            // Send error 404 for other requests
            client.println("HTTP/1.1 404 Not Found\r\n");
            break;
          }

          if (c == '\n') { // Linefeed
            // Begin of a new, empty line
            currentLineIsBlank = true;
          } else if (c != '\r') { // Other chars except carriage return
            // Mark the line is no longer empty
            currentLineIsBlank = false;
          }
        }
      }
      // Give client some time to process our response
      vTaskDelay(2 / portTICK_PERIOD_MS);

      // Disconnect client
      client.stop();
      SERIALDEBUG.println("client disconnected");
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);    
  }
}

// Send file from LittleFS to client
void SendFileToClient(WiFiClient *client, char *strFile, char *strHeader) {
  int bytesRead;
  DisplayMessage msg;
  
  SERIALDEBUG.print("Sending ");
  SERIALDEBUG.println(strFile);

  if (xSemaphoreTake( g_semaphoreLittleFS, 10000 / portTICK_PERIOD_MS) == pdTRUE) {
      if (!LittleFS.exists(strFile)) { 
        client->println("HTTP/1.1 404 Not Found\r\n");
        SERIALDEBUG.println("File not found");
      } else {
        fs::File file = LittleFS.open(strFile, "r");
        if (!file) {
          client->println("HTTP/1.1 500 Internal Server Error\r\n");
          SERIALDEBUG.println("Error: LittleFS open file failure");
        } else {
          client->println("HTTP/1.1 200 OK");
          client->println(strHeader);
          client->print("Content-Length: ");
          client->println(file.size());
          client->println("Connection: close");
          client->println();

          while (file.available()) {
            bytesRead = file.read(g_buffer,sizeof(g_buffer));
            if (bytesRead > 0) {              
              client->write(g_buffer,bytesRead); // Send buffer to client
            }
          }
          file.close();
        }
    }
    xSemaphoreGive( g_semaphoreLittleFS );
  } else {
   SERIALDEBUG.println("Error: LittleFS Semaphore Timeout");
   client->println("HTTP/1.1 504 Gateway Timeout\r\n");
   beep();
   beep(SHORTBEEP);
   beep();
  }
}

// Send test data from ram to client (for performance tests)
void SendTestFileToClient(WiFiClient *client, char *strFile, char *strHeader) {
  // 400-500 KB/s should be possible
  SERIALDEBUG.print("Sending ");
  SERIALDEBUG.println(strFile);
  
  client->println("HTTP/1.1 200 OK");
  client->println(strHeader);
  client->print("Content-Length: ");
  client->println(1400 * 100);
  client->println("Connection: close\r\n"); 

  for (int i=0;i < 100;i++) {

    client->write(g_buffer,1400); // Send buffer to client
  }
}

// Show internal program data
void SendInfoToClient(WiFiClient *client) {
  byte mac[6];
  #define MAXSTRDATALENGTH 254
  char strData[MAXSTRDATALENGTH+1];
  struct tm * ptrTimeinfo;
  time_t now;
  sensorData sensorDataSet;
  // Radius for mini pie charts
  #define RADIUS 6

  // To simplify the table HTML table creation
  #define SIMPLETABLELINE(Label,Value) client->print("<tr><td>");client->print(Label);client->print("</td><td>");client->print(Value);client->println("</td></tr>");
  #define SIMPLETABLEHEADER(Capter) client->print("<div class=\"item\"><p>");client->print(Capter); client->println("</p><table><thead><tr><th>Eigenschaft</th><th>Wert</th></tr></thead><tbody>");
  #define SIMPLETABLEEND client->println("</tbody></table></div>");

  client->println("HTTP/1.1 200 OK");
  client->println("Content-Type: text/html; charset=utf-8");
  client->println("Cache-Control: no-cache");
  client->println("X-Content-Type-Options: nosniff");
  client->println("Connection: close"); 
//  client->println("Refresh: 10");  // Autoreload every x seconds
  client->println();
  client->println(R"html(<!DOCTYPE HTML>
<html lang="de">
  <head>
    <meta charSet="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta name="description" content="device information">
    <title>Info</title>
    <style>
      body {
        background-color:#202020; 
        color:white;
        margin:0; 
        padding:0;)html");
  client->print(BACKGROUNDPATTERN);
  client->println(";");
  client->println(R"html(        font-family:'Helvetica Neue', 'Helvetica', 'Arial', sans-serif;
      }
      a:link, a:visited, a {
        color: white;
      }
      table {
        width: 20rem;
        margin-left:auto;
        margin-right:auto;
      }
      td:first-child, th:first-child {
        text-align:left
      }
      td.left, th.left {
        text-align: left;
      }
      td,th {
        text-align:center;
        vertical-align: top;
        padding: 5px;
        font-size:0.75em;
      }
      table, th, td{
        border-width: 1px;
        border-style: dotted;
        border-collapse: collapse;
      }
      table, td {
        border-color: gray;
      }
      th {
        background-color:#606060;
        border-color: black;
      }
      tbody tr:nth-child(even)  {
        background-color:#000000;
        }
      tbody tr:nth-child(odd) {
        background-color:#202020;
      }
      .container {
        display: flex;
        gap: 1rem;
        flex-wrap: wrap;
        justify-content: space-evenly;
      }
      @media screen and (max-width: 800px) { /* if width < 800px */
        .rotateableHeader th { /* Rotate labels */
          writing-mode: vertical-rl; /* Rotate Text */
          transform: rotate(180deg);  /* Mirror content vertically */
          white-space: nowrap;
          text-align:left;
        }
      }
    </style>
  </head>
  <body>
    <div class="container">)html");

  SIMPLETABLEHEADER("Ger&auml;t:");
  SIMPLETABLELINE("Hostname",HOSTNAME)
  SIMPLETABLELINE("Chip ID",ESP.getEfuseMac())
  SIMPLETABLELINE("Chip model",ESP.getChipModel())
  SIMPLETABLELINE("CPUs",ESP.getChipCores())
  SIMPLETABLELINE("Free heap",ESP.getFreeHeap());
  SIMPLETABLELINE("Revision",ESP.getChipRevision())
  SIMPLETABLELINE("Sdk/IDF-Version",ESP.getSdkVersion())
  SIMPLETABLELINE("Arduino ESP32 Release",ARDUINO_ESP32_RELEASE)
  SIMPLETABLELINE("Reset reason CPU0",rtc_get_reset_reason(0))
  SIMPLETABLELINE("Internal temperature",temperatureRead())
  SIMPLETABLELINE("Compile time",__DATE__ " " __TIME__)
  snprintf(strData,MAXSTRDATALENGTH+1,"%lld seconds (%lld days)",esp_timer_get_time()/1000/1000,esp_timer_get_time()/1000/1000/SECS_PER_DAY);
  SIMPLETABLELINE("Uptime",strData)

  if (xSemaphoreTake( g_semaphoreLittleFS, 1000 / portTICK_PERIOD_MS) == pdTRUE) {
    bool logFileExists = LittleFS.exists(LOGFILE);
    bool backupLogFileExists = LittleFS.exists(BACKUPLOGFILE);
    xSemaphoreGive( g_semaphoreLittleFS );

    if (logFileExists) {
      snprintf(strData,MAXSTRDATALENGTH+1,"<a href=\"%s\">%s</a>",LOGFILE,LOGFILE);
      SIMPLETABLELINE("Logfile",strData)
    }
    if (backupLogFileExists) {
      snprintf(strData,MAXSTRDATALENGTH+1,"<a href=\"%s\">%s</a>",BACKUPLOGFILE,BACKUPLOGFILE);
      SIMPLETABLELINE("Backup logfile",strData)
    }
  } else { // Semaphore timeout
    SERIALDEBUG.println("Skip displaying logfile information because could not get semaphore in 1 second"); 
  } 
  SIMPLETABLEEND

  SIMPLETABLEHEADER("Variablen:");
  SIMPLETABLELINE("g_ScreenSaverEnabled",g_ScreenSaverEnabled)
  SIMPLETABLELINE("g_SoundDisabled",g_SoundDisabled)
  SIMPLETABLELINE("g_PIREnabled",g_PIREnabled)
  SIMPLETABLELINE("g_wifiEnabled",g_wifiEnabled);
  SIMPLETABLELINE("g_demoModeEnabled",g_demoModeEnabled)
  SIMPLETABLELINE("g_mailAlert",g_mailAlert)
  SIMPLETABLELINE("g_pendingBlynkMailAlert",g_pendingBlynkMailAlert);
  SIMPLETABLELINE("g_BME280ready",g_BME280ready);
  SIMPLETABLELINE("g_LORAready",g_LORAready);  
  SIMPLETABLEEND

  SIMPLETABLEHEADER("Netzwerk:");
  WiFi.macAddress(mac);
  snprintf(strData,MAXSTRDATALENGTH+1,"%02X:%02X:%02X:%02X:%02X:%02X",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  SIMPLETABLELINE("MAC",strData)
  SIMPLETABLELINE("IP",WiFi.localIP())
  SIMPLETABLELINE("Subnetmask",WiFi.subnetMask())
  SIMPLETABLELINE("Gateway",WiFi.gatewayIP())
  SIMPLETABLELINE("DNS",WiFi.dnsIP())
  SIMPLETABLELINE("SSID",g_wifiSSID)
  SIMPLETABLELINE("RSSI",WiFi.RSSI());
  SIMPLETABLELINE("Channel",WiFi.channel());
  SIMPLETABLEEND

  if (xSemaphoreTake( g_semaphoreLittleFS, 1000 / portTICK_PERIOD_MS) == pdTRUE) {
    // Speedup multiple used data (LittleFS.usedBytes() seems to be time consuming)
    unsigned long usedBytes = LittleFS.usedBytes();
    unsigned long totalBytes = LittleFS.totalBytes();
    xSemaphoreGive( g_semaphoreLittleFS );

    // Mini pie chart in svg format
    snprintf(strData,MAXSTRDATALENGTH+1,"LittleFS <svg height=\"1em\" width=\"1em\" viewBox=\"0 0 %i %i\"><path d=\"M %i 1 a%i,%i 0 %i,1 %f %f L %i %i Z\" fill=\"%s\"/><circle r=\"%i\" cx=\"%i\" cy=\"%i\" fill=\"none\" stroke=\"white\" stroke-width=\"1\"/></svg> :",
      RADIUS*2+2,
      RADIUS*2+2,
      RADIUS+1,
      RADIUS,
      RADIUS,
      ((float) usedBytes/totalBytes > 0.5f ) ? 1 : 0,
      RADIUS*sin(((float) usedBytes/totalBytes)*(2*M_PI)),
      RADIUS*(1-cos(((float) usedBytes/totalBytes)*(2*M_PI))),
      RADIUS+1,
      RADIUS+1,
      (usedBytes < (float) totalBytes*0.8f) ? "white" : "red", // Warning color, when <= 20% free space
      RADIUS,
      RADIUS+1, // center x
      RADIUS+1 // center y
    );
  
    SIMPLETABLEHEADER(strData);
    SIMPLETABLELINE("Total bytes",totalBytes)
    SIMPLETABLELINE("Used bytes",usedBytes)
    SIMPLETABLELINE("Free bytes",totalBytes-usedBytes)  
    SIMPLETABLEEND

  } else { // Semaphore timeout
    SERIALDEBUG.println("Skip displaying LittleFS information because could not get semaphore in 1 second");
    
    SIMPLETABLEHEADER("LittleFS");
    SIMPLETABLELINE("Total bytes","timeout")
    SIMPLETABLELINE("Used bytes","timeout")
    SIMPLETABLELINE("Free bytes","timeout")  
    SIMPLETABLEEND
  }

  SIMPLETABLEHEADER("Zeit:");
  SIMPLETABLELINE("NTP-Server",NTPSERVER)
  SIMPLETABLELINE("Zeitzone",TIMEZONE)
  ptrTimeinfo = localtime ( &g_firstNTPTime );
  snprintf(strData,MAXSTRDATALENGTH+1,"%02d.%02d.%04d %02d:%02d:%02d",
    ptrTimeinfo->tm_mday,
    ptrTimeinfo->tm_mon + 1,
    ptrTimeinfo->tm_year + 1900,
    ptrTimeinfo->tm_hour,                 
    ptrTimeinfo->tm_min,
    ptrTimeinfo->tm_sec);
  SIMPLETABLELINE("Erste NTP-Zeit",strData)
  ptrTimeinfo = localtime ( &g_lastNTPTime );
  snprintf(strData,MAXSTRDATALENGTH+1,"%02d.%02d.%04d %02d:%02d:%02d",
    ptrTimeinfo->tm_mday,
    ptrTimeinfo->tm_mon + 1,
    ptrTimeinfo->tm_year + 1900,
    ptrTimeinfo->tm_hour,                 
    ptrTimeinfo->tm_min,
    ptrTimeinfo->tm_sec);
  SIMPLETABLELINE("Letzte NTP-Zeit",strData)
  time(&now);
  ptrTimeinfo = localtime ( &now );
  snprintf(strData,MAXSTRDATALENGTH+1,"%02d.%02d.%04d %02d:%02d:%02d",
    ptrTimeinfo->tm_mday,
    ptrTimeinfo->tm_mon + 1,
    ptrTimeinfo->tm_year + 1900,
    ptrTimeinfo->tm_hour,                 
    ptrTimeinfo->tm_min,
    ptrTimeinfo->tm_sec);
  SIMPLETABLELINE("Aktuelle Uhrzeit",strData)
  SIMPLETABLEEND

  SIMPLETABLEHEADER("Tasks/Queues:");
  SIMPLETABLELINE("MinStackFreeBytes taskWebserver",uxTaskGetStackHighWaterMark(g_handleTaskWebServer))
  SIMPLETABLELINE("MinStackFreeBytes taskDisplay",uxTaskGetStackHighWaterMark(g_handleTaskDisplay))
  extern TaskHandle_t loopTaskHandle;
  SIMPLETABLELINE("MinStackFreeBytes loop",uxTaskGetStackHighWaterMark(loopTaskHandle))
  SIMPLETABLELINE("Freie Warteschlangenplätze displayMsgQueue",uxQueueSpacesAvailable(displayMsgQueue))
  SIMPLETABLELINE("Freie Warteschlangenplätze displayDatQueue",uxQueueSpacesAvailable(displayDatQueue))
  SIMPLETABLEEND

  SIMPLETABLEHEADER("Sensor1 \"Treppenhaus\":");
  ptrTimeinfo = localtime ( &(g_pendingSensorData.sensor1LastDataTime) );
  snprintf(strData,MAXSTRDATALENGTH+1,"%02d.%02d.%04d %02d:%02d:%02d",
    ptrTimeinfo->tm_mday,
    ptrTimeinfo->tm_mon + 1,
    ptrTimeinfo->tm_year + 1900,
    ptrTimeinfo->tm_hour,                 
    ptrTimeinfo->tm_min,
    ptrTimeinfo->tm_sec);
  SIMPLETABLELINE("sensor1LastDataTime",strData)
  SIMPLETABLELINE("sensor1LowBattery",g_pendingSensorData.sensor1LowBattery)
  SIMPLETABLELINE("sensor1Temperature",g_pendingSensorData.sensor1Temperature)
  SIMPLETABLELINE("sensor1Humidity",g_pendingSensorData.sensor1Humidity)
  SIMPLETABLELINE("sensor1Vcc",g_pendingSensorData.sensor1Vcc)
  SIMPLETABLEEND

  SIMPLETABLEHEADER("Sensor2 \"Zimmer\":");
  ptrTimeinfo = localtime ( &(g_pendingSensorData.sensor2LastDataTime) );
  snprintf(strData,MAXSTRDATALENGTH+1,"%02d.%02d.%04d %02d:%02d:%02d",
    ptrTimeinfo->tm_mday,
    ptrTimeinfo->tm_mon + 1,
    ptrTimeinfo->tm_year + 1900,
    ptrTimeinfo->tm_hour,                 
    ptrTimeinfo->tm_min,
    ptrTimeinfo->tm_sec);
  SIMPLETABLELINE("sensor2LastDataTime",strData)
  SIMPLETABLELINE("sensor2Temperature",g_pendingSensorData.sensor2Temperature)
  SIMPLETABLELINE("sensor2Humidity",g_pendingSensorData.sensor2Humidity)
  SIMPLETABLELINE("sensor2Pressure",g_pendingSensorData.sensor2Pressure)
  SIMPLETABLEEND

  SIMPLETABLEHEADER("Sensor3 \"Fenster\":");
  ptrTimeinfo = localtime ( &(g_pendingSensorData.sensor3LastDataTime) );
  snprintf(strData,MAXSTRDATALENGTH+1,"%02d.%02d.%04d %02d:%02d:%02d",
    ptrTimeinfo->tm_mday,
    ptrTimeinfo->tm_mon + 1,
    ptrTimeinfo->tm_year + 1900,
    ptrTimeinfo->tm_hour,                 
    ptrTimeinfo->tm_min,
    ptrTimeinfo->tm_sec);
  SIMPLETABLELINE("sensor3LastDataTime",strData)
  SIMPLETABLELINE("sensor3LowBattery",g_pendingSensorData.sensor3LowBattery)
  SIMPLETABLELINE("sensor3Switch1",g_pendingSensorData.sensor3Switch1)
  SIMPLETABLELINE("sensor3Switch2",g_pendingSensorData.sensor3Switch2)
  SIMPLETABLELINE("sensor3Temperature",g_pendingSensorData.sensor3Temperature)
  SIMPLETABLELINE("sensor3Humidity",g_pendingSensorData.sensor3Humidity)
  SIMPLETABLELINE("sensor3Vcc",g_pendingSensorData.sensor3Vcc)
  SIMPLETABLEEND

  SIMPLETABLEHEADER("Sensor4 \"SolarPoweredSender\":");
  ptrTimeinfo = localtime ( &(g_pendingSensorData.sensor4LastDataTime) );
  snprintf(strData,MAXSTRDATALENGTH+1,"%02d.%02d.%04d %02d:%02d:%02d",
    ptrTimeinfo->tm_mday,
    ptrTimeinfo->tm_mon + 1,
    ptrTimeinfo->tm_year + 1900,
    ptrTimeinfo->tm_hour,                 
    ptrTimeinfo->tm_min,
    ptrTimeinfo->tm_sec);
  SIMPLETABLELINE("sensor4LastDataTime",strData)
  SIMPLETABLELINE("sensor4LowBattery",g_pendingSensorData.sensor4LowBattery)
  SIMPLETABLELINE("sensor4Runtime",g_pendingSensorData.sensor4Runtime)
  SIMPLETABLELINE("sensor4Vcc",g_pendingSensorData.sensor4Vcc)
  SIMPLETABLEEND

  SIMPLETABLEHEADER("Sensor5 \"Briefkasten\":");
  ptrTimeinfo = localtime ( &(g_pendingSensorData.sensor5LastDataTime) );
  snprintf(strData,MAXSTRDATALENGTH+1,"%02d.%02d.%04d %02d:%02d:%02d",
    ptrTimeinfo->tm_mday,
    ptrTimeinfo->tm_mon + 1,
    ptrTimeinfo->tm_year + 1900,
    ptrTimeinfo->tm_hour,                 
    ptrTimeinfo->tm_min,
    ptrTimeinfo->tm_sec);
  SIMPLETABLELINE("sensor5LastDataTime",strData)
  SIMPLETABLELINE("sensor5LowBattery",g_pendingSensorData.sensor5LowBattery)
  SIMPLETABLELINE("sensor5Switch1",g_pendingSensorData.sensor5Switch1)
  SIMPLETABLELINE("sensor5PCI1",g_pendingSensorData.sensor5PCI1)
  SIMPLETABLELINE("sensor5Vcc",g_pendingSensorData.sensor5Vcc)
  SIMPLETABLEEND

  SIMPLETABLEHEADER("Sensor6 \"Waschmaschine\":");
  ptrTimeinfo = localtime ( &(g_pendingSensorData.sensor6LastDataTime) );
  snprintf(strData,MAXSTRDATALENGTH+1,"%02d.%02d.%04d %02d:%02d:%02d",
    ptrTimeinfo->tm_mday,
    ptrTimeinfo->tm_mon + 1,
    ptrTimeinfo->tm_year + 1900,
    ptrTimeinfo->tm_hour,                 
    ptrTimeinfo->tm_min,
    ptrTimeinfo->tm_sec);
  SIMPLETABLELINE("sensor6LastDataTime",strData)
  SIMPLETABLELINE("sensor6LowBattery",g_pendingSensorData.sensor6LowBattery)
  SIMPLETABLELINE("sensor6Vcc",g_pendingSensorData.sensor6Vcc)
  SIMPLETABLEEND

  SIMPLETABLEHEADER("Ein/Aus-Schaltzeiten:");
  ptrTimeinfo = localtime ( &(g_last433MhzCafeOn) );
  snprintf(strData,MAXSTRDATALENGTH+1,"%02d.%02d.%04d %02d:%02d:%02d",
    ptrTimeinfo->tm_mday,
    ptrTimeinfo->tm_mon + 1,
    ptrTimeinfo->tm_year + 1900,
    ptrTimeinfo->tm_hour,                 
    ptrTimeinfo->tm_min,
    ptrTimeinfo->tm_sec);
  SIMPLETABLELINE("last433MhzCafeOn",strData)
  ptrTimeinfo = localtime ( &(g_last433MhzCafeOff) );
  snprintf(strData,MAXSTRDATALENGTH+1,"%02d.%02d.%04d %02d:%02d:%02d",
    ptrTimeinfo->tm_mday,
    ptrTimeinfo->tm_mon + 1,
    ptrTimeinfo->tm_year + 1900,
    ptrTimeinfo->tm_hour,                 
    ptrTimeinfo->tm_min,
    ptrTimeinfo->tm_sec);
  SIMPLETABLELINE("last433MhzCafeOff",strData)
  ptrTimeinfo = localtime ( &(g_last433MhzBathOn) );
  snprintf(strData,MAXSTRDATALENGTH+1,"%02d.%02d.%04d %02d:%02d:%02d",
    ptrTimeinfo->tm_mday,
    ptrTimeinfo->tm_mon + 1,
    ptrTimeinfo->tm_year + 1900,
    ptrTimeinfo->tm_hour,                 
    ptrTimeinfo->tm_min,
    ptrTimeinfo->tm_sec);
  SIMPLETABLELINE("last433MhzBathOn",strData)
  ptrTimeinfo = localtime ( &(g_last433MhzBathOff) );
  snprintf(strData,MAXSTRDATALENGTH+1,"%02d.%02d.%04d %02d:%02d:%02d",
    ptrTimeinfo->tm_mday,
    ptrTimeinfo->tm_mon + 1,
    ptrTimeinfo->tm_year + 1900,
    ptrTimeinfo->tm_hour,                 
    ptrTimeinfo->tm_min,
    ptrTimeinfo->tm_sec);
  SIMPLETABLELINE("last433MhzBathOff",strData)
  ptrTimeinfo = localtime ( &(g_last433MhzWifiOn) );
  snprintf(strData,MAXSTRDATALENGTH+1,"%02d.%02d.%04d %02d:%02d:%02d",
    ptrTimeinfo->tm_mday,
    ptrTimeinfo->tm_mon + 1,
    ptrTimeinfo->tm_year + 1900,
    ptrTimeinfo->tm_hour,                 
    ptrTimeinfo->tm_min,
    ptrTimeinfo->tm_sec);
  SIMPLETABLELINE("last433MhzWifiOn",strData)
  ptrTimeinfo = localtime ( &(g_last433MhzWifiOff) );
  snprintf(strData,MAXSTRDATALENGTH+1,"%02d.%02d.%04d %02d:%02d:%02d",
    ptrTimeinfo->tm_mday,
    ptrTimeinfo->tm_mon + 1,
    ptrTimeinfo->tm_year + 1900,
    ptrTimeinfo->tm_hour,                 
    ptrTimeinfo->tm_min,
    ptrTimeinfo->tm_sec);
  SIMPLETABLELINE("last433MhzWifiOff",strData)
  SIMPLETABLEEND

  client->println("<div class=\"item\"><p>History:</p>");
  client->println("<table class=\"rotateableHeader\"><thead><tr><th>Date</th><th>Time</th><th>Lowbat_&shy;stairs</th><th>T_&shy;stairs</th><th>H_&shy;stairs</th><th>Vcc_&shy;stairs</th><th>T_&shy;room</th><th>H_&shy;room</th><th>P_&shy;room</th><th>Lowbat_&shy;window</th><th>S1_&shy;window</th><th>S2_&shy;window</th><th>T_&shy;window</th><th>H_&shy;window</th><th>Vcc_&shy;window</th></tr></thead><tbody>");
  if (!dataHistoryBuffer.isEmpty()) {
    for (int i=0;i < dataHistoryBuffer.countElements();i++) {
      sensorDataSet = dataHistoryBuffer.getByIndex(i);
      ptrTimeinfo = localtime ( &(sensorDataSet.time) );
      snprintf(strData,MAXSTRDATALENGTH+1,"<tr><td>%02d.%02d.%04d</td><td>%02d:%02d</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%d</td></tr>",
        ptrTimeinfo->tm_mday,
        ptrTimeinfo->tm_mon + 1,
        ptrTimeinfo->tm_year + 1900,
        ptrTimeinfo->tm_hour,                 
        ptrTimeinfo->tm_min,
        sensorDataSet.sensor1LowBattery,
        sensorDataSet.sensor1Temperature,
        sensorDataSet.sensor1Humidity,
        sensorDataSet.sensor1Vcc,
        sensorDataSet.sensor2Temperature,
        sensorDataSet.sensor2Humidity,
        sensorDataSet.sensor2Pressure,
        sensorDataSet.sensor3LowBattery,
        sensorDataSet.sensor3Switch1,
        sensorDataSet.sensor3Switch2,
        sensorDataSet.sensor3Temperature,
        sensorDataSet.sensor3Humidity,
        sensorDataSet.sensor3Vcc
      );
      client->println(strData);
    }
  }
  client->println("</tbody></table><p align=\"right\">&copy; codingABI 2023</p></div></div></body></html>");    
}

// Send Wifi config page
void SendConfigPageToClient(WiFiClient *client) {
  byte mac[6];
  #define MAXSTRDATALENGTH 254
  char strData[MAXSTRDATALENGTH+1];
  
  client->println("HTTP/1.1 200 OK");
  client->println("Content-Type: text/html; charset=utf-8");
  client->println("Cache-Control: no-cache");
  client->println("X-Content-Type-Options: nosniff");
  client->println("Connection: close");
  client->println();
  client->println(R"html(<!DOCTYPE html>
<html lang="de">
  <head>
    <meta charSet="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta name="description" content="Wifi configuration">
    <title>Config</title>
    <style>
      body {
        background-color:#202020; 
        color:white;
        margin:0; 
        padding:0;)html");
  client->print(BACKGROUNDPATTERN);
  client->println(", radial-gradient(circle,#000000 0%,#555555 100%);");
  client->println(R"html(        background-blend-mode: darken;
        background-repeat: repeat, no-repeat;
        font-family:'Helvetica Neue', 'Helvetica', 'Arial', sans-serif;
      }
      .button {
        background-color: black;
        color: white;
        border: 2px solid gray;
        border-radius: 12px;
        color: white;
        padding: 15px 32px;
        text-align: center;
        text-decoration: none;
        display: inline-block;
        font-size: 16px;
        width: 100%;
        transition-duration: 0.4s;
      }
      .button:hover,.button:focus {
        background-color: white;
        color: black;
      }
      input[type=text],input[type=password] {
        width: 100%;
        padding: 12px 20px;
        margin: 8px 0;
        box-sizing: border-box;
        border-radius: 6px;
        background-color: black;
        color: white;
      }
      input[type=text]:focus, input[type=password]:focus {
        background-color: white;
        color: black;
      }
      .container {
        display: grid;
        grid-template-columns: min-content 1fr;
        justify-content:center;
        align-content: center;
        height: 100vh;
        gap: 4px;
        width:20rem;
        margin-left:auto;
        margin-right:auto;
      }
      .item2cols {
        grid-column: span 2;
      }
      .item1cols, .item2cols {
        margin-top: auto;
        margin-bottom: auto;
      }
    </style>
  </head>
  <body>
    <form method=post autocomplete="off" class="container">
    <div class="item2cols"><h1>&Auml;ndern des WLAN-Zugangs</h1></div>
    <div class="item1cols"><p>MAC</p></div>
    <div class="item1cols"><p>)html");
  WiFi.macAddress(mac);
  snprintf(strData,MAXSTRDATALENGTH+1,"%02X:%02X:%02X:%02X:%02X:%02X",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  client->print(strData);
  client->print(R"html(</p></div>
    <div class="item1cols"><label for="a">SSID</label></div>
    <div class="item1cols"><input id="a" title="SSID/Wifi-Name" type="text" name="a" value=")html");
  client->print(g_wifiSSID);
  client->print(R"html(" maxlength=)html");
  client->print(MAXSSIDLENGTH);
  client->print(R"html( autofocus></div>
    <div class="item1cols"><label for="b">Kennwort</label></div>
    <div class="item1cols"><input title="Kennwort/Preshared Key" type="password" name="b" maxlength=)html");
  client->print(MAXPASSWORDLENGTH);
  client->println(R"html(></div>
    <div class="item2cols"><input type="submit" class="button" value="Anwenden"></div>
    <div class="item2cols"><p align="right">&copy; codingABI 2023</p></div>
    </form>
  </body>
</html>)html");
}

// Send Wifi config confirmation and reset the device, if config was ok
void SendConfigChangeToClient(WiFiClient *client) {
  client->println("HTTP/1.1 200 OK");
  client->println("Content-Type: text/html; charset=utf-8");
  client->println("Cache-Control: no-cache");
  client->println("X-Content-Type-Options: nosniff");
  client->println("Connection: close");
  client->println();
  client->println(R"html(<!DOCTYPE html>
<html lang="de">
  <head>
    <meta charSet="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta name="description" content="Response to configuration change">
    <title>Config</title>
    <style>
      body {
        background-color:#202020; 
        color:white;
        margin:0; 
        padding:0;)html");
  // background-image inline because esp reboots soon and does not process additional requests 
  client->print(BACKGROUNDPATTERN);
  client->println(", radial-gradient(circle,#000000 0%,#555555 100%);");
  client->println(R"html(        background-blend-mode: darken;
        background-repeat: repeat, no-repeat;
        font-family:'Helvetica Neue', 'Helvetica', 'Arial', sans-serif;
      }
      .button {
        background-color: black;
        color: white;
        border: 2px solid gray;
        border-radius: 12px;
        color: white;
        padding: 15px 32px;
        text-align: center;
        text-decoration: none;
        display: inline-block;
        font-size: 16px;
        transition-duration: 0.4s;
        width: 100%;
      }
      .button:hover {
        background-color: white;
        color: black;
      }
      .container {
        display: grid;
        grid-template-columns: 1fr;
        justify-content:center;
        align-content: center;
        height: 100vh;
        gap: 4px;
        width:20rem;
        margin-left:auto;
        margin-right:auto;
      }
    </style>
  </head>
  <body>
    <div class="container">)html");
  if (strlen(g_wifiSSID) > 0) {
    client->println(R"html(      <div class="item"><h1>&Auml;nderung wurde angenommen</h1></div>
      <div class="item"><p>Das Ger&auml;t wird in wenigen Sekunden mit den &Auml;nderungen neu gestartet...</p><p>Dieses Fenster kann nun geschlossen werden.</p></div>
      <div class="item"><p align="right">&copy; codingABI 2023</p></div>
    </div>
  </body>
</html>)html");
    client->stop();
    storeWifiConnectionDataToEEPROM();
    delay(1000);
    reset();
  } else { // Empty SSID
    client->println(R"html(      <div class="item"><h1>Die &Auml;nderungen sind ung&uuml;ltig und werden nicht gespeichert</h1></div>
      <div class="item"><p>Eine leere SSID ist nicht erlaubt.</p></div>
      <div class="item"><form><input type="button" class="button" value="Zur&uuml;ck" onclick="history.back()"></form></div>
      <div class="item"><p align="right">&copy; codingABI 2023</p></div>
    </div>
  </body>
</html>)html");
  }
}

/* 
 * Simple HTTP-request parser to get the value of a parameter
 * The function returns 1, if parsing was successful or 0 if parsing failed
 * maxlength is the maximum allowed count of returned chars for the value 
 * To parse for a GET-Request set requestGET to true otherwise a POST-request is parsed 
 */
byte parse(const char *data,const char *parameter,char *value,int maxlength, bool requestGET) {
  enum STATES { STATE_START, STATE_DEFAULT, STATE_EQUAL, STATE_AND };

  int i=0;
  int parameterStartPos, valueStartPos;
  bool parameterMatches;
  byte state = STATE_START;

  if (requestGET) byte state = STATE_DEFAULT; 
  parameterStartPos=0;
  *value = '\0';
  parameterMatches=true;
  if (strlen(data)==0) return 0;
  do {
    switch (data[i]) {
      case '?': if (requestGET) { state=STATE_START;parameterStartPos = i+1;parameterMatches=true;} break;      
      case '=': {
        if ((state==STATE_START) || (state==STATE_AND)) {
          if ((parameterMatches) && (i-parameterStartPos) == strlen(parameter)) {
            state = STATE_EQUAL;
            valueStartPos = i+1;
          } else {
            state = STATE_DEFAULT;
          }
        } else state=STATE_DEFAULT;
        break;
      }
      case '\0' :
      case '&' : {
        if (state == STATE_EQUAL) {
          if (i-valueStartPos > maxlength) return 0;
          for (int j=0;j<i-valueStartPos;j++) {
            value[j] = data[valueStartPos+j];
          }
          value[i-valueStartPos] = '\0';
          return 1;
        } else {
          state=STATE_AND;
          parameterStartPos = i+1;
          parameterMatches=true;
        } break;
      }
      default: {
        if ((state == STATE_START) || (state == STATE_AND)) {
          if (((i-parameterStartPos) > strlen(parameter)-1) ||
            (data[i]!=parameter[i-parameterStartPos])) {
            // maxlength exceeded or parameter does not match
            parameterMatches=false;
            state = STATE_DEFAULT;
          }
        }
      }
    }
    i++;
  } while (i<=strlen(data));
  return 0;
}

/*
 * Translates a ULR-coded string to ASCII code  
 */
void urlDecode(char *data) {
  enum STATES { STATE_DEFAULT, STATE_PERCENT, STATE_DIGIT1 };
  int i=0;
  byte state = STATE_DEFAULT;
  byte asciiChar;

  if (strlen(data)==0) return;
  do {
    switch (data[i]) {
      case '+': data[i]=' ';state=STATE_DEFAULT;break;
      case '%': state=STATE_PERCENT;break;
      default : {
        switch (state) {
          case STATE_DEFAULT:break;
          case STATE_PERCENT: {
            if (strchr("0123456789ABCDEFabcdef",data[i])) {
              state=STATE_DIGIT1;
            } else {
              state=STATE_DEFAULT;
            }
          }; break;
          case STATE_DIGIT1: {
            if (strchr("0123456789ABCDEFabcdef",data[i])) {
              if (strchr("ABCDEF", data[i])) {
                asciiChar=data[i] - 'A' + 10;
              } else if (strchr("abcdef", data[i])) {
                asciiChar=data[i] - 'a' + 10;
              } else {
                asciiChar=data[i] - '0';
              }
              if (strchr("ABCDEF", data[i-1])) {
                asciiChar+=(data[i-1] - 'A' + 10)*16;
              } else if (strchr("abcdef", data[i-1])) {
                asciiChar+=(data[i-1] - 'a' + 10)*16;
              } else {
                asciiChar+=(data[i-1] - '0') * 16;
              }
              if (asciiChar <= 31) { // Filter out control chars
                for (int j=i-2;j<strlen(data)-2;j++) {
                  data[j]=data[j+3];
                }
                i-=3;                
              } else {
                data[i-2] = (char) asciiChar;
                for (int j=i-1;j<strlen(data)-1;j++) {
                  data[j]=data[j+2];
                }
                i-=2;
              }
            }
            state=STATE_DEFAULT;
          }; break;
        }
      };
    }
    i++;
  } while (i<strlen(data));
}
