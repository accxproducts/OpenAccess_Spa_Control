/* User preferences file - Modify this header to use the correct options for your 
 * installation. 
 * Be sure to set the passwords/etc, as the defaul is "1234"
 */ 

/* Hardware options
 *
 */
#define MCU328         // Set this if using any boards other than the "Mega"
#define HWV3STD        // Use this option if using Open access v3 Standard board

#define MCPIOXP        //  Set this if using the v3 hardware with the MCP23017 i2c IO chip
#define AT24EEPROM     //  Set this if you have the At24C i2c EEPROM chip installed
//#define DS18S20        //  Set this if using the Dallas/Maxim DS18S20 1-wire temperature sensor

/* Static user List - Implemented as an array for testing and access override 
*/                               
//#define LCDBOARD                        // Uncomment to use LCD board
                                        // Uses the "cLCD" library that extends arduino LCD class
                                        // Has issues - must use a non-standard pinout, disables other MCP IO pins
                                        // Library is from the TC4 Coffee Roaster project
                                        // Download here: http://code.google.com/p/tc4-shield/
                                        
                                        
#define DEBUG 3                         // Set to 4 for display of raw tag numbers in BIN, 3 for decimal, 2 for HEX, 1 for only denied, 0 for never.               
#define VERSION 1.00
#define UBAUDRATE 9600                 // Set the baud rate for the USB serial port
#define MINTEMP 90                     // Minimum thermostat setpoint on dial (Farenheit)
#define MAXTEMP 105                    // Maximum thermostat setpoint on dial (Farenheit)
#define TIMEOUT    1800000             // Max number of seconds *1000 user mode turns spa on
#define SYSTIMEOUT 1800000             // Max number of seconds *1000 user mode turns spa on
#define CL_TIMEOUT 300000              // Max number of seconds *1000 that a chlorine dispense can run.
#define POTLOW    710
#define POTHIGH   770
#define THERMINTERVAL 60000
