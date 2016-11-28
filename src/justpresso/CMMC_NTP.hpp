#ifndef CMMC_NTP_H
#define CMMC_NTP_H
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>


const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
const char* ntpServerName = "time.nist.gov";
class CMMC_NTP
{
  private:
    const unsigned int localPort = 2390;      // local port to listen for UDP packets
    unsigned long sleepS;
    unsigned long minute;
    unsigned long hour;
    //IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
    IPAddress timeServerIP; // time.nist.gov NTP server address
    byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

    // A UDP instance to let us send and receive packets over UDP
    WiFiUDP udp;
public:
  CMMC_NTP() {
    sleepS = 0;
    minute = 0;
    hour = 0;
  }

  unsigned long getSleepTime() {
    return this->sleepS;
  }

  unsigned long sendNTPpacket(IPAddress& address);
  void getTimeFromNtp(void);
  void printUnixTimeToUTCTime(unsigned long epoch);
  void init();
  void setMemoryDate(unsigned long hour, unsigned long minute) {
    this->hour = hour;
    this->minute = minute;
  }
};

// send an NTP request to the time server at the given address
unsigned long CMMC_NTP::sendNTPpacket(IPAddress& address)
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

void CMMC_NTP::init() {
  Serial.println("Starting UDP");
  udp.begin(localPort);
  delay(10);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());
}

void CMMC_NTP::getTimeFromNtp() {
  //get a random server from the pool
  int counter = 0;
  while(true) {
    WiFi.hostByName(ntpServerName, timeServerIP);
    sendNTPpacket(timeServerIP); // send an NTP packet to a time server
    counter++;
    // wait to see if a reply is available
    if (counter>= 15) {
      ESP.reset();
    }

    delay(1000);


    int cb = udp.parsePacket();
    if (!cb) {
     Serial.println("no packet yet");
    }
    else {
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
     Serial.print("Seconds since Jan 1 1900 = " );
     Serial.println(secsSince1900);

     // now convert NTP time into everyday time:
     Serial.print("Unix time = ");
     // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
     const unsigned long seventyYears = 2208988800UL;
     // subtract seventy years:
     unsigned long epoch = secsSince1900 - seventyYears;
     printUnixTimeToUTCTime(epoch);
     break;
   }
  }
}

void CMMC_NTP::printUnixTimeToUTCTime(unsigned long epoch) {
  int _current_minute;
  int _current_hour;
  int _current_second;

   // print the hour, minute and second:
   _current_hour = (epoch  % 86400L) / 3600;
   _current_minute = (epoch  % 3600) / 60;
   _current_second = (epoch % 60);
   _current_hour += 7;
   _current_hour %= 24;

   Serial.print("XThe UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
   Serial.print(_current_hour); // print the hour (86400 equals secs per day)
   Serial.print(':');

   if ( _current_minute < 10 ) {
     // In the first 10 minutes of each hour, we'll want a leading '0'
     Serial.print('0');
   }
   Serial.print(_current_minute); // print the minute (3600 equals secs per minute)
   Serial.print(':');
   if ( _current_second < 10 ) {
     // In the first 10 seconds of each minute, we'll want a leading '0'
     Serial.print('0');
   }
   Serial.println(_current_second); // print the second

  Serial.printf ("[current]: %d:%d vs [save] %d:%d ", _current_hour, _current_minute, hour, minute);

  if ( (_current_hour > hour) || (_current_hour == hour && _current_minute >= minute)) {
    sleepS = 86400 - (_current_hour-hour)*3600;
    if (_current_minute > minute) {
      sleepS -= (_current_minute-minute)*60;
    }
    else {
      sleepS += (minute-_current_minute)*60;
    }
    sleepS -= _current_second;
    //  + (minute-_current_minute)*60-_current_second;
    Serial.printf("THIS IS THE PAST!!!! WAKE UP IN %lu\r\n", sleepS);
  }
  else {
    sleepS = (hour - _current_hour)*3600 + (minute-_current_minute)*60-_current_second;
    Serial.printf("WE HAVE A FUTURE!!!! WAKE UP AT %lu\r\n", sleepS);
  }

}


#endif
