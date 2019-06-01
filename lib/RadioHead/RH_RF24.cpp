// RH_RF24.cpp
//
// Copyright (C) 2011 Mike McCauley
// $Id: RH_RF24.cpp,v 1.22 2017/11/06 00:04:08 mikem Exp mikem $

#include <RH_RF24.h>

#include "ss6c_500bps.h"

// Interrupt vectors for the 3 Arduino interrupt pins
// Each interrupt can be handled by a different instance of RH_RF24, allowing you to have
// 2 or more RF24s per Arduino
RH_RF24* RH_RF24::_deviceForInterrupt[RH_RF24_NUM_INTERRUPTS] = {0, 0, 0};
uint8_t RH_RF24::_interruptCount = 0; // Index into _deviceForInterrupt for next device

// This configuration data is defined in radio_config_Si4460.h
// which was generated with the Silicon Labs WDS program
PROGMEM const uint8_t RF24_CONFIGURATION_DATA[] = RADIO_CONFIGURATION_DATA_ARRAY;

PROGMEM const uint8_t speed_config[3][162] = {
                {0x0D,0x11,0x10,0x09,0x00,0x08,0x12,0x00,0x0A,0x31,0x00,0x00,0x00,0x00,0x10,0x11,0x20,0x0C,0x00,0x03,0x00,0x07,0x00,0x4E,0x20,0x05,0xC9,0xC3,0x80,0x00,0x00,0x05,0x11,0x20,0x01,0x0C,0x11,0x0C,0x11,0x20,0x08,0x18,0x01,0x00,0x08,0x03,0x80,0x00,0xF0,0x11,0x0D,0x11,0x20,0x09,0x22,0x01,0x39,0x01,0xA3,0x6E,0x01,0xA3,0x02,0x00,0x0B,0x11,0x20,0x07,0x2C,0x00,0x12,0xC0,0x04,0x09,0x2A,0xE0,0x0D,0x11,0x20,0x09,0x38,0x11,0x44,0x44,0x00,0x1A,0x40,0x00,0x00,0x2A,0x0D,0x11,0x20,0x09,0x42,0xA4,0x03,0xD6,0x03,0x00,0x3D,0x01,0x80,0xFF,0x05,0x11,0x20,0x01,0x4E,0x3A,0x10,0x11,0x21,0x0C,0x00,0xFF,0xBA,0x0F,0x51,0xCF,0xA9,0xC9,0xFC,0x1B,0x1E,0x0F,0x01,0x10,0x11,0x21,0x0C,0x0C,0xFC,0xFD,0x15,0xFF,0x00,0x0F,0xA2,0xA0,0x97,0x8A,0x79,0x66,0x10,0x11,0x21,0x0C,0x18,0x52,0x3F,0x2E,0x1F,0x14,0x0B,0x06,0x02,0x00,0x00,0x00,0x00,0x00},
                {0x0D,0x11,0x10,0x09,0x00,0x08,0x12,0x00,0x0A,0x31,0x00,0x00,0x00,0x00,0x10,0x11,0x20,0x0C,0x00,0x03,0x00,0x07,0x03,0x0D,0x40,0x05,0xC9,0xC3,0x80,0x00,0x00,0x05,0x11,0x20,0x01,0x0C,0xAF,0x0C,0x11,0x20,0x08,0x18,0x01,0x00,0x08,0x03,0x80,0x00,0xB0,0x20,0x0D,0x11,0x20,0x09,0x22,0x00,0x5E,0x05,0x76,0x1A,0x05,0x72,0x02,0x00,0x0B,0x11,0x20,0x07,0x2C,0x00,0x12,0xC0,0x2C,0x01,0xCE,0xE0,0x0D,0x11,0x20,0x09,0x38,0x11,0x15,0x15,0x00,0x1A,0x40,0x00,0x00,0x28,0x0D,0x11,0x20,0x09,0x42,0xA4,0x03,0xD6,0x03,0x00,0xCC,0x01,0x80,0xFF,0x05,0x11,0x20,0x01,0x4E,0x40,0x10,0x11,0x21,0x0C,0x00,0xA2,0x81,0x26,0xAF,0x3F,0xEE,0xC8,0xC7,0xDB,0xF2,0x02,0x08,0x10,0x11,0x21,0x0C,0x0C,0x07,0x03,0x15,0xFC,0x0F,0x00,0xA2,0x81,0x26,0xAF,0x3F,0xEE,0x10,0x11,0x21,0x0C,0x18,0xC8,0xC7,0xDB,0xF2,0x02,0x08,0x07,0x03,0x15,0xFC,0x0F,0x00,0x00},
                {0x0D,0x11,0x10,0x09,0x00,0x08,0x12,0x00,0x0A,0x31,0x00,0x00,0x00,0x00,0x10,0x11,0x20,0x0C,0x00,0x03,0x00,0x07,0x06,0x1A,0x80,0x05,0xC9,0xC3,0x80,0x00,0x01,0x05,0x11,0x20,0x01,0x0C,0x5E,0x0C,0x11,0x20,0x08,0x18,0x01,0x00,0x08,0x03,0x80,0x00,0x70,0x20,0x0D,0x11,0x20,0x09,0x22,0x00,0x5E,0x05,0x76,0x1A,0x05,0x72,0x02,0x00,0x0B,0x11,0x20,0x07,0x2C,0x00,0x12,0xC0,0x57,0x01,0xD2,0xE0,0x0D,0x11,0x20,0x09,0x38,0x11,0x15,0x15,0x00,0x1A,0x40,0x00,0x00,0x28,0x0D,0x11,0x20,0x09,0x42,0xA4,0x03,0xD6,0x03,0x00,0xCC,0x01,0x80,0xFF,0x05,0x11,0x20,0x01,0x4E,0x40,0x10,0x11,0x21,0x0C,0x00,0xA2,0x81,0x26,0xAF,0x3F,0xEE,0xC8,0xC7,0xDB,0xF2,0x02,0x08,0x10,0x11,0x21,0x0C,0x0C,0x07,0x03,0x15,0xFC,0x0F,0x00,0xA2,0x81,0x26,0xAF,0x3F,0xEE,0x10,0x11,0x21,0x0C,0x18,0xC8,0xC7,0xDB,0xF2,0x02,0x08,0x07,0x03,0x15,0xFC,0x0F,0x00,0x00}};

RH_RF24::RH_RF24(uint8_t slaveSelectPin, uint8_t interruptPin, uint8_t sdnPin, RHGenericSPI& spi)
    :
    RHSPIDriver(slaveSelectPin, spi)
{
    _interruptPin = interruptPin;
    _sdnPin = sdnPin;
    _idleMode = RH_RF24_DEVICE_STATE_READY;
    _myInterruptIndex = 0xff; // Not allocated yet
}

void RH_RF24::setIdleMode(uint8_t idleMode)
{
    _idleMode = idleMode;
}

