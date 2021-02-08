// Rick Steeves
// 210207
//
// Flow monitor code for integration into MisterHouse
// Using the YF-S201 Water Flow Sensor(s)
// Configured for two separate flow sensors on an Arduino Ethernet Shield with SSD
// Log date set vi NTP (which periodically renews the time/date)
// Data is logged twice:
//   last.txt is the last total flow (so total flow persists from one boot to the next)
//   simple.txt is comma-delimited for data responses
//
// Output is: 
//   Output to Serial
//   Displayed on LCD screen
//   Output as a web page (values then read in by MisterHouse)


// functions
// Flow sensor
// LCD
// SD Card
// NTP/Ethernet (reset every 7 days)
// web server

//This is the flow sensor input pin on the Arduino
#define flowSensorPin1 2
// defining flowSensorPin2 as 3 doesn't work.

// YF - S201: every liter of water that flows, the Hall Sensor outputs 450 Pulses
// YF – S402: every liter of water that flows, the Hall Sensor outputs 4380 pulses
//#define pulse 2.25
// calibrated up 25%
#define pulse 2.8125
   
const int flowInterval = 30;
double flowRate1  = 0;    //This is the value we intend to calculate.
double flowMinute1[flowInterval] = {}; // tracking a running incremental total
double flowHour1 = 0; // tracking a running hourly total
double flowTotal1 = 0;   //This is the value we intend to calculate.
double flowRate2  = 0;    //This is the value we intend to calculate.
double flowMinute2[flowInterval] = {}; // tracking a running hourly total
double flowHour2  = 0; // tracking a running incremental total
double flowTotal2 = 0;   //This is the value we intend to calculate.
int flowCounter = 0;   // to loop through our array counting minutes;
volatile int count1 = 0; //This integer needs to be set as volatile to ensure it updates correctly during the interrupt process.
volatile int count2 = 0; //This integer needs to be set as volatile to ensure it updates correctly during the interrupt process.
unsigned long currentMillis;
unsigned long cloopTime;
// Interrupts on the Arduino Uno: 0(pin 2), 1 (pin3)
// Interrupts on Mega2560: 0(pin 2), 1 (pin 3), 2 (pin 21), 3 (pin 20), 4 (pin 19), 5 (pin 18)
// pins 0 and 1 are used by the serial interface - don't use them if using serial
// 0 = digital pin 2
#define sensorInterrupt1 0
// 0 = digital pin 3
#define sensorInterrupt2 1
String logtime = String(30); // a calculated log time
String flowRate = String(16); // Used for Display
String flowMinute = String(16); // Used for Display
String flowHour = String(16); // Used for Display
String flowTotal = String(16); // used for Display
String flowLCD = String(16); // used for Display

//LCD
// https://www.makerguides.com/character-lcd-arduino-tutorial/
// HD44780 driver
// 1 VSS GND Arduino Signal ground
// 2 VDD 5 V Arduino Logic power for LCD
// 3 V0  10 kO potentiometer Contrast adjustment
// 4 RS  Register Select signal
// 5 R/W GND Arduino Read/write select signal
// 6 E   Enable signal
// 7 – 14  D0 – D7 – Data bus lines used for 8-bit mode
// 11 – 14 D4 – D7 Pin 4 – 7 Arduino Data bus lines used for 4-bit mode [What we are using]
// 15  A (LED-)  5 V Arduino Anode for LCD backlight    [ These are reversed from the website - Rick ]
// 16  K (LED+)  GND Arduino Cathode for LCD backlight  [ These are reversed from the website - Rick ]
//LiquidCrystal lcd(rs, enable, d4, d5, d6, d7);
#include <LiquidCrystal.h>

// Breadboard
//LiquidCrystal lcd( 4, 5, 6, 7, 8, 9 ); // Creates an LC object. Parameters: (rs, enable, d4, d5, d6, d7)
// PCB
//LiquidCrystal lcd( 9, 8, 7, 6, 5, 4 ); // Creates an LC object. Parameters: (rs, enable, d4, d5, d6, d7)
const int rs = 9, en = 8, d4 = 7, d5 = 6, d6 = 5, d7 = 4;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);


