#include <Arduino.h>
#include "s6c.h"
#include "RadioInterface.h"
#include <wiring_private.h>

#include "timer_utils.h"

#define NO_TRANSPORT_PROTOCOL
#include "min.h"
struct min_context min_ctx_usb;
struct min_context min_ctx_header;

/* TODO: replace with planned smart-blah-blah-mmap-y buffer system. */
#define BUFFER_SIZE 256
char transmit_buffer[BUFFER_SIZE];
char receive_buffer[BUFFER_SIZE];
char current_transmission[BUFFER_SIZE];

// If this magic number is written into location 0 in EEPROM, then we assume
// that a valid config is saved to EEPROM.
#define MAGIC_EEPROM_KEY 13

#define REV_MAJOR 2
#define REV_MINOR 0
#define USB_SERIAL_BAUD 115200
#define HEADER_SERIAL_BAUD 9600

// #define HEADER_TX 10
// #define HEADER_RX 11
//
// Uart SerialHeader(&sercom1, HEADER_RX, HEADER_TX, SERCOM_RX_PAD_2, UART_TX_PAD_0);

// NEED MORE ELEGANT SERCOM HANDLER

#if (HWREV == 102)
#define HEADER_TX 8
#define HEADER_RX 17

Uart SerialHeader(&sercom0, HEADER_RX, HEADER_TX, SERCOM_RX_PAD_0, UART_TX_PAD_2);

void SERCOM0_Handler()
{
  SerialHeader.IrqHandler();
}

#endif


#if (HWREV == 100)
#define HEADER_TX_PIN 10
#define HEADER_RX_PIN 11

Uart SerialHeader(&sercom1, HEADER_RX_PIN, HEADER_TX_PIN, SERCOM_RX_PAD_2, UART_TX_PAD_0);

void SERCOM1_Handler()
{
  SerialHeader.IrqHandler();
}
#endif

S6C s6c;


#define MODE_RECEIVING 1
#define MODE_TRANSMITTING 2

enum radio_config_datarate {
  // in BITS per second!
  DATARATE_500_BPS   = 0,
  DATARATE_5_KBPS    = 1,
  DATARATE_10_KBPS   = 2,
  // the ones below are NOT tested!
  DATARATE_50_KBPS   = 3,
  DATARATE_100_KBPS  = 4,
  DATARATE_250_KBPS  = 5,
  DATARATE_500_KBPS  = 6,
  DATARATE_1000_KBPS = 7
};


const bool ENABLE_EEPROM_CONFIG = 0; // if 0, disables all behavior relating to saving/loading configuration from EEPROM

struct radio_config {
  int mode = MODE_RECEIVING | MODE_TRANSMITTING;
  float frequency = 433.5; // MHz "Lesson: never comment your code" -- Joank
  bool transmit_continuous = 1; // if 1, resend last msg even if nothing new recvd
  enum radio_config_datarate datarate = DATARATE_5_KBPS;//DATARATE_500_BPS;
  unsigned int interval = 2500;
  unsigned int message_length = 20;
  unsigned int ack_interval = 60000;
  bool tdma_enabled = 0;
  uint8_t epoch_slots = 4;
  uint16_t allocated_slots = 0b0001; // which slot(s) this device is permitted to transmit in
};

struct radio_config global_config;      // global config
struct radio_config last_eeprom_config; // last-EEPROM-written config
struct radio_config quicksave_config;   // Joank
uint32_t quicksave_acktime = 0;




uint32_t last_transmission_time = 0;
bool force_transmit = false;


/* ************************************************************************* */
/* EEPROM ****************************************************************** */
/* ************************************************************************* */

void read_eeprom_config(uint8_t *out) {
  for (size_t i = 0; i<(sizeof(global_config)); i++) {
    out[i] = EEPROM.read(LOC_CONFIG + 1 + i);
  }
}

