/*
 * Open Source Spa Control
 *
 * 6/15/2016 - v1.0
 * Last build test with Arduino v1:1.05
 * Arclight - arclight@23.org
 * Danozano - danozano@gmail.com
 *
 * Notice: This is free software and is probably buggy. Use it at
 * at your own peril.  Use of this software may result in your
 * doors being left open, your stuff going missing, or buggery by
 * high seas pirates. No warranties are expressed on implied.
 * You are warned.
 *
 *
 * For latest downloads,  see the Wiki at:
 * http://www.accxproducts.com/wiki/index.php?title=Open_Access_4.0
 *
 * For the SVN repository and alternate download site, see:
 * http://code.google.com/p/open-access-control/downloads/list
 *
 * Latest update puts configuration variables in user.h
 * This version supports the new Open Access v4 hardware.
 * 
 * Uses a DS18B20 1-wire temperature sensor, two switches and a potentiometer as the user interface.
 * Output channels include lighting, heater, pump and chlorine dispenser.
 *
 * Version 4.x of the hardware implements these features and emulates an
 * Arduino Duemilanova.
 * The "standard" hardware uses the MC23017 i2c 16-channel I/O expander.
 * I/O pins are addressed in two banks, as GPA0..7 and GPB0..7
 *
 * Relay outpus on digital pins:  GPA6, GPA7, GPB0, GPB1
 * DS1307 Real Time Clock (I2C):  A4 (SDA), A5 (SCL)
 * Analog pins (for alarm):       A0,A1,A2,A3 
 * Digital input in (tamper):     D9 (Onewire Temp sensor)
 * Reader 1:                      D2,D3 (Momentary puff switch #1, #2)
 * Reader 2:                      D4,D5 (D4 = Pump Output, D5=Heater Output)
 * RS485 TX enable / RX disable:  D8
 * RS485 RX, TX:                  D6,D7
 * Reader1 LED:                   GPB2 (LED1)
 * Reader1 Buzzer:                GPB3 (LED2)
 * Reader2 LED:                   GPB4 (LED3)
 * Reader2 Buzzer:                GPB5 (LED4/spa lights)
 * Status LED:                    GPB6
 
 * LCD RS:                        GPA0
 * LCD EN:                        GPA1
 * LCD D4..D7:                    GPA2..GPA5
 
 * Ethernet/SPI:                  D10..D13  (Not used, reserved for the Ethernet shield)
 * 
 * Quickstart tips: 
 * Set the console password(PRIVPASSWORD) value to a numeric DEC or HEX value.
 * Define the static user list by swiping a tag and copying the value received into the #define values shown below 
 * Compile and upload the code, then log in via serial console at 57600,8,N,1
 *
 */
#include "user.h"         // User preferences file. Use this to select hardware options, passwords, etc.
#include <Wire.h>         // Needed for I2C Connection to the DS1307 date/time chip
#include <EEPROM.h>       // Needed for saving to non-voilatile memory on the Arduino.
#include <avr/pgmspace.h> // Allows data to be stored in FLASH instead of RAM
#include <math.h>         // Needed for Thermistor calculator

#include <DS1307.h>             // DS1307 RTC Clock/Date/Time chip library

#ifdef DS18S20
#include <OneWire.h>            // Onewire support for temp sensor
#include <DallasTemperature.h>  // DS18S20 temperature conversion and polling library
#endif


#ifdef MCPIOXP
#include <Adafruit_MCP23017.h>  // Library for the MCP23017 i2c I/O expander
#endif


#ifdef AT24EEPROM
#include <E24C1024.h>           // AT24C i2C EEPOROM library
#define MIN_ADDRESS 0
#define MAX_ADDRESS 4096        // 1x32K device
#endif

#define EEPROM_CAPACITY   1              // EEPROM address to store spa capacity figures
#define EEPROM_CHLOR_CONC 2              // EEPROM address to store Chlorine percentage


#define ONEWIRE_PIN    9                // Define the pin for the 1-wire bus.  Requires 4.7k pull-up and 0-ohm jumper instead of 2.2K 
#define BUTTON1        2                // Define the pin for puff switch #1 (top)
#define BUTTON2        3                // Define the pin for puff switch #1 (top)
#define PUMP           9
#define HEATER         9
#define CHLORINATOR    8              
#define LV1            7
#define LV2            6
  
