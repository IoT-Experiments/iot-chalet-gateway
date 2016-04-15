#include <Arduino.h>
#include <math.h>

#include <RF24.h>
#include <SPI.h>

#define LED_S1_BLUE A0
#define LED_S1_GREEN 5
#define LED_S1_RED 6

#define LED_S2_BLUE A1
#define LED_S2_GREEN 9
#define LED_S2_RED 10

#define MAX_SENSORS 2
#define LED_MSG_RECEIVED_BLINK_DELAY 75
#define LED_MSG_RECEIVED_BLINK_COUNT 3
#define LED_POWER_COEFF 0.1f

/////////

typedef struct {
  unsigned long lastBlinkEnd;
  unsigned short blinkLeft;
  bool active;
  int pin;
} LedStatus;

typedef struct {
  uint8_t address[5];
  int currentValue;
  int minValue;
  int maxValue;
  LedStatus ledStatus;
  int pinRed;
  int pinGreen;
} SensorNode;

//////////

void setMessageReceived(SensorNode &sensor);
void saveValue(SensorNode &sensor, int value);
void processLeds(unsigned long currentTime);
void processLedMessageReceived(unsigned long currentTime);
void processLedColor(unsigned long currentTime);

//////////

RF24 radio(7, 8);

uint8_t gatewayAddress[] = { 0x00, 0xA1, 0xB2, 0xC3, 0xD4 };

byte buffer[32];

SensorNode sensorNodes[MAX_SENSORS];

void setup() {
  Serial.begin(115200);

  // Analog output
  pinMode(LED_S1_RED, OUTPUT);
  pinMode(LED_S1_GREEN, OUTPUT);
  pinMode(LED_S1_BLUE, OUTPUT);
  pinMode(LED_S2_RED, OUTPUT);
  pinMode(LED_S2_GREEN, OUTPUT);
  pinMode(LED_S2_BLUE, OUTPUT);

  // Setup and configure radio
  radio.begin();
  radio.enableDynamicPayloads();
  radio.setDataRate(RF24_250KBPS);
  radio.setPALevel(RF24_PA_HIGH);
  radio.setAutoAck(true);

  radio.openReadingPipe(1, gatewayAddress);
  radio.startListening();

  //radio.printDetails();

  sensorNodes[0].ledStatus.pin = LED_S1_BLUE;
  sensorNodes[0].pinRed = LED_S1_RED;
  sensorNodes[0].pinGreen = LED_S1_GREEN;
  sensorNodes[1].ledStatus.pin = LED_S2_BLUE;
  sensorNodes[1].pinRed = LED_S1_RED;
  sensorNodes[1].pinGreen = LED_S2_GREEN;
}

void loop() {
  byte pipeNo;
  while(radio.available(&pipeNo)) {
    int payloadSize = radio.getDynamicPayloadSize();
    if(payloadSize < 1) {
      Serial.println("Corrupted payload");
      return;
    }
    radio.read( &buffer, sizeof(buffer) );

    // TODO : gérer l'adressage et le buffer
    saveValue(sensorNodes[0], buffer[0]);
    setMessageReceived(sensorNodes[0]);
  }

  const unsigned long currentTime = millis();
  processLeds(currentTime);
}

void processLeds(unsigned long currentTime) {
  processLedMessageReceived(currentTime);
  processLedColor(currentTime);
}

void processLedMessageReceived(unsigned long currentTime) {
  for(unsigned int i = 0; i < MAX_SENSORS; i++) {
    if(sensorNodes[i].ledStatus.blinkLeft > 0) {
      unsigned long cycleTime = currentTime - sensorNodes[i].ledStatus.lastBlinkEnd;

      if (cycleTime < LED_MSG_RECEIVED_BLINK_DELAY) {
        sensorNodes[i].ledStatus.active = true;
      } else if(sensorNodes[i].ledStatus.active) {
        sensorNodes[i].ledStatus.blinkLeft -= 1;
        sensorNodes[i].ledStatus.active = false;
        sensorNodes[i].ledStatus.lastBlinkEnd = currentTime + LED_MSG_RECEIVED_BLINK_DELAY;
      }
      digitalWrite(sensorNodes[i].ledStatus.pin, sensorNodes[i].ledStatus.active ? HIGH : LOW);
    }
  }
}

void processLedColor(unsigned long currentTime) {
  for(unsigned int i = 0; i < MAX_SENSORS; i++) {
    if(sensorNodes[i].currentValue != 0 && (sensorNodes[i].maxValue - sensorNodes[i].minValue) > 0) {
      int currentPercent = 100 * (sensorNodes[i].currentValue - sensorNodes[i].minValue) / (sensorNodes[i].maxValue - sensorNodes[i].minValue);
      int redValue = (255 * currentPercent) / 100;
      int greenValue = (255 * (100 - currentPercent)) / 100;

      analogWrite(sensorNodes[i].pinRed, (int)round(redValue * LED_POWER_COEFF));
      analogWrite(sensorNodes[i].pinGreen, (int)round(greenValue * LED_POWER_COEFF));
    }
  }
}

void setMessageReceived(SensorNode &sensor) {
  sensor.ledStatus.lastBlinkEnd = millis();
  sensor.ledStatus.blinkLeft = LED_MSG_RECEIVED_BLINK_COUNT;
  sensor.ledStatus.active = false;
}

void saveValue(SensorNode &sensor, int value) {
  Serial.print(F("Got value "));
  Serial.println(value);

  if(value == 0) {
    return;
  }

  sensor.currentValue = value;
  if(value > sensor.maxValue) {
    sensor.maxValue = value;
  }
  if(value < sensor.minValue) {
    sensor.minValue = value;
  }
}