void maybe_save_config() {
  if(ENABLE_EEPROM_CONFIG){ // only proceed if EEPROM disabled
    uint8_t *new_config = (uint8_t *) &global_config;
    int should_save = memcmp(&last_eeprom_config, new_config, sizeof(struct radio_config));
    if (should_save != 0) { // "The memcmp() function returns zero if the two strings are identical"
      SerialUSB.println("Saving new config to EEPROM...\n");
      EEPROM.write(LOC_CONFIG, MAGIC_EEPROM_KEY);
      for (size_t i = 0; i<(sizeof(struct radio_config)); i++) {
        EEPROM.write(LOC_CONFIG + 1 + i, new_config[i]);
      }
      EEPROM.commit();
      memcpy(&last_eeprom_config, new_config, sizeof(global_config));
    }
  }
}


/* ************************************************************************* */
/* TDMA ******************************************************************** */
/* ************************************************************************* */

const unsigned long bits_per_second[] = {500, 5000, 10000, 50000, 100000, 250000, 500000, 1000000};
const unsigned long us_per_byte[] = {16000, 1600, 800, 160, 80, 32, 16, 8};
const unsigned long TDMA_SLOT_MARGIN_US = 5000; // how much larger is a slot than the message within it
const unsigned long TDMA_MSG_MARGIN_US = 1000; // how much margin to use for time required to send a message - only used to check if there is enough time left to send

void setTDMAlengths();
void updateTDMA();
void resyncTDMA(long offset);
unsigned long validTDMAsend();
unsigned long dif_micros(unsigned long start, unsigned long end);

unsigned long epoch_start_us = 0; // Time this TDMA epoch started
unsigned long epoch_time_us = 0; // Time elapsed so far during this TDMA epoch
uint16_t epoch_slot = 0; // Which slot in the TDMA epoch is currently active
unsigned long slot_start_us = 0; // Time this slot started
unsigned long slot_time_us = 0; // Time elapsed so far during this TDMA slot
unsigned int msg_length_bytes; // size of each transmission, in bytes
unsigned long msg_length_us; // size of each transmission, in microseconds - this is the exact length of a message, with no margin
unsigned long slot_length_us; // size of each slot, in microseconds - this equals msg_length_us + TDMA_SLOT_MARGIN_US
unsigned long epoch_length_us; // length of a TDMA epoch, in microseconds
bool TDMA_sync = 0; // whether or not the device is properly TDMA synced

void setTDMAlengths(){
  msg_length_bytes = PREAMBLE_LENGTH + NUM_SYNC_WORDS + global_config.message_length +    NPAR;
  msg_length_us = msg_length_bytes * us_per_byte[global_config.datarate];
  slot_length_us = msg_length_us + TDMA_SLOT_MARGIN_US;
  epoch_length_us = slot_length_us * global_config.epoch_slots;

  TDMA_sync = 0; // desync TDMA, preventing transmission until resynced
}

// sets up TDMA epoch to start at the current time minus an offset
// offset supplied as the new epoch time; give this function the new epoch time
// and it will figure out which slot it is currently in and when the epoch started
// enables TDMA_sync, telling the radio it has been properly synced up and can safely transmit
void resyncTDMA(unsigned long new_epoch_time){

  unsigned long mutable_new_epoch_time = new_epoch_time;

  // if you specify an offset longer than an epoch (not desirable, but possible)
  // using the modulus "skips" forward to the current epoch, gets epoch time
  // in this epoch and makes slot determination work correctly
  if (mutable_new_epoch_time > epoch_length_us) mutable_new_epoch_time %= epoch_length_us;

  // this is rollover-safe; if offset > micros(), this will set new_epoch_start
  // to a large positive value, which other code will correctly interpret
  unsigned long now = micros();
  unsigned long new_epoch_start = now - mutable_new_epoch_time;

  // figure out which slot it currently is
  uint8_t new_slot = mutable_new_epoch_time/slot_length_us;
  unsigned long new_slot_start = mutable_new_epoch_time + (slot_length_us * new_slot);
  unsigned long new_slot_time = now - (slot_length_us * new_slot);

  epoch_start_us = new_epoch_start;
  slot_start_us = new_slot_start;
  slot_time_us = new_slot_time;
  epoch_time_us = mutable_new_epoch_time;

  // TDMA has been synchronized; reenable transmission
  TDMA_sync = 1;
}