#define LED1     10                    // Define the status LED pins (GBP2..5)
#define LED2     11
#define LED3     12
#define LED4     13
#define RS485ENA        6               // Arduino Pin D6
#define STATUSLED       14              // MCP pin 14
#define POTPIN    1
#define TPIN      0


/*  Global Boolean values
 *
 */
boolean     pumpOn=false;                       // Keeps track of whether the doors are supposed to be locked right now
boolean     heaterOn=false;
boolean     chlorinatorOn=false;
boolean     LV1On=false;                        // Keep track of when door chime last activated
boolean     LV2On=false;                       // Keep track of when door last closed for exit delay

volatile boolean     button1Changed=false;
volatile int        userMode=0;
volatile unsigned long button1timer=0;

int        setPoint=0;
int        lightMode=0;
/*  Global Timers
 *
 */
unsigned long chlorineTimer=0;                 // Keep track of how long chlorine dispense has been running
unsigned long userTimer=0;                     // Keep track of how long spa has been running in user mode
unsigned long systemTimer=0;                   // Keep track of automated system activation runtime
unsigned long thermTimer=0;                    // Keep track of last heater activation

//Other global variables
uint8_t second, minute, hour, dayOfWeek, dayOfMonth, month, year;     // Global RTC clock variables. Can be set using DS1307.getDate function.

// Serial terminal buffer (needs to be global)
char inString[64]={0};                                         // Size of command buffer (<=128 for Arduino)
uint8_t inCount=0;
boolean privmodeEnabled = false;                               // Switch for enabling "priveleged" commands



/* Create an instance of the various C++ libraries we are using.
 */

#ifdef DS18S20
OneWire oneWire(9);                   // Temp sensor on D9 (Tamper zone on PCB)
DallasTemperature sensors(&oneWire);  // Dallas Temp sensor library
#endif

DS1307 ds1307;                        // RTC Instance

#ifdef MCPIOXP
Adafruit_MCP23017 mcp;                //I2C IO expander library
#endif

#ifdef LCDBOARD
cLCD lcd;
#endif

/* Set up some strings that will live in flash instead of memory. This saves our precious 2k of
 * RAM for something else.
*/
const prog_uchar rebootMessage[]          PROGMEM  = {"SpaOS 1.00 System Ready."};
const prog_uchar progMessage1[]           PROGMEM  = {"Enter spa capacity in liters:"};
const prog_uchar progMessage2[]           PROGMEM  = {"Enter Chlorine conc. in percent"};


const prog_uchar consolehelpMessage1[]    PROGMEM  = {"Valid commands are:"};
const prog_uchar consolehelpMessage2[]    PROGMEM  = {"(d)ate, (s)tatus"};
const prog_uchar consolehelpMessage3[]    PROGMEM  = {"(t)ime set <sec 0..59> <min 0..59> <hour 0..23> <day of week 1..7>"};
const prog_uchar consolehelpMessage4[]    PROGMEM  = {"           <day 0..31> <mon 0..12> <year 0.99>"};
const prog_uchar consolehelpMessage5[]    PROGMEM  = {"(e)nable <password> - enable or disable priveleged mode"};                                       
const prog_uchar consolehelpMessage6[]    PROGMEM  = {"(h)ardware Test <iterations> - Run the hardware test"};   
const prog_uchar consoledefaultMessage[]  PROGMEM  = {"Invalid command. Press '?' for help."};

const prog_uchar statusMessage1[]         PROGMEM  = {"Pump state (1=running):"};
const prog_uchar statusMessage2[]         PROGMEM  = {"Heater state (1=heating):"};
const prog_uchar statusMessage3[]         PROGMEM  = {"Temperature (Farenheit):"}; 
const prog_uchar statusMessage4[]         PROGMEM  = {"Chrlorinator state (1=dispensing):"};
const prog_uchar statusMessage5[]         PROGMEM  = {"LV light1 state (1=On):"};     
const prog_uchar statusMessage6[]         PROGMEM  = {"LV light2 state (1=On):"};                   