bool RH_RF24::init()
{
    if (!RHSPIDriver::init()){
      initializationStatus = 1;
	    return false;
    }

    // Determine the interrupt number that corresponds to the interruptPin
    int interruptNumber = digitalPinToInterrupt(_interruptPin);
    if (interruptNumber == NOT_AN_INTERRUPT){
      initializationStatus = 2;
	    return false;
    }
#ifdef RH_ATTACHINTERRUPT_TAKES_PIN_NUMBER
    interruptNumber = _interruptPin;
#endif

    // Tell the low level SPI interface we will use SPI within this interrupt
    spiUsingInterrupt(interruptNumber);

    // Initialise the radio
    power_on_reset();
    cmd_clear_all_interrupts();

    // Get the device type and check it
    // This also tests whether we are really connected to a device
	SerialUSB.println("A");
    uint8_t buf[8];
    if (!command(RH_RF24_CMD_PART_INFO, 0, 0, buf, sizeof(buf))){
      initializationStatus = 3;
      return false; // SPI error? Not connected?
    }
    _deviceType = (buf[1] << 8) | buf[2];
	SerialUSB.println("B");
	SerialUSB.println(_deviceType);
    // Check PART to be either 0x4460, 0x4461, 0x4463, 0x4464
    if (_deviceType != 0x4460 &&
	_deviceType != 0x4461 &&
	_deviceType != 0x4463 &&
	_deviceType != 0x4464){
    initializationStatus = 4;
    return false; // Unknown radio type, or not connected
  }


	SerialUSB.println("C");
    // Here we use a configuration generated by the Silicon Labs Wireless Development Suite
    // #included above
    // We override a few things later that we ned to be sure of.
    configure(RF24_CONFIGURATION_DATA);

	SerialUSB.println("D");
    // Add by Adrien van den Bossche <vandenbo@univ-tlse2.fr> for Teensy
    // ARM M4 requires the below. else pin interrupt doesn't work properly.
    // On all other platforms, its innocuous, belt and braces
    pinMode(_interruptPin, INPUT);

    // Set up interrupt handler
    // Since there are a limited number of interrupt glue functions isr*() available,
    // we can only support a limited number of devices simultaneously
    // ON some devices, notably most Arduinos, the interrupt pin passed in is actuallt the
    // interrupt number. You have to figure out the interruptnumber-to-interruptpin mapping
    // yourself based on knwledge of what Arduino board you are running on.
    if (_myInterruptIndex == 0xff)
    {
    	// First run, no interrupt allocated yet
    	if (_interruptCount <= RH_RF24_NUM_INTERRUPTS){
    	    _myInterruptIndex = _interruptCount++;
    	}else{
        initializationStatus = 5;
        return false; // Too many devices, not enough interrupt vectors
      }
    }
	SerialUSB.println("E");
    _deviceForInterrupt[_myInterruptIndex] = this;
    if (_myInterruptIndex == 0)
	attachInterrupt(interruptNumber, isr0, FALLING);
    else if (_myInterruptIndex == 1)
	attachInterrupt(interruptNumber, isr1, FALLING);
    else if (_myInterruptIndex == 2)
	attachInterrupt(interruptNumber, isr2, FALLING);
    else{
      initializationStatus = 6;
      return false; // Too many devices, not enough interrupt vectors
    }


	SerialUSB.println("F");
    // Ensure we get the interrupts we need, irrespective of whats in the radio_config
    uint8_t int_ctl[] = {RH_RF24_MODEM_INT_STATUS_EN | RH_RF24_PH_INT_STATUS_EN, 0xff, 0xff, 0x00 };
    set_properties(RH_RF24_PROPERTY_INT_CTL_ENABLE, int_ctl, sizeof(int_ctl));

    // RSSI Latching should be configured in MODEM_RSSI_CONTROL in radio_config

    // PKT_TX_THRESHOLD and PKT_RX_THRESHOLD should be set to about 0x30 in radio_config

	set4GFSK(false);

	//setMessageLength(40);

	SerialUSB.println("G");
    // Clear all other fields so they are never used, irrespective of the radio_config
    uint8_t pkt_fieldn[] = { 0x00, 0x00, 0x00, 0x00 };
    set_properties(RH_RF24_PROPERTY_PKT_FIELD_2_LENGTH_12_8, pkt_fieldn, sizeof(pkt_fieldn));
    set_properties(RH_RF24_PROPERTY_PKT_FIELD_3_LENGTH_12_8, pkt_fieldn, sizeof(pkt_fieldn));
    set_properties(RH_RF24_PROPERTY_PKT_FIELD_4_LENGTH_12_8, pkt_fieldn, sizeof(pkt_fieldn));
    set_properties(RH_RF24_PROPERTY_PKT_FIELD_5_LENGTH_12_8, pkt_fieldn, sizeof(pkt_fieldn));

    setPreambleLength(5);
    setTxPower(0x7f);

	SerialUSB.println("H");
	setDatarate(0);
	SerialUSB.println("I");

  initializationStatus = 0;
  return true;
}

bool RH_RF24::set4GFSK(bool yeah) {
    uint8_t pkt_config1[] = { (uint8_t)(yeah ? 0x22 : 0x02) };
    set_properties(RH_RF24_PROPERTY_PKT_CONFIG1, pkt_config1, sizeof(pkt_config1));
	return true;
}

bool RH_RF24::setMessageLength(int len) {
	uint8_t pkt_field1[] = { 0x00, (uint8_t)len};
	set_properties(RH_RF24_PROPERTY_PKT_FIELD_1_LENGTH_12_8, pkt_field1, sizeof(pkt_field1));
	return true;
}

bool RH_RF24::setDatarate(int mode) {
	set4GFSK(mode >= 7);

	SerialUSB.println("configuring speed");
	SerialUSB.println("yooo");
	SerialUSB.println((int)speed_config[0]);
	SerialUSB.println((int)speed_config[1]);
	SerialUSB.println((int)speed_config[2]);
	configure(speed_config[mode]);
	setPreambleLength(5);

	return true;
}