void updateTDMA(){
  unsigned long now = micros();
  unsigned long new_epoch_time = dif_micros(epoch_start_us, now);
  unsigned long new_slot_time = dif_micros(slot_start_us, now);

  // slot rollover
  if(new_slot_time > slot_length_us){
    new_slot_time -= slot_length_us;
    slot_start_us = now - new_slot_time;
    epoch_slot++;
  }

  // epoch rollover
  if(new_epoch_time > epoch_length_us){
    new_epoch_time -= epoch_length_us;
    epoch_start_us = now - new_epoch_time;
    epoch_slot = 0;
  }

  slot_time_us = new_slot_time;
  epoch_time_us = new_epoch_time;
}

// assess whether or not a message can currently be sent per TDMA rules
unsigned long validTDMAsend(){
  if(!global_config.tdma_enabled) return -1; // note: this returns a nonzero value and therefore allows a message to be sent

  if(!TDMA_sync) return 0; // if TDMA enabled, and not synced, prevent messages from being sent (until synced)

  uint16_t one_hot_slot = 0b1 << epoch_slot; // one-hot representation of which slot is currently active

  if(one_hot_slot & global_config.allocated_slots){ // check if currently in a slot allocated to this device
    if(slot_length_us - slot_time_us > msg_length_us + TDMA_MSG_MARGIN_US){ // check if enough time left in this slot to send a message
      return slot_length_us - (slot_time_us + msg_length_us + TDMA_MSG_MARGIN_US); // return leftover margin in us
    }
  }

  return 0;
}

// rollover-safe timekeeping function
// COMPUTES POSITIVE DISTANCE BETWEEN TWO TIMESTAMPS
unsigned long dif_micros(unsigned long start, unsigned long end){
  if(end >= start) return end - start;
  else return end + ((-1 - start) + 1); // compute how far start was from rollover, add to how far end is past rollover
}




void apply_global_config() {
  s6c.rf24->setFrequency(global_config.frequency);
  s6c.rf24->setDatarate(global_config.datarate);
  s6c.rf24->setMessageLength(global_config.message_length + NPAR);
  setTDMAlengths();
}

void restore_saved_config() {
  memcpy(&global_config, (void*) &quicksave_config, sizeof(struct radio_config));
  apply_global_config();
  quicksave_acktime = 0;
}






/* ************************************************************************* */
/* MIN ********************************************************************* */
/* ************************************************************************* */

int num_messages = 0;
bool schedule_config = false;

void min_tx_start(uint8_t port) {}
void min_tx_finished(uint8_t port) {}

uint16_t min_tx_space(uint8_t port) {
  uint16_t n = 1;
  switch(port) {
    case 0: n = SerialUSB.availableForWrite(); break;
    case 1: n = SerialHeader.availableForWrite(); break;
    default: break;
  }
  return n;
}

void min_tx_byte(uint8_t port, uint8_t byte) {
  switch(port) {
    case 0: SerialUSB.write(&byte, 1U); break;
    case 1: SerialHeader.write(&byte, 1U); break;
    default: break;
  }
}

uint32_t min_time_ms(void) {
  return millis();
}

