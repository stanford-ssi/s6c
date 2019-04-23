#include <Arduino.h>
#include "s6c.h"
#include "RadioInterface.h"
#include <wiring_private.h>

#include "timer_utils.h"

#define NO_TRANSPORT_PROTOCOL
#include "min.h"

#define MODE_RECEIVING 1
#define MODE_TRANSMITTING 2

#define BUFFER_SIZE 256

#define REV_MAJOR 2
#define REV_MINOR 0

/* Configuration
 * -------------
 *   mode: transmit and/or receive
 *   frequency: frequency in Hz
 *   transmit_continuous: if true, resend last message, even if nothing new
 *      was received.
 *   datarate: one of
 *      - 0: 500 bps = 62.5 B/s
 *      - 1: 5 kbps = 625 B/s
 *      - 2: 10 kbps = 2.5 kB/s
 *     not tested:
 *      - 3: 50 kbps = 6.25 kB/s
 *      - 4: 100 kbps = 12.5 kB/s
 *      - 5: 250 kbps = 31.25 kB/s
 *      - 6: 500 kbps = 62.5 kB/s
 *      - 7: 1000 kbps = 125 kB/s
 */

#define HEADER_TX 10
#define HEADER_RX 11

Uart SerialHeader(&sercom1, HEADER_RX, HEADER_TX, SERCOM_RX_PAD_2, UART_TX_PAD_0);

void SERCOM1_Handler()
{
  SerialHeader.IrqHandler();
}

struct radio_config {
    int mode = 0b11;
    float frequency = 433.5;
    bool transmit_continuous = 0;
    int datarate = 0;
    unsigned int interval = 1000;
    unsigned int message_length = 20;
    unsigned int ack_interval = 60000;
    bool tdma_enabled = 1;
    uint8_t epoch_slots = 4;
    uint32_t allocated_slots = 0b0001; // which slot(s) this device is permitted to transmit in
} CONFIG;

char saved_config[sizeof(struct radio_config)];
uint32_t ack_time = 0;

/* TODO: replace with planned smart-blah-blah-mmap-y buffer system. */
char transmit_buffer[BUFFER_SIZE];
char receive_buffer[BUFFER_SIZE];
char current_transmission[BUFFER_SIZE];

uint32_t last_transmission_time = 0;
bool force_transmit = false;

int recv = 0;

S6C s6c;

struct min_context min_ctx_usb;
struct min_context min_ctx_header;


// TDMA
////////////////////////////////////////////////////
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
    msg_length_bytes = PREAMBLE_LENGTH + NUM_SYNC_WORDS + CONFIG.message_length +    NPAR;
    msg_length_us = msg_length_bytes * us_per_byte[CONFIG.datarate];
    slot_length_us = msg_length_us + TDMA_SLOT_MARGIN_US;
    epoch_length_us = slot_length_us * CONFIG.epoch_slots;

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
    if(!CONFIG.tdma_enabled) return -1; // note: this returns a nonzero value and therefore allows a message to be sent

    if(!TDMA_sync) return 0; // if TDMA enabled, and not synced, prevent messages from being sent (until synced)

    uint16_t one_hot_slot = 0b1 << epoch_slot; // one-hot representation of which slot is currently active

    if(one_hot_slot & CONFIG.allocated_slots){ // check if currently in a slot allocated to this device
        if(slot_length_us - slot_time_us > msg_length_us + TDMA_MSG_MARGIN_US){ // check if enough time left in this slot to send a message
            return slot_length_us - (slot_time_us + msg_length_us + TDMA_MSG_MARGIN_US); // return leftover margin in us
        }
    }

    return 0;
}

// rollover-safe timekeeping function
// COMPUTES POSITIVE DISTANCE BETWEEN TWO TIMESTAMPS
unsigned long dif_micros(unsigned long start, unsigned long end){
  if(end > start) return end - start;
  else return end + ((-1 - start) + 1); // compute how far start was from rollover, add to how far end is past rollover
}