// C++ level interrupt handler for this instance
void RH_RF24::handleInterrupt()
{
    uint8_t status[8];
    command(RH_RF24_CMD_GET_INT_STATUS, NULL, 0, status, sizeof(status));

    // Decode and handle the interrupt bits we are interested in
//    if (status[0] & RH_RF24_INT_STATUS_CHIP_INT_STATUS)
    if (status[0] & RH_RF24_INT_STATUS_MODEM_INT_STATUS)
    {
//	if (status[4] & RH_RF24_INT_STATUS_INVALID_PREAMBLE)
	if (status[4] & RH_RF24_INT_STATUS_INVALID_SYNC)
	{
	    // After INVALID_SYNC, sometimes the radio gets into a silly state and subsequently reports it for every packet
	    // Need to reset the radio and clear the RX FIFO, cause sometimes theres junk there too
	    _mode = RHModeIdle;
	    clearRxFifo();
	    clearBuffer();
	}
    }
    if (status[0] & RH_RF24_INT_STATUS_PH_INT_STATUS)
    {
	if (status[2] & RH_RF24_INT_STATUS_CRC_ERROR)
	{
	    // CRC Error
	    // Radio automatically went to _idleMode
	    _mode = RHModeIdle;
	    _rxBad++;

	    clearRxFifo();
	    clearBuffer();
	}
	if (status[2] & RH_RF24_INT_STATUS_PACKET_SENT)
	{
	    _txGood++;
	    // Transmission does not automatically clear the tx buffer.
	    // Could retransmit if we wanted
	    // RH_RF24 configured to transition automatically to Idle after packet sent
	    _mode = RHModeIdle;
	    clearBuffer();
	}
	if (status[2] & RH_RF24_INT_STATUS_PACKET_RX)
	{
	    // A complete message has been received with good CRC
	    // Get the RSSI, configured to latch at sync detect in radio_config
	    uint8_t modem_status[6];
	    command(RH_RF24_CMD_GET_MODEM_STATUS, NULL, 0, modem_status, sizeof(modem_status));
	    _lastRssi = modem_status[3];
	    _lastPreambleTime = millis();

	    // Save it in our buffer
	    readNextFragment();
	    // And see if we have a valid message
	    validateRxBuf();
	    // Radio will have transitioned automatically to the _idleMode
	    _mode = RHModeIdle;
	}
	if (status[2] & RH_RF24_INT_STATUS_TX_FIFO_ALMOST_EMPTY)
	{
	    // TX FIFO almost empty, maybe send another chunk, if there is one
	    sendNextFragment();
	}
	if (status[2] & RH_RF24_INT_STATUS_RX_FIFO_ALMOST_FULL)
	{
	    // Some more data to read, get it
	    readNextFragment();
	}
    }
}

// Check whether the latest received message is complete and uncorrupted
void RH_RF24::validateRxBuf()
{
    // Validate headers etc
    /*if (_bufLen >= RH_RF24_HEADER_LEN)
    {
	_rxHeaderTo    = _buf[0];
	_rxHeaderFrom  = _buf[1];
	_rxHeaderId    = _buf[2];
	_rxHeaderFlags = _buf[3];
	if (_promiscuous ||
	    _rxHeaderTo == _thisAddress ||
	    _rxHeaderTo == RH_BROADCAST_ADDRESS)
	{*/
	    // Its for us
	    _rxGood++;
	    _rxBufValid = true;
	//}
  //  }
}

bool RH_RF24::clearRxFifo()
{
    uint8_t fifo_clear[] = { 0x02 };
    return command(RH_RF24_CMD_FIFO_INFO, fifo_clear, sizeof(fifo_clear));
}

void RH_RF24::clearBuffer()
{
    _bufLen = 0;
    _txBufSentIndex = 0;
    _rxBufValid = false;
}

// These are low level functions that call the interrupt handler for the correct
// instance of RH_RF24.
// 3 interrupts allows us to have 3 different devices
void RH_RF24::isr0()
{
    if (_deviceForInterrupt[0])
	_deviceForInterrupt[0]->handleInterrupt();
}
void RH_RF24::isr1()
{
    if (_deviceForInterrupt[1])
	_deviceForInterrupt[1]->handleInterrupt();
}
void RH_RF24::isr2()
{
    if (_deviceForInterrupt[2])
	_deviceForInterrupt[2]->handleInterrupt();
}

bool RH_RF24::available()
{
    if (_mode == RHModeTx)
	return false;
    if (!_rxBufValid)
	setModeRx(); // Make sure we are receiving
    return _rxBufValid;
}

bool RH_RF24::recv(uint8_t* buf, uint8_t* len)
{
    if (!available())
	return false;
    // CAUTION: first 4 octets of _buf contain the headers
    if (buf && len && (_bufLen >= 0))
    {
	ATOMIC_BLOCK_START;
	if (*len > _bufLen)
	    *len = _bufLen;
	memcpy(buf, _buf, *len);
	ATOMIC_BLOCK_END;
    }
    clearBuffer(); // Got the most recent message
    return true;
}

bool RH_RF24::send(const uint8_t* data, uint8_t len)
{
    if (len > RH_RF24_MAX_MESSAGE_LEN)
	return false;

    waitPacketSent(); // Make sure we dont interrupt an outgoing message
    setModeIdle(); // Prevent RX while filling the fifo

    if (!waitCAD())
	return false;  // Check channel activity

    // Put the payload in the FIFO
    // First the length in fixed length field 1. This wont appear in the receiver fifo since
    // we have turned off IN_FIFO in PKT_LEN
    /*_buf[0] = len + RH_RF24_HEADER_LEN;
    // Now the rest of the payload in variable length field 2
    // First the headers
    _buf[1] = _txHeaderTo;
    _buf[2] = _txHeaderFrom;
    _buf[3] = _txHeaderId;
    _buf[4] = _txHeaderFlags;*/
    // Then the message
    memcpy(_buf, data, len);
    _bufLen = len;
    _txBufSentIndex = 0;

    // Set the field 2 length to the variable payload length
    //uint8_t l[] = { (uint8_t)(len + RH_RF24_HEADER_LEN)};
    //set_properties(RH_RF24_PROPERTY_PKT_FIELD_2_LENGTH_7_0, l, sizeof(l));

    sendNextFragment();
    setModeTx();
    return true;
}

// This is different to command() since we must not wait for CTS
bool RH_RF24::writeTxFifo(uint8_t *data, uint8_t len)
{
    ATOMIC_BLOCK_START;
    // First send the command
    digitalWrite(_slaveSelectPin, LOW);
    _spi.beginTransaction();
    _spi.transfer(RH_RF24_CMD_TX_FIFO_WRITE);
    // Now write any write data
    while (len--)
	_spi.transfer(*data++);
    digitalWrite(_slaveSelectPin, HIGH);
    _spi.endTransaction();
    ATOMIC_BLOCK_END;
    return true;
}

void RH_RF24::sendNextFragment()
{
    if (_txBufSentIndex < _bufLen)
    {
	// Some left to send?
	uint8_t len = _bufLen - _txBufSentIndex;
	// But dont send too much, see how much room is left
	uint8_t fifo_info[2];
	command(RH_RF24_CMD_FIFO_INFO, NULL, 0, fifo_info, sizeof(fifo_info));
	// fifo_info[1] is space left in TX FIFO
	if (len > fifo_info[1])
	    len = fifo_info[1];

	writeTxFifo(_buf + _txBufSentIndex, len);
	_txBufSentIndex += len;
    }
}

