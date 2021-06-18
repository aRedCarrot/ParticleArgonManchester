// BELJ2434 FONH3001
#include "Particle.h"
#include "math.h"
SYSTEM_THREAD(ENABLED);

unsigned int transmitterOut = D4;
int outBaudRate = (int)(1000/100); // Symbol / s

template<class T>
void sendBytes(T B, size_t amount) {
  char bit = 0;
  for (size_t i = 0; i < 8 * amount; i++) {
    T bit = (B >> ((8*amount)-i-1)) & 1;
    if(bit == 0){
      digitalWrite(transmitterOut,0);
    } else {
      digitalWrite(transmitterOut,1);
    }
    delay(outBaudRate); // Since we're using Manchester protocol , we have to send the bits twice
    if(bit == 0){
      digitalWrite(transmitterOut,1);
    } else {
      digitalWrite(transmitterOut,0);
    }
    delay(outBaudRate);
  }
}

void sendMessage(String s){
  Serial.println("BEGIN SENDING");
  // Send preambule
  sendBytes((uint8_t)(0x55),1);
  // Send start
  sendBytes((uint8_t)(0x7E),1);
  // Send header
  sendBytes((uint8_t)(0x00),1); // Flags
  sendBytes((uint8_t)(s.length()),1); // Length
  // Send message
  for(size_t i = 0; i < s.length();i++){
    char c = s.charAt(i);
    sendBytes(c,1);
  }
  // Send CRC16
  sendBytes((uint16_t)(0xFFFF),2);
  // Send End
  sendBytes((uint8_t)(0x7E),1);
}

void setup() {
  // Put initialization like pinMode and begin functions here.
  Serial.begin(9600);
  pinMode(transmitterOut,OUTPUT);
  digitalWrite(transmitterOut,1); // Keep the pin high until the transmission begins
  delay(2000);
}

void loop() {
    sendMessage("NNNNNNNNNNNNNNNNNNNNNNNNNNNB");
    delay(10000);
}