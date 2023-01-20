/* ----------- Stuff for the tft display ----------
 * License: 2-Clause BSD License
 * Copyright (c) 2023 codingABI
 */
#define FONTCOLOR TFT_WHITE // Color for text
#define BACKGROUNDCOLOR TFT_BLACK // Default background color

// Show battery status (black/green/red)
void drawBatteryStates(int LowBattery1, int LowBattery3, int LowBattery5, bool reset=false) {
  uint16_t color;
  #define BATWIDTH 6
  #define BATHEIGHT 10
  #define BATPOSX 1
  #define BATPOSY 2
  
  // Sensor 1
  color = BACKGROUNDCOLOR;
  if (!reset) {
    switch(LowBattery1) {
      case 0: color=TFT_GREEN;break;
      case 1: color=TFT_RED;break;
    }
  }
  if ((color != BACKGROUNDCOLOR) || reset) g_tft.fillRect(BATPOSX,BATPOSY,BATWIDTH,BATHEIGHT,color);
  // Sensor 3
  color = BACKGROUNDCOLOR;
  if (!reset) {
    switch(LowBattery3) {
      case 0: color=TFT_GREEN;break;
      case 1: color=TFT_RED;break;
    }
  }
  if ((color != BACKGROUNDCOLOR) || reset) g_tft.fillRect(BATPOSX,BATPOSY+32,BATWIDTH,BATHEIGHT,color);
  // Sensor 5
  color = BACKGROUNDCOLOR;
  if (!reset) {
    switch(LowBattery5) {
      case 0: color=TFT_GREEN;break;
      case 1: color=TFT_RED;break;
    }
  }
  if ((color != BACKGROUNDCOLOR) || reset) g_tft.fillRect(BATPOSX,BATPOSY+32+32+32,BATWIDTH,BATHEIGHT,color);
}

// Draw switches on display
void drawSwitches() {
  uint16_t x, y;
  #define SWITCHPOSX 53
  #define SWITCHPOSY 215
  #define DOTSIZE 7
  #define DOTDISTANCE 15 // 14 is a little bit too small for DOTSIZE 7

  y = SWITCHPOSY;

  // Screensaver switch
  x = SWITCHPOSX;
  if (g_ScreenSaverEnabled) {
    g_tft.fillCircle(x, y, DOTSIZE, FONTCOLOR);
    g_tft.fillCircle(x + DOTDISTANCE, y, DOTSIZE, BACKGROUNDCOLOR);
  } else {
    g_tft.fillCircle(x, y, DOTSIZE, BACKGROUNDCOLOR);
    g_tft.fillCircle(x + DOTDISTANCE, y, DOTSIZE, TFT_DARKGREY);         
  }

  // Sound switch
  x = SWITCHPOSX+80;
  if (g_SoundDisabled) {
    g_tft.fillCircle(x, y, DOTSIZE, FONTCOLOR);
    g_tft.fillCircle(x + DOTDISTANCE, y, DOTSIZE, BACKGROUNDCOLOR);
  } else {
    g_tft.fillCircle(x, y, DOTSIZE, BACKGROUNDCOLOR);
    g_tft.fillCircle(x + DOTDISTANCE, y, DOTSIZE, TFT_DARKGREY);         
  }

  // PIR switch
  x = SWITCHPOSX+80+80;
  if (g_PIREnabled) {
    g_tft.fillCircle(x, y, DOTSIZE, FONTCOLOR);
    g_tft.fillCircle(x + DOTDISTANCE, y, DOTSIZE, BACKGROUNDCOLOR);
  } else {
    g_tft.fillCircle(x, y, DOTSIZE, BACKGROUNDCOLOR);
    g_tft.fillCircle(x + DOTDISTANCE, y, DOTSIZE, TFT_DARKGREY);         
  }

  y = SWITCHPOSY+32;

  // Wifi switch
  x = SWITCHPOSX;
  if (g_wifiSwitch) {
    g_tft.fillCircle(x, y, DOTSIZE, FONTCOLOR);
    g_tft.fillCircle(x + DOTDISTANCE, y, DOTSIZE, BACKGROUNDCOLOR);
  } else {
    g_tft.fillCircle(x, y, DOTSIZE, BACKGROUNDCOLOR);
    g_tft.fillCircle(x + DOTDISTANCE, y, DOTSIZE, TFT_DARKGREY);         
  }

  // Demo mode switch
  x = SWITCHPOSX+80;
  if (g_demoModeSwitch) {
    g_tft.fillCircle(x, y, DOTSIZE, FONTCOLOR);
    g_tft.fillCircle(x + DOTDISTANCE, y, DOTSIZE, BACKGROUNDCOLOR);
  } else {
    g_tft.fillCircle(x, y, DOTSIZE, BACKGROUNDCOLOR);
    g_tft.fillCircle(x + DOTDISTANCE, y, DOTSIZE, TFT_DARKGREY);         
  }

  // Mail alert symbol
  #define SYMBOLSIZE 20 // width = SYMBOLSIZE*2, height = SYMBOLSIZE
  x = 80+80+80/2-SYMBOLSIZE;
  y = SWITCHPOSY+32-SYMBOLSIZE/2;
  if (g_mailAlert) {
    g_tft.fillRect(x,y,SYMBOLSIZE*2,SYMBOLSIZE,TFT_RED);
    g_tft.drawRect(x,y,SYMBOLSIZE*2,SYMBOLSIZE,TFT_WHITE);
    g_tft.drawLine(x,y,x+SYMBOLSIZE-1,y+SYMBOLSIZE/2,TFT_WHITE);
    g_tft.drawLine(x+SYMBOLSIZE,y+SYMBOLSIZE/2,x+SYMBOLSIZE*2-1,y,TFT_WHITE);    
  } else g_tft.fillRect(x,y,SYMBOLSIZE*2,SYMBOLSIZE,BACKGROUNDCOLOR);
}