// SD Card
#include <SD.h>
#include <SPI.h>
File myFile;
char fileName[] = "simple.txt";
const int chipSelect = 4;
char charRead;

//Ethernet
//The Arduino board communicates with the shield using digital pins 11, 12, and 13 
//on the Uno and pins 50, 51, and 52 on the Mega. On both boards, pin 10 is used as SS. 
//On the Mega, the hardware SS pin, 53, is not used to select the Ethernet controller chip, 
//but it must be kept as an output or the SPI interface won't work.
#include <Ethernet.h>
#include <EthernetUdp.h>
// Enter a MAC address for your controller below.
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
byte ip[] = { 192, 168, 2, 121 };                   // ip in lan
byte gateway[] = { 192, 168, 2, 1 };                   // ip in lan
byte subnet[] = { 255, 255, 255, 0 };                   // ip in lan
byte dns[] = { 8, 8, 8, 8 };                   // ip in lan

// NTP
unsigned int localPort = 8888;       // local port to listen for UDP packets
char timeServer[] = "time.nist.gov"; // time.nist.gov NTP server
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
// A UDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

// Time
#include <Time.h>
#include <TimeLib.h>
unsigned long epochStart; // used for epoch seconds
unsigned long epoch; // used for epoch seconds + the time since all this started.
// TimeAlarm (for NTP)
#include <TimeAlarms.h>

// web server
// Initialize the Ethernet server library with port
// (port 80 is default for HTTP):
EthernetServer server(80);

void setup() {
  // put your setup code here, to run once:
  // Flow sensor input
  pinMode(flowSensorPin1, INPUT);           //Sets the pin as an input
  digitalWrite(flowSensorPin1, HIGH); // Optional Internal Pull-Up
  //pinMode(flowSensorPin1, INPUT_PULLUP);           //Sets the pin as an input, and use the internal Arduino pullup instead of the resistor
  Serial.begin(9600);  //Start Serial
  while (!Serial) { ; } // wait for serial port to connect. Needed for native USB port only

  lcd.begin(16, 2); // don't need clear as display cleared during initialization
  lcd.setCursor(0, 0);
  lcd.print("Water Flow Meter");
  lcd.setCursor(0, 1);
  lcd.print("No Data");

  currentMillis = millis();
  cloopTime = currentMillis;

  // SD Card
  if (SD.begin(chipSelect)) { // Check to make sure there's a SD card ready
  Serial.println("SD ready"); 
  } else {
    Serial.println("SD missing/failure");
    lcd.setCursor(0, 1);
    lcd.print("SD failure");
    while(1);  //wait here forever
  }

  myFile = SD.open("simple.txt", FILE_WRITE);
  if (myFile) {
    Serial.println("Starting ...");
    myFile.close();
  } else {
    Serial.println("Error opening Logfile");
    lcd.print("Err: Logfile");
    while(1);  //wait here forever
  }

  //Read in the last totals when device reset.
  // comma-delimited
  myFile = SD.open("last.txt");
  if (myFile) {
    String lastflow = myFile.readStringUntil('\r');
    Serial.println("Last Total (last.txt):" + lastflow);
//    // parse a CSV string of no more than 2 items
//    for (int i = 0; i < lastflow.length(); i++) {
//      if (lastflow.substring(i, i+1) == ",") {
//        flowTotal1 = lastflow.substring(0,i).toDouble();
//        flowTotal2 = lastflow.substring(i+1).toDouble();
//        // no need to keep going once we find the comma
//        break;
//      }
//    }
    flowTotal1 = lastflow.toDouble();
    Serial.println("flowTotal1:" + String(flowTotal1));
    Serial.println("flowTotal2:" + String(flowTotal2));
    // close the file:
    myFile.close();
  } else {
    // if the file didn't open, print an error:
    Serial.println("error opening last.txt");
    lcd.setCursor(0, 1);
    lcd.print("Err open Last");
  }

  // Ethernet
  Ethernet.begin(mac, ip, dns, gateway, subnet);
  //Ethernet.begin(mac);
  //if (Ethernet.begin(mac) == 0) {
  //  Serial.println("Failed to configure DHCP");
  //  // no point in carrying on, so do nothing forevermore:
  //}
  // Determine Ethernet Controller
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println("Ethernet shield was not found.");
    lcd.setCursor(0, 1);
    lcd.print("No Net shield");
     while(1);  //wait here forever
  }
  else if (Ethernet.hardwareStatus() == EthernetW5100) {
    Serial.println("W5100 Ethernet controller detected.");
  }
  if ( Ethernet.linkStatus() == LinkOFF ) {
      Serial.println( "Ethernet cable not connected." );
      lcd.setCursor(0, 1);
      lcd.print("No Net cable");
      while(1);  //wait here forever
  }
  Serial.print("IP Address: ");
  Serial.println(Ethernet.localIP());
  
  // NTP
  callNTP();
  
  // Web server
  server.begin();
  Serial.print("Web Server: ");
  Serial.println(Ethernet.localIP());

  attachInterrupt(sensorInterrupt1, Flow1, RISING);  //Configures interrupt 0 (pin 2 on the Arduino Uno) to run the function "Flow1". // sensorInterrupt = 0
  attachInterrupt(sensorInterrupt2, Flow2, RISING);  //Configures interrupt 0 (pin 2 on the Arduino Uno) to run the function "Flow2". // sensorInterrupt = 0
  interrupts();   //Enables interrupts on the Arduino
  Alarm.timerRepeat(14400, callNTP);          // update time every 4 hours
  Alarm.timerRepeat(60, resetMinute);        // reset values every minute
  Serial.println("Monitoring Starting!");
}