void RH_RF24::readNextFragment()
{
    // Get the packet length from the RX FIFO length
    uint8_t fifo_info[1];
    command(RH_RF24_CMD_FIFO_INFO, NULL, 0, fifo_info, sizeof(fifo_info));
    uint8_t fifo_len = fifo_info[0];

    // Check for overflow
    if ((_bufLen + fifo_len) > sizeof(_buf))
    {
	// Overflow pending
	_rxBad++;
	setModeIdle();
	clearRxFifo();
	clearBuffer();
	return;
    }
    // So we have room
    // Now read the fifo_len bytes from the RX FIFO
    // This is different to command() since we dont wait for CTS
    digitalWrite(_slaveSelectPin, LOW);
    _spi.transfer(RH_RF24_CMD_RX_FIFO_READ);
    uint8_t* p = _buf + _bufLen;
    uint8_t l = fifo_len;
    while (l--)
	*p++ = _spi.transfer(0);
    digitalWrite(_slaveSelectPin, HIGH);
    _bufLen += fifo_len;
}

uint8_t RH_RF24::maxMessageLength()
{
    return RH_RF24_MAX_MESSAGE_LEN;
}

// Sets registers from a canned modem configuration structure
void RH_RF24::setModemRegisters(const ModemConfig* config)
{
  SerialUSB.println("Programming Error: setModemRegisters is obsolete. Generate custom radio config file with WDS instead");
  (void)config; // Prevent warnings
}

// Set one of the canned Modem configs
// Returns true if its a valid choice
bool RH_RF24::setModemConfig(ModemConfigChoice index)
{
  SerialUSB.println("Programming Error: setModemRegisters is obsolete. Generate custom radio config file with WDS instead");
  (void)index; // Prevent warnings
  return false;
}

void RH_RF24::setPreambleLength(uint16_t bytes)
{
    uint8_t config[] = { (uint8_t)bytes, 0x14, 0x00, 0x00,
			 RH_RF24_PREAMBLE_FIRST_1 | RH_RF24_PREAMBLE_LENGTH_BYTES | RH_RF24_PREAMBLE_STANDARD_1010, 0x21};
    set_properties(RH_RF24_PROPERTY_PREAMBLE_TX_LENGTH, config, sizeof(config));
}

bool RH_RF24::setCRCPolynomial(CRCPolynomial polynomial)
{
    if (polynomial >= CRC_NONE &&
	polynomial <= CRC_Castagnoli)
    {
	// Caution this only has effect if CRCs are enabled
	uint8_t config[] = { (uint8_t)((polynomial & RH_RF24_CRC_MASK) | RH_RF24_CRC_SEED_ALL_1S) };
	return set_properties(RH_RF24_PROPERTY_PKT_CRC_CONFIG, config, sizeof(config));
    }
    else
	return false;
}

void RH_RF24::setSyncWords(const uint8_t* syncWords, uint8_t len)
{
    if (len > 4 || len < 1)
	return;
    uint8_t config[] = { (uint8_t)(len-1), 0, 0, 0, 0};
    memcpy(config+1, syncWords, len);
    set_properties(RH_RF24_PROPERTY_SYNC_CONFIG, config, sizeof(config));
}

bool RH_RF24::setFrequency(float centre, float afcPullInRange)
{
  (void)afcPullInRange; // Not used
    // See Si446x Data Sheet section 5.3.1
    // Also the Si446x PLL Synthesizer / VCO_CNT Calculator Rev 0.4
    uint8_t outdiv;
    uint8_t band;
    if (_deviceType == 0x4460 ||
	_deviceType == 0x4461 ||
	_deviceType == 0x4463)
    {
	// Non-continuous frequency bands
	if (centre <= 1050.0 && centre >= 850.0)
	    outdiv = 4, band = 0;
	else if (centre <= 525.0 && centre >= 425.0)
	    outdiv = 8, band = 2;
	else if (centre <= 350.0 && centre >= 284.0)
	    outdiv = 12, band = 3;
	else if (centre <= 175.0 && centre >= 142.0)
	    outdiv = 24, band = 5;
	else
	    return false;
    }
    else
    {
	// 0x4464
	// Continuous frequency bands
	if (centre <= 960.0 && centre >= 675.0)
	    outdiv = 4, band = 1;
	else if (centre < 675.0 && centre >= 450.0)
	    outdiv = 6, band = 2;
	else if (centre < 450.0 && centre >= 338.0)
	    outdiv = 8, band = 3;
	else if (centre < 338.0 && centre >= 225.0)
	    outdiv = 12, band = 4;
	else if (centre < 225.0 && centre >= 169.0)
	    outdiv = 16, band = 4;
	else if (centre < 169.0 && centre >= 119.0)
	    outdiv = 24, band = 5;
	else
	    return false;
    }

    // Set the MODEM_CLKGEN_BAND (not documented)
    uint8_t modem_clkgen[] = { (uint8_t)(band + 8) };
    if (!set_properties(RH_RF24_PROPERTY_MODEM_CLKGEN_BAND, modem_clkgen, sizeof(modem_clkgen)))
	return false;

    centre *= 1000000.0; // Convert to Hz

    // Now generate the RF frequency properties
    // Need the Xtal/XO freq from the radio_config file:
    uint32_t xtal_frequency = RADIO_CONFIGURATION_DATA_RADIO_XO_FREQ;
    unsigned long f_pfd = 2 * xtal_frequency / outdiv;
    unsigned int n = ((unsigned int)(centre / f_pfd)) - 1;
    float ratio = centre / (float)f_pfd;
    float rest  = ratio - (float)n;
    unsigned long m = (unsigned long)(rest * 524288UL);
    unsigned int m2 = m / 0x10000;
    unsigned int m1 = (m - m2 * 0x10000) / 0x100;
    unsigned int m0 = (m - m2 * 0x10000 - m1 * 0x100);

    // PROP_FREQ_CONTROL_GROUP
    uint8_t freq_control[] = { (uint8_t)n, (uint8_t)m2, (uint8_t)m1, (uint8_t)m0 };
    return set_properties(RH_RF24_PROPERTY_FREQ_CONTROL_INTE, freq_control, sizeof(freq_control));
}

void RH_RF24::setModeIdle()
{
    if (_mode != RHModeIdle)
    {
	// Set the antenna switch pins using the GPIO, assuming we have an RFM module with antenna switch
	uint8_t config[] = { RH_RF24_GPIO_HIGH, RH_RF24_GPIO_HIGH };
	command(RH_RF24_CMD_GPIO_PIN_CFG, config, sizeof(config));

	uint8_t state[] = { _idleMode };
	command(RH_RF24_CMD_CHANGE_STATE, state, sizeof(state));
	_mode = RHModeIdle;
    }
}

bool RH_RF24::sleep()
{
    if (_mode != RHModeSleep)
    {
	// This will change to SLEEP or STANDBY, depending on the value of GLOBAL_CLK_CFG:CLK_32K_SEL.
	// which default to 0, eg STANDBY
	uint8_t state[] = { RH_RF24_DEVICE_STATE_SLEEP };
	command(RH_RF24_CMD_CHANGE_STATE, state, sizeof(state));

	_mode = RHModeSleep;
    }
    return true;
}