void min_application_handler(uint8_t min_id, uint8_t *min_payload, uint8_t len_payload, uint8_t port) {
  if (len_payload == 0) {
    return;
  }
  int i = 0;
  float vf = 0;
  int16_t vi = 0;
  unsigned long vl = 0;
  while (i < len_payload) {
    int message_type = min_payload[i];
    int remaining = len_payload - i - 1;
    bool break_out = false;
    switch (message_type) {
      case MESSAGE_SEND:
        if (i + 1 + min_payload[i+1] <= len_payload) {
          memset(transmit_buffer, 0, global_config.message_length);
          transmit_buffer[0] = ((num_messages++) % 128) | 128;
          memcpy(transmit_buffer + 1, min_payload + i + 2, min_payload[i+1]);
          force_transmit = true;
        }
        i += min_payload[i+1] + 2;
        break;
      case MESSAGE_SET_MODE:
        if (remaining < 2) { break_out = true; break; }
        memcpy(&vi, min_payload + i + 1, 2);
        if (vi >= 0 && vi <= 3) {
          global_config.mode = vi;
        }
        i += 3;
        break;
      case MESSAGE_SET_FREQUENCY:
        if (remaining < 4) { break_out = true; break; }
        memcpy(&vf, min_payload + i + 1, 4);
        if (vf >= 420 && vf <= 450) {
          SerialUSB.println("Set frequency");
          s6c.rf24->setFrequency(vf);
          global_config.frequency = vf;
        }
        i += 5;
        break;
      case MESSAGE_SET_DATARATE:
        if (remaining < 2) { break_out = true; break; }
        memcpy(&vi, min_payload + i + 1, 2);
        if (vi >= 0 && vi <= 3) {
          SerialUSB.println("Set datarate");
          s6c.rf24->setDatarate(vi);
          global_config.datarate = (enum radio_config_datarate) vi;
          setTDMAlengths();
        }
        i += 3;
        break;
      case MESSAGE_SET_INTERVAL:
        if (remaining < 2) { break_out = true; break; }
        memcpy(&vi, min_payload + i + 1, 2);
        if (vi >= 0 && vi <= 3600) {
          SerialUSB.println("Set interval");
          global_config.interval = vi;
        }
        i += 3;
        break;
      case MESSAGE_SET_LENGTH:
        if (remaining < 2) { break_out = true; break; }
        memcpy(&vi, min_payload + i + 1, 2);
        if (vi >= 0 && vi <= 234) {
          vi++;
          SerialUSB.println("Set message length");
          s6c.rf24->setMessageLength(vi + NPAR);
          global_config.message_length = vi;
          setTDMAlengths();
        }
        i += 3;
        break;
      case MESSAGE_SET_CONTINUOUS:
        if (remaining < 2) { break_out = true; break; }
        memcpy(&vi, min_payload + i + 1, 2);
        if (vi >= 0 && vi <= 1) {
          SerialUSB.println("Set continuous mode");
          SerialUSB.println(global_config.transmit_continuous);
          SerialUSB.println(vi);
          global_config.transmit_continuous = vi;
          global_config.tdma_enabled = !global_config.transmit_continuous; // disable TDMA if set to continuous transmission, enable if not continuous
        }
        i += 3;
        break;
      case MESSAGE_QUICKSAVE:
        memcpy(&quicksave_config, (void*)&global_config, sizeof(struct radio_config));
        quicksave_acktime = millis() + global_config.ack_interval;
        i += 1;
        break;
      case MESSAGE_QUICKLOAD:
        restore_saved_config();
        i += 1;
        break;
      case MESSAGE_QUICKACK:
        quicksave_acktime = 0;
        i += 1;
        break;
      case MESSAGE_SEND_CONFIG:
        if (i + 1 + min_payload[i+1] <= len_payload) {
          memset(transmit_buffer, 0, global_config.message_length);
          transmit_buffer[0] = 128 | min_payload[i+1];
          memcpy(transmit_buffer + 1, min_payload + i + 2, min_payload[i+1]);
          force_transmit = true;
          last_transmission_time = millis() + 1000;
          schedule_config = true;
        }
        i += min_payload[i+1] + 2;
        break;
      case MESSAGE_ARM:
        digitalWrite(PIN_ARM1,HIGH);
        digitalWrite(PIN_ARM2,HIGH);
        digitalWrite(PIN_ARM3,HIGH);
        digitalWrite(PIN_ARM4,HIGH);
        i += 1;
        break;
      case MESSAGE_DISARM:
        digitalWrite(PIN_ARM1,LOW);
        digitalWrite(PIN_ARM2,LOW);
        digitalWrite(PIN_ARM3,LOW);
        digitalWrite(PIN_ARM4,LOW);
        i += 1;
        break;

      case MESSAGE_READ_HWID:
        {
          uint16_t hwid = s6c.getHWID();

          switch(port) {
            case 0: SerialUSB.println(hwid, HEX); break;
            case 1: SerialHeader.println(hwid, HEX); break;
            default: break;// command initiated over radio; ignore
          }
          break;
        }
      case MESSAGE_SET_HWID:
        if (remaining < 2) { break_out = true; break; }
        s6c.setHWID(((uint16_t)(min_payload[i+1]) << 8) | min_payload[i+2]);
        SerialUSB.println("Setting HWID");
        i += 3;
        break;

      case MESSAGE_CLEAR_HWID_FUSE: // don't do it!
        s6c.clearHWIDfuse();
        SerialUSB.println("Clearing HWID fuse");
        break;

      case MESSAGE_TDMA_SYNC:
        if (remaining < 4) { break_out = true; break; }
        memcpy(&vl, min_payload + i + 1, 4);
        resyncTDMA(vl);
        break;

      case MESSAGE_TDMA_SET_SLOTS:
        if (remaining < 2) { break_out = true; break; }
        memcpy(&vi, min_payload + i + 1, 2);
        SerialUSB.println("Setting slots");
        global_config.allocated_slots = vi;
        i += 3;
        break;
      case MESSAGE_TDMA_SET_NUM_SLOTS:
        if (remaining < 2) { break_out = true; break; }
        memcpy(&vi, min_payload + i + 1, 2);
    		if (vi >= 1 && vi <= 16) {
    			SerialUSB.println("Setting num slots");
    			global_config.epoch_slots = vi;
    		}
        i += 3;
        break;
      case MESSAGE_TDMA_ENABLE:
    		global_config.tdma_enabled = 1;
    		i += 1;
    		break;
      default:
        break_out = true;
        break;
    }
    if (break_out) break;
  }

  maybe_save_config();

  SerialUSB.print("MIN frame with ID ");
  SerialUSB.print(min_id);
  SerialUSB.print(" received at ");
  SerialUSB.println(millis());
}







