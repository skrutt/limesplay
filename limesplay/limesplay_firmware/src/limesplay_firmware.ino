#include <max6675.h>

// include the library code:
#include <LiquidCrystal.h>

#include "petprotocol.h"

#define BUFFER_SIZE 40


#define screenDELAYMS 100
#define REFRESH_TIMEOUT 15000

#define NOT_CONNECTED_TIMEOUT 1000

#define RED_LED_PIN 10
#define GREEN_LED_PIN 11
#define BLUE_LED_PIN 9

#define LED_SINUS_STEPS 1000
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
led_color_t online_leds = {.red = 255, .green = 0, .blue = 255};
static led_color_t current_led_color = {.red = 0, .green = 0, .blue = 255};

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(7, 6, 5, 4, 3, 2);

//Thermocouple
MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);
double current_temp = 20;

//Do we have a connection?
bool have_heartbeat = 0;


 uint8_t charao[8]  = {0b10000100, 0b10000000, 0b10001110, 0b10000001, 0b10001111, 0b10010001, 0b10001111, 128};
 uint8_t charAO[8]  = {0b10000100, 0b10001110, 0b10010001, 0b10010001, 0b10011111, 0b10010001, 0b10010001, 128};
//ä already exists
 uint8_t charAE[8]  = {0b10001010, 0b10001110, 0b10010001, 0b10010001, 0b10011111, 0b10010001, 0b10010001, 128};
//ö already exists
 uint8_t charOE[8]  = {0b10001010, 0b10001110, 0b10010001, 0b10010001, 0b10010001, 0b10010001, 0b10001110, 128};



//Screen reset function
void screen_reset()
{
   //Reset screen if its gone wonky
   lcd.begin(20, 2);
   // make a cute degree symbol
   uint8_t degree[8]  = {140,146,146,140,128,128,128,128};
   lcd.createChar(0, degree);
   lcd.createChar(1, charao);
   lcd.createChar(2, charAO);
   lcd.createChar(3, charAE);
   lcd.createChar(4, charOE);
   
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
  Serial.begin(115200);
}

void rewrite_screen(uint8_t *buffer)
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

static uint8_t lcd_buffer[BUFFER_SIZE] = "This is a test string that is kind of long...";

//Update text in lcd buffer
void update_lcd_buffer(uint8_t * text, uint8_t len)
{
        if (len > BUFFER_SIZE)
        {
            len = BUFFER_SIZE;
        }
        if(text)
            memcpy(lcd_buffer, text, len);
}

int handle_serial_screen_refresh()
{
        static int count = 0;

        screen_reset_func();  //Call periodically to make sure screen is online
        rewrite_screen(lcd_buffer);

        current_led_color = online_leds;

        return 0;
}

float sinval;

