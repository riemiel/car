#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

// LCD setup (address 0x27, 16x2)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// LED pins
const int ledPins[] = {2,3,4,5,6,7,8,9,10,11,12,13};
const int numLeds = 12;

// Water sensor ANALOG pin
const int waterSensorPin = A0;

int resval = 0;  // holds sensor value

void setup() {

  // Initialize LCD
  lcd.init();
  lcd.backlight();

  // Initialize LEDs (always ON)
  for(int i = 0; i < numLeds; i++){
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], HIGH);
  }

  // Print title
  lcd.setCursor(0,0);
  lcd.print("FLOODED AREA");
}

void loop() {

  // Read analog value
  resval = analogRead(waterSensorPin);

  // Set cursor to second line
  lcd.setCursor(0,1);

  if (resval <= 100) {
    lcd.print("     SAFE       ");
  }
  else if (resval > 100 && resval <= 300) {
    lcd.print("    CAUTION      ");
  }
  else {
    lcd.print(" BE CAREFUL    ");
  }

  delay(1000);
}