void restore_saved_config() {
    memcpy(&CONFIG, (void*)saved_config, sizeof(struct radio_config));
    s6c.rf24->setFrequency(CONFIG.frequency);
    s6c.rf24->setDatarate(CONFIG.datarate);
    s6c.rf24->setMessageLength(CONFIG.message_length + NPAR);
  setTDMAlengths();
    ack_time = 0;
}

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

int num_messages = 0;
bool schedule_config = false;

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
                memset(transmit_buffer, 0, CONFIG.message_length);
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
                CONFIG.mode = vi;
            }
            i += 3;
            break;
        case MESSAGE_SET_FREQUENCY:
            if (remaining < 4) { break_out = true; break; }
            memcpy(&vf, min_payload + i + 1, 4);
            if (vf >= 420 && vf <= 450) {
                SerialUSB.println("Set frequency");
                s6c.rf24->setFrequency(vf);
                CONFIG.frequency = vf;
            }
            i += 5;
            break;
        case MESSAGE_SET_DATARATE:
            if (remaining < 2) { break_out = true; break; }
            memcpy(&vi, min_payload + i + 1, 2);
            if (vi >= 0 && vi <= 3) {
                SerialUSB.println("Set datarate");
                s6c.rf24->setDatarate(vi);
                CONFIG.datarate = vi;
                setTDMAlengths();
            }
            i += 3;
            break;
        case MESSAGE_SET_INTERVAL:
            if (remaining < 2) { break_out = true; break; }
            memcpy(&vi, min_payload + i + 1, 2);
            if (vi >= 0 && vi <= 3600) {
                SerialUSB.println("Set interval");
                CONFIG.interval = vi;
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
                CONFIG.message_length = vi;
        		setTDMAlengths();
            }
            i += 3;
            break;
        case MESSAGE_SET_CONTINUOUS:
            if (remaining < 2) { break_out = true; break; }
            memcpy(&vi, min_payload + i + 1, 2);
            if (vi >= 0 && vi <= 1) {
                SerialUSB.println("Set continuous mode");
                SerialUSB.println(CONFIG.transmit_continuous);
                SerialUSB.println(vi);
                CONFIG.transmit_continuous = vi;
        		CONFIG.tdma_enabled = !CONFIG.transmit_continuous; // disable TDMA if set to continuous transmission, enable if not continuous
            }
            i += 3;
            break;
        case MESSAGE_QUICKSAVE:
            memcpy(saved_config, (void*)&CONFIG, sizeof(struct radio_config));
            ack_time = millis() + CONFIG.ack_interval;
            i += 1;
            break;
        case MESSAGE_QUICKLOAD:
            restore_saved_config();
            i += 1;
            break;
        case MESSAGE_QUICKACK:
            ack_time = 0;
            i += 1;
            break;
        case MESSAGE_SEND_CONFIG:
            if (i + 1 + min_payload[i+1] <= len_payload) {
                memset(transmit_buffer, 0, CONFIG.message_length);
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


        default:
            break_out = true;
            break;
        }
        if (break_out) break;
    }

    SerialUSB.print("MIN frame with ID ");
    SerialUSB.print(min_id);
    SerialUSB.print(" received at ");
    SerialUSB.println(millis());
    recv++;
}

void min_tx_start(uint8_t port) {}
void min_tx_finished(uint8_t port) {
   /* switch(port) {
    case 0: SerialUSB.flush();
    case 1: SerialHeader.flush();
    }*/
}

char serial_buffer_usb[32];
char serial_buffer_header[32];

void TC3_Handler() {
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
        //SerialUSB.println(micros());
        REG_TC3_INTFLAG = TC_INTFLAG_OVF;         // Clear the OVF interrupt flag
    }
}

