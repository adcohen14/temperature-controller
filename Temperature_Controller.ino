/*
 * Temperature Controller
 *
 * Keeps the temperature of a cooling unit, such as a refridgerator or freezer
 * very close to a target temperature. Includes push-button controls for
 * adjusting the temperature up and down.
 *
 *
 * Copyright (c) 2012 David Hassler.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <LiquidCrystal.h>

/* Initialize LCD */
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

// which analog pin to connect
#define THERMISTORPIN A0
// resistance at 25 degrees C
#define THERMISTORNOMINAL 51000
// temp. for nominal resistance (almost always 25 C)
#define TEMPERATURENOMINAL 25
// how many samples to take and average, more takes longer
// but is more 'smooth'
#define NUMSAMPLES 10
// The beta coefficient of the thermistor (usually 3000-4000)
#define BCOEFFICIENT -3950
// the value of the 'other' resistor
#define SERIESRESISTOR 44000

// Target temperature control
#define DOWN_INPUT_PIN 6
#define UP_INPUT_PIN 7
#define FREEZER_CONTROL_PIN 1

// How long to wait before turning on the compressor again (in seconds)
#define MIN_COMPRESSOR_WAIT 600L

long secondsSinceLastCooling = MIN_COMPRESSOR_WAIT + 1;
int samples[NUMSAMPLES];
float targetTemp = 50.0;

// the setup routine runs once when you press reset:
void setup() {
  analogReference(EXTERNAL);

  // set up the LCD's number of columns and rows: 
  lcd.begin(16, 2);

  pinMode(UP_INPUT_PIN, INPUT);
  pinMode(DOWN_INPUT_PIN, INPUT);
  pinMode(FREEZER_CONTROL_PIN, OUTPUT);
  digitalWrite(FREEZER_CONTROL_PIN, LOW);
}

void loop() {
  checkButtonStatus();
  float temp = getTemperature();
  checkTemperature(temp);
  updateLCD(temp);
  //debugToSerial(temp);
  delay(1000);
}

boolean debounce(int pin) {
  boolean state;
  boolean previousState;

  previousState = digitalRead(pin);
  for(int counter=0; counter < 10; counter++) {
    delay(1);
    state = digitalRead(pin);
    if ( state != previousState) {
      counter = 0;
      previousState = state;
    }
  }

  if(state == HIGH)
    return true;
  else
    return false;
}

float getTemperature() {
  uint8_t i;
  float average;

  // take N samples in a row, with a slight delay
  for (i=0; i< NUMSAMPLES; i++) {
   samples[i] = analogRead(THERMISTORPIN);
   delay(10);
  }

  // average all the samples out
  average = 0;
  for (i=0; i< NUMSAMPLES; i++) {
     average += samples[i];
  }
  average /= NUMSAMPLES;

  // convert the value to resistance
  average = 1023 / average - 1;
  average = SERIESRESISTOR / average;

  float steinhart;
  steinhart = average / THERMISTORNOMINAL;     // (R/Ro)
  steinhart = log(steinhart);                  // ln(R/Ro)
  steinhart /= BCOEFFICIENT;                   // 1/B * ln(R/Ro)
  steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15); // + (1/To)
  steinhart = 1.0 / steinhart;                 // Invert
  steinhart -= 273.15;                         // convert to C
  steinhart = (steinhart * 9.0) / 5.0 + 32.0;  // convert to F
  return steinhart;
}

void debugToSerial(float temp) {
  float reading = analogRead(THERMISTORPIN);

  Serial.print("Analog reading "); 
  Serial.println(reading);

  // convert the value to resistance
  reading = (1023 / reading)  - 1;
  reading = SERIESRESISTOR / reading;
  Serial.print("Thermistor resistance "); 
  Serial.println(reading);

  Serial.print("Calculated temperature: ");
  Serial.println(temp);

  Serial.print("Target temperature: ");
  Serial.println(targetTemp);

  Serial.print("Seconds since last cooling: ");
  Serial.println(secondsSinceLastCooling);

  Serial.print("Freezer on? ");
    if(freezerCurrentlyOn()) {
    Serial.println("Yes");
  } else {
    Serial.println("No");
  }

  Serial.println();
}

void checkButtonStatus() {
    if (debounce(DOWN_INPUT_PIN)) {
    targetTemp = targetTemp - 1;
  }

  if (debounce(UP_INPUT_PIN)) {
    targetTemp = targetTemp + 1;
  }
}

boolean freezerCurrentlyOn() {
  return bitRead(PORTD,FREEZER_CONTROL_PIN) == HIGH;
}

void checkTemperature(float temp) {
  /*
  If the freezer is on, keep on until target - 1.5.
  If the freezer is off, turn it on when temp > target + 1
  */
  if (freezerCurrentlyOn()) {
    secondsSinceLastCooling = 0;

    if (temp < (targetTemp - 1.5)) {
      Serial.println("Turning freezer off");
      digitalWrite(FREEZER_CONTROL_PIN, LOW);
    }
  }
  else {
    secondsSinceLastCooling += 1;
    if (temp > (targetTemp + 1) && (secondsSinceLastCooling > MIN_COMPRESSOR_WAIT)) {
      Serial.println("Turning freezer on");
      digitalWrite(FREEZER_CONTROL_PIN, HIGH);
      delay(100);
      Serial.println(bitRead(PORTD,FREEZER_CONTROL_PIN));
    } else {
      Serial.println("Writing low to freezer pin");
      digitalWrite(FREEZER_CONTROL_PIN, LOW);
      delay(100);
      Serial.println(bitRead(PORTD,FREEZER_CONTROL_PIN));
    }
  }
}

void updateLCD(float temp) {  
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Target: ");
  lcd.print(targetTemp);
  lcd.setCursor(0,1);
  lcd.print("Temp: ");
  lcd.print(temp);
  lcd.print(" F");
  if (freezerCurrentlyOn()) { lcd.print(" *"); }
}
