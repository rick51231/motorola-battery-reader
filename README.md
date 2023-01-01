# Motorola battery reader
Development of open wireless Motorola IMPRES battery reader (gen 2 batteries)

Each battery have two chips: DS2433 (1Wire EEPROM) and DS2438 (1Wire battery monitor). 
In folder [images](images) you can find some prototype assembly images.

## Firmware installation
* Clone repository
* Install dependencies (WiFi, WiFiMulti, OneWire, HTTPClient)
* Copy settings-example.h to settings.h and edit it
* Upload firmware via Arduino IDE or Arduino-CLI

## Hardware
* Cheap chinese IMPRES (not really) charger
* ESP32 devkit
* 5V DC/DC
* Wires, resistors

## Software
For parsing battery data you can use `BMS` class from the [rick51231/node-dmr-lib](https://github.com/rick51231/node-dmr-lib) (Services.BMS.fromBatteryChip)

## TODO list
* Provide hardware assembly instructions
* Provide schematic
* Provide battery information and pinout
* Add 1-Wire bypass mode to work with original IMPRES BatteryReader software
* Make own PCB
* Provide example server application