// BELJ2434 FONH3001
#include "Particle.h"
#include "math.h"
SYSTEM_THREAD(ENABLED);
// IDLE : Waiting for message
// INITIATING_CONNECT : Attempting to figure out the baud rate
// SENDING : Sending bits on the line
// RECEIVING : Receiving bits on the line
int bit_rates[3] = {10, 10, 100};  //Number represents the delay between each bits
int rate_index = 0; //Index of previous table
enum ThreadState{
  IDLE,INITIATING_CONNECT, WAITING_FOR_REPLY, CONNECTED,
  RECEIVING_PREAMBULE,RECEIVING_START,RECEIVING_HEADER,RECEIVING_DATA,RECEIVING_CRC,RECEIVING_END,
};
Thread* rThread;
Thread* tThread;
unsigned int PinIn = D4;
unsigned int PinOut = D5;
int outBaudRate = bit_rates[rate_index]; // Symbol / s
int inBaudRate = -1; // Symbol / s

volatile ThreadState receiverThreadState = ThreadState::IDLE;
volatile ThreadState transmitterThreadState = ThreadState::INITIATING_CONNECT;

uint16_t getCRC16(uint8_t* data_p, uint8_t length){
    uint8_t x;
    uint8_t crc = 0xFFFF;

    while (length--){
        x = crc >> 8 ^ *data_p++;
        x ^= x>>4;
        crc = (crc << 8) ^ ((uint8_t)(x << 12)) ^ ((uint8_t)(x <<5)) ^ ((uint8_t)x);
    }
    return crc;
}

template<class T>
void sendBytes(T B, size_t amount) {
  char bit = 0;
  for (size_t i = 0; i < 8 * amount; i++) {
    T bit = (B >> ((8*amount)-i-1)) & 1;
    if(bit == 0){
      digitalWrite(PinOut,0);
    } else {
      digitalWrite(PinOut,1);
    }
    delay(outBaudRate); // Since we're using Manchester protocol , we have to send the bits twice
    if(bit == 0){
      digitalWrite(PinOut,1);
    } else {
      digitalWrite(PinOut,0);
    }
    delay(outBaudRate);
  }
}

void sendMessage(String s){
  uint8_t* characters = new uint8_t[s.length()];
  for(int i = 0; i < s.length();i++){
    characters[i] = s.charAt(i);
  }
  uint16_t CRC16 = getCRC16(characters,s.length());
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
  sendBytes(CRC16,2);
  // Send End
  sendBytes((uint8_t)(0x7E),1);
}

void setup() {
  // Put initialization like pinMode and begin functions here.
  Serial.begin(9600);
  pinMode(PinOut,OUTPUT);
  pinMode(PinIn,INPUT);
  digitalWrite(PinOut,1); // Keep the pin high until the transmission begins
  delay(2000);
  rThread = new Thread("receiverThread", receiverThread); // Start the receiverThread
  delay(1000);
  tThread = new Thread("transmitterThread", transmitterThread);
}

void loop() {
}

void transmitterThread(void){
  while(true){
    switch (transmitterThreadState)
    {
      case INITIATING_CONNECT:{
        WITH_LOCK(Serial)
        {
          Serial.println("TRANSMITTER : INITIATING CONNECT");
        }
        outBaudRate = bit_rates[rate_index];
        sendBytes((uint32_t)0x55555555, 4);
        transmitterThreadState = ThreadState::WAITING_FOR_REPLY;
        break;
      }
      case WAITING_FOR_REPLY:{
        WITH_LOCK(Serial)
        {
          Serial.println("TRANSMITTER : WAITING FOR REPLY");
        }
        bool connected = false;
        uint8_t currentValue = digitalRead(PinIn);
        for (int i = 0; i < 5000 && !connected; i++){
          if(digitalRead(PinIn) != currentValue){
            connected = true;
          }
          delay(outBaudRate);
        }
        delay(3000);
        if(connected){
          WITH_LOCK(Serial)
          {
            Serial.printlnf("TRANSMITTER : CONNECTED AT %d bits/s", 1000/bit_rates[rate_index]);
          }
          transmitterThreadState = ThreadState::CONNECTED;
        } else {
          WITH_LOCK(Serial)
          {
            Serial.printlnf("TRANSMITTER : FAILED CONNECTING AT %d bits/s", 1000/bit_rates[rate_index]);
          }
          transmitterThreadState = ThreadState::INITIATING_CONNECT;
          if (rate_index < 2){
            rate_index++;
          }
        }
        break;
      }
      case CONNECTED:{
        WITH_LOCK(Serial)
        {
          Serial.println("TRANSMITTER : SENDING MESSAGE");
        }
        String msg = "ALLO";
        sendMessage(msg);
        delay((8 * 54 * bit_rates[rate_index]) + (8*msg.length() * bit_rates[rate_index]) + 1000); // Delay for each bits sent by frame + message + extra 1000 ms
        break;
      }
    }
  }
}


