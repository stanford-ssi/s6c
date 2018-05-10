#include <Arduino.h>
#include "../../src/config.h"
#include "RH_RF24.h"
#include "ecc.h"

/*
SRADio GFSK Library
This library provides an abstraction for the SRADio hardware developed by SSI.
*/

class SRADio
{
public:
  SRADio();
  void configureRF();
  void encode_and_transmit(void *msg_data, uint8_t msg_size);
  uint8_t tryToRX(void *msg_data, uint8_t msg_size);
  uint8_t getRSSI();
  String getSyndrome();

private:
  void RadioOff();
  void RadioOn();
  void configurePins();
  RH_RF24 *rf24;    //the RadioHead Driver Object
  uint8_t lastRssi; //RSSI of last reception
  String errorSyndrome;
};
