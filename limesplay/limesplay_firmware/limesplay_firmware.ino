#include <max6675.h>

// include the library code:
#include <LiquidCrystal.h>

#define BUFFER_SIZE 40


#define screenDELAYMS 100
#define REFRESH_TIMEOUT 15000

#define NOT_CONNECTED_TIMEOUT 1000

#define RED_LED_PIN 10
#define GREEN_LED_PIN 11
#define BLUE_LED_PIN 9

#define LED_SINUS_STEPS 300
#define LED_MAX_BRIGHTNESS 255

#define thermoCLK 14
#define thermoCS 15
#define thermoDO 16
#define thermoDELAYMS 250
#define thermoOFFSET 4.5
#define thermofilterconstant 50
#define thermofilterconstantLOW (1.0/thermofilterconstant)
#define thermofilterconstantHIGH (1.0-thermofilterconstantLOW)

typedef struct 
{
  uint8_t red;
  uint8_t green;
  uint8_t blue;
} led_color_t;

const led_color_t offline_leds = {.red = 0, .green = 255, .blue = 255};
const led_color_t online_leds = {.red = 255, .green = 0, .blue = 255};
static led_color_t current_led_color = {.red = 0, .green = 0, .blue = 255};

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(7, 6, 5, 4, 3, 2);

//Thermocouple
MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO); 
double current_temp = 20;


//Screen reset function
void screen_reset()
{
   //Reset screen if its gone wonky
   lcd.begin(20, 2);
   // make a cute degree symbol
   uint8_t degree[8]  = {140,146,146,140,128,128,128,128};
   lcd.createChar(0, degree);
}

//Screen reset with a counter to delay it
void screen_reset_func()
{
  static unsigned long last_reset_time = 0;
  
  //Check if time has come to reset again!
  if(millis() - last_reset_time > REFRESH_TIMEOUT)
  {
      last_reset_time = millis();
    
      screen_reset();
  }
}

void setup()
{
  //Set led pins to putputs
  pinMode(RED_LED_PIN, OUTPUT); 
  pinMode(GREEN_LED_PIN, OUTPUT); 
  pinMode(BLUE_LED_PIN, OUTPUT); 
  
  screen_reset();
  
  // initialize the serial communications:
  Serial.begin(19200);
}

void rewrite_screen(int *buffer)
{
  lcd.setCursor(0,0);
  
  for(int i = 0; i < BUFFER_SIZE;i++)
  {
      lcd.write(buffer[i]);
    
      if(i == BUFFER_SIZE/2 - 1)
      {
          //end of first line! go to next
          lcd.setCursor(0,1);
      }
  }
  
}

int handle_serial_screen_refresh()
{
  static int count = 0, screen_refresh = 0;
  static int buffer[BUFFER_SIZE];
  
  
  static uint16_t not_connected_timeout = NOT_CONNECTED_TIMEOUT;
  
  // when characters arrive over the serial port...
  int got_char = Serial.read();
  while(got_char > 0)  //Got something, handle
  {
    if(got_char == 1)
    {
        screen_reset_func();  //Call periodically to make sure screen is online
        rewrite_screen(buffer);
  
        not_connected_timeout = 0;  //Reset timeout
        count = 0;  
    }
    else
    {
        if(count < BUFFER_SIZE)
        {
            buffer[count] = got_char; // set into buffer  
        }
        count++;
        
    } 
    got_char = Serial.read();
  }
  //Increment timeout
  not_connected_timeout++;
  
  if(not_connected_timeout > NOT_CONNECTED_TIMEOUT)
  {
    return -1;  //We got no data, return fail
  }
  
  current_led_color = online_leds;
  return 0;
}

float sinval;

void update_leds()
{
  static uint16_t it_counter = 0;
  it_counter++;
  if(it_counter > LED_SINUS_STEPS)
  {
    it_counter = 0;
  }
  const float addme = PI/LED_SINUS_STEPS;
  
  sinval = sin(addme * it_counter);
  
  analogWrite(RED_LED_PIN, sinval * current_led_color.red);
  analogWrite(GREEN_LED_PIN, sinval * current_led_color.green);
  analogWrite(BLUE_LED_PIN, sinval * current_led_color.blue);
  
}
void print_temp()
{
  lcd.print(current_temp, 1);
  lcd.write((byte)0);
  lcd.print("C");
}
void handle_standalone_screen_refresh()
{
  
  static unsigned long last_run_time = 0;
  
  //Check if time has come to read again!
  if(millis() - last_run_time > thermoDELAYMS)
  {
    last_run_time = millis();
    
    screen_reset_func();  //Call periodically to make sure screen is online
    lcd.setCursor(0,0);
    lcd.print("Hello world!!               ");
    
    lcd.setCursor(0, 1);
    lcd.print(sinval);
    lcd.print("          ");
    print_temp();
    lcd.print("     ");
     
  }
  
  current_led_color = offline_leds;
  
}

//Read temp from max6675, taking into account a delay in reading only 4 Hz and LP filtering
void update_temp()
{
  static unsigned long last_run_time = 0;
  
  //Check if time has come to read again!
  if(millis() - last_run_time > screenDELAYMS)
  {
     //lowpassfilter according to constants
     current_temp = current_temp * thermofilterconstantHIGH + (thermocouple.readCelsius() - thermoOFFSET) * thermofilterconstantLOW;
     last_run_time = millis();
  }
}

void loop()
{    
  
    if(handle_serial_screen_refresh())  //if we get a return value, routine failed
    {
      
      //This is what we do when we have nothing to do!
      handle_standalone_screen_refresh();
    }
    
    //Call tempreading routine
    update_leds();
    update_temp();
    
    delay(1);  
}



