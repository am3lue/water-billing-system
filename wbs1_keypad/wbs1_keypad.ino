/*
 * Ultrasonic Simple
 * Prints the distance read by an ultrasonic sensor in
 * centimeters. They are supported to four pins ultrasound
 * sensors (liek HC-SC04) and three pins (like PING)))
 * and Seeed Studio sensors).
 *
 * The circuit:
 * * Module HR-SC04 (four pins) or PING))) (and other with
 *   three pins), attached to digital pins as follows:
 * ---------------------    --------------------
 * | HC-SC04 | Arduino |    | 3 pins | Arduino |
 * ---------------------    --------------------
 * |   Vcc   |   5V    |    |   Vcc  |   5V    |
 * |   Trig  |   5     | OR |   SIG  |   6     |
 * |   Echo  |   6     |    |   Gnd  |   GND   |
 * |   Gnd   |   GND   |    --------------------
 * ---------------------
 * Note: You do not obligatorily need to use the pins defined above
 * 
 * By default, the distance returned by the read()
 * method is in centimeters. To get the distance in inches,
 * pass INC as a parameter.
 * Example: ultrasonic.read(INC)
 *
 * created 3 Apr 2014
 *
 * by Erick Simões (github: @am3lue | twitter: @am3lue)
 *
 * This example code is released into the MIT License.
 */

#include <Ultrasonic.h>

/*
 * Pass as a parameter the trigger and echo pin, respectively,
 * or only the signal pin (for sensors 3 pins), like:
 * Ultrasonic ultrasonic(13);
 */
Ultrasonic ultrasonic(5, 6);
int distance;
float percentvol;
float volume;

//Initializing The Libraries of lcd
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 4);


void setup() {

  //Set Up for the lcd
  lcd.init();
  lcd.backlight();
  lcd.clear();

  Serial.begin(9600);
}

void loop() {
  // Pass INC as a parameter to get the distance in inches
  distance = ultrasonic.read();

  // calculations for Volume of the vessel
  volume =2258211.45 - distance*14379.944;// The area is in m hence distance * 100 

  percentvol = (volume/2258211.45)*100 + 10;
  if (volume >= 0){
  Serial.print("volume in cubic cm: ");
  Serial.println(volume/100, 3);
  Serial.print("%: ");
  Serial.println(percentvol,0);
  delay(1000);

  if (percentvol > 75){
    print("Maji yanatosha");
  }
  else if (percentvol <= 75){
    print("maji Hayatoshi");
  }
  }
}

void print(String input){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Arusha Science");
  lcd.setCursor(0,1);
  lcd.print("Maji Smart");
  lcd.setCursor(0,2);
  lcd.print("Asilimia baki: ");
  lcd.setCursor(14, 2);
  lcd.print(percentvol,0);
  lcd.print(" %");
  lcd.setCursor(5,3);
  lcd.print(input);
}
