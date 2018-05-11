#include <Arduino.h>
#include "s6b.h"
#include "RadioInterface.h"

vb_rf_message message;
bool parsing = true;
uint8_t parse_pos = 0;
uint8_t last_bytes[4] = {0};
uint8_t frame_size;
uint8_t latest_frame[256];

void parse_radio_command() {
	Serial.print(" >");
	int sz = message.data[0];
	Serial.print("Got ");
	Serial.println(sz);
	memcpy(latest_frame, message.data+1, sz);
	frame_size = sz;
}

void receive_byte() {
	if (Serial1.available() > 0) {
		uint8_t byte = Serial1.read();
		last_bytes[0] = last_bytes[1];
		last_bytes[1] = last_bytes[2];
		last_bytes[2] = last_bytes[3];
		last_bytes[3] = byte;
		if (*(uint32_t*)RADIO_START_SEQUENCE == *(uint32_t*)last_bytes) {
			parsing = true;
			parse_pos = 0;
			memset(&message, 0, sizeof(vb_rf_message));
			return;
		}
		if (*(uint32_t*)RADIO_END_SEQUENCE == *(uint32_t*)last_bytes) {
			parsing = false;
			((uint8_t*)&message)[parse_pos] = 0;
			((uint8_t*)&message)[parse_pos-1] = 0;
			((uint8_t*)&message)[parse_pos-2] = 0;
			((uint8_t*)&message)[parse_pos-3] = 0;
			parse_radio_command();
		}
		if (parsing && parse_pos < sizeof(vb_rf_message)) {
			((uint8_t*)&message)[parse_pos] = byte;
			parse_pos++;
		}
	}
}

int main() {
	delay(1000);
	Serial.begin(115200);
	Serial.println("Starting...");
	Serial1.begin(115200);
	S6B s6b;
	s6b.configureRF();
	Serial.println("Configured!!!!!");
	delay(1000);
	char DATA[] = "Desperta ferro! Desperta ferro! Sant Jordi! Sant Jordi! Arago! Arago!";

	uint32_t radioTimer = millis();
	while (true) {
		if (millis() >= radioTimer) {
			radioTimer = millis() + 1000;
			Serial.println("Sending");
			uint32_t t0 = micros();
			s6b.encode_and_transmit(latest_frame, MAX_MSG_LENGTH);
			Serial.println(((float)(micros()-t0))/1000.);
		} else {
			receive_byte();
		}
	}
}