void loop() {
  currentMillis = millis();
  // Every second, calculate and print liters/hour
  if (currentMillis >= (cloopTime + 1000))
  {
    // Disable the interrupt while calculating flow rate and sending the value to the host
    //201215 noInterrupts(); //Disable the interrupts on the Arduino
    //detachInterrupt(sensorInterrupt1); // sensorInterrupt = 0
    cloopTime = currentMillis; // Updates cloopTime

    //Start the math
    if (count1 != 0  || count2 != 0) {  // if there is output from the flow sensor
      flowRate1                  = (count1 * pulse);        //Take counted pulses in the last second and multiply by 2.25mL. Another sites suggests / 7.5
      flowRate1                  = flowRate1 * 60;         //Convert seconds to minutes, giving you mL / Minute
      flowRate1                  = flowRate1 / 1000;       //Convert mL to Liters, giving you Liters / Minute
      flowMinute1[flowCounter]  += (count1 * pulse) / 1000; //The total amount of fluid through the system in the last minute
      flowHour1                 += (count1 * pulse) / 1000;
      flowTotal1                += (count1 * pulse) / 1000; //The total amount of fluid through the system
      
      flowRate2                  = (count2 * pulse);        //Take counted pulses in the last second and multiply by 2.25mL
      flowRate2                  = flowRate2 * 60;         //Convert seconds to minutes, giving you mL / Minute
      flowRate2                  = flowRate2 / 1000;       //Convert mL to Liters, giving you Liters / Minutef
      flowMinute2[flowCounter]  += (count2 * pulse) / 1000; //The total amount of fluid through the system in the last minute
      flowHour2                 += (count2 * pulse) / 1000;
      flowTotal2                += (count2 * pulse) / 1000; //The total amount of fluid through the system

      epoch = epochStart + currentMillis / 1000; // update epoch to now
      // Output the flow log entry
      LogTime();
      Serial.print(logtime);
      // Print the flow data
      // note these are read off the website in as an array, so changing the order will break things there. 
      // Strings build to make it easy to swap out display for different numbers of flow sensors
      flowRate = "";
      if (flowRate1 < 10) { flowRate = "0"; }
      flowRate   += String(flowRate1) + ",";
      if (flowRate2 < 10) { flowRate += "0"; }
      flowRate   += String(flowRate2); 
      
      flowMinute = String(flowMinute1[flowCounter]) + "," + String(flowMinute2[flowCounter]);
      flowHour   = String(flowHour1) + "," + String(flowHour2); 

      // get rid of decimals for large values of flowTotal
      flowTotal = "";
      if (flowTotal1 < 10) { flowTotal += "0"; }
      if (flowTotal1 > 999.99)  { 
          flowTotal = String(int(flowTotal1));
      } else {
         flowTotal = String(flowTotal1);
      }
      flowTotal += ",";   
      if (flowTotal2 < 10) { flowTotal += "0"; }
      if (flowTotal2 > 999.99)  { 
          flowTotal += String(int(flowTotal2));
      } else {
         flowTotal += String(flowTotal2);
      }
      
      Serial.print("\tR:"         + flowRate   + " Lpm");
      Serial.print("\tV(Minute):" + flowMinute + " L");
      Serial.print("\tV(Hour):"   + flowHour   + " L");
      Serial.print("\tV(Total):" + flowTotal   + " L");
      Serial.print("\tCount:"        + String(flowCounter));
      Serial.println();

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("R:" + flowRate +"Lpm");
      lcd.setCursor(0, 1);
      lcd.print("V:" + flowTotal + "L");

      myFile = SD.open("simple.txt", FILE_WRITE);// Open SD card for writing
      if (myFile) {
        // write temps to SD card
        myFile.print(logtime);
        myFile.print(",R:,"          + flowRate + ",Lpm");
        myFile.print(",V(Minute):,"  + flowMinute);
        myFile.println(",V(Total): " + flowTotal + ",L(total)");
        // close the file
        myFile.close();
      } 
      else { Serial.println("Error opening file in loop."); }
    }
    else {
      // if we have no flow data, reset the LCD
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("R:00.00,00.00Lpm");
      // These aren't changing so no need to rewrite them.
      lcd.setCursor(0, 1);
      lcd.print("V:" + flowTotal + "L");
    }

    count1 = 0;      // Reset the counter so we start counting from 0 again
    count2 = 0;      // Reset the counter so we start counting from 0 again
    // Enable the interrupt again now that we've finished sending output
    //201215interrupts();   //Enables interrupts on the Arduino
    //attachInterrupt(sensorInterrupt1, Flow1, RISING);  // sensorInterrupt = 0
    //attachInterrupt(sensorInterrupt2, Flow2, RISING);  // sensorInterrupt = 0
  }
  WebServer();
  Alarm.delay(0); // update the timers
}