void setup(){           // Runs once at Arduino boot-up

attachInterrupt(0, checkSwitch1, CHANGE);

#ifdef DS18S20
  sensors.begin();  // Start the temp sensor library
#endif

  Wire.begin();     // start Wire library as I2C-Bus Master
  mcp.begin();      // use default address 0

  pinMode(BUTTON1,INPUT);                // Initialize the Arduino built-in pins
  pinMode(BUTTON2,INPUT);
  pinMode(PUMP,OUTPUT);
  pinMode(HEATER,OUTPUT);
  pinMode(RS485ENA, OUTPUT);
  pinMode(LV1, OUTPUT);
  pinMode(LV2, OUTPUT);
  
  for(int i=0; i<=15; i++)        // Initialize the I/O expander pins
  {
   mcp.pinMode(i, OUTPUT);
   mcp.digitalWrite(i, LOW);
  }
   
  digitalWrite(RS485ENA, HIGH);           // Set the RS485 chip to HIGH (not asserted)
    
  Serial.begin(UBAUDRATE);	          // Set up Serial output 


#ifdef LCDBOARD
lcd.begin(16,2);
lcd.setCursor(0,0);
lcd.print("Open Access ");
lcd.print(VERSION);
delay(500);
lcd.clear();
#endif
                                            //Set up the MCP23017 IO expander and initialize
#ifdef MCPIOXP 
mcp.digitalWrite(STATUSLED, LOW);           // Turn the status LED green
#endif

  Serial.println("Initializing...");  
   for(int i=10; i<=13; i++)                        // Set all LED outputs high on MCP chip (high=on)
     {
       mcp.digitalWrite(i,HIGH);                     
       delay(250);
     }     
   delay(250);


   for(int i=10; i<=13; i++)                        // Set all LED outputs low on MCP chip (high=off)
     {
       mcp.digitalWrite(i,LOW);                     
     }     
 
safetyCheck();  // Check for out-of-range thermocouple.  Freeze system if present.

   mcp.digitalWrite(LED4,HIGH);                    // Set the "Ready" LED on

   systemStatus();   
   checkThermostat();
   thermTimer = millis();
   logReboot();

}




void loop()                                     // Main branch, runs over and over again
{                         
safetyCheck();                                 // Check for button presses.
userModeCheck();                               // Button 1 (spa on) workflow
checkSwitch2();


readCommand();                                 // Check for commands entered at serial console


  if(((millis() - userTimer) >= TIMEOUT) && (userMode == 1))  // Turn off spa if left running by user.
  { 
    pumptoOff();                                // Turn off pump and related accessories
    LV2On=0;                                  // Turn off spa light
    Serial.println("User spa timeout reached. Turning off.");
  }

  if(((millis() -  systemTimer) >= SYSTIMEOUT) && (userMode == 2))
  { 
    pumptoOff();
    logDate();
    Serial.println("Spa maintenance cycle complete.");
  }

  if(((millis() - chlorineTimer) >= CL_TIMEOUT) && (chlorinatorOn !=0))
  { 

    mcp.digitalWrite(CHLORINATOR, LOW);
    chlorinatorOn=0;
    mcp.digitalWrite(LED1, LOW);
    logDate();
    Serial.println("Chlorine dispense off by timeout.");
  }   

  if(((millis() - thermTimer) >= THERMINTERVAL))
  { 
   Serial.println("Checking thermostat setpoint");  
   checkThermostat();
   Serial.print("Measured: ");
   Serial.println(Thermistor(getADCvalue(TPIN)));
   
   if((Thermistor(getADCvalue(TPIN)) < (setPoint+1)) && (pumpOn==true) && (heaterOn==false)) 
    {
     digitalWrite(PUMP, HIGH);
     mcp.digitalWrite(HEATER, HIGH);
     mcp.digitalWrite(LED3, HIGH);
     heaterOn=true;
     Serial.println("Heater activated.");
    }

           thermTimer = millis();  
  }

 
     if((heaterOn == true) && Thermistor(getADCvalue(TPIN)) >= setPoint)
      {
         mcp.digitalWrite(HEATER, LOW);
         mcp.digitalWrite(LED3, LOW);
         heaterOn=false;
         Serial.println("Heater deactivated.");
      }

        

  ds1307.getDateDs1307(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);   // Get the current date/time

  if((hour==6) && (minute==0) && (userMode !=2))   // Automatic timed maintenance cycle
  
  {
        userMode=2;
         systemTimer=millis();
         chlorineTimer=millis();
         thermTimer=millis();
         pumpOn=true;
         digitalWrite(PUMP,HIGH);
         chlorinatorOn=true;
         mcp.digitalWrite(CHLORINATOR, HIGH)           ;
         mcp.digitalWrite(LED1, HIGH);
         mcp.digitalWrite(LED2, HIGH);
         logDate();
         Serial.println("Spa maintenance cycle begin.");
  }


  if((hour==19) && (lightMode!=1))
  {
 
      lightMode=1;
      lightModeCheck();
  }   
   

  if((hour==7) && (lightMode==1)){
 
      lightMode=2;
      lightModeCheck();
  }           

}  // End of Loop






