#include <SoftwareSerial.h>
#include "Arduino.h"

const byte pinIO[3][5] = { //pinIO[0][x] = serOutput, pinIO[1][x] = serInput, pinIO[3][x] = balancePins
  {2, 4, 6, 8, 10},
  {3, 5, 7, 9, 11},
  {A4, A3, A2, A1, A0}
};
bool isCharging = false;
bool isBalancing = false;
float voltageLimit = 3.70;
int cellVoltage[41];
int cellTemperature[41];
byte firstGroupIndex = 0;

int validate(const byte measurements[])
{
  int i = 16;
  while (--i > 1 && measurements[i] == measurements[1]);
  return i != 1;
}

void serIndicator(byte number) { //Blink serial debugging leds. Input number between 0 - 7.
  const byte serLed[] = {12, A5, 13}; //Debugging leds.
  for (int i = 0; i <= 2; i++) {
    digitalWrite(serLed[i], ((number >> i) & 0x01) == 1 ? HIGH : LOW);
  }
}

void handleGroup(byte nextSerOut, byte serIn, byte serOut, byte startIdx, byte endIdx, byte groupNum) {
  unsigned long startTime = millis();
  const byte balancePins[] = {A4, A3, A2, A1, A0};

  digitalWrite(nextSerOut, HIGH);
  SoftwareSerial groupSerial(serIn, serOut, false); //RX, TX
  groupSerial.begin(1200);

  groupSerial.write(5);      //hexA5
  delay(20);
  groupSerial.write(5);
  delay(20);
  groupSerial.write(5);
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

      float fVolt = (float)atof(volt);
      if (fVolt >= voltageLimit && !isBalancing && isCharging) {
        Serial.println("$!serialCharge");
        do {} while (Serial.readString() != "$B1"); //Wait for response
        isBalancing = true;
        for (int b = 0; b <= 4; b++) {
          digitalWrite(balancePins[b], HIGH);
          delay(2000);
        }
      }

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

void readSerialInput() {
  /**
     1XY = toggle balancer on/off X = Index, Y = State.
     110 = Set 2nd balancer LOW.
     131 = Set 4th balancer HIGH.

     CY0 = Vehicle charging status. Y 0 = Off, 1 = On.
  */
  byte balancePins[] = {A4, A3, A2, A1, A0};
  char command[3];
  for (int i = 0; Serial.available() > 0 && i < 3; i++) {
    command[i] = Serial.read();
  }
  if (command[0] == '1') {
    String state = String(command[1] - 48) + String(command[2] == '1' ? 1 : 0);
    String _confirm = "{\"origin\":\"Controller\",\"type\":\"param\",\"name\":\"balanceStatus\",\"value\":\"" + state + "\",\"importance\":\"Low\"}";
    digitalWrite(balancePins[(int)command[1] - 48], command[2] == '1' ? HIGH : LOW);
    Serial.println(_confirm);
  } else if (command[0] == 'C') {
    if (command[1] == '1') { //Vehicle is in charging state, monitor cell voltage. Set relays to 0
      isCharging = true;
      isBalancing = false;
      for (int i = 0; i <= 4; i++) {
        digitalWrite(balancePins[i], LOW);
      }
    } else {
      isCharging = false;
      isBalancing = false;
      for (int i = 0; i <= 4; i++) {
        digitalWrite(balancePins[i], LOW);
      }
    }
  } else if (command[0] == '$' && command[1] == 'B' && command[2] == '1') {
    isBalancing = true;
    for (int b = 0; b <= 4; b++) {
      digitalWrite(balancePins[b], HIGH);
      delay(2000);
    }
  }
}

int main(void) {
  init();
  Serial.begin(9600);
  for (int row = 0; row <= 2; row++) { //Set pins to OUTPUT / INPUT. Set OUTPUT pins to LOW state.
    for (int column = 0; column <= 4; column++) {
      pinMode(pinIO[row][column], row == 0 || 2 ? OUTPUT : INPUT); //Row 0 = serialOut, Row = 2 balancePins, Row 1 = serialInput
      if(row == 0 || 2) digitalWrite(pinIO[row][column], LOW);
    }
  }
  pinMode(12, OUTPUT); //Serial indicators
  pinMode(A5, OUTPUT);
  pinMode(13, OUTPUT);
  serIndicator(7);
  /**
     Request settings from the API
  */
  unsigned long startTime = millis();
  byte requestIndex = 1;
  Serial.println("$init ");
  do {
    digitalWrite(13, HIGH); //Wait for response
    if (millis() - startTime > 5000 * requestIndex) {
      Serial.println("$init");
      requestIndex++;
    }
  } while (Serial.available() == 0 && requestIndex <= 5); //Wait for response, if API does not respond => Use default settings
  digitalWrite(13, LOW);
  if (Serial.available() > 0) { //API reponse
    String response = Serial.readString();
    char *arguments;
    char *sstring = response.c_str();
    arguments = strtok(sstring, ",");
    int loopCount = 0;
    while (arguments != NULL) {
      if (loopCount == 0) {
        firstGroupIndex = String(arguments).toInt();
      } else if (loopCount == 1) {
        voltageLimit = atof(arguments);
      }
      arguments = strtok(NULL, ",");
      loopCount++;
    }
  }

  digitalWrite(pinIO[0][0], HIGH);
  delay(1000);

  while (true) { //Main loop
    serIndicator(1);
    handleGroup(pinIO[0][1], pinIO[1][0], pinIO[0][0], 1, 8, firstGroupIndex);
    readSerialInput();
    delay(50);
    serIndicator(2);
    handleGroup(pinIO[0][2], pinIO[1][1], pinIO[0][1], 9, 16, (firstGroupIndex + 1));
    readSerialInput();
    delay(50);
    serIndicator(3);
    handleGroup(pinIO[0][3], pinIO[1][2], pinIO[0][2], 17, 24, (firstGroupIndex + 2));
    readSerialInput();
    delay(50);
    serIndicator(4);
    handleGroup(pinIO[0][4], pinIO[1][3], pinIO[0][3], 25, 32, (firstGroupIndex + 3));
    readSerialInput();
    delay(50);
    serIndicator(5);
    handleGroup(pinIO[0][0], pinIO[1][4], pinIO[0][4], 33, 40, (firstGroupIndex + 4));
    readSerialInput();
    delay(50);
    serIndicator(0);
  }
}
