// BELJ2434
#include "Particle.h"
SYSTEM_THREAD(ENABLED);
// IDLE : Waiting for message
// INITIATING_CONNECT : Attempting to figure out the baud rate
// SENDING : Sending bits on the line
// RECEIVING : Receiving bits on the line
enum ThreadState{
  IDLE,INITIATING_CONNECT,
  SENDING_PREAMBULE,SENDING_START,SENDING_HEADER,SENDING_DATA,SENDING_CRC,SENDING_END,
  RECEIVING_PREAMBULE,RECEIVING_START,RECEIVING_HEADER,RECEIVING_DATA,RECEIVING_CRC,RECEIVING_END,
};
Thread* rThread;
unsigned int LoopbackOut = D4;
unsigned int LoopbackIn = D5;
int outBaudRate = 1;
int inBaudRate = 1;

volatile ThreadState receiverThreadState = ThreadState::IDLE;

void send(String s){
  // Manchester
  // 0 : low to high
  // 1 : high to low
  for(unsigned int i = 0; i < s.length();i++){
    char c = s.charAt(i);
    if(c == '0'){
      digitalWrite(LoopbackOut,0);
      Serial.println("Send : 0");
    } else {
      digitalWrite(LoopbackOut,1);
      Serial.println("Send : 1");
    }
    delay(1000/outBaudRate); // Since we're using Manchester protocol , we have to send the bits twice
    if(c == '0'){
      Serial.println("Send : 1");
      digitalWrite(LoopbackOut,1);
    } else {
      Serial.println("Send : 0");
      digitalWrite(LoopbackOut,0);
    }
    delay(1000/outBaudRate);
  }
}

void sendMessage(){
  // Send preambule
  send("01010101");
  // Send start
  // Send header
  // Send CRC16
  // Send End
}

void setup() {
  // Put initialization like pinMode and begin functions here.
  Serial.begin(9600);
  pinMode(LoopbackOut,OUTPUT);
  pinMode(LoopbackIn,INPUT);
  digitalWrite(LoopbackOut,1); // Keep the pin high until the transmission begins
  rThread = new Thread("receiverThread", receiverThread); // Start the receiverThread
}

int counter = 0;
void loop() {
  if(counter == 5){
    Serial.println("BEGIN SENDING");
    sendMessage();
  }
  counter++;
  delay(100);
}

volatile unsigned int previousBitReceived = 1;
volatile unsigned int newBitReceived = 0;
void receiverThread(void){
  while(true) {
    switch(receiverThreadState)
    {
      case IDLE:
        newBitReceived = digitalRead(LoopbackIn);
        if(previousBitReceived == 1 && newBitReceived == 0){ // Someone is trying to communicate
          if(inBaudRate == -1){ // Try to figure out the baud rate by indentifying the preambule
            receiverThreadState = ThreadState::INITIATING_CONNECT;
          } else {
            receiverThreadState = ThreadState::RECEIVING_PREAMBULE;
            previousBitReceived = newBitReceived;
          }
        }
        previousBitReceived = newBitReceived;
        break;
      case INITIATING_CONNECT:
        //newBitReceived = digitalRead(LoopbackIn);
        break;
      case RECEIVING_PREAMBULE:
        digitalRead(LoopbackIn);
        delay(1000/inBaudRate);
        uint8_t preambule[8] = {0};
        for(int i = 1; i < 8;i++){ // We start at 1 because we already got the 0 from the start of tranmission
          unsigned int newBitReceived_1 = digitalRead(LoopbackIn);
          delay(1000/inBaudRate);
          unsigned int newBitReceived_2 = digitalRead(LoopbackIn);
          delay(1000/inBaudRate);
          if(newBitReceived_1 == 0 && newBitReceived_2 == 1){
            preambule[i] == 0;
          } else {
            preambule[i] == 1;
          }
        }
        Serial.print("Received : ");
        for(int i = 0; i < 8;i++){
          Serial.print(preambule[i]);
        }
        Serial.println();
        receiverThreadState = ThreadState::IDLE;
        break;
    }
    delay(1000/inBaudRate);
  }
}