/* Logging Functions - Modify these as needed for your application. 
 Logging may be serial to USB or via Ethernet (to be added later)
 */



void PROGMEMprintln(const prog_uchar str[])    // Function to retrieve logging strings from program memory
{                                              // Prints newline after each string  
  char c;
  if(!str) return;
  while((c = pgm_read_byte(str++))){
    Serial.write(c);
                                   }
    Serial.println();
}

void PROGMEMprint(const prog_uchar str[])    // Function to retrieve logging strings from program memory
{                                            // Does not print newlines
  char c;
  if(!str) return;
  while((c = pgm_read_byte(str++))){
    Serial.write(c);
                                   }

}


void logDate()
{
  ds1307.getDateDs1307(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);
  Serial.print(hour, DEC);
  Serial.print(":");
  Serial.print(minute, DEC);
  Serial.print(":");
  Serial.print(second, DEC);
  Serial.print("  ");
  Serial.print(month, DEC);
  Serial.print("/");
  Serial.print(dayOfMonth, DEC);
  Serial.print("/");
  Serial.print(year, DEC);
  Serial.print(' ');
  
  switch(dayOfWeek){

    case 1:{
     Serial.print("SUN");
     break;
           }
    case 2:{
     Serial.print("MON");
     break;
           }
    case 3:{
     Serial.print("TUE");
     break;
          }
    case 4:{
     Serial.print("WED");
     break;
           }
    case 5:{
     Serial.print("THU");
     break;
           }
    case 6:{
     Serial.print("FRI");
     break;
           }
    case 7:{
     Serial.print("SAT");
     break;
           }  
  }
  
  Serial.print(" ");

}

void logReboot() {                                  //Log system startup
  logDate();
  PROGMEMprintln(rebootMessage);
}




