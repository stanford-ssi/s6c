#include "s6c.h"

//S6C:
//Constructs a new S6C object
S6C::S6C()
{
  initialize_ecc();
  rf24 = new RH_RF24(GFSK_CS, GFSK_IRQ, GFSK_SDN);

}

//getHWID
//returns hardware ID number for board
//HWID is stored and returned MSB first
//i.e. for the HWID 0x6C42, EEPROM position 1 will be 0x6C
uint16_t S6C::getHWID()
{
  return (EEPROM.read(LOC_HWID_MSB) << 8) | EEPROM.read(LOC_HWID_LSB);
}

//clearHWIDfuse
//adds a step that makes it harder to overwrite the HW ID
void S6C::clearHWIDfuse()
{
  EEPROM.write(LOC_HWIDFUSE, 0);
}

//setHWID
//if fuse is disabled, overwrites hardware ID in EEPROM
//DANGER: Do not call this repeatedly, as EEPROM has
//a limited number of writes before the hardware is irreparably damaged
uint16_t S6C::setHWID(uint16_t new_HWID){
  if(!EEPROM.read(LOC_HWIDFUSE)){
    EEPROM.write(LOC_HWID_MSB, (uint8_t)((new_HWID & 0xFF00) >> 8));
    EEPROM.write(LOC_HWID_LSB, (uint8_t)(new_HWID & 0xFF));
    EEPROM.write(LOC_HWIDFUSE, 1);
    EEPROM.commit();
  }
}

//configureRF:
//Configures the Radio Hardware, sets registers, and prepares for transmission
void S6C::configureRF()
{
  configurePins();
  initialize_ecc();
  RadioOff();
  delay(500);
  RadioOn();
  bool initSuccess = rf24->init();


  while(!initSuccess){
    SerialUSB.print("Bootup failure with error code ");
    SerialUSB.print(rf24->initializationStatus);
    SerialUSB.println(". See RH_RF24.cpp for codes.");
    blinkStatus(rf24->initializationStatus);
    delay(1000);
    SerialUSB.println("Attempting to reinitialize...");
    initSuccess = rf24->init();
  }

  uint8_t buf[8];
  if (!rf24->command(RH_RF24_CMD_PART_INFO, 0, 0, buf, sizeof(buf)))
  {
    SerialUSB.println("SPI ERROR");
  }
  else
  {
    SerialUSB.println("SPI OK");
  }
  if (!rf24->setFrequency(RF_FREQ))
  {
    SerialUSB.println("Set Frequency failed");
  }
  else
  {
    SerialUSB.print("Frequency set to ");
    SerialUSB.print(RF_FREQ);
    SerialUSB.println(" MHz.");
  }

  SerialUSB.println("RF Configured");

  rf24->setTxPower(0x7f);
}

//encode_and_transmit:
//Performs ECC encoding, packages a frame, and transmits.
//takes:
//  -msg_data, a pointer to an array of data to transmit
//  -msg_size, the size of msg_data
void S6C::encode_and_transmit(void *msg_data, uint8_t msg_size)
{
  //message must be withing the frame size constraints
  if (msg_size > 255 - NPAR)
  {
    SerialUSB.println("Message too large!");
  }

  //add padding zeroes to normalize message length
  uint8_t padded_msg_data[256] = {0};
  memcpy(padded_msg_data, msg_data, msg_size);

  //calculate ECC data and package it into a frame
  uint8_t frame_data[256] = {0};
  //memcpy(frame_data, padded_msg_data, MAX_MSG_LENGTH);
  encode_data(padded_msg_data, msg_size, frame_data); //This does the ECC

//debug frame contents
#ifdef PRINT_ENCODED_DATA
  SerialUSB.println("encoded data");
  for (int i = 0; i < (msg_size + NPAR); i++)
  {
    uint8_t k = frame_data[i];
    SerialUSB.print(k);
    if (k < 10)
      SerialUSB.print(" ");
    if (k < 100)
      SerialUSB.print(" ");
    SerialUSB.print(" ");
  }
  SerialUSB.println();
#endif

  //transmit frame (blocking)
  uint32_t timer = micros();
  rf24->send(frame_data, msg_size + NPAR);
  rf24->waitPacketSent();

//debug transmission time
#ifdef PRINT_TIMING
  SerialUSB.print("Sent ");
  SerialUSB.print(msg_size + NPAR);
  SerialUSB.print(" bytes in ");
  SerialUSB.print((micros() - timer) / 1000.);
  SerialUSB.println(" ms");
#endif
}