volatile unsigned int previousBitReceived = 1;
volatile unsigned int newBitReceived = 0;
volatile int DataLength = 0;
uint8_t* arr;

template<class T>
void readBytes(T* B,size_t amount){
  for(size_t i = 0; i < 8*amount;i++){
    unsigned int newBitReceived_1 = digitalRead(PinIn);
    delay(inBaudRate);
    unsigned int newBitReceived_2 = digitalRead(PinIn);
    delay(inBaudRate);
    if(newBitReceived_1 == 0 && newBitReceived_2 == 1){
      *B = *B | (0 << (((8*amount)-1)-i));
    } else {
      *B = *B | (1 << (((8*amount)-1)-i));
    }
  }
}

void receiverThread(void){
  while(true) {
    switch(receiverThreadState)
    {
      case IDLE:
      {
        newBitReceived = digitalRead(PinIn);
        if(previousBitReceived == 1 && newBitReceived == 0){ // Someone is trying to communicate
          if(inBaudRate == -1){ // Try to figure out the baud rate by indentifying the preambule
            receiverThreadState = ThreadState::INITIATING_CONNECT;
          } else {
            receiverThreadState = ThreadState::RECEIVING_PREAMBULE; // Immideately jump to Preambule
            previousBitReceived = newBitReceived;
            break;
          }
        }
        previousBitReceived = newBitReceived;
        delayMicroseconds(100);
        break;
      }
      case INITIATING_CONNECT:
      {
        inBaudRate = bit_rates[rate_index];
        uint16_t bytes_read = 0;
        readBytes(&bytes_read, 2);
        WITH_LOCK(Serial)
        {
          Serial.println("RECEIVER : INITIATING CONNECT");
        }
        if (bytes_read == (uint16_t)(0x5555)){
          digitalWrite(PinOut,0);
          delay(inBaudRate*2);
          digitalWrite(PinOut,1);
        } else {
          WITH_LOCK(Serial)
          {
            Serial.println("RECEIVER : FAILED CONNECT, SLEEPING 3sec");
          }
          delay(3000);
          inBaudRate = -1;
        }
        receiverThreadState = ThreadState::IDLE;
        break;
      }
      case RECEIVING_PREAMBULE:
      {
        uint8_t premabuleBits = 0;
        readBytes(&premabuleBits,1); // Read the next 8 bits to see if it matches the preambule
        WITH_LOCK(Serial)
        {
          Serial.printlnf("Preambule : %X,  should be : %X",premabuleBits,0x55);
        }
        receiverThreadState = ThreadState::RECEIVING_START;
        break;
      }
      case RECEIVING_START:
      {
        uint8_t startBits = 0;
        readBytes(&startBits,1); // Read the next 8 bits to see if it matches the start
        WITH_LOCK(Serial)
        {
          Serial.printlnf("Start : %X,  should be : %X",startBits,0X7E);
        }
        receiverThreadState = ThreadState::RECEIVING_HEADER;
        break;
      }
      case RECEIVING_HEADER:
      {
        uint8_t TypeFlagBits = 0;
        uint8_t MessageLength = 0;
        readBytes(&TypeFlagBits,1);
        readBytes(&MessageLength,1);
        DataLength = MessageLength;
        WITH_LOCK(Serial)
        {
          Serial.printlnf("TypeFlag : %X",TypeFlagBits);
          Serial.printlnf("Length : %d",MessageLength);
        }
        receiverThreadState = ThreadState::RECEIVING_DATA;
        break;
      }
      case RECEIVING_DATA:
      { 
        arr = new uint8_t[DataLength]{0};
        for(int i = 0; i < DataLength;i++){
          readBytes(&arr[i],1);
        }
        WITH_LOCK(Serial)
        {
          Serial.print("Message : ");
          for(int i = 0; i < DataLength;i++){
              Serial.printf("%c",arr[i]);
          }
          Serial.println();
        }
      }
      case RECEIVING_CRC:
      {
        uint16_t CRCBits = 0;
        readBytes(&CRCBits,2);
        uint16_t CRCCalculated = getCRC16(arr,DataLength);
        WITH_LOCK(Serial)
        {
          Serial.printlnf("CRC : %X,  should be : %X",CRCBits,CRCCalculated);
        }
        delete[] arr;
        arr = nullptr;
        receiverThreadState = ThreadState::RECEIVING_END;
        break;
      }
      case RECEIVING_END:
      {
        uint8_t EndBits = 0;
        readBytes(&EndBits,1);
        WITH_LOCK(Serial)
        {
          Serial.printlnf("END : %X,  should be : %X\n",EndBits,0x7E);
        }
        receiverThreadState = ThreadState::IDLE;
        break;
      }
    }
  }
}