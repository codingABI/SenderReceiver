/* ----------- Sensor data definitions ----------
 * License: 2-Clause BSD License
 * Copyright (c) 2023 codingABI
 */

// Invalid sensor values (to distinguish valid values)
#define NOVALIDTEMPERATUREDATA 255
#define NOVALIDHUMIDITYDATA 255
#define NOVALIDPRESSUREDATA 2000
#define NOVALIDSWITCHDATA 255
#define NOVALIDLOWBATTERY 255
#define NOVALIDVCCDATA 255
#define NOVALIDRUNTIME 1023
#define NOVALIDPCI 255

// Size of ringbuffer
#define sensorDataBufferSize 24

// Structur for a sensor data set
typedef struct {
  time_t time;
  time_t sensor1LastDataTime = 0;
  byte sensor1LowBattery = NOVALIDLOWBATTERY;
  int sensor1Temperature = NOVALIDTEMPERATUREDATA;
  byte sensor1Humidity = NOVALIDHUMIDITYDATA;
  byte sensor1Vcc = NOVALIDVCCDATA;
  time_t sensor2LastDataTime = 0;
  int sensor2Temperature = NOVALIDTEMPERATUREDATA;
  byte sensor2Humidity = NOVALIDHUMIDITYDATA;
  int sensor2Pressure = NOVALIDPRESSUREDATA;
  time_t sensor3LastDataTime = 0;
  byte sensor3LowBattery = NOVALIDLOWBATTERY;
  byte sensor3Switch1 = NOVALIDSWITCHDATA;
  byte sensor3Switch2 = NOVALIDSWITCHDATA;
  int sensor3Temperature = NOVALIDTEMPERATUREDATA;
  byte sensor3Humidity = NOVALIDHUMIDITYDATA;
  byte sensor3Vcc = NOVALIDVCCDATA;
  time_t sensor4LastDataTime = 0;
  byte sensor4LowBattery = NOVALIDLOWBATTERY;
  byte sensor4Vcc = NOVALIDVCCDATA;
  int sensor4Runtime = NOVALIDRUNTIME;
  time_t sensor5LastDataTime = 0;
  byte sensor5LowBattery = NOVALIDLOWBATTERY;
  byte sensor5Vcc = NOVALIDVCCDATA;
  byte sensor5Switch1 = NOVALIDSWITCHDATA;
  byte sensor5PCI1 = NOVALIDPCI;
} sensorData;

