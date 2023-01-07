# SenderReceiver
## Receiver (433 MHz ASK and LoRa)
To be done...
## Sender 1 (433 MHz ASK)
![Schematic](assets/images/Sender1/Schematic.png)
## Sender 3 (433 MHz ASK)
Magnetic reed switch sensor to detect if a window is open or tilted. Sends the window state, temperature, humidity and battery state every 30 minutes or triggered by magnetic reed switch change via a 433MHz-ASK to a receiver.

Hardware:
* Microcontroller ATmega328P (without crystal, in 8 MHz-RC mode. Board manager: "ATmega328 on a breadboard (8 MHz internal clock)" ) 
* DHT22 sensor
* 433MHz FS1000A sender
* 3x AA-Batteries without voltage regulation (runtime >8 months, dependent on usage of rechargeable or normal batteries)
* Control LED (blinks every 8 seconds) which can be enabled by a physical jumper
* Two magnetic reed switches "normally closed" to detect the window state

[Arduino-Sketch](/Sender3/Sender3.ino)

![Schematic](assets/images/Sender3/Schematic.png)

![Schematic](assets/images/Sender3/PCB.jpg)
Case for the 3xAA batteries is a pice standard PVC installation tube (20mm)
![Schematic](assets/images/Sender3/BatteriesPCB.jpg)
Case for the device is a standard PVC installation tube (25mm)
![Schematic](assets/images/Sender3/Case.jpg)
![Schematic](assets/images/Sender3/WindowReedSwitches.jpg)

## Sender 5 (433 MHz LoRa)
Sensor for a mailbox. When the lid of the slot is opened, a magnetic reed switch triggers and sends a LoRa signal to the receiver. Additionally once per day the current battery voltage and the magnetic reed switch state will also be sent to the receiver.

Hardware:
* Microcontroller ATmega328P (without crystal, in 8 MHz-RC mode. Board manager: "ATmega328 on a breadboard (8 MHz internal clock)" ) 
* HT7333 voltage regulator
* Lora SX1278 Ra-02 (433 MHz)
* 18650 Battery with integrated protection against deep discharge
* Magnetic reed-switch "normally closed" with external pullup resistor
* Control LED which can be enabled/disabled on demand with physical jumper JP2

[Arduino-Sketch](/Sender5/Sender5.ino)

![Schematic](assets/images/Sender5/Schematic.png)
![Perfboard frontside](assets/images/Sender5/PerfboardFrontside.jpg)
![Perfboard backside](assets/images/Sender5/PerfboardBackside.jpg)
![Components](assets/images/Sender5/Components.jpg)
The sender is in a standard junction box
![Case opened](assets/images/Sender5/CaseOpened.jpg)
![Case closed](assets/images/Sender5/CaseClosed.jpg)
The device is inside the mailbox
![Mailbox](assets/images/Sender5/Mailbox.jpg)
