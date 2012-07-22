// Consume NMEA data from Adafruit Ultimate GPS Breakout (MTK3339)
// and display summary to Nokia 5110 LCD on a Sparkfun breakout

// Derived from sample code in the Adafruit GPS library

#include <Adafruit_GPS.h>      // https://github.com/adafruit/Adafruit-GPS-Library
#include <SoftwareSerial.h>
#include <Adafruit_GFX.h>      // https://github.com/adafruit/Adafruit-GFX-Library
#include <Adafruit_PCD8544.h>  // https://github.com/adafruit/Adafruit-PCD8544-Nokia-5110-LCD-library

// Connect the GPS Power pin to 5V
// Connect the GPS Ground pin to ground
// Connect the GPS TX (transmit) pin to Digital 3
// Connect the GPS RX (receive) pin to Digital 2
#define PIN_GPS_RX  2
#define PIN_GPS_TX  3
#define PIN_GPS_FIX 4

SoftwareSerial mySerial(PIN_GPS_TX, PIN_GPS_RX);
Adafruit_GPS GPS(&mySerial);

#define PIN_SCE   9  // Orange  (connected to GND)
#define PIN_RESET 8  // Yellow  (connected to RESET)
#define PIN_DC    7  // Green
#define PIN_SDIN  6   // Blue
#define PIN_SCLK  5   // White

Adafruit_PCD8544 display = Adafruit_PCD8544(PIN_SCLK, PIN_SDIN, PIN_DC, PIN_SCE, PIN_RESET);

static char dtostrfbuffer[9];

// Set GPSECHO to 'false' to turn off echoing the GPS data to the Serial console
// Set to 'true' if you want to debug and listen to the raw GPS sentences
#define GPSECHO  true

// this keeps track of whether we're using the interrupt
// off by default!
boolean usingInterrupt = false;
void useInterrupt(boolean); // Func prototype keeps Arduino 0023 happy

/**
 * Derived from http://www.utopiamechanicus.com/article/sprintf-arduino/
 */
void _dpx(char *format, boolean line, va_list args)
{
  char buff[128];
  vsnprintf(buff,sizeof(buff),format,args);
  buff[sizeof(buff)/sizeof(buff[0])-1]='\0';
  if (!line) { 
    display.print(buff);
  } else {
    display.println(buff);
  }
}

void displayPrint(char *format,...)
{
  va_list args;
  va_start (args,format);
  _dpx(format, false, args);
  va_end(args);
}

void displayPrintln(char *format,...)
{
  va_list args;
  va_start (args,format);
  _dpx(format, true, args);
  va_end (args);
}

void displayPrintln(const __FlashStringHelper *ifsh, ...) {
  va_list args;
   char PROGMEM *format = ( char PROGMEM *)ifsh;
  va_start (args,format);
  _dpx(format, true, args);
  va_end (args);
}


void setup()  
{
    
  // connect at 115200 so we can read the GPS fast enough and echo without dropping chars
  // also spit it out
  Serial.begin(115200);
  Serial.println(F("GPS/LCD basic test!"));

  // 9600 NMEA is the default baud rate for Adafruit MTK GPS's- some use 4800
  GPS.begin(9600);
  
  // uncomment this line to turn on RMC (recommended minimum) and GGA (fix data) including altitude
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  // uncomment this line to turn on only the "minimum recommended" data
  //GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);
  // For parsing data, we don't suggest using anything but either RMC only or RMC+GGA since
  // the parser doesn't care about other sentences at this time
  
  // Set the update rate
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);   // 1 Hz update rate
  // For the parsing code to work nicely and have time to sort thru the data, and
  // print it out we don't suggest using anything higher than 1 Hz

  // the nice thing about this code is you can have a timer0 interrupt go off
  // every 1 millisecond, and read data from the GPS for you. that makes the
  // loop code a heck of a lot easier!
  useInterrupt(true);

  pinMode(PIN_GPS_FIX, OUTPUT);
  
  display.begin();
  // init done
  
  // you can change the contrast around to adapt the display
  // for the best viewing!
  display.setContrast(50);

  display.display(); // show splashscreen
  delay(2000);
  display.clearDisplay();   // clears the screen and buffer
  display.display();

  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(0,0);
  displayPrintln("GPS Monitor");
  display.display();

}


// Interrupt is called once a millisecond, looks for any new GPS data, and stores it
SIGNAL(TIMER0_COMPA_vect) {
  char c = GPS.read();
  // if you want to debug, this is a good time to do it!
  if (GPSECHO)
    if (c) UDR0 = c;  
    // writing direct to UDR0 is much much faster than Serial.print 
    // but only one character can be written at a time. 
}