void RH_RF24::setModeRx()
{
    if (_mode != RHModeRx)
    {
	// CAUTION: we cant clear the rx buffers here, else we set up a race condition
	// with the _rxBufValid test in available()

	// Tell the receiver the max data length we will accept (a TX may have changed it)
	//uint8_t l[] = { sizeof(_buf) };
	//set_properties(RH_RF24_PROPERTY_PKT_FIELD_2_LENGTH_7_0, l, sizeof(l));

	// Set the antenna switch pins using the GPIO, assuming we have an RFM module with antenna switch
	uint8_t gpio_config[] = { RH_RF24_GPIO_HIGH, RH_RF24_GPIO_LOW };
	command(RH_RF24_CMD_GPIO_PIN_CFG, gpio_config, sizeof(gpio_config));

	uint8_t rx_config[] = { 0x00, RH_RF24_CONDITION_RX_START_IMMEDIATE, 0x00, 0x00, _idleMode, _idleMode, _idleMode};
	command(RH_RF24_CMD_START_RX, rx_config, sizeof(rx_config));
	_mode = RHModeRx;
    }
}

void RH_RF24::setModeTx()
{
    if (_mode != RHModeTx)
    {
	// Set the antenna switch pins using the GPIO, assuming we have an RFM module with antenna switch
	uint8_t config[] = { RH_RF24_GPIO_LOW, RH_RF24_GPIO_HIGH };
	command(RH_RF24_CMD_GPIO_PIN_CFG, config, sizeof(config));

	uint8_t tx_params[] = { 0x00,
				(uint8_t)((_idleMode << 4) | RH_RF24_CONDITION_RETRANSMIT_NO | RH_RF24_CONDITION_START_IMMEDIATE)};
	command(RH_RF24_CMD_START_TX, tx_params, sizeof(tx_params));
	_mode = RHModeTx;
    }
}

void RH_RF24::setTxPower(uint8_t power)
{
    uint8_t pa_bias_clkduty = 0;
    // These calculations valid for advertised power from Si chips at Vcc = 3.3V
    // you may get lower power from RFM modules, depending on Vcc voltage, antenna etc
    if (_deviceType == 0x4460)
    {
	// 0x4f = 13dBm
	pa_bias_clkduty = 0xc0;
	if (power > 0x4f)
	    power = 0x4f;
    }
    else if (_deviceType == 0x4461)
    {
	// 0x7f = 16dBm
	pa_bias_clkduty = 0xc0;
	if (power > 0x7f)
	    power = 0x7f;
    }
    else if (_deviceType == 0x4463 || _deviceType == 0x4464 )
    {
	// 0x7f = 20dBm
	pa_bias_clkduty = 0x00; // Per WDS suggestion
	if (power > 0x7f)
	    power = 0x7f;
    }
    uint8_t power_properties[] = {0x08, 0x00, 0x00 }; // PA_MODE from WDS sugggestions (why?)
    power_properties[1] = power;
    power_properties[2] = pa_bias_clkduty;
    set_properties(RH_RF24_PROPERTY_PA_MODE, power_properties, sizeof(power_properties));
}

// Caution: There was a bug in A1 hardware that will not handle 1 byte commands.
bool RH_RF24::command(uint8_t cmd, const uint8_t* write_buf, uint8_t write_len, uint8_t* read_buf, uint8_t read_len)
{
    bool   done = false;

    ATOMIC_BLOCK_START;
    // First send the command
    digitalWrite(_slaveSelectPin, LOW);
    _spi.transfer(cmd);

    // Now write any write data
    if (write_buf && write_len)
    {
	while (write_len--)
	    _spi.transfer(*write_buf++);
    }
    // Sigh, the RFM26 at least has problems if we deselect too quickly :-(
    // Innocuous timewaster:
    digitalWrite(_slaveSelectPin, LOW);
    // And finalise the command
    digitalWrite(_slaveSelectPin, HIGH);

    uint16_t count; // Number of times we have tried to get CTS
    for (count = 0; !done && count < RH_RF24_CTS_RETRIES; count++)
    {
	// Wait for the CTS
	digitalWrite(_slaveSelectPin, LOW);

	_spi.transfer(RH_RF24_CMD_READ_BUF);
	if (_spi.transfer(0) == RH_RF24_REPLY_CTS)
	{
	    // Now read any expected reply data
	    if (read_buf && read_len)
	    {
		while (read_len--)
		    *read_buf++ = _spi.transfer(0);
	    }
	    done = true;
	}
	// Sigh, the RFM26 at least has problems if we deselect too quickly :-(
	// Innocuous timewaster:
	digitalWrite(_slaveSelectPin, LOW);
	// Finalise the read
	digitalWrite(_slaveSelectPin, HIGH);
    }
    ATOMIC_BLOCK_END;

    //SerialUSB.print("done got ");
    //SerialUSB.println(done);
    return done; // False if too many attempts at CTS
}

bool RH_RF24::configure(const uint8_t* commands)
{
	SerialUSB.println("fuckign doot");
SerialUSB.println(commands[0]);
SerialUSB.println(commands[2]);
SerialUSB.println(commands[4]);
    // Command strings are constructed in radio_config_Si4460.h
    // Each command starts with a count of the bytes in that command:
    // <bytecount> <command> <bytecount-2 bytes of args/data>
    uint8_t next_cmd_len;

    while (memcpy_P(&next_cmd_len, commands, 1), next_cmd_len > 0)
    {
		SerialUSB.println("LENGTH HEX");
		SerialUSB.println(next_cmd_len, HEX);
	uint8_t buf[20]; // As least big as the biggest permitted command/property list of 15
	memcpy_P(buf, commands+1, next_cmd_len);
	command(buf[0], buf+1, next_cmd_len - 1);
	//SerialUSB.print("config ");
	//SerialUSB.print(buf[0]);
	//SerialUSB.print((int)commands);
	commands += (next_cmd_len + 1);
    }
    return true;
}

void RH_RF24::power_on_reset()
{
    // Sigh: its necessary to control the SDN pin to reset this ship.
    // Tying it to GND does not produce reliable startups
    // Per Si4464 Data Sheet 3.3.2
    digitalWrite(_sdnPin, HIGH); // So we dont get a glitch after setting pinMode OUTPUT
    pinMode(_sdnPin, OUTPUT);
    delay(10);
    digitalWrite(_sdnPin, LOW);
    delay(10);
}

bool RH_RF24::cmd_clear_all_interrupts()
{
    uint8_t write_buf[] = { 0x00, 0x00, 0x00 };
    return command(RH_RF24_CMD_GET_INT_STATUS, write_buf, sizeof(write_buf));
}

bool RH_RF24::set_properties(uint16_t firstProperty, const uint8_t* values, uint8_t count)
{
    uint8_t buf[15];

    buf[0] = firstProperty >> 8;   // GROUP
    buf[1] = count;                // NUM_PROPS
    buf[2] = firstProperty & 0xff; // START_PROP
    uint8_t i;
    for (i = 0; i < 12 && i < count; i++)
	buf[3 + i] = values[i]; // DATAn
    return command(RH_RF24_CMD_SET_PROPERTY, buf, count + 3);
}