void Flow1() {
  count1++; //Every time this function is called, increment "count" by 1
}

void Flow2() {
  count2++; //Every time this function is called, increment "count" by 1
}

// NTP
void sendNTPpacket(char* address) 
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // As NTP fields have been given values, send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

void callNTP() {
  Serial.println("Initialize UDP");
  Udp.begin(localPort);
  Serial.println("Transmit NTP Request");
  sendNTPpacket(timeServer); // send an NTP packet to a time server
  // wait for response
  uint32_t beginWait = millis();
  while (millis() - beginWait < 2000) {
    if (Udp.parsePacket() >= 48) {
      Serial.println("Receiving NTP Response");
      // We've received a packet, read the data from it
      Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

      // the timestamp starts at byte 40 of the received packet and is four bytes,
      // or two words, long. First, extract the two words:
      unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
      unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
      // combine the four bytes (two words) into a long integer
      // this is NTP time (seconds since Jan 1 1900):
      unsigned long secsSince1900 = highWord << 16 | lowWord;
      //Serial.print("Seconds since Jan 1 1900 = ");
      //Serial.println(secsSince1900);

      // now convert NTP time into epoch time:
      Serial.print("Unix time = ");
      // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
      //const unsigned long seventyYears = 2208988800UL;
      // subtract seventy years:
      epochStart = secsSince1900 - 2208988800UL; // 70 years
      // Fix for GMT
      epochStart = epochStart + 25200 - 43200;
      // print Unix time:
      epoch = epochStart;
      Serial.println(epoch);
      LogTime();
      Serial.println("LogTime: " + String(logtime));
      Ethernet.maintain();
    }
  }
  
}