void hardwareTest(long iterations)
{

  /* Hardware testing routing. Performs a read of all digital inputs and
   * a write to each relay output. Also reads the analog value of each
   * alarm pin. Use for testing hardware. Wiegand26 readers should read 
   * "HIGH" or "1" when connected.
   */

  pinMode(2,INPUT);
  pinMode(3,INPUT);
  pinMode(4,INPUT);
  pinMode(5,INPUT);

  pinMode(6,OUTPUT);
  pinMode(7,OUTPUT);
  pinMode(8,OUTPUT);
  pinMode(9,OUTPUT);


  for(long counter=1; counter<=iterations; counter++) {                                  // Do this number of times specified
    digitalWrite(6, HIGH);
    logDate();

    Serial.print("\n"); 
    Serial.println("Pass: "); 
    Serial.println(counter); 
    
    Serial.print("Thermistor temp:");
    Serial.println(Thermistor(getADCvalue(TPIN)),2);       // read ADC and  convert it to Celsius

    Serial.print("Button 1: ");                    // Digital input testing
    Serial.println(digitalRead(BUTTON1));
    Serial.print("Button 2: ");
    Serial.println(digitalRead(BUTTON2));
  
//    Serial.print("Reader2-0:");
//    Serial.println(digitalRead(4));
//    Serial.print("Reader2-1:");
//    Serial.println(digitalRead(5));
    Serial.print("Thermistor raw:");                   // Analog input testing
    Serial.println(getADCvalue(TPIN));

    Serial.print("Temp control knob: :");
    Serial.println(getADCvalue(POTPIN));
//    Serial.print("Zone 3:");
//    Serial.println(analogRead(2));
//    Serial.print("Zone 4:");
//    Serial.println(analogRead(3));


#ifdef DS18S20
  Serial.print("Read temperature on D9...");
  sensors.requestTemperatures(); // Send the command to get temperatures
  Serial.println("DONE");
  Serial.print("Temperature for Device on D9: ");
  Serial.print(sensors.getTempFByIndex(0));
  Serial.println(" Degrees F");
#endif

  for(int i=0; i<=14; i++)                        // Set all relay/LCD outputs low on MCP chip (low=off)
     {
       mcp.digitalWrite(i,LOW);                     
 
     }
  delay(500);

  Serial.println("Turning on LEDs");  
   for(int i=10; i<=13; i++)                        // Set all LED outputs high on MCP chip (high=off)
     {
       mcp.digitalWrite(i,HIGH);                     
       delay(500);
     }     
   delay(500);

Serial.println("Turning on  output loads");  
digitalWrite(PUMP,HIGH);
delay(500);
mcp.digitalWrite(HEATER,HIGH);                    //Turn on all loads
delay(500);
mcp.digitalWrite(CHLORINATOR,HIGH);  
delay(500);
mcp.digitalWrite(LV1,HIGH);  
delay(500);
mcp.digitalWrite(LV2,HIGH);  


Serial.println("Turning off output loads and LEDs");  
digitalWrite(PUMP,LOW);
delay(500);
              
  for(int i=0; i<=14; i++)                        // Set all relay/LCD outputs low on MCP chip (low=off)
     {
       mcp.digitalWrite(i,LOW);     
     }
   
    Serial.println("Relays and LEDs off");

  }
mcp.digitalWrite(LED4, HIGH);
}




void readCommand() {                                               
byte cmds=7;
byte cmdlen=9;

uint8_t stringSize=(sizeof(inString)/sizeof(char));                    
char cmdString[cmds][cmdlen];                                             // Size of commands (4=number of items to parse, 10 = max length of each)


uint8_t j=0;                                                          // Counters
uint8_t k=0;
char cmd=0;


char ch;

 if (Serial.available()) {                                       // Check if user entered a command this round	                                  
  ch = Serial.read();                                            
  if( ch == '\r' || inCount >=stringSize-1)  {                   // Check if this is the terminating carriage return
   inString[inCount] = 0;
   inCount=0;
                         }
  else{
  (inString[inCount++] = ch); }
  //Serial.print(ch);                        // Turns echo on or off


if(inCount==0) {
  for(uint8_t i=0;  i<stringSize; i++) {
    cmdString[j][k] = inString[i];
    if(k<cmdlen) k++;
    else break;
 
    if(inString[i] == ' ') // Check for space and if true, terminate string and move to next string.
    {
      cmdString[j][k-1]=0;
      if(j<=cmds)j++;
      else break;
      k=0;             
    }

  }
 cmd = cmdString[0][0];
                                     
               switch(cmd) {


 
                  case 'd': {                                                 // Display current time
                   logDate();
                   Serial.println();
                   break;
                            }

   case 's': {                                                 // Display system status
                   systemStatus();
                   break;
                            }
                           
  
                   
                 case 't': {                                                                // Change date/time 
                

                   Serial.print("Old time :");           
                   logDate();
                   Serial.println();
                   ds1307.setDateDs1307(atoi(cmdString[1]),atoi(cmdString[2]),atoi(cmdString[3]),
                   atoi(cmdString[4]),atoi(cmdString[5]),atoi(cmdString[6]),atoi(cmdString[7]));
                   Serial.print("New time :");
                   logDate();
                   Serial.println();                                         
                   break;
                          }                          
                          
                  case 'h': {                                                    // Run hardware test
                    hardwareTest(atoi(cmdString[1]));  
                             }        
                             
                  case '?': {                                                  // Display help menu
                     PROGMEMprintln(consolehelpMessage1);
                     PROGMEMprintln(consolehelpMessage2);
                     PROGMEMprintln(consolehelpMessage3);
                     PROGMEMprintln(consolehelpMessage4);
                     PROGMEMprintln(consolehelpMessage5);                     
                     PROGMEMprintln(consolehelpMessage6);                  
   
                   break;
                            }

                   default:  
                    PROGMEMprintln(consoledefaultMessage);
                    break;
                                     }  
                        
  }                                    // End of 'if' statement for Serial.available
 }                                     // End of 'if' for string finished
}                                      // End of function 



