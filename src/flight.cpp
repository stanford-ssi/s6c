#include <Arduino.h>
#include "s6b.h"

int main() {
  delay(1000);
  Serial.begin(115200);
  Serial.println("Starting...");
  S6B s6b;
  s6b.configureRF();
  Serial.println("Configured!!!!!");
  delay(1000);
  char DATA[] = "Hi this is a test hello very long blah blah valbal test yada heh eh dfksa kdsaf lkdsf jdfsalka me anem fent vinga va somhi una mica mes de text random Hi this is a test hello very long blah blah valbal test yada heh eh dfksa kdsaf lkdsf jdfsalka me anem fent vinga va somhi una mica mes de text random Hi this is a test hello very long blah blah valbal test yada heh eh dfksa kdsaf lkdsf jdfsalka me anem fent vinga va somhi una mica mes de text random";
  while (true) {
    Serial.println("Sending");
    uint32_t t0 = micros();
    s6b.encode_and_transmit(DATA, MAX_MSG_LENGTH);
    Serial.println(((float)(micros()-t0))/1000.);
    Serial.println(s6b.getRSSI(), DEC);
    Serial.println("Sent");
    delay(1000);
  }
}