bool RH_RF24::get_properties(uint16_t firstProperty, uint8_t* values, uint8_t count)
{
    if (count > 16)
	count = 16;
    uint8_t buf[3];
    buf[0] = firstProperty >> 8;   // GROUP
    buf[1] = count;                // NUM_PROPS
    buf[2] = firstProperty & 0xff; // START_PROP
    return command(RH_RF24_CMD_GET_PROPERTY, buf, sizeof(buf), values, count);
}

float RH_RF24::get_temperature()
{
    uint8_t write_buf[] = { 0x10 };
    uint8_t read_buf[8];
    // Takes nearly 4ms
    command(RH_RF24_CMD_GET_ADC_READING, write_buf, sizeof(write_buf), read_buf, sizeof(read_buf));
    uint16_t temp_adc = (read_buf[4] << 8) | read_buf[5];
    return ((800 + read_buf[6]) / 4096.0) * temp_adc - ((read_buf[7] / 2) + 256);
}

float RH_RF24::get_battery_voltage()
{
    uint8_t write_buf[] = { 0x08 };
    uint8_t read_buf[8];
    // Takes nearly 4ms
    command(RH_RF24_CMD_GET_ADC_READING, write_buf, sizeof(write_buf), read_buf, sizeof(read_buf));
    uint16_t battery_adc = (read_buf[2] << 8) | read_buf[3];
    return 3.0 * battery_adc / 1280;
}

float RH_RF24::get_gpio_voltage(uint8_t gpio)
{
    uint8_t write_buf[] = { 0x04 };
    uint8_t read_buf[8];
    write_buf[0] |= (gpio & 0x3);
    // Takes nearly 4ms
    command(RH_RF24_CMD_GET_ADC_READING, write_buf, sizeof(write_buf), read_buf, sizeof(read_buf));
    uint16_t gpio_adc = (read_buf[0] << 8) | read_buf[1];
    return 3.0 * gpio_adc / 1280;
}

uint8_t RH_RF24::frr_read(uint8_t reg)
{
    uint8_t ret;

    // Do not wait for CTS
    ATOMIC_BLOCK_START;
    // First send the command
    digitalWrite(_slaveSelectPin, LOW);
    _spi.transfer(RH_RF24_PROPERTY_FRR_CTL_A_MODE + reg);
    // Get the fast response
    ret = _spi.transfer(0);
    digitalWrite(_slaveSelectPin, HIGH);
    ATOMIC_BLOCK_END;
    return ret;
}

// List of command replies to be printed by prinRegisters()
PROGMEM static const RH_RF24::CommandInfo commands[] =
{
    { RH_RF24_CMD_PART_INFO,            8 },
    { RH_RF24_CMD_FUNC_INFO,            6 },
    { RH_RF24_CMD_GPIO_PIN_CFG,         7 },
    { RH_RF24_CMD_FIFO_INFO,            2 },
    { RH_RF24_CMD_PACKET_INFO,          2 },
    { RH_RF24_CMD_GET_INT_STATUS,       8 },
    { RH_RF24_CMD_GET_PH_STATUS,        2 },
    { RH_RF24_CMD_GET_MODEM_STATUS,     8 },
    { RH_RF24_CMD_GET_CHIP_STATUS,      3 },
    { RH_RF24_CMD_REQUEST_DEVICE_STATE, 2 },
};
#define NUM_COMMAND_INFO (sizeof(commands)/sizeof(CommandInfo))

