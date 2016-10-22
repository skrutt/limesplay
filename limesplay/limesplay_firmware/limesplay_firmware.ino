/*
  LiquidCrystal Library - Serial Input
 
 Demonstrates the use a 16x2 LCD display.  The LiquidCrystal
 library works with all LCD displays that are compatible with the 
 Hitachi HD44780 driver. There are many of them out there, and you
 can usually tell them by the 16-pin interface.
 
 This sketch displays text sent over the serial port 
 (e.g. from the Serial Monitor) on an attached LCD.
 
 The circuit:
 * LCD RS pin to digital pin 12
 * LCD Enable pin to digital pin 11
 * LCD D4 pin to digital pin 5
 * LCD D5 pin to digital pin 4
 * LCD D6 pin to digital pin 3
 * LCD D7 pin to digital pin 2
 * LCD R/W pin to ground
 * 10K resistor:
 * ends to +5V and ground
 * wiper to LCD VO pin (pin 3)
 
 Library originally added 18 Apr 2008
 by David A. Mellis
 library modified 5 Jul 2009
 by Limor Fried (http://www.ladyada.net)
 example added 9 Jul 2009
 by Tom Igoe 
 modified 22 Nov 2010
 by Tom Igoe
 
 This example code is in the public domain.
 
 http://arduino.cc/en/Tutorial/LiquidCrystalSerial
 */

// include the library code:
#include <LiquidCrystal.h>

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(7, 6, 5, 4, 3, 2);

void setup(){
    // set up the LCD's number of columns and rows: 
  lcd.begin(20, 2);
  // initialize the serial communications:
  Serial.begin(19200);
}

#define BUFFER_SIZE 40
#define REFRESH_WRITES 15

void loop()
{
    static int count = 0, screen_refresh = 0;
    static int buffer[BUFFER_SIZE];
    
    
    
    // when characters arrive over the serial port...
    int got_char = Serial.read();
    if(got_char > 0)  //Got something, handle
    {
        if(got_char == 1)
        {
            screen_refresh++;
            if(screen_refresh > REFRESH_WRITES)
            {
                //Reset screen if its gone wonky
                lcd.begin(20, 2);
                screen_refresh = 0;
            }else{
              lcd.setCursor(0,0);
            }
            
            for(int i = 0; i < 40;i++)
            {
                lcd.write(buffer[i]);
              
                if(i == 19)
                {
                    //end of first line! go to next
                    lcd.setCursor(0,1);
                }
            }
            
            count = 0;  
        }else{
         
            if(count < BUFFER_SIZE)
            {
                buffer[count] = got_char; // set into buffer  
            }
            count++;
            
        } 
    }
}
