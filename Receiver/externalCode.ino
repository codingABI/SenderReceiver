/* Code which is not mine:
 *  
 * a) Hardwarescrolling functions "scroll_line", "scrollAddress", "setupScrollArea" are 
 *    from bodmers TFT_Terminal.ino (https://github.com/Bodmer/TFT_eSPI/blob/master/examples/320%20x%20240/TFT_Terminal/TFT_Terminal.ino)
 *    Original header: 
 *************************************************************
  This sketch implements a simple serial receive terminal
  program for monitoring serial debug messages from another
  board.
  
  Connect GND to target board GND
  Connect RX line to TX line of target board
  Make sure the target and terminal have the same baud rate
  and serial stettings!
  The sketch works with the ILI9341 TFT 240x320 display and
  the called up libraries.
  
  The sketch uses the hardware scrolling feature of the
  display. Modification of this sketch may lead to problems
  unless the ILI9341 data sheet has been understood!
  Updated by Bodmer 21/12/16 for TFT_eSPI library:
  https://github.com/Bodmer/TFT_eSPI
  
  BSD license applies, all text above must be included in any
  redistribution
 *************************************************************
 *
 * b) BMP functions "drawBmp", "read16", "read32" are 
 *    from bodmers BMP_functions.ino (https://github.com/Bodmer/TFT_eSPI/blob/master/examples/Generic/TFT_SPIFFS_BMP/BMP_functions.ino)
 */

// The initial y coordinate of the top of the scrolling area
uint16_t g_yStart = TOP_FIXED_AREA;

// ##############################################################################################
// Call this function to scroll the display one text line
// ##############################################################################################
int scroll_line(int textWidth) {
  int yTemp = g_yStart; // Store the old g_yStart, this is where we draw the next line
  if (g_yStart % (TEXT_HEIGHT*2) == 0) {
    g_tft.fillRect(textWidth,g_yStart,g_tft.width()-textWidth,TEXT_HEIGHT, TFT_BLACK);
  } else {
    g_tft.fillRect(textWidth,g_yStart,g_tft.width()-textWidth,TEXT_HEIGHT, TFT_NAVY);    
  }
  // Change the top of the scroll area
  g_yStart+=TEXT_HEIGHT;
  // The value must wrap around as the screen memory is a circular buffer
  if (g_yStart >= g_tft.height()) g_yStart = TOP_FIXED_AREA + (g_yStart - g_tft.height());
  // Now we can scroll the display
  scrollAddress(g_yStart);
  return  yTemp;
}

// ##############################################################################################
// Setup the vertical scrolling start address pointer
// ##############################################################################################
void scrollAddress(uint16_t vsp) {
  g_tft.writecommand(ILI9341_VSCRSADD); // Vertical scrolling pointer
  g_tft.writedata(vsp>>8);
  g_tft.writedata(vsp);
}

// ##############################################################################################
// Setup a portion of the screen for vertical scrolling
// ##############################################################################################
// We are using a hardware feature of the display, so we can only scroll in portrait orientation
void setupScrollArea(uint16_t tfa, uint16_t bfa) {
  g_tft.writecommand(ILI9341_VSCRDEF); // Vertical scroll definition
  g_tft.writedata(tfa >> 8);           // Top Fixed Area line count
  g_tft.writedata(tfa);
  g_tft.writedata((g_tft.height()-tfa-bfa)>>8);  // Vertical Scrolling Area line count
  g_tft.writedata(g_tft.height()-tfa-bfa);
  g_tft.writedata(bfa >> 8);           // Bottom Fixed Area line count
  g_tft.writedata(bfa);
}

// Bodmers BMP image rendering function

void drawBmp(const char *filename, int16_t x, int16_t y) {

  if (xSemaphoreTake( g_semaphoreLittleFS, 10000 / portTICK_PERIOD_MS) == pdTRUE) {

      if ((x >= g_tft.width()) || (y >= g_tft.height())) return;
    
      fs::File bmpFS = LittleFS.open(filename,  FILE_READ );
    
      if (!bmpFS)
      {
        SERIALDEBUG.print("File not found");
        return;
      }
    
      uint32_t seekOffset;
      uint16_t w, h, row, col;
      uint8_t  r, g, b;
    
      uint32_t startTime = millis();
    
      if (read16(bmpFS) == 0x4D42)
      {
        read32(bmpFS);
        read32(bmpFS);
        seekOffset = read32(bmpFS);
        read32(bmpFS);
        w = read32(bmpFS);
        h = read32(bmpFS);
    
        if ((read16(bmpFS) == 1) && (read16(bmpFS) == 24) && (read32(bmpFS) == 0))
        {
          y += h - 1;
    
          bool oldSwapBytes = g_tft.getSwapBytes();
          g_tft.setSwapBytes(true);
          bmpFS.seek(seekOffset);
    
          uint16_t padding = (4 - ((w * 3) & 3)) & 3;
          uint8_t lineBuffer[w * 3 + padding];
    
          for (row = 0; row < h; row++) {
            
            bmpFS.read(lineBuffer, sizeof(lineBuffer));
            uint8_t*  bptr = lineBuffer;
            uint16_t* tptr = (uint16_t*)lineBuffer;
            // Convert 24 to 16 bit colours
            for (uint16_t col = 0; col < w; col++)
            {
              b = *bptr++;
              g = *bptr++;
              r = *bptr++;
              *tptr++ = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
            }
    
            // Push the pixel row to screen, pushImage will crop the line if needed
            // y is decremented as the BMP image is drawn bottom up
            g_tft.pushImage(x, y--, w, 1, (uint16_t*)lineBuffer);
          }
          g_tft.setSwapBytes(oldSwapBytes);
          SERIALDEBUG.print("Loaded in "); SERIALDEBUG.print(millis() - startTime);
          SERIALDEBUG.println(" ms");
        }
        else SERIALDEBUG.println("BMP format not recognized.");
      } else SERIALDEBUG.println("Wrong header");
      bmpFS.close();
      xSemaphoreGive( g_semaphoreLittleFS );
  } else {
    SERIALDEBUG.println("Error: LittleFS Semaphore Timeout");
    beep();
    beep(SHORTBEEP);
    beep();
  }
}

// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t read16(fs::File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(fs::File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}