void useInterrupt(boolean v) {
  if (v) {
    // Timer0 is already used for millis() - we'll just interrupt somewhere
    // in the middle and call the "Compare A" function above
    OCR0A = 0xAF;
    TIMSK0 |= _BV(OCIE0A);
    usingInterrupt = true;
  } else {
    // do not call the interrupt function COMPA anymore
    TIMSK0 &= ~_BV(OCIE0A);
    usingInterrupt = false;
  }
}

uint32_t timer = millis();
void loop()                     // run over and over again
{
  // in case you are not using the interrupt above, you'll
  // need to 'hand query' the GPS, not suggested :(
  if (! usingInterrupt) {
    // read data from the GPS in the 'main loop'
    char c = GPS.read();
    // if you want to debug, this is a good time to do it!
    if (GPSECHO)
      if (c) UDR0 = c;
      // writing direct to UDR0 is much much faster than Serial.print 
      // but only one character can be written at a time. 
  }
  
  // if a sentence is received, we can check the checksum, parse it...
  if (GPS.newNMEAreceived()) {
    // a tricky thing here is if we print the NMEA sentence, or data
    // we end up not listening and catching other sentences! 
    // so be very wary if using OUTPUT_ALLDATA and trytng to print out data
    //Serial.println(GPS.lastNMEA());   // this also sets the newNMEAreceived() flag to false
  
    if (!GPS.parse(GPS.lastNMEA()))   // this also sets the newNMEAreceived() flag to false
      return;  // we can fail to parse a sentence in which case we should just wait for another
  }

  // if millis() or timer wraps around, we'll just reset it
  if (timer > millis())  timer = millis();

  // approximately every 2 seconds or so, print out the current stats
  if (millis() - timer > 2000) { 
    timer = millis(); // reset the timer
    
    Serial.print(F("\nTime: "));
    Serial.print(GPS.hour, DEC); Serial.print(F(":"));
    Serial.print(GPS.minute, DEC); Serial.print(F(":"));
    Serial.print(GPS.seconds, DEC); Serial.print(F("."));
    Serial.println(GPS.milliseconds);
    Serial.print(F("Date: "));
    Serial.print(GPS.day, DEC); Serial.print(F("/"));
    Serial.print(GPS.month, DEC); Serial.print(F("/20"));
    Serial.println(GPS.year, DEC);
    Serial.print(F("Fix: ")); Serial.print((int)GPS.fix);
    Serial.print(F(" quality: ")); Serial.println((int)GPS.fixquality); 
    Serial.print(F("Satellites: ")); Serial.println((int)GPS.satellites);

    display.clearDisplay();
    display.setCursor(0,0);
    if (GPS.day == 0) {
      displayPrintln("GPS Monitor");
    } else {
      displayPrint("%s%d/%s%d/%s%d %s%d:%s%d", GPS.day < 10 ? "0" : "", GPS.day, GPS.month < 10 ? "0" : "", GPS.month, GPS.year < 10 ? "0" : "", GPS.year, GPS.hour < 10 ? "0" : "", GPS.hour, GPS.minute < 10 ? "0" : "", GPS.minute);
    }
    displayPrintln("Fix: %s Sat: %d", GPS.fix ? "Y" : "N", GPS.satellites); 
    
    if (GPS.fix) {
      Serial.print(F("Location: "));
      Serial.print(GPS.latitude, 4); Serial.print(GPS.lat);
      Serial.print(F(", ")); 
      Serial.print(GPS.longitude, 4); Serial.println(GPS.lon);
      
      Serial.print(F("Speed (mph): ")); Serial.println((int)(GPS.speed * 1.15078));
      Serial.print(F("Angle: ")); Serial.println(cardinal(GPS.angle));
      Serial.print(F("Altitude: ")); Serial.println(GPS.altitude);

      dtostrf(GPS.latitude, 9, 4, dtostrfbuffer);
      displayPrintln("%s%c", dtostrfbuffer, GPS.lat);
      dtostrf(GPS.longitude, 9, 4, dtostrfbuffer);
      displayPrintln("%s%c", dtostrfbuffer, GPS.lon);
      displayPrintln("%d mph -> %s", (int)(GPS.speed * 1.15078), cardinal(GPS.angle));
      dtostrf(GPS.altitude, 5, 0, dtostrfbuffer);
      displayPrintln("Alt: %s m", dtostrfbuffer);
    }
    display.display();

  }
  digitalWrite(PIN_GPS_FIX, GPS.fix ? HIGH : LOW); 
}

/**
 * TinyGPS implementation
 */
const char *cardinal (float course)
{
  static const char* directions[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};

  int direction = (int)((course + 11.25f) / 22.5f);
  return directions[direction % 16];
}