// Simple ringbuffer class for sensor data sets
class SensorDataBuffer {
  public : 
    sensorData DataBuffer[sensorDataBufferSize];
    void show() {
      byte i;
      #define MAXSTRLEN 127
      char strData[MAXSTRLEN+1];
      struct tm * ptrTimeinfo;

      if (isEmpty()) { return; }

      i = DataBufferFirstElement;
      do {
        assert(i < sensorDataBufferSize);
        // UTC Zeit
        ptrTimeinfo = gmtime ( &(DataBuffer[i].time) );
        snprintf(strData,MAXSTRLEN+1,"UTC %02d %02d.%02d.%04d;%02d:%02d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d",i,
          ptrTimeinfo->tm_mday,
          ptrTimeinfo->tm_mon + 1,
          ptrTimeinfo->tm_year + 1900,
          ptrTimeinfo->tm_min,
          ptrTimeinfo->tm_hour,
          DataBuffer[i].sensor1LowBattery,
          DataBuffer[i].sensor1Temperature,
          DataBuffer[i].sensor1Humidity,
          DataBuffer[i].sensor1Vcc,
          DataBuffer[i].sensor2Temperature,
          DataBuffer[i].sensor2Humidity,
          DataBuffer[i].sensor2Pressure,
          DataBuffer[i].sensor3LowBattery,
          DataBuffer[i].sensor3Switch1,
          DataBuffer[i].sensor3Switch2,
          DataBuffer[i].sensor3Vcc,
          DataBuffer[i].sensor3Temperature,
          DataBuffer[i].sensor3Humidity,
          DataBuffer[i].sensor4LowBattery,
          DataBuffer[i].sensor4Vcc,
          DataBuffer[i].sensor4Runtime,
          DataBuffer[i].sensor5LowBattery,
          DataBuffer[i].sensor5Vcc,
          DataBuffer[i].sensor5Switch1,
          DataBuffer[i].sensor5PCI1
          );

        Serial.println(strData);
        if (i == DataBufferLastElement) break;
        i++;
        if (i >= sensorDataBufferSize) { i = 0; };
      } while (true);
    }
    bool addLast(sensorData newSensorData) {
      byte DataBufferNextElement;

      if (isFull()) { return false; }

      DataBufferNextElement = nextElement();
      
      if (isEmpty()) { DataBufferFirstElement = 0; }

      assert(DataBufferNextElement < sensorDataBufferSize);
      DataBuffer[DataBufferNextElement] = newSensorData;
      
      DataBufferLastElement = DataBufferNextElement;
      return true;
    }
    bool removeFirst() {
      if (isEmpty()) { return false; } // Empty

      assert(DataBufferFirstElement < sensorDataBufferSize);
      DataBuffer[DataBufferFirstElement].time = 0;
      DataBuffer[DataBufferFirstElement].sensor1LowBattery = NOVALIDLOWBATTERY;
      DataBuffer[DataBufferFirstElement].sensor1Temperature = NOVALIDTEMPERATUREDATA;
      DataBuffer[DataBufferFirstElement].sensor1Vcc = NOVALIDVCCDATA;
      DataBuffer[DataBufferFirstElement].sensor1Humidity = NOVALIDHUMIDITYDATA;
      DataBuffer[DataBufferFirstElement].sensor2Temperature = NOVALIDTEMPERATUREDATA;
      DataBuffer[DataBufferFirstElement].sensor2Humidity = NOVALIDHUMIDITYDATA;
      DataBuffer[DataBufferFirstElement].sensor2Pressure = NOVALIDPRESSUREDATA;
      DataBuffer[DataBufferFirstElement].sensor3LowBattery = NOVALIDLOWBATTERY;
      DataBuffer[DataBufferFirstElement].sensor3Switch1 = NOVALIDSWITCHDATA;
      DataBuffer[DataBufferFirstElement].sensor3Switch2 = NOVALIDSWITCHDATA; 
      DataBuffer[DataBufferFirstElement].sensor3Vcc = NOVALIDVCCDATA;
      DataBuffer[DataBufferFirstElement].sensor3Temperature = NOVALIDTEMPERATUREDATA;
      DataBuffer[DataBufferFirstElement].sensor3Humidity = NOVALIDHUMIDITYDATA;
      DataBuffer[DataBufferFirstElement].sensor4LowBattery = NOVALIDLOWBATTERY;
      DataBuffer[DataBufferFirstElement].sensor4Vcc = NOVALIDVCCDATA;
      DataBuffer[DataBufferFirstElement].sensor4Runtime = NOVALIDRUNTIME;
      DataBuffer[DataBufferFirstElement].sensor5LowBattery = NOVALIDLOWBATTERY;
      DataBuffer[DataBufferFirstElement].sensor5Vcc = NOVALIDVCCDATA;
      DataBuffer[DataBufferFirstElement].sensor5Switch1 = NOVALIDSWITCHDATA;
      DataBuffer[DataBufferFirstElement].sensor5PCI1 = NOVALIDPCI;
       
      if (DataBufferFirstElement == DataBufferLastElement) { DataBufferFirstElement = sensorDataBufferSize; return true; } // now empty
      DataBufferFirstElement ++;
      if (DataBufferFirstElement >= sensorDataBufferSize) { DataBufferFirstElement = 0; };
      return true;
    }
    sensorData getByIndex(int i) {
      int element;
      if (i > countElements() - 1) i = countElements() - 1;
      element = DataBufferFirstElement + i;
      if (element >= sensorDataBufferSize) {
        element = element - sensorDataBufferSize;
      }
      assert(element < sensorDataBufferSize);
      return DataBuffer[element];
    }
    bool isEmpty() {
      if (DataBufferFirstElement == sensorDataBufferSize) { return true; } else { return false; }
    }
    bool isFull() {
      if (isEmpty()) { return false; } // Empty
      if (nextElement() == DataBufferFirstElement) { return true; } else { return false; }
    }
    byte countElements() {
      if (isEmpty()) { return 0; }
      if (isFull()) { return sensorDataBufferSize; }
      if (DataBufferLastElement >= DataBufferFirstElement) { 
        return DataBufferLastElement - DataBufferFirstElement + 1; 
      } else {
        return sensorDataBufferSize - DataBufferFirstElement + DataBufferLastElement + 1;         
      }
    }
  private :
    byte DataBufferFirstElement = sensorDataBufferSize;
    byte DataBufferLastElement = 0;

    byte nextElement() {
      byte DataBufferNextElement;

      if (DataBufferFirstElement == sensorDataBufferSize) { return 0; } // Empty
      DataBufferNextElement = DataBufferLastElement + 1;
      if (DataBufferNextElement >= sensorDataBufferSize) { DataBufferNextElement = 0; }
      return DataBufferNextElement;
    }
};
