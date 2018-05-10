#include <Arduino.h>
#include "s6b.h"

int main() {
  delay(1000);
  Serial.begin(115200);
  Serial.println("Starting receiver...");
  S6B s6b;
  s6b.configureRF();
  Serial.println("Configured.");
  delay(1000);
  char REF[] = "Hi this is a test hello very long blah blah valbal test yada heh eh dfksa kdsaf lkdsf jdfsalka me anem fent vinga va somhi una mica mes de text random Hi this is a test hello very long blah blah valbal test yada heh eh dfksa kdsaf lkdsf jdfsalka me anem fent vinga va somhi una mica mes de text random Hi this is a test hello very long blah blah valbal test yada heh eh dfksa kdsaf lkdsf jdfsalka me anem fent vinga va somhi una mica mes de text random";
  char DATA[] = "Hi this is a test hello very long blah blah valbal test yada heh eh dfksa kdsaf lkdsf jdfsalka me anem fent vinga va somhi una mica mes de text random Hi this is a test hello very long blah blah valbal test yada heh eh dfksa kdsaf lkdsf jdfsalka me anem fent vinga va somhi una mica mes de text random Hi this is a test hello very long blah blah valbal test yada heh eh dfksa kdsaf lkdsf jdfsalka me anem fent vinga va somhi una mica mes de text random";
  uint32_t last = millis();
  int dropped = 0;
  int received = 0;
  int dt = (MAX_MSG_LENGTH+10)*8/500.*1000;
  while (true) {
    uint8_t rx = s6b.tryToRX(DATA, MAX_MSG_LENGTH);
    if (rx == 1 || rx == 3) {
      received++;
      uint32_t diff = millis()-last;
      last = millis();
      Serial.println("dt");
      Serial.println(diff);
      if (diff > (1900+dt)) dropped++;
      int byterr = 0;
      int biterr = 0;
      for (int i=0; i<MAX_MSG_LENGTH; i++) {
        if (REF[i] != DATA[i]) {
          byterr++;
        }
        uint8_t diff = REF[i] ^ DATA[i];
        for (int k=0; k<8; k++) {
          if (diff & 1) biterr++;
          diff >>= 1;
        }
      }
      Serial.print("Byte error rate: ");
      Serial.print(((float)byterr)/MAX_MSG_LENGTH*100);
      Serial.println("%");
      Serial.print("Bit error rate: ");
      Serial.print(((float)biterr)/(8*MAX_MSG_LENGTH)*100);
      Serial.println("%");
      Serial.print("Droppped: ");
      Serial.println(dropped);
      Serial.print("Received: ");
      Serial.println(received);
    }
  }
}
