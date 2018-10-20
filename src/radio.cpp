#include <Arduino.h>
#include "s6b.h"
#include "RadioInterface.h"

#include "timer_utils.h"

#define NO_TRANSPORT_PROTOCOL
#include "min.h"

#define MODE_RECEIVING 1
#define MODE_TRANSMITTING 2

#define BUFFER_SIZE 256

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

struct radio_config {
		int mode = 0b11;
		float frequency = 433.5;
		bool transmit_continuous = 0;
		int datarate = 0;
		unsigned int interval = 1000;
		unsigned int message_length = 20;
		unsigned int ack_interval = 60000;
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

S6B s6b;

struct min_context min_ctx_usb;

void restore_saved_config() {
	memcpy(&CONFIG, (void*)saved_config, sizeof(struct radio_config));
	s6b.rf24->setFrequency(CONFIG.frequency);
	s6b.rf24->setDatarate(CONFIG.datarate);
	s6b.rf24->setMessageLength(CONFIG.message_length + NPAR);
	ack_time = 0;
}

uint16_t min_tx_space(uint8_t port) {
	uint16_t n = 1;
	if (port == 0) n = SerialUSB.availableForWrite();
	return n;
}

void min_tx_byte(uint8_t port, uint8_t byte) {
	if (port == 0) SerialUSB.write(&byte, 1U);
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
				s6b.rf24->setFrequency(vf);
				CONFIG.frequency = vf;
			}
			i += 5;
			break;
		case MESSAGE_SET_DATARATE:
			if (remaining < 2) { break_out = true; break; }
			memcpy(&vi, min_payload + i + 1, 2);
			if (vi >= 0 && vi <= 3) {
				SerialUSB.println("Set datarate");
				s6b.rf24->setDatarate(vi);
				CONFIG.datarate = vi;
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
				s6b.rf24->setMessageLength(vi + NPAR);
				CONFIG.message_length = vi;
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
void min_tx_finished(uint8_t port) { SerialUSB.flush(); }

char serial_buffer_usb[32];

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
		//SerialUSB.println(micros());
		REG_TC3_INTFLAG = TC_INTFLAG_OVF;         // Clear the OVF interrupt flag
	}
}


void setup() {
	delay(3000);
	SerialUSB.begin(115200);
	SerialUSB.setTimeout(1);
	SerialUSB.println("Starting...");
	SerialUSB.println("hullo s6c");
	//Serial1.begin(115200);
	s6b.configureRF();
	SerialUSB.println("Configured!!!!!");

	min_init_context(&min_ctx_usb, 0);

	setup_timer();
	delay(1000);
}

char DATA[] = "Desperta ferro! Desperta ferro! Sant Jordi! Sant Jordi! Arago! Arago!";
uint8_t RB_CMD[32] = {0};
uint32_t last = 0;

void loop() {
	/*if (millis() - last > 1000) {
		min_queue_frame(&min_ctx_usb, 4, (uint8_t*)("got it fam"), 10);
		last = millis();
	}*/
	if (ack_time > 0 && millis() > ack_time) {
		restore_saved_config();
	}
	if (CONFIG.mode & MODE_TRANSMITTING) {
		if ((CONFIG.transmit_continuous || force_transmit) &&
				(millis() - last_transmission_time >= CONFIG.interval)) {
			last_transmission_time = millis();
			noInterrupts();
			memcpy(current_transmission, transmit_buffer, CONFIG.message_length);
			interrupts();
			SerialUSB.println("Sending");
			uint32_t t0 = micros();
			s6b.encode_and_transmit(current_transmission, CONFIG.message_length);
			SerialUSB.println(((float)(micros()-t0))/1000.);
			force_transmit = false;
		}
	}
	if (CONFIG.mode & MODE_RECEIVING) {
		uint8_t rx = s6b.tryToRX(receive_buffer, CONFIG.message_length);
		if (rx == 3 || rx == 1) {
			if (schedule_config) {
				schedule_config = false;
				force_transmit = true;
				last_transmission_time = millis() - 2*CONFIG.interval;
				return;
			}
			if (receive_buffer[0] & 128) {
				receive_buffer[0] &= ~128U;
				SerialUSB.println("it's a config message!");
				min_application_handler(0x02, (uint8_t*)(receive_buffer + 1), min(receive_buffer[0], CONFIG.message_length - 1), 0);
			}
			receive_buffer[0] = s6b.getRSSI();
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

			SerialUSB.println("Got message!");
			/*Serial1.write(RADIO_START_SEQUENCE,4);
			Serial1.write((uint8_t*)RB_CMD, k);
			Serial1.write(RADIO_END_SEQUENCE,4);*/
		}
	}
}