void LogTime() 
{
  //I'd return a value here, except that returning values in arduino is apparently hard, so screw that.
      logtime = String(year(epoch));
      if (month(epoch) < 10 )  { logtime += '0'; }
      logtime += String(month(epoch));
      if (day(epoch) < 10 )  { logtime += '0'; }
      logtime += String(day(epoch)) + " ";
      logtime += String(hour(epoch)) + ":";
      if (((epoch % 3600) / 60) < 10) { logtime += '0'; }
       logtime += String(minute(epoch) ) + ":";
      if ((epoch % 60) < 10) { logtime += '0'; }
      logtime += String(second(epoch));
}

void WebServer () 
{
   // listen for incoming clients
  EthernetClient client = server.available();
  if (client) {
    Serial.println("new client");
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");  // the connection will be closed after completion of the response
          client.println("Refresh: 5");  // refresh the page automatically every 5 sec
          client.println();
          client.println("<!DOCTYPE HTML>");
          client.println("<html>");
          // Print the flow data
//          client.print("Rate (Lpm):\t" + String(flowRate1) + "\t" + String(flowRate2));
//          client.print("\tV(L/last minute):\t" + String(flowMinute1[flowCounter]) + "\t" + String(flowMinute2[flowCounter]));
//          client.print("\tV(L/last hour):\t" + String(flowHour1) + "\t" + String(flowHour2));       
//          client.println("\tV(L/total):\t" + String(flowTotal1) +"\t" + String(flowTotal2));
          client.print("Rate (Lpm):\t" + String(flowRate1));
          client.print("\tV(L/last minute):\t" + String(flowMinute1[flowCounter]));
          client.print("\tV(L/last hour):\t" + String(flowHour1));       
          client.println("\tV(L/total):\t" + String(flowTotal1) + "\tL");
          client.println("<br />");
          client.println("</html>");
          break;
        }
        // you're starting a new line
        if (c == '\n') { currentLineIsBlank = true; }
        // you've gotten a character on the current line
        else if (c != '\r') { currentLineIsBlank = false; }
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
    Serial.println("client disconnected");
  }
}

void resetMinute() 
{
  LogTime();
  // This reset code can go away once the system is stable
  Serial.print(logtime);
  Serial.print("\tReset:" + String(flowCounter));
  Serial.print(","       + flowMinute);
  Serial.print(","       + flowHour);
  Serial.println(","     + flowTotal);
  
  // keep a running total for the last flowInterval minutes
  byte index = 0;
  // reset the value for the last flowInterval minutes
  flowHour1 = 0;
  flowHour2 = 0;
  // Sum up all the results in the last flowInterval minutes
  for(index = 0; index < flowInterval ; index++)  { 
    flowHour1 += flowMinute1[index]; 
    flowHour2 += flowMinute2[index]; 
  }
  // reset the most recent minute's value
  // increment the minute counter
  flowCounter += 1;
  // reset the minute counter if it's the next flowInterval minutes
  if (flowCounter > (flowInterval-1)) { 
    flowCounter = 0;
    // write out the data for this hour (so this function is hourly
    myFile = SD.open("last.txt", O_WRITE | O_CREAT |O_TRUNC);// Open SD card for writing, not append
    if (myFile) {
        // write temps to SD card
//        myFile.println(String(flowTotal1) + "," + String(flowTotal2));         //Print the variable flowRate and flowTotal to Serial
        myFile.println(flowTotal1);         //Print the variable flowRate and flowTotal to Serial
        myFile.println(logtime);
        // close the file
        myFile.close();
      } else {
        Serial.println("Error opening last.txt file.");
      }
  } 

  // reset the counter for this minute, so it's blank when we start.
  flowMinute1[flowCounter] = 0; 
  flowMinute2[flowCounter] = 0; 
  flowRate1 = 0;
  flowRate2 = 0;
}


// Make custom characters:
//byte Heart[] = {
//  B00000,
//  B01010,
//  B11111,
//  B11111,
//  B01110,
//  B00100,
//  B00000,
//  B00000
// };