// Reset min/max/last data
void resetLastMinMaxData(sensorData *datLast,sensorData *datMin, sensorData *datMax) {
  datLast->sensor1LowBattery = -NOVALIDLOWBATTERY;
  datLast->sensor1Temperature = -NOVALIDTEMPERATUREDATA;
  datLast->sensor1Humidity = -NOVALIDHUMIDITYDATA;
  datLast->sensor2Temperature = -NOVALIDTEMPERATUREDATA;
  datLast->sensor2Humidity = -NOVALIDHUMIDITYDATA;
  datLast->sensor2Pressure = -NOVALIDPRESSUREDATA;
  datLast->sensor3Switch1 = -NOVALIDSWITCHDATA;
  datLast->sensor3Switch2 = -NOVALIDSWITCHDATA;
  datLast->sensor3Temperature = -NOVALIDTEMPERATUREDATA;
  datLast->sensor3Humidity = -NOVALIDHUMIDITYDATA;
  datMin->sensor1Temperature = NOVALIDTEMPERATUREDATA;
  datMin->sensor1Humidity = NOVALIDHUMIDITYDATA;
  datMin->sensor2Temperature = NOVALIDTEMPERATUREDATA;
  datMin->sensor2Humidity = NOVALIDHUMIDITYDATA;
  datMin->sensor2Pressure = NOVALIDPRESSUREDATA;
  datMin->sensor3Temperature = NOVALIDTEMPERATUREDATA;
  datMin->sensor3Humidity = NOVALIDHUMIDITYDATA;
  datMax->sensor1Temperature = -NOVALIDTEMPERATUREDATA;
  datMax->sensor1Humidity = -NOVALIDHUMIDITYDATA; // because of byte -255 is 0 and 0 is no valid value for humidity on my device
  datMax->sensor2Temperature = -NOVALIDTEMPERATUREDATA;
  datMax->sensor2Humidity = -NOVALIDHUMIDITYDATA;
  datMax->sensor2Pressure = -NOVALIDPRESSUREDATA;
  datMax->sensor3Temperature = -NOVALIDTEMPERATUREDATA;
  datMax->sensor3Humidity = -NOVALIDHUMIDITYDATA;
}

