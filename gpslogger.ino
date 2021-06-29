#include <Arduino.h>
#include <NMEAGPS.h>
#include <GPSport.h>
#include <SPI.h>
#include <SdFat.h>

/*
 SPI pins:
  - CS   - pin 10
  - MOSI - pin 11
  - MISO - pin 12
  - CLK  - pin 13
*/

SdFat SD;
const int chipSelect = 10;
File logfile;

static NMEAGPS gps; 
static gps_fix fix;

const int PIN_BUZZER = 4;
const int PIN_SHUTDOWN = 5;
const int PIN_BATTERY = A2;
const int PIN_LED = 6;

// used to beep when the first fix is obtained
bool first_fix_obtained = false;
bool first_fix_reported = false;
unsigned long first_fix_beep_time = 0L;
int first_fix_beep_count = 0;

// used to beep when battery is low
const float LOW_BATT_VOLTAGE = 3.2;
unsigned long low_batt_beep_time = 0L;
bool low_batt_reported = false;
const unsigned long LOW_BATT_TIMEOUT = 60000L;

// shutdown switch status
bool shutdown_status;

const unsigned long FLUSH_TIMEOUT = 60000L;

unsigned long lastLoggingDuration = 0L;
unsigned long startLoggingTime;
unsigned long lastFlushTime = 0L;

// initialized to 9 so the also the first fix is recorded
int fix_count = 9;

void setup() {
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_SHUTDOWN, INPUT_PULLUP);
  pinMode(PIN_BATTERY, INPUT);
  pinMode(PIN_LED, OUTPUT);
  
  DEBUG_PORT.begin(9600);
  while (!DEBUG_PORT)
    ; // wait for serial port to connect. 

  initSD();

  beepBatteryStatus();

  shutdown_status = getShutdownStatus();

  gpsPort.begin(9600);

  // Configure the GPS: 1Hz update rate for MTK GPS devices
  // gps.send_P(&gpsPort, F("PMTK220,1000"));
} // setup

void loop(){
  GPSloop();

  if (gps.overrun()) {
    gps.overrun(false);
    //DEBUG_PORT.println( F("DATA OVERRUN: fix data lost!") );
    //beep(6, 100, 100);
  }

  if (!first_fix_reported && first_fix_obtained) {
    // beep to report first fix
    if ((millis() - first_fix_beep_time) >= 200L) {
      digitalWrite(PIN_BUZZER, !digitalRead(PIN_BUZZER));
      first_fix_beep_time = millis();
      first_fix_beep_count += 1;
    }
    // 4 beep -> (ON, OFF, ON, OFF, ON, OFF, ON, OFF)
    if (first_fix_beep_count >= 8) {
      // stop the beep
      first_fix_reported = true;
    }
  }

  if (!low_batt_reported && getBatteryVoltage() < LOW_BATT_VOLTAGE && (millis()-low_batt_beep_time) > LOW_BATT_TIMEOUT) {
    low_batt_reported = true;
    digitalWrite(PIN_BUZZER, HIGH);
    low_batt_beep_time = millis();
  }
  if (low_batt_reported && (millis()-low_batt_beep_time) >= 200L) {
    low_batt_reported = false;
    digitalWrite(PIN_BUZZER, LOW);
  }

  if (shutdown_status != getShutdownStatus()) {
    logfile.close();
    digitalWrite(PIN_LED, HIGH);
    beepBatteryStatus();
    while (true) {}
  }
} // loop

static void GPSloop () {
  while (gps.available(gpsPort)) {
    fix = gps.read();
    
    if (fix.valid.status && (fix.status >= gps_fix::STATUS_STD)) {
      first_fix_obtained = true;
      fix_count += 1;

      if (fix_count >= 10 || (millis() - startLoggingTime) > 10000L) {
        fix_count = 0;
        startLoggingTime = millis();
      
        writeData();
            
        logfile.print(lastLoggingDuration); // write how long the previous logging took
        logfile.println();
      
        // This flushes once in a while.
        if (startLoggingTime - lastFlushTime > FLUSH_TIMEOUT) {
          //logfile.print(F("flush: ")); // DEBUG
          //logfile.print(startLoggingTime); // DEBUG
          //logfile.print(','); // DEBUG
          //logfile.println(lastFlushTime); // DEBUG
          lastFlushTime = startLoggingTime; // close enough
          logfile.flush();
        }
      
        // All logging is finished, figure out how long that took.
        // This will be written in the *next* record.    
        lastLoggingDuration = millis() - startLoggingTime;
        
      }
    }
  }

} // GPSloop