void update_leds()
{
    static unsigned long last_run_time = 0;

    //Check if at least one ms has passed
    if(millis() != last_run_time)
    {
        last_run_time = millis();

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
}
void print_temp()
{
      lcd.print(current_temp, 1);
      lcd.write((byte)0);
      lcd.print("C");
}
static char lcd_str[] = "Hello world me tioo!!               ";
void handle_standalone_screen_refresh()
{

        screen_reset_func();  //Call periodically to make sure screen is online
        lcd.setCursor(0,0);
        lcd.print(lcd_str);
        //Possibly clear screen after here

        lcd.setCursor(0, 1);
        lcd.print(sinval);
        lcd.print("          ");
        print_temp();  
        lcd.print("     ");

        current_led_color = offline_leds;

}

//Read temp from max6675, taking into account a delay in reading only 4 Hz and LP filtering
void update_temp()
{
      static unsigned long last_run_time = 0;

      //Check if time has come to read again!
      if(millis() - last_run_time > thermoDELAYMS)
      {
         //lowpassfilter according to constants
         current_temp = current_temp * thermofilterconstantHIGH + (thermocouple.readCelsius() - thermoOFFSET) * thermofilterconstantLOW;
         last_run_time = millis();
      }
}

MyBuffer<uint8_t> buf(200);

void sendFunc(const void * data, uint16_t length)
{
        // Serial.print("putting data into buf: ");
        uint8_t* ptr = (uint8_t*)data;
        for (uint8_t i = 0; i < length; i++)
        {
            buf.put(ptr[i]);
            // Serial.print(ptr[i]);
            // Serial.print(" ");
        }
        // Serial.println(" ");
}

void printfunc(uint8_t byte)
{
        Serial.write(byte);
}
void printfunc2(uint8_t *byte_ptr, uint16_t len)
{
        for(int i = 0; i < len; i++)
        {
            Serial.write(byte_ptr[i]);
        }
}

static petcol test(printfunc2, printfunc);

typedef struct {
    uint16_t type;
    float value;
} com_float_type_t;


void handle_serial_conn()
{
        static unsigned long last_run_time = 0;
        if((millis() - last_run_time) > 150)
        {
          last_run_time = millis();
            // Serial.write("Sending packets\n");
            // const char data[] = "Now we are sendi\xA\0\xAA some real data \n";
            // static uint8_t num = 1;
            // num %= sizeof(data);
            // num++;
            //
            // const char data1[] = " \n HEY! \n And this is some distractions! \n\n";

            // sendFunc(data1, sizeof(data1) - 1);
            // test.sendFunc(data, num);

            com_float_type_t temp_msg;
            temp_msg.type = 1;
            temp_msg.value = current_temp;
            test.sendFunc(&temp_msg, 6);
        }

            // if (Serial.available())
            // {
            //     Serial.write("We got data!");
            // }

    #define HEARTBEAT_TIMEOUT 5000

            static unsigned long heartbeat_timer = 0;
            have_heartbeat = (millis() - heartbeat_timer) < HEARTBEAT_TIMEOUT;


            while (Serial.available())
            {
                packet_recieved *gotpac;
                uint8_t byte = Serial.read();
                if(gotpac = test.recv_byte_input(byte))
                {
                    //We got a msg!
                    Serial.print("We got a packet!");
                    //Reset heartbeat timer
                    heartbeat_timer = millis();

                    switch (gotpac->data[0])
                    {
                        case 1:
                            update_lcd_buffer(gotpac->data+1, gotpac->length - 1);
                            break;
                        case 2:
                            online_leds.red     = gotpac->data[1];
                            online_leds.green   = gotpac->data[2];
                            online_leds.blue    = gotpac->data[3];
                            break;
                    }

                }

            }
}

void loop()
{

    // if (buf.available())
    {
        // packet_recieved *gotpac;
        // while(buf.available())
        // {
        //     uint8_t byte = buf.get();
        //     if(gotpac = test.recv_byte_input(byte))
        //     {
        //         Serial.write("Looks good! we got a packet!\n");
        //         if(gotpac->length == 1)
        //             Serial.print(*gotpac->data);
        //         else
        //             Serial.write((char*)gotpac->data, gotpac->length);
        //     }
        //     // else
        //     // {
        //     //     Serial.print(byte);
        //     //     Serial.print(" ");
        //     // }
        //
        //
        // }
        // if(!gotpac)
        // {
        //     Serial.write(" No packet yet!\n");
        //
        //     delay(5000);
        // }
    }
    // else

        // // const char TEST[] = "\n TEST\n\n";
        // //
        // // Serial.write(TEST, sizeof(TEST));
        // Serial.print("size of temp_msg ");
        // Serial.println(sizeof(temp_msg));
        //
        //
        // // sendFunc(data, sizeof(data) - 1);
        // test.sendFunc(data1, sizeof(data1) - 1);
        // test.sendFunc(&num, sizeof(num));
        // Serial.println(num);
    // }
    handle_serial_conn();

    static unsigned long last_run_time = 0;

    //Check if time has come to update again!
    if(millis() - last_run_time > screenDELAYMS)
    {
        last_run_time = millis();
        if(have_heartbeat)
            handle_serial_screen_refresh();  //if we get a return value, routine failed
        else
            //This is what we do when we have nothing to do!
            handle_standalone_screen_refresh();
    }

    //Call tempreading routine
    update_leds();
    update_temp();

}