void checkThermostat()
{

 int value=getADCvalue(POTPIN);
 
value = map(value,POTLOW,POTHIGH,MINTEMP,MAXTEMP);
if(value < MINTEMP) {value=40;}
if(value > MAXTEMP) {value=MAXTEMP;};
Serial.print("Current pot read: ");
Serial.println(value, DEC);

if(setPoint !=value)
 {
   setPoint=value;
   Serial.print("New temp setpoint: ");
   Serial.println(setPoint, DEC);
}

}

void lightModeCheck()
{
  
 
switch(lightMode) 

{
 case 0:
  {
    LV2On=true;
    mcp.digitalWrite(LV2, HIGH);
    Serial.println("Spa lights on");
    mcp.digitalWrite(LV1, HIGH);
    Serial.println("Exterior lights on");
    break;
  }


  case 1:
  {
    LV2On=false;
    mcp.digitalWrite(LV2, LOW);
    mcp.digitalWrite(LV1, HIGH);
    Serial.println("Spa lights off, exterior lights on");
    break;
  }

   case 2:
  {
       LV1On=false;
       mcp.digitalWrite(LV2, LOW);
       mcp.digitalWrite(LV1, LOW);
       Serial.println("All lights off");  
   break;
  }

  
  default:
  {
   break;
  } 
  


}
}


void checkSwitch2()          // Check for button presses
{
  while(digitalRead(BUTTON2) == 0) 
   {

     if(lightMode <2) 
      {
       lightMode++;
      }
     else 
     {
       lightMode =0;

     }
    delay(350);                                        // 250ms delay between calue changes
    lightModeCheck(); 
   }
   
}






void checkSwitch1()          // Check for button presses.  This one is interrupt-driven.
{ 
  if((millis()-button1timer)>=600)
 { 
  button1Changed=true;
  button1timer=millis();
  
  if(userMode==2)
  {
    return;
  }
  

if(userMode==0) 
  {
    userMode=1;
  }
  else
  {
    userMode=0; 
 
  }
 }
}



 
     



void userModeCheck()
{
  if(button1Changed==true)
  {
    button1Changed=false;


     switch(userMode){



    case 1:
    {
       userTimer=millis();
       thermTimer=millis();       // Start the 30 second countdown to thermostat activation.  Gives water time to circulate.
       pumpOn=1;
       mcp.digitalWrite(LED2, HIGH);
       digitalWrite(PUMP, HIGH);
       Serial.println("Pump on by user button.");
       break;
    }
    case 0:
      {
       pumpOn=0;
       digitalWrite(PUMP, LOW);
       mcp.digitalWrite(LED2, LOW);
       Serial.println("Pump off by user button.");
       break;
      }
      default:
      {
      break;
      }
    }
}
   }
   