// display current, min and max temperature value
void drawTemperatureText(int x, int y, int &currentValue, int &lastValue, int &maxValue, int &minValue) {
  #define MAXSTRDATALENGTH 79
  char strData[MAXSTRDATALENGTH+1];

  if (lastValue == currentValue) return; // no nothing if value unchanged

  // Current value
  g_tft.setTextDatum(MR_DATUM);
  g_tft.setTextPadding( g_tft.textWidth("+88", 4) );
  if (currentValue != NOVALIDTEMPERATUREDATA) snprintf(strData,MAXSTRDATALENGTH+1,"%d",currentValue); else snprintf(strData,MAXSTRDATALENGTH+1,"-");
  g_tft.drawString(strData,x,y, 4);
  g_tft.setTextPadding(0);

  // Max value
  g_tft.setTextDatum(ML_DATUM);
  g_tft.setTextPadding( g_tft.textWidth("+88", 1) );
  strData[0]='\0';
  if ((abs(currentValue) != NOVALIDTEMPERATUREDATA) && 
    (maxValue < currentValue)) {
    maxValue = currentValue;
    snprintf(strData,MAXSTRDATALENGTH+1,"%d",maxValue);
  }
  if (maxValue == -NOVALIDTEMPERATUREDATA) snprintf(strData,MAXSTRDATALENGTH+1,"-");
  if (strlen(strData) > 0) g_tft.drawString(strData,x+3,y-12,1);

  // Min value
  strData[0]='\0';
  if ((abs(currentValue) != NOVALIDTEMPERATUREDATA) && 
    (minValue > currentValue)) {
    minValue = currentValue;
    snprintf(strData,MAXSTRDATALENGTH+1,"%d",minValue);
  }
  if (minValue == NOVALIDTEMPERATUREDATA) snprintf(strData,MAXSTRDATALENGTH+1,"-");
  if (strlen(strData) > 0) g_tft.drawString(strData,x+3,y-4,1);
  g_tft.setTextPadding(0);
}

// display current, min and max humidity value
void drawHumidityText(int x, int y, byte &currentValue, byte &lastValue, byte &maxValue, byte &minValue) {
  #define MAXSTRDATALENGTH 79
  char strData[MAXSTRDATALENGTH+1];

  if (lastValue == currentValue) return; // no nothing if value unchanged

  // Current value
  g_tft.setTextDatum(MR_DATUM);
  g_tft.setTextPadding( g_tft.textWidth("188", 4) );
  if (currentValue != NOVALIDHUMIDITYDATA) snprintf(strData,MAXSTRDATALENGTH+1,"%d",currentValue); else snprintf(strData,MAXSTRDATALENGTH+1,"-");
  g_tft.drawString(strData,x,y, 4);
  g_tft.setTextPadding(0);

  // Max value
  g_tft.setTextDatum(ML_DATUM);
  g_tft.setTextPadding( g_tft.textWidth("188", 1) );
  strData[0]='\0';
  if ((abs(currentValue) != NOVALIDHUMIDITYDATA) && 
    (maxValue < currentValue)) {
    maxValue = currentValue;
    snprintf(strData,MAXSTRDATALENGTH+1,"%d",maxValue);
  }
  if (maxValue == (byte) -NOVALIDHUMIDITYDATA) snprintf(strData,MAXSTRDATALENGTH+1,"-");
  if (strlen(strData) > 0) g_tft.drawString(strData,x+3,y-12,1);

  // Min value
  strData[0]='\0';
  if ((abs(currentValue) != NOVALIDHUMIDITYDATA) && 
    (minValue > currentValue)) {
    minValue = currentValue;
    snprintf(strData,MAXSTRDATALENGTH+1,"%d",minValue);
  }
  if (minValue == NOVALIDHUMIDITYDATA) snprintf(strData,MAXSTRDATALENGTH+1,"-");
  if (strlen(strData) > 0) g_tft.drawString(strData,x+3,y-4,1);
  g_tft.setTextPadding(0);
}

