// BELJ2434 FONH3001
#include "Particle.h"
#include "math.h"
SYSTEM_THREAD(ENABLED);
// IDLE : Waiting for message
// INITIATING_CONNECT : Attempting to figure out the baud rate
// SENDING : Sending bits on the line
// RECEIVING : Receiving bits on the line
enum ThreadState
{
  IDLE,
  INITIATING_CONNECT,
  SENDING_PREAMBULE,
  SENDING_START,
  SENDING_HEADER,
  SENDING_DATA,
  SENDING_CRC,
  SENDING_END,
  RECEIVING_PREAMBULE,
  RECEIVING_START,
  RECEIVING_HEADER,
  RECEIVING_DATA,
  RECEIVING_CRC,
  RECEIVING_END,
};

int inBaudRate = -1; // Symbol / s
const int transmissionIN = D5;
unsigned int previousBitReceived = 1;
unsigned int newBitReceived = 0;
int DataLength = 0;

template <class T>
void readBytes(T *B, size_t amount)
{
  for (size_t i = 0; i < 8 * amount; i++)
  {
    unsigned int newBitReceived_1 = digitalRead(transmissionIN);
    delayMicroseconds(inBaudRate);
    unsigned int newBitReceived_2 = digitalRead(transmissionIN);
    delayMicroseconds(inBaudRate);
    if (newBitReceived_1 == 0 && newBitReceived_2 == 1)
    {
      *B = *B | (0 << (((8 * amount) - 1) - i));
    }
    else
    {
      *B = *B | (1 << (((8 * amount) - 1) - i));
    }
  }
}

ThreadState receiverThreadState = ThreadState::IDLE;

void setup()
{
  // Put initialization like pinMode and begin functions here.
  Serial.begin(9600);
  pinMode(transmissionIN, INPUT);
  delay(2000);
}

void loop()
{
  switch (receiverThreadState)
  {
    case IDLE:
    {
      newBitReceived = digitalRead(transmissionIN);
      if (previousBitReceived == 1 && newBitReceived == 0)
      { // Someone is trying to communicate
        if (inBaudRate == -1)
        { // Try to figure out the baud rate by indentifying the preambule
          receiverThreadState = ThreadState::INITIATING_CONNECT;
        }
        else
        {
          receiverThreadState = ThreadState::RECEIVING_PREAMBULE; // Immideately jump to Preambule
          previousBitReceived = newBitReceived;
          break;
        }
      }
      previousBitReceived = newBitReceived;
      delay(1);
      break;
    }
    case INITIATING_CONNECT:
    {
      unsigned long duration = pulseIn(transmissionIN,LOW);
      inBaudRate = duration;
      receiverThreadState = ThreadState::IDLE;
      break;
    }
    case RECEIVING_PREAMBULE:
    {
      uint8_t premabuleBits = 0;
      readBytes(&premabuleBits, 1); // Read the next 8 bits to see if it matches the preambule
      WITH_LOCK(Serial)
      {
        Serial.printlnf("Preambule : %X,  should be : %X", premabuleBits, 0x55);
      }
      receiverThreadState = ThreadState::RECEIVING_START;
      break;
    }
    case RECEIVING_START:
    {
      uint8_t startBits = 0;
      readBytes(&startBits, 1); // Read the next 8 bits to see if it matches the start
      WITH_LOCK(Serial)
      {
        Serial.printlnf("Start : %X,  should be : %X", startBits, 0X7E);
      }
      receiverThreadState = ThreadState::RECEIVING_HEADER;
      break;
    }
    case RECEIVING_HEADER:
    {
      uint8_t TypeFlagBits = 0;
      uint8_t MessageLength = 0;
      readBytes(&TypeFlagBits, 1);
      readBytes(&MessageLength, 1);
      DataLength = MessageLength;
      WITH_LOCK(Serial)
      {
        Serial.printlnf("TypeFlag : %X", TypeFlagBits);
        Serial.printlnf("Length : %d", MessageLength);
      }
      receiverThreadState = ThreadState::RECEIVING_DATA;
      break;
    }
    case RECEIVING_DATA:
    {
      uint8_t *arr = new uint8_t[DataLength]{0};
      for (int i = 0; i < DataLength; i++)
      {
        readBytes(&arr[i], 1);
      }
      WITH_LOCK(Serial)
      {
        Serial.print("Message : ");
        for (int i = 0; i < DataLength; i++)
        {
          Serial.printf("%c", arr[i]);
        }
        Serial.println();
      }
    }
    case RECEIVING_CRC:
    {
      uint16_t CRCBits = 0;
      readBytes(&CRCBits, 2);
      WITH_LOCK(Serial)
      {
        Serial.printlnf("CRC : %X,  should be : %X", CRCBits, 0xFFFF);
      }
      receiverThreadState = ThreadState::RECEIVING_END;
      break;
    }
    case RECEIVING_END:
    {
      uint8_t EndBits = 0;
      readBytes(&EndBits, 1);
      WITH_LOCK(Serial)
      {
        Serial.printlnf("END : %X,  should be : %X\n", EndBits, 0x7E);
      }
      receiverThreadState = ThreadState::IDLE;
      break;
    }
  }
}