// List of properties to be printed by printRegisters()
PROGMEM static const uint16_t properties[] =
{
    RH_RF24_PROPERTY_GLOBAL_XO_TUNE,
    RH_RF24_PROPERTY_GLOBAL_CLK_CFG,
    RH_RF24_PROPERTY_GLOBAL_LOW_BATT_THRESH,
    RH_RF24_PROPERTY_GLOBAL_CONFIG,
    RH_RF24_PROPERTY_GLOBAL_WUT_CONFIG,
    RH_RF24_PROPERTY_GLOBAL_WUT_M_15_8,
    RH_RF24_PROPERTY_GLOBAL_WUT_M_7_0,
    RH_RF24_PROPERTY_GLOBAL_WUT_R,
    RH_RF24_PROPERTY_GLOBAL_WUT_LDC,
    RH_RF24_PROPERTY_INT_CTL_ENABLE,
    RH_RF24_PROPERTY_INT_CTL_PH_ENABLE,
    RH_RF24_PROPERTY_INT_CTL_MODEM_ENABLE,
    RH_RF24_PROPERTY_INT_CTL_CHIP_ENABLE,
    RH_RF24_PROPERTY_FRR_CTL_A_MODE,
    RH_RF24_PROPERTY_FRR_CTL_B_MODE,
    RH_RF24_PROPERTY_FRR_CTL_C_MODE,
    RH_RF24_PROPERTY_FRR_CTL_D_MODE,
    RH_RF24_PROPERTY_PREAMBLE_TX_LENGTH,
    RH_RF24_PROPERTY_PREAMBLE_CONFIG_STD_1,
    RH_RF24_PROPERTY_PREAMBLE_CONFIG_NSTD,
    RH_RF24_PROPERTY_PREAMBLE_CONFIG_STD_2,
    RH_RF24_PROPERTY_PREAMBLE_CONFIG,
    RH_RF24_PROPERTY_PREAMBLE_PATTERN_31_24,
    RH_RF24_PROPERTY_PREAMBLE_PATTERN_23_16,
    RH_RF24_PROPERTY_PREAMBLE_PATTERN_15_8,
    RH_RF24_PROPERTY_PREAMBLE_PATTERN_7_0,
    RH_RF24_PROPERTY_SYNC_CONFIG,
    RH_RF24_PROPERTY_SYNC_BITS_31_24,
    RH_RF24_PROPERTY_SYNC_BITS_23_16,
    RH_RF24_PROPERTY_SYNC_BITS_15_8,
    RH_RF24_PROPERTY_SYNC_BITS_7_0,
    RH_RF24_PROPERTY_PKT_CRC_CONFIG,
    RH_RF24_PROPERTY_PKT_CONFIG1,
    RH_RF24_PROPERTY_PKT_LEN,
    RH_RF24_PROPERTY_PKT_LEN_FIELD_SOURCE,
    RH_RF24_PROPERTY_PKT_LEN_ADJUST,
    RH_RF24_PROPERTY_PKT_TX_THRESHOLD,
    RH_RF24_PROPERTY_PKT_RX_THRESHOLD,
    RH_RF24_PROPERTY_PKT_FIELD_1_LENGTH_12_8,
    RH_RF24_PROPERTY_PKT_FIELD_1_LENGTH_7_0,
    RH_RF24_PROPERTY_PKT_FIELD_1_CONFIG,
    RH_RF24_PROPERTY_PKT_FIELD_1_CRC_CONFIG,
    RH_RF24_PROPERTY_PKT_FIELD_2_LENGTH_12_8,
    RH_RF24_PROPERTY_PKT_FIELD_2_LENGTH_7_0,
    RH_RF24_PROPERTY_PKT_FIELD_2_CONFIG,
    RH_RF24_PROPERTY_PKT_FIELD_2_CRC_CONFIG,
    RH_RF24_PROPERTY_PKT_FIELD_3_LENGTH_12_8,
    RH_RF24_PROPERTY_PKT_FIELD_3_LENGTH_7_0,
    RH_RF24_PROPERTY_PKT_FIELD_3_CONFIG,
    RH_RF24_PROPERTY_PKT_FIELD_3_CRC_CONFIG,
    RH_RF24_PROPERTY_PKT_FIELD_4_LENGTH_12_8,
    RH_RF24_PROPERTY_PKT_FIELD_4_LENGTH_7_0,
    RH_RF24_PROPERTY_PKT_FIELD_4_CONFIG,
    RH_RF24_PROPERTY_PKT_FIELD_4_CRC_CONFIG,
    RH_RF24_PROPERTY_PKT_FIELD_5_LENGTH_12_8,
    RH_RF24_PROPERTY_PKT_FIELD_5_LENGTH_7_0,
    RH_RF24_PROPERTY_PKT_FIELD_5_CONFIG,
    RH_RF24_PROPERTY_PKT_FIELD_5_CRC_CONFIG,
    RH_RF24_PROPERTY_PKT_RX_FIELD_1_LENGTH_12_8,
    RH_RF24_PROPERTY_PKT_RX_FIELD_1_LENGTH_7_0,
    RH_RF24_PROPERTY_PKT_RX_FIELD_1_CONFIG,
    RH_RF24_PROPERTY_PKT_RX_FIELD_1_CRC_CONFIG,
    RH_RF24_PROPERTY_PKT_RX_FIELD_2_LENGTH_12_8,
    RH_RF24_PROPERTY_PKT_RX_FIELD_2_LENGTH_7_0,
    RH_RF24_PROPERTY_PKT_RX_FIELD_2_CONFIG,
    RH_RF24_PROPERTY_PKT_RX_FIELD_2_CRC_CONFIG,
    RH_RF24_PROPERTY_PKT_RX_FIELD_3_LENGTH_12_8,
    RH_RF24_PROPERTY_PKT_RX_FIELD_3_LENGTH_7_0,
    RH_RF24_PROPERTY_PKT_RX_FIELD_3_CONFIG,
    RH_RF24_PROPERTY_PKT_RX_FIELD_3_CRC_CONFIG,
    RH_RF24_PROPERTY_PKT_RX_FIELD_4_LENGTH_12_8,
    RH_RF24_PROPERTY_PKT_RX_FIELD_4_LENGTH_7_0,
    RH_RF24_PROPERTY_PKT_RX_FIELD_4_CONFIG,
    RH_RF24_PROPERTY_PKT_RX_FIELD_4_CRC_CONFIG,
    RH_RF24_PROPERTY_PKT_RX_FIELD_5_LENGTH_12_8,
    RH_RF24_PROPERTY_PKT_RX_FIELD_5_LENGTH_7_0,
    RH_RF24_PROPERTY_PKT_RX_FIELD_5_CONFIG,
    RH_RF24_PROPERTY_PKT_RX_FIELD_5_CRC_CONFIG,
    RH_RF24_PROPERTY_MODEM_MOD_TYPE,
    RH_RF24_PROPERTY_MODEM_MAP_CONTROL,
    RH_RF24_PROPERTY_MODEM_DSM_CTRL,
    RH_RF24_PROPERTY_MODEM_DATA_RATE_2,
    RH_RF24_PROPERTY_MODEM_DATA_RATE_1,
    RH_RF24_PROPERTY_MODEM_DATA_RATE_0,
    RH_RF24_PROPERTY_MODEM_TX_NCO_MODE_3,
    RH_RF24_PROPERTY_MODEM_TX_NCO_MODE_2,
    RH_RF24_PROPERTY_MODEM_TX_NCO_MODE_1,
    RH_RF24_PROPERTY_MODEM_TX_NCO_MODE_0,
    RH_RF24_PROPERTY_MODEM_FREQ_DEV_2,
    RH_RF24_PROPERTY_MODEM_FREQ_DEV_1,
    RH_RF24_PROPERTY_MODEM_FREQ_DEV_0,
    RH_RF24_PROPERTY_MODEM_TX_RAMP_DELAY,
    RH_RF24_PROPERTY_MODEM_MDM_CTRL,
    RH_RF24_PROPERTY_MODEM_IF_CONTROL,
    RH_RF24_PROPERTY_MODEM_IF_FREQ_2,
    RH_RF24_PROPERTY_MODEM_IF_FREQ_1,
    RH_RF24_PROPERTY_MODEM_IF_FREQ_0,
    RH_RF24_PROPERTY_MODEM_DECIMATION_CFG1,
    RH_RF24_PROPERTY_MODEM_DECIMATION_CFG0,
    RH_RF24_PROPERTY_MODEM_BCR_OSR_1,
    RH_RF24_PROPERTY_MODEM_BCR_OSR_0,
    RH_RF24_PROPERTY_MODEM_BCR_NCO_OFFSET_2,
    RH_RF24_PROPERTY_MODEM_BCR_NCO_OFFSET_1,
    RH_RF24_PROPERTY_MODEM_BCR_NCO_OFFSET_0,
    RH_RF24_PROPERTY_MODEM_BCR_GAIN_1,
    RH_RF24_PROPERTY_MODEM_BCR_GAIN_0,
    RH_RF24_PROPERTY_MODEM_BCR_GEAR,
    RH_RF24_PROPERTY_MODEM_BCR_MISC1,
    RH_RF24_PROPERTY_MODEM_AFC_GEAR,
    RH_RF24_PROPERTY_MODEM_AFC_WAIT,
    RH_RF24_PROPERTY_MODEM_AFC_GAIN_1,
    RH_RF24_PROPERTY_MODEM_AFC_GAIN_0,
    RH_RF24_PROPERTY_MODEM_AFC_LIMITER_1,
    RH_RF24_PROPERTY_MODEM_AFC_LIMITER_0,
    RH_RF24_PROPERTY_MODEM_AFC_MISC,
    RH_RF24_PROPERTY_MODEM_AGC_CONTROL,
    RH_RF24_PROPERTY_MODEM_AGC_WINDOW_SIZE,
    RH_RF24_PROPERTY_MODEM_AGC_RFPD_DECAY,
    RH_RF24_PROPERTY_MODEM_AGC_IFPD_DECAY,
    RH_RF24_PROPERTY_MODEM_FSK4_GAIN1,
    RH_RF24_PROPERTY_MODEM_FSK4_GAIN0,
    RH_RF24_PROPERTY_MODEM_FSK4_TH1,
    RH_RF24_PROPERTY_MODEM_FSK4_TH0,
    RH_RF24_PROPERTY_MODEM_FSK4_MAP,
    RH_RF24_PROPERTY_MODEM_OOK_PDTC,
    RH_RF24_PROPERTY_MODEM_OOK_CNT1,
    RH_RF24_PROPERTY_MODEM_OOK_MISC,
    RH_RF24_PROPERTY_MODEM_RAW_SEARCH,
    RH_RF24_PROPERTY_MODEM_RAW_CONTROL,
    RH_RF24_PROPERTY_MODEM_RAW_EYE_1,
    RH_RF24_PROPERTY_MODEM_RAW_EYE_0,
    RH_RF24_PROPERTY_MODEM_ANT_DIV_MODE,
    RH_RF24_PROPERTY_MODEM_ANT_DIV_CONTROL,
    RH_RF24_PROPERTY_MODEM_RSSI_THRESH,
    RH_RF24_PROPERTY_MODEM_RSSI_JUMP_THRESH,
    RH_RF24_PROPERTY_MODEM_RSSI_CONTROL,
    RH_RF24_PROPERTY_MODEM_RSSI_CONTROL2,
    RH_RF24_PROPERTY_MODEM_RSSI_COMP,
    RH_RF24_PROPERTY_MODEM_ANT_DIV_CONT,
    RH_RF24_PROPERTY_MODEM_CLKGEN_BAND,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX1_CHFLT_COE13_7_0,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX1_CHFLT_COE12_7_0,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX1_CHFLT_COE11_7_0,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX1_CHFLT_COE10_7_0,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX1_CHFLT_COE9_7_0,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX1_CHFLT_COE8_7_0,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX1_CHFLT_COE7_7_0,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX1_CHFLT_COE6_7_0,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX1_CHFLT_COE5_7_0,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX1_CHFLT_COE4_7_0,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX1_CHFLT_COE3_7_0,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX1_CHFLT_COE2_7_0,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX1_CHFLT_COE1_7_0,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX1_CHFLT_COE0_7_0,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX1_CHFLT_COEM0,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX1_CHFLT_COEM1,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX1_CHFLT_COEM2,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX1_CHFLT_COEM3,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX2_CHFLT_COE13_7_0,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX2_CHFLT_COE12_7_0,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX2_CHFLT_COE11_7_0,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX2_CHFLT_COE10_7_0,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX2_CHFLT_COE9_7_0,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX2_CHFLT_COE8_7_0,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX2_CHFLT_COE7_7_0,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX2_CHFLT_COE6_7_0,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX2_CHFLT_COE5_7_0,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX2_CHFLT_COE4_7_0,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX2_CHFLT_COE3_7_0,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX2_CHFLT_COE2_7_0,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX2_CHFLT_COE1_7_0,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX2_CHFLT_COE0_7_0,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX2_CHFLT_COEM0,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX2_CHFLT_COEM1,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX2_CHFLT_COEM2,
    RH_RF24_PROPERTY_MODEM_CHFLT_RX2_CHFLT_COEM3,
    RH_RF24_PROPERTY_PA_MODE,
    RH_RF24_PROPERTY_PA_PWR_LVL,
    RH_RF24_PROPERTY_PA_BIAS_CLKDUTY,
    RH_RF24_PROPERTY_PA_TC,
    RH_RF24_PROPERTY_SYNTH_PFDCP_CPFF,
    RH_RF24_PROPERTY_SYNTH_PFDCP_CPINT,
    RH_RF24_PROPERTY_SYNTH_VCO_KV,
    RH_RF24_PROPERTY_SYNTH_LPFILT3,
    RH_RF24_PROPERTY_SYNTH_LPFILT2,
    RH_RF24_PROPERTY_SYNTH_LPFILT1,
    RH_RF24_PROPERTY_SYNTH_LPFILT0,
    RH_RF24_PROPERTY_MATCH_VALUE_1,
    RH_RF24_PROPERTY_MATCH_MASK_1,
    RH_RF24_PROPERTY_MATCH_CTRL_1,
    RH_RF24_PROPERTY_MATCH_VALUE_2,
    RH_RF24_PROPERTY_MATCH_MASK_2,
    RH_RF24_PROPERTY_MATCH_CTRL_2,
    RH_RF24_PROPERTY_MATCH_VALUE_3,
    RH_RF24_PROPERTY_MATCH_MASK_3,
    RH_RF24_PROPERTY_MATCH_CTRL_3,
    RH_RF24_PROPERTY_MATCH_VALUE_4,
    RH_RF24_PROPERTY_MATCH_MASK_4,
    RH_RF24_PROPERTY_MATCH_CTRL_4,
    RH_RF24_PROPERTY_FREQ_CONTROL_INTE,
    RH_RF24_PROPERTY_FREQ_CONTROL_FRAC_2,
    RH_RF24_PROPERTY_FREQ_CONTROL_FRAC_1,
    RH_RF24_PROPERTY_FREQ_CONTROL_FRAC_0,
    RH_RF24_PROPERTY_FREQ_CONTROL_CHANNEL_STEP_SIZE_1,
    RH_RF24_PROPERTY_FREQ_CONTROL_CHANNEL_STEP_SIZE_0,
    RH_RF24_PROPERTY_FREQ_CONTROL_VCOCNT_RX_ADJ,
    RH_RF24_PROPERTY_RX_HOP_CONTROL,
    RH_RF24_PROPERTY_RX_HOP_TABLE_SIZE,
    RH_RF24_PROPERTY_RX_HOP_TABLE_ENTRY_0,
};
#define	NUM_PROPERTIES (sizeof(properties)/sizeof(uint16_t))