/* Timer to read from UART
 * Called "frequently"...
 */
void TC3_Handler() {
  char serial_buffer_usb[32];
  char serial_buffer_header[32];
  if (TC3->COUNT16.INTFLAG.bit.OVF && TC3->COUNT16.INTENSET.bit.OVF) {
    int available = SerialUSB.available();
    if (available > 0) {
      if (available > 32) available = 32;
      size_t buf_len = SerialUSB.readBytes(serial_buffer_usb, available);
      min_poll(&min_ctx_usb, (uint8_t*)serial_buffer_usb, (uint8_t)buf_len);
    } else {
      min_poll(&min_ctx_usb, 0, 0);
    }

    available = SerialHeader.available();
    if (available > 0) {
      if (available > 32) available = 32;
      size_t buf_len = SerialHeader.readBytes(serial_buffer_header, available);
      min_poll(&min_ctx_header, (uint8_t*)serial_buffer_header, (uint8_t)buf_len);
    } else {
      min_poll(&min_ctx_header, 0, 0);
    }
    REG_TC3_INTFLAG = TC_INTFLAG_OVF;         // Clear the OVF interrupt flag
  }
}













/* SETUP
 *
 */

void setup() {
  s6c.configureLED();
  s6c.blinkStatus(REV_MAJOR);
  delay(500);
  s6c.blinkStatus(REV_MINOR);
  s6c.LEDOn(true);
  delay(1000);

  SerialUSB.begin(USB_SERIAL_BAUD);
  SerialUSB.setTimeout(1);
  SerialUSB.println("Starting...");

  SerialHeader.begin(HEADER_SERIAL_BAUD);
  //pinPeripheral(HEADER_RX_PIN, PIO_SERCOM);
  //pinPeripheral(HEADER_TX_PIN, PIO_SERCOM);

  SerialUSB.println("Configuring RF...");
  s6c.configureRF();
  apply_global_config();
  setTDMAlengths();

  uint8_t config_saved = EEPROM.read(LOC_CONFIG);
  if (ENABLE_EEPROM_CONFIG && config_saved == MAGIC_EEPROM_KEY) {
    SerialUSB.println("Loading config from EEPROM...");
    read_eeprom_config((uint8_t*) &global_config);
    memcpy(&last_eeprom_config, &global_config, sizeof(global_config));
  }


  SerialUSB.println("Configured.");

  min_init_context(&min_ctx_usb, 0);
  min_init_context(&min_ctx_header, 1);

  // TODO: This really shouldn't be in radio.cpp?
  pinMode(PIN_ARM1, OUTPUT);
  pinMode(PIN_ARM2, OUTPUT);
  pinMode(PIN_ARM3, OUTPUT);
  pinMode(PIN_ARM4, OUTPUT);

  setup_timer(); // TC3_Handler related
  delay(100);
  s6c.LEDOff(true);

  maybe_save_config();
}

