#include "SRADio.h"

//SRADio:
//Constructs a new SRADio object
SRADio::SRADio()
{
  rf24 = new RH_RF24(GFSK_CS, GFSK_IRQ, GFSK_SDN);
}

//configureRF:
//Configures the Radio Hardware, sets registers, and prepares for transmission
void SRADio::configureRF()
{
  configurePins();
  initialize_ecc();
  RadioOff();
  delay(500);
  RadioOn();
  rf24->init(FRAME_SIZE);
  uint8_t buf[8];
  if (!rf24->command(RH_RF24_CMD_PART_INFO, 0, 0, buf, sizeof(buf)))
  {
    Serial.println("SPI ERROR");
  }
  else
  {
    Serial.println("SPI OK");
  }
  if (!rf24->setFrequency(RF_FREQ))
  {
    Serial.println("Set Frequency failed");
  }
  else
  {
    Serial.print("Frequency set to ");
    Serial.print(RF_FREQ);
    Serial.println(" MHz.");
  }

  rf24->setModemConfig(RF_MODE);

  Serial.println("RF Configured");

  rf24->setTxPower(0x7f);
}

//encode_and_transmit:
//Performs ECC encoding, packages a frame, and transmits.
//takes:
//  -msg_data, a pointer to an array of data to transmit
//  -msg_size, the size of msg_data
void SRADio::encode_and_transmit(void *msg_data, uint8_t msg_size)
{
  //message must be withing the frame size constraints
  if (msg_size > MAX_MSG_LENGTH)
  {
    Serial.println("Message too large!");
  }

  //add padding zeroes to normalize message length
  uint8_t padded_msg_data[MAX_MSG_LENGTH] = {0};
  memcpy(padded_msg_data, msg_data, msg_size);

  //calculate ECC data and package it into a frame
  uint8_t frame_data[FRAME_SIZE] = {0};                     //frame buffer
  encode_data(padded_msg_data, MAX_MSG_LENGTH, frame_data); //This does the ECC

//debug frame contents
#ifdef PRINT_ENCODED_DATA
  Serial.println("encoded data");
  for (int i = 0; i < FRAME_SIZE; i++)
  {
    uint8_t k = frame_data[i];
    Serial.print(k);
    if (k < 10)
      Serial.print(" ");
    if (k < 100)
      Serial.print(" ");
    Serial.print(" ");
  }
  Serial.println();
#endif

  //transmit frame (blocking)
  uint32_t timer = micros();
  rf24->send(frame_data, FRAME_SIZE); //WHAT IS MESSAGE_LENGTH?
  rf24->waitPacketSent();

//debug transmission time
#ifdef PRINT_TIMING
  Serial.print("Sent ");
  Serial.print(FRAME_SIZE);
  Serial.print(" bytes in ");
  Serial.print((micros() - timer) / 1000.);
  Serial.println(" ms");
#endif
}

//configurePins:
//Configures the pins needed for the radio.
//The SPI configuration might not be needed, as RadioHead seems to already do it.
void SRADio::configurePins()
{
  pinMode(GFSK_GATE, OUTPUT);
  pinMode(GFSK_SDN, OUTPUT);

  //THIS MIGHT BE NOT NEEDED!
  SPI.setSCK(GFSK_SCK);
  SPI.setMOSI(GFSK_MOSI);
  SPI.setMISO(GFSK_MISO);
  SPI.setDataMode(SPI_MODE0);
  SPI.setClockDivider(SPI_CLOCK_DIV2); // Setting clock speed to 8mhz, as 10 is the max for the rfm22
  SPI.begin();
  //
}

//RadioOff:
//disables the SiLabs Radio chip
void SRADio::RadioOff()
{
  digitalWrite(GFSK_GATE, HIGH);
}

//RadioOn:
//enables the SiLabs Radio chip
void SRADio::RadioOn()
{
  digitalWrite(GFSK_GATE, LOW);
}

//tryToRX:
//attempts to process any recived messages
uint8_t SRADio::tryToRX(void *msg_data, uint8_t msg_size)
{
  uint8_t data[FRAME_SIZE + 32] = {0}; //32 bytes buffer room
  uint8_t data_size = FRAME_SIZE;
  uint8_t returnCode = 0; //bits: recived, ECC used, ECC failed, Frame err

  if (rf24->recv(data, &data_size))
  {
    bitSet(returnCode,0);
    lastRssi = (uint8_t)rf24->lastRssi();

#ifdef PRINT_RSSI
    Serial.print("Got stuff at RSSI: ");
    Serial.println(lastRssi);
#endif

#ifdef PRINT_DEBUG
    if (data_size != FRAME_SIZE)
    {
      Serial.print("Error, got frame of size ");
      Serial.print(data_size);
      Serial.print(", expecting ");
      Serial.println(FRAME_SIZE);
      bitSet(returnCode,3);
    }
#endif

#ifdef PRINT_ENCODED_DATA
    for (int kk = 0; kk < data_size; kk++)
      Serial.print((char)data[kk]);
    Serial.println();
#endif

    unsigned char copied[FRAME_SIZE];
    memcpy(copied, data, FRAME_SIZE);
    decode_data(copied, data_size);

    if (check_syndrome() != 0)
    {
      bitSet(returnCode,1);
      Serial.println("There were errors");
      int correct = correct_errors_erasures(copied, FRAME_SIZE, 0, NULL);
      
      errorSyndrome = String();
      for (int i = 0; i < NPAR; i++)
        errorSyndrome += synBytes[i];
        errorSyndrome += ",";

      if (correct)
      {
        Serial.println("Corrected successfully.");
        Serial.println("Sydrome: " + errorSyndrome);
      }
      else
      {
        Serial.println("Uncorrectable Errors!");
        Serial.println("Sydrome: " + errorSyndrome);
        bitSet(returnCode,2);
      }
    }

    memcpy(msg_data, copied, msg_size);

    return returnCode;
  }
  return returnCode;
}

//getRSSI:
//returns the last RSSI data
uint8_t SRADio::getRSSI()
{
  return lastRssi;
}

String SRADio::getSyndrome(){
  return errorSyndrome;
}