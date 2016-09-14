#include <Wire.h>
#include <SPI.h>
#include <ctime>
#include <string>
#include <sstream>
#include <iomanip>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME280.h"
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <FirebaseArduino.h>

#define BME_SCK 2
#define BME_SDO 0 //MISO
#define BME_SDI 4 //MOSI
#define BME_CS 5

// Set these to run example.
#define FIREBASE_HOST "weather-station-69119.firebaseio.com"
#define FIREBASE_AUTH "i1HCioFFfSsoiNOQuZWGg5nqrS2EGhhn4TXVK6AU"
#define WIFI_SSID "D3F2C"
#define WIFI_PASSWORD "97YZPWV9JPKVKCBV"

#define localPort  2390      // local port to listen for UDP packets
#define ET_TIME   -4

#define RW_DELAY_MILSEC 5000 

/* Don't hardwire the IP address or we won't get the benefits of the pool.
 *  Lookup the IP address for the host name instead */
IPAddress timeServerIP(129, 6, 15, 28); // time.nist.gov NTP server
//IPAddress timeServerIP; // time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov";

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

//constants are used for DB fields
const String yearPath       = "Year";
const String monthPath      = yearPath + "/Month";
const String dayPath        = monthPath + "/Day";
const String hourPath       = dayPath + "/Hour";
const String minPath        = dayPath + "/Min";
const String tempPath       = dayPath + "/temperature";
const String humPath        = dayPath + "/humidity";
const String pressPath      = dayPath + "/pressure";
const String latPath        = dayPath + "/latitude";
const String lonPath        = dayPath + "/longitude";



// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;


Adafruit_BME280 bme(BME_CS, BME_SDI, BME_SDO,  BME_SCK);

//
unsigned long sendNTPpacket(IPAddress& address)
{
  Serial.println("sending NTP packet...");
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

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

//
void setupBme() 
{

  while (!bme.begin()) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    delay(100);
  }

}

//
void printBmeData (float temp, float pres, float hum)
{
  Serial.print("Temperature ");
  Serial.println(temp);
  Serial.print("Pressure ");
  Serial.println(pres);
  Serial.print("humidity");
  Serial.println(hum);
  Serial.println();
}

//
void setupWiFi()
{
    // connect to wifi.
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  udp.begin(localPort);
}

//
void setupFireBase () 
{
   setupWiFi();
   Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
}

//
bool GetCurDateTime(tm& date)
{
    sendNTPpacket(timeServerIP); // send an NTP packet to a time server
    // wait to see if a reply is available
    delay(5000);
  
    int cb = udp.parsePacket();
    if (!cb)
      return false;

    Serial.print("packet received, length=");
    Serial.println(cb);
    // We've received a packet, read the data from it
    udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
   
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    const time_t epoch = secsSince1900 - seventyYears;

    date = *localtime(&epoch);  
    return true;
 
}


//
void writeToFireBase (const float temp, const float pres, const float hum, const tm& now)
{
  // set value


  const int year    = (now.tm_year + 1900);
  const int month   = (now.tm_mon + 1);
  const int day     = now.tm_mday;
  const int hour    = abs(now.tm_hour + ET_TIME) % 24;
  const int min     = now.tm_min;


  //const unsigned long timestamp = year * 100000000 + month*1000000 + day * 10000 + hour*100 + min;
  
  Serial.print("Timestamp: ");
  //Serial.println(timestamp);

  Firebase.pushInt(yearPath, year);
  Firebase.pushInt(monthPath, month);
  Firebase.pushInt(dayPath, day);
  Firebase.pushInt(hourPath, hour);
  Firebase.pushInt(minPath,min);
  
  Firebase.pushFloat(tempPath, temp);
  Firebase.pushFloat(humPath, hum);
  Firebase.pushFloat(pressPath, pres);
  Firebase.pushFloat(latPath, 42.4553210);
  Firebase.pushFloat(lonPath, -71.1316960);
  
  // handle error
  if (Firebase.failed()) {
    Serial.print("setting /number failed:");
    Serial.println(Firebase.error());  
  }
 
}


//
void setup() {

  Serial.begin(115200);
  setupBme();
  setupFireBase();
  Serial.print("setup completed");
}


//
void loop() 
{
  tm curDate;
  if(!GetCurDateTime(curDate))
    return;
  
  float temp  = bme.readTemperature();
  float pres  = bme.readPressure();
  float hum   = bme.readHumidity();

  printBmeData (temp, pres, hum);
  writeToFireBase (temp, pres, hum, curDate);  

  delay(RW_DELAY_MILSEC);

}