void initSD () {
    // see if the card is present and can be initialized:
    if (!SD.begin(chipSelect)) {
      // Initialization failed
      while (true) {
        beep(10, 100, 100);
        delay(1000);
      }
    }

    // SD initialization OK

    // Pick a numbered filename, 00 to 99.
    char filename[15] = "data_##.txt";

    for (uint8_t i=0; i<100; i++) {
      filename[5] = '0' + i/10;
      filename[6] = '0' + i%10;
      if (!SD.exists(filename)) {
        // Use this one!
        break;
      }
    }

    logfile = SD.open(filename, FILE_WRITE);
    if (!logfile) {
      // If the file can't be created for some reason:
      while (true) {
        beep(10, 100, 100);
        delay(1000);
      }
    }
    
} // initSD

static void writeData () {
  // datetime (UTC)
  logfile.print(fix.dateTime.year);
  logfile.print('-');
  if (fix.dateTime.month < 10)
    logfile.print('0');
  logfile.print(fix.dateTime.month);
  logfile.print('-');
  if (fix.dateTime.date < 10)
    logfile.print('0');
  logfile.print(fix.dateTime.date);
  logfile.print('T');
  if (fix.dateTime.hours < 10)
    logfile.print('0');
  logfile.print(fix.dateTime.hours);
  logfile.print(':');
  if (fix.dateTime.minutes < 10)
    logfile.print('0');
  logfile.print(fix.dateTime.minutes);
  logfile.print(':');
  if (fix.dateTime.seconds < 10)
    logfile.print('0');
  logfile.print(fix.dateTime.seconds);
  logfile.print(F("Z,"));
                  
  // latitude
  printL(logfile, fix.latitudeL());
  logfile.print(',');

  // longitude
  printL(logfile, fix.longitudeL());
  logfile.print(',');
      
  // elevation
  logfile.print(fix.alt.whole);
  logfile.print('.');
  logfile.print(fix.alt.frac / 10);
  logfile.print(',');
      
  // speed, written in m/s with 1 floating point digit
  logfile.print((int) (fix.speed_kph()*2.77778) / 10.0, 1);
      
  logfile.println();
}

static void beepBatteryStatus() {
  float voltage = getBatteryVoltage();
  if (voltage >= 3.7) {
    beep(3, 300, 250); // battery 3/3
  }
  else if (voltage >= 3.4) {
    beep(2, 400, 300); // battery 2/3
  }
  else {
    beep(1, 800, 0);   // battery 1/3
  }
}

static float getBatteryVoltage() {
  float accumulator = 0;
  for (int i = 0; i < 20; i++) {
    accumulator += analogRead(PIN_BATTERY);
  }

  return (accumulator/20)*0.004170; // *4.2/1007
}

static bool getShutdownStatus() {
  int total = 0;
  for (int i = 0; i < 10; i++) {
    total += digitalRead(PIN_SHUTDOWN);
  }
  return total > 5;
} // getShutdownStatus

static void beep (int n_times, int milliseconds_on, int milliseconds_off) {
  for (int i = 0; i < n_times; i++) {
    digitalWrite(PIN_BUZZER, HIGH);
    delay(milliseconds_on);
    digitalWrite(PIN_BUZZER, LOW);
    delay(milliseconds_off); 
  }
} // beep

//  Utility to print a long integer like it's a float with 9 significant digits.
void printL(Print & outs, int32_t degE7) {
  // Extract and print negative sign
  if (degE7 < 0) {
    degE7 = -degE7;
    outs.print('-');
  }

  // Whole degrees
  int32_t deg = degE7 / 10000000L;
  outs.print(deg);
  outs.print('.');

  // Get fractional degrees
  degE7 -= deg*10000000L;

  // Print leading zeroes, if needed
  if (degE7 < 10L)
    outs.print(F("000000"));
  else if (degE7 < 100L)
    outs.print(F("00000"));
  else if (degE7 < 1000L)
    outs.print(F("0000"));
  else if (degE7 < 10000L)
    outs.print(F("000"));
  else if (degE7 < 100000L)
    outs.print(F("00"));
  else if (degE7 < 1000000L)
    outs.print(F("0"));
  
  // Print fractional degrees
  outs.print(degE7);
}
