#include <Arduino.h>
#include "RH_RF24.h"

//#define MAX_MSG_LENGTH 23
//#define NPAR 20
//#define FRAME_SIZE ( MAX_MSG_LENGTH + NPAR )

#include "ecc.h"

// see https://github.com/arduino/ArduinoCore-samd/blob/master/variants/arduino_zero/variant.cpp

#define GFSK_SDN 13
#define GFSK_IRQ 12
#define GFSK_GATE 5
#define GFSK_GPIO_0 7
#define GFSK_GPIO_1 8
#define GFSK_GPIO_2 20
#define GFSK_GPIO_3 21
#define GFSK_CS 38
#define GFSK_SCK 5
#define GFSK_MOSI 4
#define GFSK_MISO 18

#define RF_FREQ 433.5

#define LED_PIN 27

#define PRINT_TIMING
#define PRINT_ENCODED_DATA
#define PRINT_RSSI
/*
S6B GFSK Library
This library provides an abstraction for the S6B hardware developed by SSI.
*/

class S6B
{
public:
  S6B();
  void configureRF();
  void encode_and_transmit(void *msg_data, uint8_t msg_size);
  uint8_t tryToRX(void *msg_data, uint8_t msg_size);
  uint8_t getRSSI();
  String getSyndrome();
  RH_RF24 *rf24;    //the RadioHead Driver Object
  void LEDOn();
  void LEDOff();

private:
  void RadioOff();
  void RadioOn();
  void configurePins();
  uint8_t lastRssi; //RSSI of last reception
  String errorSyndrome;
};
