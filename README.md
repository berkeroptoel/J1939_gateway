# CAN gateway project

Let's start with an example: 
PGN61444(0x0F004) includes "engine speed" information.   
We get 29bit CAN ID from CAN BUS for every CAN Frame. (is stored 32bits variable)  
So, we need to convert this 29bit CAN ID to 18bit PGN. 

- [For reference](https://www.csselectronics.com/pages/j1939-explained-simple-intro-tutorial)

| PGN | CAN ID DECIMAL | CAN ID HEX | INFORMATION
| --- | --- | --- | --- | 
| 64929 | 419275009 | 18FDA101 | Fuel control
| 61444 | 217056510 | 0CF004FE | Engine control
| 61713 | 418451966 | 18F111FE | Fuel pressure

- [Converter](https://docs.google.com/spreadsheets/d/10f7-TFU9oViSQZYGFYVPDia2w1hd5eOPMlgJXmx31Lg/edit#gid=1130918092)

CAN IDs which are filtered to send to MQTT broker is stored in a csv file called [J1939_PGN_table.csv](https://github.com/berkeroptoel/J1939_gate/blob/master/J1939_logger/Records/J1939_PGN_table.csv) 

29bitCAN ID based filtering is done in the current FW, but it can be done 18bitPGN based as well.  


### Testing with CANKing

![CANKing](https://github.com/berkeroptoel/J1939_gateway/blob/master/J1939_logger/Records/CANKing.png)

 


### Decoding to SPN  
For instance there're seven SPNs in for PGN61444. Like SPN190 for Engine speed, SPN899 for Engine torque...  
PGN61444(217056510 in decimal) has 8 byte payload: FF FF FF 68 13 FF FF FF
2 bytes 6813 means 621 RPM engine speed.  

0x68 = 104  
0x13 = 19  
19*256 + 104 = 4968 


![SPN190](https://github.com/berkeroptoel/J1939_gateway/blob/master/Records/RPM.png)

-Json deserialize  
-Pysical value conversion should be done in MQTT client.  


### Methods(Operating mode can be selected from menuconfig)  
1) FAST MODE: CAN frame is instantly transmitted to mqtt server.  
2) TRIGGER MODE: CAN frame spiffs are stored in SPIFFS memory, then when the request comes, transmitted to mqtt server.   

![MODES](https://github.com/berkeroptoel/J1939_gateway/blob/master/Records/M1.drawio.png)

## Example folder contents

The project **J1939_logger** contains one source file in C language [main.c](J1939_logger/main/main.c). 

```
├── CMakeLists.txt
├── mqtt_test.py               Python script used for mqtt testing
├── mqtt_getlogs.py            Python script used for getting logs
├── main
│   ├── CMakeLists.txt
│   ├── component.mk           Component make file
│   └── main.c
├── Makefile                   Makefile used by legacy GNU Make
└── README.md                  This is the file you are currently reading
```

