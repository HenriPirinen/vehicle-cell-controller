#include <SoftwareSerial.h>
#include "Arduino.h"

int cellVoltage[41];
int cellTemperature[41];
byte firstGroupIndex = 0;

byte DCDC1 = A4;
byte DCDC2 = A3;
byte DCDC3 = A2;
byte DCDC4 = A1;
byte DCDC5 = A0;

byte serialLed1 = 12;
byte analogiaIn2 = A7;
byte analogiaIn1 = A6;
byte serialLed2 = A5;
byte serialLed3 = 13;

byte serialOut1 = 2;
byte serialOut2 = 4;
byte serialOut3 = 6;
byte serialOut4 = 8;
byte serialOut5 = 10;

byte serialIn1 = 3;
byte serialIn2 = 5;
byte serialIn3 = 7;
byte serialIn4 = 9;
byte serialIn5 = 11;

boolean balanceStatus1 = false;
boolean balanceStatus2 = false;
boolean balanceStatus3 = false;
boolean balanceStatus4 = false;
boolean balanceStatus5 = false;

int validate(const byte measurements[])
{
  int i = 16;
  while (--i > 1 && measurements[i] == measurements[1]);
  return i != 1;
}

void handleGroup(byte nextSerOut, byte serIn, byte serOut, byte startIdx, byte endIdx, byte groupNum) {
  unsigned long startTime = millis();
  digitalWrite(nextSerOut, HIGH);
  SoftwareSerial groupSerial(serIn, serOut, false); //RX, TX
  groupSerial.begin(1200);

  groupSerial.write(165);      //hexA5
  delay(20);
  groupSerial.write(165);
  delay(20);
  groupSerial.write(165);
  delay(20);
  groupSerial.write(13);
  delay(20);
  groupSerial.write(254);       //hexFE
  delay(20);

  digitalWrite(serOut, LOW);

  byte index = 1;
  byte inByte;
  byte inBytes[14];

  for (int i = 1; i < 6; i++) {
    while (groupSerial.available() < 1) { //Wait for group response
      if (millis() - startTime > 5000) {
        break;
      }
    }

    inByte = groupSerial.read();
    if (inByte == 13) {
      i = 6; //Found start
    }
  }
  for (int i = 1; i < 18; i++, index++) {
    while (groupSerial.available() < 1) {
      if (millis() - startTime > 5000) {
        break;
      }
    }
    inByte = groupSerial.read();
    inBytes[index] = inByte;
    if (inByte == 254) { //Found last bit, stop reading.
      i = 18;
    }
  }

  if (validate(inBytes)) {
    for (int i = startIdx, inIdx = 1; i <= endIdx; i++, inIdx += 2) {
      cellVoltage[i] = inBytes[inIdx];
      cellTemperature[i] = inBytes[inIdx + 1];
    }

    String measurements[2]; //[0] = voltage, [1] = temperature
    String data; //Measuremets formated to JSON
    measurements[0].concat("\"voltage\":[");
    measurements[1].concat("\"temperature\":[");
    
    for (int i = startIdx; i <= endIdx; i++) {
      char volt[7];
      char temp[7];
      dtostrf(((cellVoltage[i] * 2.00) / 100.00), 4, 2, volt);
      dtostrf(cellTemperature[i] + (180.00 - cellVoltage[i]), 4, 2, temp);
      measurements[0] += volt;
      measurements[1] += temp;
      if (i < endIdx) measurements[0].concat(",");
      if (i < endIdx) measurements[1].concat(",");
    }
    measurements[0].concat("],");
    measurements[1].concat("]");
    data.concat("{\"Group\":" + String(groupNum) + ",\"type\":\"data\"," + measurements[0] + measurements[1] + "}");
    Serial.println(data);
  } else {
    String _message = "{\"origin\":\"Controller\",\"type\":\"log\",\"msg\":\"Group " + String(groupNum) + " out of sync\",\"importance\":\"High\"}";
    Serial.println(_message);
  }
}