//configurePins:
//Configures the pins needed for the radio.
//The SPI configuration might not be needed, as RadioHead seems to already do it.
void S6C::configurePins()
{
  pinMode(GFSK_GATE, OUTPUT);
  pinMode(GFSK_SDN, OUTPUT);

  //THIS MIGHT BE NOT NEEDED!
  /*
  SPI.setSCK(GFSK_SCK);
  SPI.setMOSI(GFSK_MOSI);
  SPI.setMISO(GFSK_MISO);
  SPI.setDataMode(SPI_MODE0);
  SPI.setClockDivider(SPI_CLOCK_DIV2); // Setting clock speed to 8mhz, as 10 is the max for the rfm22
  */
  SPI.begin();
  //
}

//RadioOff:
//disables the SiLabs Radio chip
void S6C::RadioOff()
{
  digitalWrite(GFSK_GATE, HIGH);
}

//RadioOn:
//enables the SiLabs Radio chip
void S6C::RadioOn()
{
  digitalWrite(GFSK_GATE, LOW);
}

//configureLED:
//sets up LED
void S6C::configureLED()
{
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
}

//LEDOn:
//turns on LED
//TODO allow config to disable LED on/off
void S6C::LEDOn(bool force)
{
  digitalWrite(LED_PIN, HIGH);
}

//LEDOff:
//turns off LED
//TODO allow config to disable LED on/off
void S6C::LEDOff(bool force)
{
  digitalWrite(LED_PIN, LOW);
}

//blinkStatus:
//Blink a couple times to indicate a status - blocking
void S6C::blinkStatus(int blinks)
{
  digitalWrite(LED_PIN, LOW);
  delay(250);
  for(int i = 0; i < blinks; i++){
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
  }
  delay(250);
}

//tryToRX:
//attempts to process any recived messages
uint8_t S6C::tryToRX(void *msg_data, uint8_t msg_size)
{
  uint8_t data[300] = {0}; //32 bytes buffer room
  uint8_t data_size = msg_size + NPAR;
  uint8_t returnCode = 0; //bits: recived, ECC used, ECC failed, Frame err

  if (rf24->recv(data, &data_size))
  {
    bitSet(returnCode,0);
    lastRssi = (uint8_t)rf24->lastRssi();

#ifdef PRINT_RSSI
    SerialUSB.print("Got stuff at RSSI: ");
    SerialUSB.println(lastRssi);
#endif

#ifdef PRINT_DEBUG
    if (data_size != FRAME_SIZE)
    {
      SerialUSB.print("Error, got frame of size ");
      SerialUSB.print(data_size);
      SerialUSB.print(", expecting ");
      SerialUSB.println(FRAME_SIZE);
      bitSet(returnCode,3);
    }
#endif

#ifdef PRINT_ENCODED_DATA
    for (int kk = 0; kk < data_size; kk++)
      SerialUSB.print((char)data[kk]);
    SerialUSB.println();
#endif

    unsigned char copied[300];
    memcpy(copied, data, 300);
    decode_data(copied, data_size);

    if (check_syndrome() != 0)
    {
      bitSet(returnCode,1);
      SerialUSB.println("There were errors");
      int correct = correct_errors_erasures(copied, msg_size + NPAR, 0, NULL);

      errorSyndrome = String();
      for (int i = 0; i < NPAR; i++)
        errorSyndrome += synBytes[i];
        errorSyndrome += ",";

      if (correct)
      {
        SerialUSB.println("Corrected successfully.");
        SerialUSB.println("Sydrome: " + errorSyndrome);
      }
      else
      {
        SerialUSB.println("Uncorrectable Errors!");
        SerialUSB.println("Sydrome: " + errorSyndrome);
        bitSet(returnCode,2);
      }
    } else {
      SerialUSB.println("no errors");
    }

    memcpy(msg_data, copied, msg_size);

    return returnCode;
  }
  return returnCode;
}

//getRSSI:
//returns the last RSSI data
uint8_t S6C::getRSSI()
{
  return lastRssi;
}

String S6C::getSyndrome(){
  return errorSyndrome;
}