bool RH_RF24::printRegisters()
{
#ifdef RH_HAVE_SERIAL
    uint8_t i;
    // First print the commands that return interesting data
    for (i = 0; i < NUM_COMMAND_INFO; i++)
    {
	CommandInfo cmd;
	memcpy_P(&cmd, &commands[i], sizeof(cmd));
	uint8_t buf[10]; // Big enough for the biggest command reply
	if (command(cmd.cmd, NULL, 0, buf, cmd.replyLen))
	{
	    // Print the results:
	    SerialUSB.print("cmd: ");
	    SerialUSB.print(cmd.cmd, HEX);
	    SerialUSB.print(" : ");
	    uint8_t j;
	    for (j = 0; j < cmd.replyLen; j++)
	    {
		SerialUSB.print(buf[j], HEX);
		SerialUSB.print(" ");
	    }
	    SerialUSB.println("");
	}
    }

    // Now print the properties
    for (i = 0; i < NUM_PROPERTIES; i++)
    {
	uint16_t prop;
	memcpy_P(&prop, &properties[i], sizeof(prop));
	uint8_t result;
	get_properties(prop, &result, 1);
	SerialUSB.print("prop: ");
	SerialUSB.print(prop, HEX);
	SerialUSB.print(": ");
	SerialUSB.print(result, HEX);
	SerialUSB.println("");
    }
#endif
    return true;
}