int main(void) {
  init();

  Serial.begin(9600);
  pinMode(serialOut1, OUTPUT);
  pinMode(serialIn1, INPUT);
  pinMode(serialOut2, OUTPUT);
  pinMode(serialIn2, INPUT);
  pinMode(serialOut3, OUTPUT);
  pinMode(serialIn3, INPUT);
  pinMode(serialOut4, OUTPUT);
  pinMode(serialIn4, INPUT);
  pinMode(serialOut5, OUTPUT);
  pinMode(serialIn5, INPUT);

  pinMode(DCDC1, OUTPUT);
  pinMode(DCDC2, OUTPUT);
  pinMode(DCDC3, OUTPUT);
  pinMode(DCDC4, OUTPUT);
  pinMode(DCDC5, OUTPUT);

  pinMode(serialLed1, OUTPUT);
  pinMode(serialLed2, OUTPUT);
  pinMode(serialLed3, OUTPUT);

  digitalWrite(serialOut1, HIGH);
  digitalWrite(serialOut2, HIGH);
  digitalWrite(serialOut3, HIGH);
  digitalWrite(serialOut4, HIGH);
  digitalWrite(serialOut5, HIGH);

  digitalWrite(DCDC1, LOW);
  digitalWrite(DCDC2, LOW);
  digitalWrite(DCDC3, LOW);
  digitalWrite(DCDC4, LOW);
  digitalWrite(DCDC5, LOW);

  digitalWrite(serialLed1, HIGH);
  digitalWrite(serialLed2, HIGH);
  digitalWrite(serialLed3, HIGH);

  //Request settings from servers
  unsigned long startTime = millis();
  byte requestIndex = 1;
  Serial.println("$init");
  do {
    digitalWrite(13, HIGH); //Wait for response
    if (millis() - startTime > 5000 * requestIndex) {
      Serial.println("$init");
      requestIndex++;
    }
  } while (Serial.available() == 0 && requestIndex <= 5); //Wait for response, if server does not respond => Use default settings
  digitalWrite(13, LOW);
  if (Serial.available() > 0) {
    firstGroupIndex = Serial.parseInt();
  }

  digitalWrite(serialOut1, HIGH);
  delay(1000);

  while (true) {

    digitalWrite(serialLed1, HIGH);
    digitalWrite(serialLed2, LOW);
    digitalWrite(serialLed3, LOW);
    handleGroup(serialOut2, serialIn1, serialOut1, 1, 8, firstGroupIndex);
    delay(50);
    digitalWrite(serialLed1, LOW);
    digitalWrite(serialLed2, LOW);
    digitalWrite(serialLed3, LOW);

    digitalWrite(serialLed1, LOW);
    digitalWrite(serialLed2, HIGH);
    digitalWrite(serialLed3, LOW);
    handleGroup(serialOut3, serialIn2, serialOut2, 9, 16, (firstGroupIndex + 1));
    delay(50);
    digitalWrite(serialLed1, LOW);
    digitalWrite(serialLed2, LOW);
    digitalWrite(serialLed3, LOW);

    digitalWrite(serialLed1, HIGH);
    digitalWrite(serialLed2, HIGH);
    digitalWrite(serialLed3, LOW);
    handleGroup(serialOut4, serialIn3, serialOut3, 17, 24, (firstGroupIndex + 2));
    delay(50);
    digitalWrite(serialLed1, LOW);
    digitalWrite(serialLed2, LOW);
    digitalWrite(serialLed3, LOW);

    digitalWrite(serialLed1, LOW);
    digitalWrite(serialLed2, LOW);
    digitalWrite(serialLed3, HIGH);
    handleGroup(serialOut5, serialIn4, serialOut4, 25, 32, (firstGroupIndex + 3));
    delay(50);
    digitalWrite(serialLed1, LOW);
    digitalWrite(serialLed2, LOW);
    digitalWrite(serialLed3, LOW);

    digitalWrite(serialLed1, HIGH);
    digitalWrite(serialLed2, LOW);
    digitalWrite(serialLed3, HIGH);
    handleGroup(serialOut1, serialIn5, serialOut5, 33, 40, (firstGroupIndex + 4));
    delay(50);
    digitalWrite(serialLed1, LOW);
    digitalWrite(serialLed2, LOW);
    digitalWrite(serialLed3, LOW);
  }
}