// display current, min and max pressure value
void drawPressureText(int x, int y, int &currentValue, int &lastValue, int &maxValue, int &minValue) {
  #define MAXSTRDATALENGTH 79
  char strData[MAXSTRDATALENGTH+1];

  if (lastValue == currentValue) return; // no nothing if value unchanged

  // Current value
  g_tft.setTextDatum(MR_DATUM);
  g_tft.setTextPadding( g_tft.textWidth("1888", 4) );
  if (currentValue != NOVALIDPRESSUREDATA) snprintf(strData,MAXSTRDATALENGTH+1,"%d",currentValue); else snprintf(strData,MAXSTRDATALENGTH+1,"-");
  g_tft.drawString(strData,x,y, 4);
  g_tft.setTextPadding(0);

  // Max value
  g_tft.setTextDatum(ML_DATUM);
  g_tft.setTextPadding( g_tft.textWidth("1888", 1) );
  strData[0]='\0';
  if ((abs(currentValue) != NOVALIDPRESSUREDATA) && 
    (maxValue < currentValue)) {
    maxValue = currentValue;
    snprintf(strData,MAXSTRDATALENGTH+1,"%d",maxValue);
  }
  if (maxValue == -NOVALIDPRESSUREDATA) snprintf(strData,MAXSTRDATALENGTH+1,"-");
  if (strlen(strData) > 0) g_tft.drawString(strData,x+3,y-12,1);

  // Min value
  strData[0]='\0';
  if ((abs(currentValue) != NOVALIDPRESSUREDATA) && 
    (minValue > currentValue)) {
    minValue = currentValue;
    snprintf(strData,MAXSTRDATALENGTH+1,"%d",minValue);
  }
  if (minValue == NOVALIDPRESSUREDATA) snprintf(strData,MAXSTRDATALENGTH+1,"-");
  if (strlen(strData) > 0) g_tft.drawString(strData,x+3,y-4,1);
  g_tft.setTextPadding(0);
}