void setup() {
    s6c.configureLED();
    s6c.blinkStatus(REV_MAJOR);
    delay(500);
    s6c.blinkStatus(REV_MINOR);
    s6c.LEDOn(true);
    delay(1000);
    SerialUSB.begin(115200);
    SerialUSB.setTimeout(1);
    SerialUSB.println("Starting...");
    SerialUSB.println("hullo s6c");

    SerialHeader.begin(9600);
    pinPeripheral(HEADER_RX, PIO_SERCOM);
    pinPeripheral(HEADER_TX, PIO_SERCOM);

    s6c.configureRF();
    s6c.rf24->setMessageLength(CONFIG.message_length + NPAR);
    setTDMAlengths();
    SerialUSB.println("Configured!!!!!");

    min_init_context(&min_ctx_usb, 0);
    min_init_context(&min_ctx_header, 1);

    pinMode(PIN_ARM1,OUTPUT);
    pinMode(PIN_ARM2,OUTPUT);
    pinMode(PIN_ARM3,OUTPUT);
    pinMode(PIN_ARM4,OUTPUT);

    setup_timer();
    delay(100);
    s6c.LEDOff(true);


}

char DATA[] = "Desperta ferro! Desperta ferro! Sant Jordi! Sant Jordi! Arago! Arago!";
uint8_t RB_CMD[32] = {0};
uint32_t last = 0;

void loop() {
    /*if (millis() - last > 1000) {
        min_queue_frame(&min_ctx_usb, 4, (uint8_t*)("got it fam"), 10);
        last = millis();
    }*/

    updateTDMA();

    if (ack_time > 0 && millis() > ack_time) {
        restore_saved_config();
    }
    if (CONFIG.mode & MODE_TRANSMITTING) {
        if ((CONFIG.transmit_continuous && (millis() - last_transmission_time >= CONFIG.interval)) || force_transmit) {
            if (validTDMAsend()) {
                last_transmission_time = millis();
                noInterrupts();
                memcpy(current_transmission, transmit_buffer, CONFIG.message_length);
                interrupts();
                s6c.LEDOn();
                SerialUSB.println("Sending");
                uint32_t t0 = micros();
                s6c.encode_and_transmit(current_transmission, CONFIG.message_length);
                SerialUSB.println(((float)(micros()-t0))/1000.);
                force_transmit = false;
                s6c.LEDOff();
            }
        }
    }
    if (CONFIG.mode & MODE_RECEIVING) {
        uint8_t rx = s6c.tryToRX(receive_buffer, CONFIG.message_length);
        if (rx == 3 || rx == 1) {
            s6c.LEDOn();
            if (schedule_config) {
                schedule_config = false;
                force_transmit = true;
                last_transmission_time = millis() - 2*CONFIG.interval;
                return;
            }
            if (receive_buffer[0] & 128) {
                receive_buffer[0] &= ~128U;
                SerialUSB.println("it's a config message!");
                min_application_handler(0x02, (uint8_t*)(receive_buffer + 1), min(receive_buffer[0], CONFIG.message_length - 1), 2);
            }
            receive_buffer[0] = s6c.getRSSI();
            receive_buffer[0] &= ~1U;
            receive_buffer[0] |= (rx == 3);
            for (int k=0; k<4; k++) SerialUSB.println();
            /*int k = 0;
            for (; k<CONFIG.message_length; k++) {
                SerialUSB.print((char)RB_CMD[k]);
            }
            SerialUSB.println();
            k++;
            SerialUSB.println(k);
            SerialUSB.println("lesgo");*/

            min_send_frame(&min_ctx_usb, 3, (uint8_t*)(receive_buffer), CONFIG.message_length);
            min_send_frame(&min_ctx_header, 3, (uint8_t*)(receive_buffer), CONFIG.message_length);

            SerialUSB.println("Got message!");
            s6c.LEDOff();
            /*Serial1.write(RADIO_START_SEQUENCE,4);
            Serial1.write((uint8_t*)RB_CMD, k);
            Serial1.write(RADIO_END_SEQUENCE,4);*/
        }
    }
}