float Thermistor(int RawADC) {


float vcc = 5.00;                       // only used for display purposes, if used
                                        // set to the measured Vcc.
float pad = 10050;                       // balance/pad resistor value, set this to
                                        // the measured resistance of your pad resistor
float thermr = 10000;                   // thermistor nominal resistance


  long Resistance;  
  float Temp;  // Dual-Purpose variable to save space.

//Equation for a Varef-Thermistor-Pullup-GND circuit
RawADC = map(RawADC,0,1024,1024,0);
Resistance=pad*((1024.0 / RawADC) - 1);
// For a GND-Thermistor-PullUp--Varef circuit it would be Rtherm=Rpullup/(1024.0/ADC-1)


//   Resistance=pad/(1024.0 / RawADC- 1);
/*
A,B,C values for the thermistor in use.
Calculator at: https://www.thermistor.com/calculators?r=sheccr
1.961390709e-3
0.9674752431e-4
6.386648944e-7
*/
  Temp = log(Resistance); // Saving the Log(resistance) so not to calculate  it 4 times later
  Temp = 1 / ( 1.961390709e-3 + (0.9674752431e-4 * Temp) + ( 6.386648944e-7 * Temp * Temp * Temp));
  Temp = Temp - 273.15;  // Convert Kelvin to Celsius                      
// Temp = log(Resistance); // Saving the Log(resistance) so not to calculate  it 4 times later
// Temp = 1 / (0.001129148 + (0.000234125 * Temp) + (0.0000000876741 * Temp * Temp * Temp));
// Temp = Temp - 273.15;  // Convert Kelvin to Celsius                      
  

  // BEGIN- Remove these lines for the function not to display anything
  //Serial.print("ADC: ");
  //Serial.print(RawADC);
  //Serial.print("/1024");                           // Print out RAW ADC Number
  //Serial.print(", vcc: ");
  //Serial.print(vcc,2);
  //Serial.print(", pad: ");
  //Serial.print(pad/1000,3);
  //Serial.print(" Kohms, Volts: ");
  //Serial.print(((RawADC*vcc)/1024.0),3);  
  //Serial.print(", Resistance: ");
  //Serial.print(Resistance);
  //Serial.print(" ohms, ");
  // END- Remove these lines for the function not to display anything

  // Uncomment this line for the function to return Fahrenheit instead.
  Temp = (Temp * 9.0)/ 5.0 + 32.0;                  // Convert to Fahrenheit
  return Temp;                                      // Return the Temperature
}

void LV1turnOn()
 {
         LV1On=true;
         mcp.digitalWrite(LV1,HIGH);
         logDate();
         Serial.println("Exterior lights on.");
 }
 
 
void LV1turnOff()
 {
         LV1On=true;
         mcp.digitalWrite(LV1,LOW);
         logDate();
         Serial.println("Exterior lights off.");
 }
 
 void systemStatus()
 {
  Serial.print("Current set point: ");
  Serial.println(setPoint, DEC);
  Serial.print("Thermistor water temp:");
  Serial.println(Thermistor(analogRead(TPIN)),2);       // read ADC and  convert it to Celsius
  Serial.print("LV light #1: ");
  Serial.println(LV1On, DEC);
  Serial.print("LV light #2: ");
  Serial.println(LV2On, DEC);
  Serial.print("Pump: ");
  Serial.println(pumpOn, DEC);
  Serial.print("Heater: ");
  Serial.println(heaterOn, DEC);   
  Serial.print("Chlorinator: ");
  Serial.println(chlorinatorOn, DEC);   
  ;  
//  sensors.requestTemperatures(); // Send the command to get temperatures
  //Serial.print("DS Temperature: ");
  //Serial.print(sensors.getTempFByIndex(0));
  //Serial.println(" Degrees F");
  //                 Serial.println();
 }
 
 int getADCvalue(int pin)
 {
 
 
  int temp[5]={0};
  int avg;

        

    for(int j=0; j<5;j++){                          
      temp[j]=analogRead(pin);
      delay(5);                                         // Give the readings time to settle
    }
    avg=((temp[0]+temp[1]+temp[2]+temp[3]+temp[4])/5);  // Average the results to get best values  
    return avg;
 }
 
 void pumptoOn()
 {
 
 }
 
 void pumptoOff()
 {
    mcp.digitalWrite(HEATER, LOW);
    digitalWrite(PUMP, LOW);
    mcp.digitalWrite(CHLORINATOR, LOW);
    mcp.digitalWrite(LED1, LOW);
    mcp.digitalWrite(LED2, LOW);
    mcp.digitalWrite(LED3, LOW);
    heaterOn=0;
    pumpOn=0;
    chlorinatorOn=0;
    userMode=0;
  }

void safetyCheck()
{
  float temp=Thermistor(getADCvalue(TPIN)); 
  if((temp <40) ||(temp > 125))  // Check for shorted or open thermocouple and freeze system if present.
   {
    pumptoOff();
    Serial.println("Error! Temperature out of range! Shutting down system.");
    while(1 >0)
    {
       mcp.digitalWrite(LED4,HIGH);
       delay
       (250);
       mcp.digitalWrite(LED4,LOW);
       delay(250);   
    }
   }
}
