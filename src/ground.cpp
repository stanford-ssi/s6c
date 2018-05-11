#include <Arduino.h>
#include "s6b.h"
#include "RadioInterface.h"

typedef struct __attribute__((packed)) {
  uint8_t rssi = 0;
  uint32_t received = 0;
  uint32_t dropped = 0;
  uint8_t data[MAX_MSG_LENGTH];
} doot_doot;

int main() {
  delay(1000);
  Serial.begin(115200);
  Serial.println("Starting receiver...");
  S6B s6b;
  s6b.configureRF();
  Serial.println("Configured.");
  delay(1000);

  uint32_t last = millis();

  doot_doot frame;
  frame.dropped = 0;
  frame.received = 0;
  while (true) {
    uint8_t rx = s6b.tryToRX(frame.data, MAX_MSG_LENGTH);
    if (rx == 1 || rx == 3) {
      frame.received++;
      frame.rssi = s6b.getRSSI();
      uint32_t diff = millis()-last;
      last = millis();
      Serial.write(RADIO_START_SEQUENCE, 4);
      Serial.write((char*)&frame, sizeof(doot_doot));
      Serial.write(RADIO_END_SEQUENCE, 4);
      if (diff > 1100 && frame.received != 1) frame.dropped++;
      Serial.print("Droppped: ");
      Serial.println(frame.dropped);
      Serial.print("Received: ");
      Serial.println(frame.received);
    }
  }
}