// Task for tft display
void taskDisplay(void * parameter) {
  DisplayMessage msg;
  sensorData dat, datLast, datMax, datMin;
  #define SPRITEY 160
  #define MAXSTRDATALENGTH 79
  char strData[MAXSTRDATALENGTH+1];
  #define GRAPHDATAMAXITEMS 48 // Polygonpoints for graph. 48 is a step size of 5 pixel (Width-1)/(GRAPHDATAMAXITEMS-1)
  int graphData[GRAPHDATAMAXITEMS];
  byte graphHour[GRAPHDATAMAXITEMS];
  int graphLastPos, graphNextPos, graphIterationPos;

  bool touchWaitingRelease = true;
  unsigned long lastScreensaverMS=0;
  unsigned long lastTouchMS = 0;
  unsigned long lastTime = 0;
  uint16_t x, y, touchX, touchY;
  uint16_t yDraw = 0;
  time_t now;
  struct tm * ptrTimeinfo;
  TFT_eSprite img = TFT_eSprite(&g_tft);

  SERIALDEBUG.print("taskDisplay, CPU:");
  SERIALDEBUG.print(xPortGetCoreID());
  SERIALDEBUG.print(", Prio:");
  SERIALDEBUG.print(uxTaskPriorityGet(NULL));
  SERIALDEBUG.print(", MinStackFreeBytes:");
  SERIALDEBUG.print(uxTaskGetStackHighWaterMark(NULL));
  SERIALDEBUG.println();

  for (int i=0;i<GRAPHDATAMAXITEMS;i++) { graphData[i]=NOVALIDHUMIDITYDATA; graphHour[i]=255; };
  graphLastPos=0;

  resetLastMinMaxData(&datLast,&datMin,&datMax);

  img.setColorDepth(16);
  img.createSprite(240, 40);

  // Screen init
  if (xSemaphoreTake( g_semaphoreSPIBus, 10000 / portTICK_PERIOD_MS) == pdTRUE) {
    g_tft.setRotation(0); // Need 0 for hardware scrolling
    g_tft.fillRect(0,TOP_FIXED_AREA,g_tft.width(),g_tft.height()-TOP_FIXED_AREA,TFT_BLACK);
    drawBmp("/gui01.bmp", 0, 0);

    drawSwitches();
    drawBatteryStates(NOVALIDLOWBATTERY,NOVALIDLOWBATTERY,NOVALIDLOWBATTERY);

    g_tft.setTextDatum(ML_DATUM);
    x = g_tft.width()/2-7;
    y = 22;
    g_tft.drawString("`C",x,y,2);
    g_tft.drawString("`C",x,y+=32,2);
    g_tft.drawString("`C",x,y+=32,2);

    x = g_tft.width()/2+78;
    y = 22;
    g_tft.drawString("%",x,y,2);
    g_tft.drawString("%",x,y+=32,2);
    g_tft.drawString("%",x,y+=32,2);

    x = g_tft.width()/2+67;
    y = 118;
    g_tft.drawString("hPa",x,y,2);
    
    setupScrollArea(TOP_FIXED_AREA,0);

    xSemaphoreGive( g_semaphoreSPIBus );
  } else {
    SERIALDEBUG.println("Error: SPI Semaphore Timeout");
    beep();
    beep();
    beep();
  }
  
  for (;;) {
    if (xSemaphoreTake( g_semaphoreSPIBus, 10000 / portTICK_PERIOD_MS) == pdTRUE) {
      
      while( xQueueReceive( displayMsgQueue,&msg , 0 ) == pdPASS ) { // Process complete message queue
        time(&now);
        ptrTimeinfo = localtime ( &now );
  
        if ((msg.id == ID_INFO) || (msg.id == ID_ALERT)) {
          snprintf(strData,MAXSTRDATALENGTH+1,"%02d:%02d:%02d %s",ptrTimeinfo->tm_hour,ptrTimeinfo->tm_min,ptrTimeinfo->tm_sec,msg.strData);
          #define MAXSCREENCHARS 40
          if (strlen(strData) > MAXSCREENCHARS) {
            strData[MAXSCREENCHARS]='\0';
            strData[MAXSCREENCHARS-1]='.';
            strData[MAXSCREENCHARS-2]='.';                    
          }
          g_tft.setTextDatum(TL_DATUM);
          yDraw = scroll_line(g_tft.textWidth(strData,1));
          if (yDraw % (TEXT_HEIGHT*2) == 0) {
            if (msg.id == ID_ALERT) {
              g_tft.setTextColor(TFT_RED,TFT_BLACK);
              beep(LONGBEEP);
            } else g_tft.setTextColor(TFT_WHITE,TFT_BLACK);
          } else {
            if (msg.id == ID_ALERT) g_tft.setTextColor(TFT_RED,TFT_NAVY); else g_tft.setTextColor(TFT_YELLOW,TFT_NAVY);          
          }
          g_tft.drawString(strData,0, yDraw, 1);
  
        }
        if ((msg.id == ID_DISPLAYON) || (msg.id == ID_ALERT)) lastScreensaverMS = millis();
        if (msg.id == ID_REFESHBUTTONS) drawSwitches();
        if (msg.id == ID_RESET) {
          for (int i=0;i<GRAPHDATAMAXITEMS;i++) { graphData[i]=NOVALIDHUMIDITYDATA; graphHour[i]=255; };
          graphLastPos=0;
          g_tft.fillRect(0,SPRITEY,img.width(),img.height(),BACKGROUNDCOLOR); // Overwrite sprite area
          drawBatteryStates(NOVALIDLOWBATTERY,NOVALIDLOWBATTERY,NOVALIDLOWBATTERY,true);
          resetLastMinMaxData(&datLast,&datMin,&datMax);
        }
      }

      if( xQueueReceive( displayDatQueue,&dat , 0 ) == pdPASS ) { // Process next data queue entry
        time(&now);
        ptrTimeinfo = localtime ( &now );

        // show battery states
        drawBatteryStates(dat.sensor1LowBattery,dat.sensor3LowBattery,dat.sensor5LowBattery);

        // ------------------ Temperature -----------------
        g_tft.setTextColor(FONTCOLOR,BACKGROUNDCOLOR);
        x = g_tft.width()/2-10;
        y = 16;
        drawTemperatureText(x, y,     dat.sensor1Temperature, datLast.sensor1Temperature, datMax.sensor1Temperature, datMin.sensor1Temperature);
        drawTemperatureText(x, y+=32, dat.sensor3Temperature, datLast.sensor3Temperature, datMax.sensor3Temperature, datMin.sensor3Temperature); 
        drawTemperatureText(x, y+=32, dat.sensor2Temperature, datLast.sensor2Temperature, datMax.sensor2Temperature, datMin.sensor2Temperature); 
  
        // ------------------ Humidity -----------------
        x = g_tft.width()/2+75;
        y = 16;
        drawHumidityText(x, y,     dat.sensor1Humidity, datLast.sensor1Humidity, datMax.sensor1Humidity, datMin.sensor1Humidity);
        drawHumidityText(x, y+=32, dat.sensor3Humidity, datLast.sensor3Humidity, datMax.sensor3Humidity, datMin.sensor3Humidity); 
        drawHumidityText(x, y+=32, dat.sensor2Humidity, datLast.sensor2Humidity, datMax.sensor2Humidity, datMin.sensor2Humidity); 
        
        // ------------------ Pressure -----------------
        x = g_tft.width()/2+65;
        y = 112;
        drawPressureText(x, y, dat.sensor2Pressure, datLast.sensor2Pressure, datMax.sensor2Pressure, datMin.sensor2Pressure);

        // ------------------ Window sensors -----------------
        #define CROSSWIDTH 19
        #define CROSSHEIGHT 27

        x = 37-8-2-2;
        y = 112-64-8-8+1;

        if ((datLast.sensor3Switch1 != dat.sensor3Switch1) || (datLast.sensor3Switch2 != dat.sensor3Switch2))  {
          if ((dat.sensor3Switch1 == NOVALIDSWITCHDATA) || (dat.sensor3Switch2 == NOVALIDSWITCHDATA)) { // Unknown
            g_tft.drawLine(x+CROSSWIDTH/2,y,x+CROSSWIDTH/2,y+CROSSHEIGHT-1,TFT_DARKGREY);
            g_tft.drawLine(x,y+CROSSHEIGHT/2,x+CROSSWIDTH-1,y+CROSSHEIGHT/2,TFT_DARKGREY);
          } else {
            if (dat.sensor3Switch2 == 1) { // Open
                g_tft.drawLine(x+CROSSWIDTH/2,y,x+CROSSWIDTH/2,y+CROSSHEIGHT-1,TFT_BLACK);
                g_tft.drawLine(x,y+CROSSHEIGHT/2,x+CROSSWIDTH-1,y+CROSSHEIGHT/2,TFT_BLACK);
            } else {
              if (dat.sensor3Switch1 == 1) { // Tilted 
                g_tft.drawLine(x+CROSSWIDTH/2,y,x+CROSSWIDTH/2,y+CROSSHEIGHT/2-1,TFT_BLACK);
                g_tft.drawLine(x+CROSSWIDTH/2,y+CROSSHEIGHT/2,x+CROSSWIDTH/2,y+CROSSHEIGHT-1,TFT_WHITE);
                g_tft.drawLine(x,y+CROSSHEIGHT/2,x+CROSSWIDTH-1,y+CROSSHEIGHT/2,TFT_WHITE);
              } else { // Closed
                g_tft.drawLine(x+CROSSWIDTH/2,y,x+CROSSWIDTH/2,y+CROSSHEIGHT-1,TFT_WHITE);
                g_tft.drawLine(x,y+CROSSHEIGHT/2,x+CROSSWIDTH-1,y+CROSSHEIGHT/2,TFT_WHITE);
              }
            }
          }
        }

        // ------------------ Timestamp for last sensor data -----------------
        g_tft.setTextDatum(MR_DATUM);
        x = g_tft.textWidth("88:88:88", 4);
        y = 144;

        g_tft.setTextPadding( x );
        if (dat.time > 0) snprintf(strData,MAXSTRDATALENGTH+1,"%02d:%02d:%02d",ptrTimeinfo->tm_hour,ptrTimeinfo->tm_min,ptrTimeinfo->tm_sec); else snprintf(strData,MAXSTRDATALENGTH+1,"-");
        g_tft.drawString(strData,x,y,4);

        g_tft.setTextPadding( g_tft.textWidth("88.", 1) );
        g_tft.setTextDatum(TL_DATUM);
        if (dat.time > 0) snprintf(strData,MAXSTRDATALENGTH+1,"%02d.",ptrTimeinfo->tm_mday); else snprintf(strData,MAXSTRDATALENGTH+1,"-");
        g_tft.drawString(strData,x+1,y-12,1);
        g_tft.setTextDatum(ML_DATUM);
        if (dat.time > 0) snprintf(strData,MAXSTRDATALENGTH+1,"%02d.",ptrTimeinfo->tm_mon+1); else snprintf(strData,MAXSTRDATALENGTH+1,"-");
        g_tft.drawString(strData,x+1,y,1);
        g_tft.setTextPadding(0);
        g_tft.setTextDatum(TL_DATUM);

        // ------------------ Graph -----------------

        // Store current value in list at next position
        if (dat.sensor1Humidity != NOVALIDHUMIDITYDATA) {
          graphLastPos++;
          if (graphLastPos > GRAPHDATAMAXITEMS - 1) graphLastPos = 0;
          if (g_demoModeEnabled) graphHour[graphLastPos] = (graphLastPos % 24); else graphHour[graphLastPos]=ptrTimeinfo->tm_hour; 
          graphData[graphLastPos]=dat.sensor1Humidity;
        }

        // Create line graph
        if ((datMax.sensor1Humidity - datMin.sensor1Humidity) > 0) {
          img.fillSprite(TFT_BLACK);

          // First: Create the grid
          img.drawFastHLine(0,0,g_tft.width(),TFT_DARKGREY);
          img.drawFastHLine(0,39,g_tft.width(),TFT_DARKGREY);
 
          graphIterationPos = graphLastPos;
          for (int i=0;i<GRAPHDATAMAXITEMS;i++) {
            graphIterationPos++; // Iterate from oldest to newest value
            if (graphIterationPos > GRAPHDATAMAXITEMS-1) graphIterationPos = 0;

            if (graphData[graphIterationPos] != NOVALIDHUMIDITYDATA) {

              if (graphHour[graphIterationPos] == 0) { // Long grid line at 00:00
                img.drawFastVLine(
                  g_tft.width()-1 - ((g_tft.width()-1)/(GRAPHDATAMAXITEMS-1))*(GRAPHDATAMAXITEMS-1 - i),
                  1, 38,TFT_DARKGREY);
              } else { 
                if (graphHour[graphIterationPos] == 12) { // Middle size grid line at 12:00/noon
                  img.drawFastVLine(
                    g_tft.width()-1 - ((g_tft.width()-1)/(GRAPHDATAMAXITEMS-1))*(GRAPHDATAMAXITEMS-1 - i),
                    1, 10,TFT_DARKGREY);              
                  img.drawFastVLine(
                    g_tft.width()-1 - ((g_tft.width()-1)/(GRAPHDATAMAXITEMS-1))*(GRAPHDATAMAXITEMS-1 - i),
                    29,10,TFT_DARKGREY);        
                } else { 
                  // Default short grid lines
                  img.drawFastVLine(
                    g_tft.width()-1 - ((g_tft.width()-1)/(GRAPHDATAMAXITEMS-1))*(GRAPHDATAMAXITEMS-1 - i),
                    1, 5,TFT_DARKGREY);              
                  img.drawFastVLine(
                    g_tft.width()-1 - ((g_tft.width()-1)/(GRAPHDATAMAXITEMS-1))*(GRAPHDATAMAXITEMS-1 - i),
                    34,5,TFT_DARKGREY); 
                }             
              }
            }
          }

          // Second: Show min/max values
          // Note: Font size 1 in this sprite gave me an esp32 exception
          x = 0;
          y = 1;
          img.setTextColor(TFT_DARKGREY,BACKGROUNDCOLOR);    
          img.setTextDatum(TL_DATUM);
          snprintf(strData,MAXSTRDATALENGTH+1,"%d%%",datMax.sensor1Humidity);
          img.drawString(strData,x,y,2);
 
          y = 38;
          img.setTextDatum(BL_DATUM);
          snprintf(strData,MAXSTRDATALENGTH+1,"%d%%",datMin.sensor1Humidity);
          img.drawString(strData,x,y,2);  
          img.setTextPadding(0);

          // Third: Draw line graph
          graphIterationPos = graphLastPos;
          for (int i=0;i<GRAPHDATAMAXITEMS-1;i++) {
            graphIterationPos++; // Iterate a from oldest to newest value
            if (graphIterationPos > GRAPHDATAMAXITEMS-1) graphIterationPos = 0;
            graphNextPos = graphIterationPos+1; // Next value b to make line from a to b
            if (graphNextPos > GRAPHDATAMAXITEMS-1) graphNextPos = 0;

            if (graphData[graphIterationPos] != NOVALIDHUMIDITYDATA) {
              img.drawLine(
                g_tft.width()-1 - ((g_tft.width()-1)/(GRAPHDATAMAXITEMS-1))*(GRAPHDATAMAXITEMS-1 - i),
                39 - 39 * ((float)(graphData[graphIterationPos]-datMin.sensor1Humidity)/(datMax.sensor1Humidity - datMin.sensor1Humidity)) , 
                g_tft.width()-1 - ((g_tft.width()-1)/(GRAPHDATAMAXITEMS-1))*(GRAPHDATAMAXITEMS-1 - i-1),
                39 - 39 * ((float)(graphData[graphNextPos]-datMin.sensor1Humidity)/(datMax.sensor1Humidity - datMin.sensor1Humidity)),
                FONTCOLOR);
            }
          }
          graphLastPos = graphNextPos;
          
          img.pushSprite(0,SPRITEY);
        }
        datLast = dat;
      }
      if (digitalRead(TFT_LED) == LOW) { // Is display powered off/screensaver active?
        if (v_touchedTFT) { // touch IRQ?
          lastScreensaverMS = millis();
          detachInterrupt(TIRQ_PIN);  // g_tft.getTouch pulls IRQ pin to LOW and would cause next interrupt
          v_touchedTFT = false; 
        }
      } else { 
        if (g_tft.getTouch(&touchY, &touchX)) { // touch pad was pressed?
          // Fix touch x/y when using setRotation(0) in TFT_eSPI-Lib
          touchX = ((240 * touchX) / 320);
          touchY = ((320 * touchY) / 240);
          lastScreensaverMS = millis();
  
          if (digitalRead(TFT_LED) == HIGH) { // Is display powered on?
            if (millis() - lastTouchMS > 1000) touchWaitingRelease = false;
   
            if ((millis() - lastTouchMS > 300) && !touchWaitingRelease) {
              y = 200; // Begin of the button area
              if ((touchX < 80) && (touchY > y) && (touchY < y+32)) {
                g_ScreenSaverEnabled = !g_ScreenSaverEnabled;
                drawSwitches();
                lastTouchMS = millis();
                beep(SHORTBEEP);
                touchWaitingRelease = true;
              }
              if ((touchX > 80) && (touchX < 80+80) && (touchY > y) && (touchY < y+32)) {
                g_SoundDisabled = !g_SoundDisabled;
                drawSwitches();
                lastTouchMS = millis();
                beep(SHORTBEEP);
                touchWaitingRelease = true;
              }
              if ((touchX > 80+80) && (touchY > y) && (touchY < y+32)) {
                g_PIREnabled = !g_PIREnabled;
                drawSwitches();
                lastTouchMS = millis();
                beep(SHORTBEEP);
                touchWaitingRelease = true;
              }
              y = y + 32; // next line
              if ((touchX < 80) && (touchY > y) && (touchY < y+32)) {
                g_wifiSwitch = !g_wifiSwitch;
                drawSwitches();
                lastTouchMS = millis();
                beep(SHORTBEEP);
                touchWaitingRelease = true;
              }
              if ((touchX > 80) && (touchX < 80+80) && (touchY > y) && (touchY < y+32)) {
                g_demoModeSwitch = !g_demoModeSwitch;
                drawSwitches();
                lastTouchMS = millis();
                beep(SHORTBEEP);
                touchWaitingRelease = true;          
              }
              if ((touchX > 80+80) && (touchY > y) && (touchY < y+32) && g_mailAlert) {
                g_mailAlert=false; 
                g_pendingBlynkMailAlert = clear;
                drawSwitches();
                lastTouchMS = millis();
                beep(SHORTBEEP);
                touchWaitingRelease = true;
              }
            }
          } else {
            lastTouchMS = millis(); // Debounce when display is powered on
            touchWaitingRelease = true;
          }
        } else {
          // Screen not touched
          touchWaitingRelease = false;
        }
      }
      // Display time
      if ((millis() - lastTime) >= 1000) {
        time(&now);
        ptrTimeinfo = localtime ( &now );

        g_tft.setTextDatum(ML_DATUM);
        g_tft.setTextColor(FONTCOLOR,BACKGROUNDCOLOR);          

        x = g_tft.width()/2 + 16;
        y = 144;

        g_tft.setTextPadding( g_tft.textWidth("88:88:88", 4) );
        snprintf(strData,MAXSTRDATALENGTH+1,"%02d:%02d:%02d",ptrTimeinfo->tm_hour,ptrTimeinfo->tm_min,ptrTimeinfo->tm_sec);
        g_tft.drawString(strData,x,y,4);

        lastTime = millis();
      }
      // Power off display when screensaver timeout is reached
      if ((millis() - lastScreensaverMS > SCREENSAVERMS) && g_ScreenSaverEnabled) {
        digitalWrite(TFT_LED,LOW); 
        attachInterrupt(TIRQ_PIN, TouchISR, FALLING); // Enable touch IRQ
      } else {
        digitalWrite(TFT_LED,HIGH);
      }

      xSemaphoreGive( g_semaphoreSPIBus );
    } else {
      SERIALDEBUG.println("Error: SPI Semaphore Timeout");
      beep();
      beep();
      beep();
    }
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }  
}