char DATA[] = "Desperta ferro! Desperta ferro! Sant Jordi! Sant Jordi! Arago! Arago!";
uint8_t RB_CMD[32] = {0};
uint32_t last = 0;

void loop() {
  updateTDMA();

  if (quicksave_acktime > 0 && millis() > quicksave_acktime) {
    restore_saved_config();
  }
  if (global_config.mode & MODE_TRANSMITTING) {
    if ((global_config.transmit_continuous && (millis() - last_transmission_time >= global_config.interval)) || force_transmit) {
      if (validTDMAsend()) {
        unsigned long tx_start = micros();
        SerialUSB.println(tx_start);
        last_transmission_time = millis();

        if(global_config.transmit_continuous){
          transmit_buffer[0] = ((num_messages++) % 128) | 128;
          *(unsigned long*)(transmit_buffer+1) = micros();
        }

        noInterrupts();
        memcpy(current_transmission, transmit_buffer, global_config.message_length);
        interrupts();
        s6c.LEDOn();
        SerialUSB.println("Sending");
        uint32_t t0 = micros();
        s6c.encode_and_transmit(current_transmission, global_config.message_length);
        SerialUSB.println(((float)(micros()-t0))/1000.);
        force_transmit = false;
        s6c.LEDOff();
        unsigned long tx_end = micros();
        SerialUSB.println(tx_end);
        SerialUSB.println(tx_end - tx_start);
        SerialUSB.println(msg_length_bytes);
        SerialUSB.println(msg_length_us);
        SerialUSB.println(slot_length_us);
        SerialUSB.println(epoch_length_us);
        SerialUSB.println();
      }
    }
  }
  if (global_config.mode & MODE_RECEIVING) {
    uint8_t rx = s6c.tryToRX(receive_buffer, global_config.message_length);
    if (rx == 3 || rx == 1) {
      s6c.LEDOn();
      if (schedule_config) {
        schedule_config = false;
        force_transmit = true;
        last_transmission_time = millis() - 2*global_config.interval;
        return;
      }
      if (receive_buffer[0] & 128) {
        receive_buffer[0] &= ~128U;
        SerialUSB.println("it's a config message!");
        min_application_handler(0x02, (uint8_t*)(receive_buffer + 1), min(receive_buffer[0], global_config.message_length - 1), 2);
      }
      receive_buffer[0] = s6c.getRSSI();
      receive_buffer[0] &= ~1U;
      receive_buffer[0] |= (rx == 3);
      for (int k=0; k<4; k++) SerialUSB.println();
      /*int k = 0;
        for (; k<global_config.message_length; k++) {
        SerialUSB.print((char)RB_CMD[k]);
        }
        SerialUSB.println();
        k++;
        SerialUSB.println(k);
        SerialUSB.println("lesgo");*/

      min_send_frame(&min_ctx_usb, 3, (uint8_t*)(receive_buffer), global_config.message_length);
      min_send_frame(&min_ctx_header, 3, (uint8_t*)(receive_buffer), global_config.message_length);

      SerialUSB.println("Got message!");
      s6c.LEDOff();
      /*Serial1.write(RADIO_START_SEQUENCE,4);
        Serial1.write((uint8_t*)RB_CMD, k);
        Serial1.write(RADIO_END_SEQUENCE,4);*/
    }
  }
}
