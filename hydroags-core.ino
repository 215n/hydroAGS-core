//-- hydroags-core
#define HA_VER "0.9.11"

//-- used libraries
#include <EtherCard.h>
#include <tinyFAT.h>
#include <FastLED.h>
#include <Timer.h>
#include <OneWire.h>
#include <DallasTemperature.h>

//-- Schluffi's libraries
#include "Relay.h"
#include "HCSR04.h"
#include "DHT.h"
#include "FC51.h"
#include "Funduino.h"
#include "SEN0169.h"

//-- pin defines
#define CS_SD 53
#define CS_NET 49

//- leds
#define CS_LED 5

//-- water level
#define WL_MAX 5 

//- luminosity sensor
#define CS_LUM A0

//-- PH
#define CS_PH A3

//- water level pipe
#define CS_WL_B A1
#define CS_WL_T A2

//- relais for engines
#define REL_ENG_1 39
#define REL_ENG_2 43
#define REL_ENG_3 41

//- relay for nutrient valve
#define REL_NUT 37  //10S/18ml | 30S/63ml

//- relay for heating
#define REL_HEAT 35

//- relais for fans
#define REL_FAN_1 31
#define REL_FAN_2 33

//- relay for air
#define REL_AIR 29

//- water level 
#define CS_DIST_TRIG 25
#define CS_DIST_ECHO 24

//- water temperature
#define CS_TEMP 6

//- humidity & temp
#define CS_HUM 22

//-- water level defines
HCSR04 waterLevel(CS_DIST_TRIG,CS_DIST_ECHO);

//-- LED defines
#define NUM_LEDS 4*29
CRGB leds[NUM_LEDS];

//-- network
static byte mymac[] = {0xA0,0xA8,0xCD,0x01,0x02,0x03};  //try to generate this from ds18b20 serial later on

byte Ethernet::buffer[700];
unsigned long cur;
unsigned long pos;
byte res;

//-- heating
int heatTarget = 25;
int heatTolerance = 2;

//-- config engines
int engine1Duration = 10;
int engine1Offtime = 60;

int engine2Duration = 10;
int engine2Offtime = 60;

//int engine3Duration = 0;
//int engine3Offtime = 30;

//-- draining cycle
int drTargetTop = 300;
int drTargetBottom = 500;

//-- config air
int airDuration = 120;
int airOfftime = 240;

//-- config fans
int fan1Duration = 50;
int fan1Offtime = 150;

int fan2Duration = 50;
int fan2Offtime = 175;

//-- nutrients -- solenoid valve
int threshold = 0;
int ml = 0;
int phWait = 0;

//-- LEDs
int ledDuration = 0;
int ledRepeat = 0;

//-- Luminosity
FC51 luminosity(CS_LUM);

//-- OneWire/DS18B20
OneWire oneWire(CS_TEMP);
DallasTemperature ds18b20(&oneWire);
DeviceAddress dsAddr;
float tempWater = 0;

//-- DHT
DHT dht(CS_HUM, 22);

//-- Pipe water levels
Funduino pBottom(CS_WL_B);
Funduino pTop(CS_WL_T);

//-- PH
SEN0169 ph(CS_PH, 0.88);

//-- create timers
Timer tRelEng;
Timer tRelFan;
Timer tRelAir;

Timer tSen;

//-- setup relais
Relay relEngine1(REL_ENG_1,false);
Relay relEngine2(REL_ENG_2,false);
Relay relEngine3(REL_ENG_3,false);

Relay relAir(REL_AIR, false);
Relay relFan1(REL_FAN_1, false);
Relay relFan2(REL_FAN_2, false);

Relay relHeat(REL_HEAT, false);

Relay relNut(REL_NUT, false);

//-- setup DHT

void setup () {

  //-- setup serial
  Serial.begin(9600);
  while(!Serial);
  
  Serial.println(F("hydroAGS-core"));
  Serial.println(HA_VER);
  Serial.println(F("Booting ..."));
  Serial.println();

  //-- setup network
  Serial.println(F("Initializing network ..."));
  while(!ether.begin(sizeof Ethernet::buffer, mymac, CS_NET)) {
    Serial.println(F("Failed to initialize network, retrying ..."));
    delay(5000);
  }
  Serial.println(F("Network initialized"));

  Serial.println(F("Obtaining IP ..."));
  while(!ether.dhcpSetup()) {
    Serial.println(F("Failed to get IP address, retrying ..."));
    delay(5000);
  }
    
  ether.printIp("Success, IP is: ", ether.myip);
  Serial.println();
  
  //-- setup sd
  //pinMode(CS_SD, OUTPUT);
  //digitalWrite(CS_SD, HIGH);
  
  Serial.println(F("Initializing SD card ..."));
  file.setSSpin(CS_SD);
  res = file.initFAT();
  if (res != NO_ERROR) {
    Serial.println(F("Failed to initialize SD card"));
    while(1);
  }
  
  Serial.println(F("SD card initialized"));
  Serial.print(F("Card size: "));
  Serial.print(file.BS.partitionSize);
  Serial.println(F("MB"));
  Serial.println();

  Serial.println("Listing Files...");
  res = file.findFirstFile(&file.DE);
  if (res==NO_ERROR) {
    Serial.print(file.DE.filename);
    Serial.print(".");
    Serial.println(file.DE.fileext);
  }
  else {
    Serial.println("No files found...");
  }
  while (res==NO_ERROR)
  {
    res = file.findNextFile(&file.DE);
    if (res==NO_ERROR) {
      Serial.print(file.DE.filename);
      Serial.print(".");
      Serial.println(file.DE.fileext);
    }
  } 
  
  //-- setup LEDs
  Serial.println(F("Initializing LEDs ..."));
  FastLED.addLeds<WS2812B, CS_LED, GRB>(leds, NUM_LEDS);

  for(int i=0; i<NUM_LEDS; i++) { leds[i] = CRGB::Red; FastLED.show(); delay(10); Serial.print("*"); }
  Serial.println();
  for(int i=0; i<NUM_LEDS; i++) { leds[i] = CRGB::Green; FastLED.show(); delay(10); Serial.print("*"); }
  Serial.println();
  for(int i=0; i<NUM_LEDS; i++) { leds[i] = CRGB::Blue; FastLED.show(); delay(10); Serial.print("*"); }
  Serial.println();
  for(int i=0; i<NUM_LEDS; i++) { leds[i] = CRGB::White; FastLED.show(); delay(10); Serial.print("*"); }
  Serial.println();
  for(int i=0; i<NUM_LEDS; i++) { leds[i] = CRGB::Black; FastLED.show(); delay(10); Serial.print("*"); }
  Serial.println(); 


  for(int i=0; i<NUM_LEDS; i++) 
    if(i%3) 
     { leds[i] = CRGB(15,0,0); FastLED.show(); delay(15); }
    else if(i%5) 
      { leds[i] = CRGB(0,0,15); FastLED.show(); delay(15); }
    else 
      { leds[i] = CRGB(15,0,0); FastLED.show(); delay(15); }
      
  delay(25);

for(int j=15; j<255; j+=5) {
    for(int i=0; i<NUM_LEDS; i++) 
      if(i%3) 
        leds[i] = CRGB(j,0,0);
      else if(i%5) 
        leds[i] = CRGB(0,0,j);
      else 
        leds[i] = CRGB(0,j,0);
  delay(50);
  FastLED.show();
}

  Serial.println(F("LEDs initialized"));
  Serial.println();

  //--setup DS18B20
  Serial.print("Parasite power is: "); 
  if (ds18b20.isParasitePowerMode()) Serial.println("ON");
  else Serial.println("OFF");

  if (!ds18b20.getAddress(dsAddr, 0)) Serial.println("Unable to find address for Device 0"); 

  Serial.print("Device 0 Address: ");
  for (int i=0; i<8; i++)
  {
    if (dsAddr[i] < 16) Serial.print("0");
    Serial.print(dsAddr[i], HEX);
  }
  Serial.println();

  ds18b20.setResolution(dsAddr, 9);
 
  Serial.print("Device 0 Resolution: ");
  Serial.print(ds18b20.getResolution(dsAddr), DEC); 
  Serial.println();

  //setup dht
  dht.begin();

  //--setup timers
  tSen.every(5000, wUpdateDHT, (void*)0); delay(1000);
  tSen.every(5000, wUpdateDS, (void*)0); delay(1000);
  tSen.every(5000, wUpdateWL, (void*)0); delay(1000);
  tSen.every(5000, wUpdateLUM, (void*)0); delay(1000);
  tSen.every(5000, wUpdatePWL, (void*)0); delay(1000);
  tSen.every(5000, wUpdatePH, (void*)0); delay(1000);

  tRelAir.every(airOfftime*1000L, wRelAirOn, (void*)0);
  
  tRelFan.every(fan1Offtime*1000L, wRelFan1On, (void*)0);
  tRelFan.every(fan2Offtime*1000L, wRelFan2On, (void*)0);

  tRelEng.every(engine1Offtime*1000L, wRelEng1On, (void*)0);
  tRelEng.every(engine2Offtime*1000L, wRelEng2On, (void*)0);

  tRelEng.every(5000, wRelCheckLevel, (void*)0);

  
  //tRelEng.every(engine3Offtime*1000L, wRelEng3On, (void*)0);

  tSen.after(10000, wOpenNut, (void*)0);
}

//-- helper functions, these are needed as we can't pass a non-static callback function
//-- need to find a better workarround for this later

void wOpenNut(void* context) { relNut.on(); tSen.after(30000, wCloseNut, (void*)0); }
void wCloseNut(void* context) { relNut.off(); }

void wUpdatePH(void* context) { ph.update(); }

void wRelCheckLevel(void* context) { 
  if( (pTop.getValue() >= drTargetTop) && (waterLevel.getValue() > WL_MAX)) {
    relEngine3.on(); Serial.println("Draining ..."); }
  else
    relEngine3.off();  
}

void wUpdatePWL(void* context) { pBottom.update(); pTop.update(); Serial.print(pBottom.getValue()); Serial.print(":"); Serial.println(pTop.getValue()); }

void wUpdateLUM(void* context) { luminosity.update(); }

void wUpdateWL(void* context) { waterLevel.update(); }

void wUpdateDS(void* context) { 
  ds18b20.requestTemperatures(); tempWater = ds18b20.getTempC(dsAddr); 
  if(tempWater <= (heatTarget-heatTolerance)) {
    relHeat.on();  Serial.println("Heating ..."); }
  else
    relHeat.off();  
}

void wUpdateDHT(void* context) { dht.updateH(); dht.updateC(); }

void wRelAirOn(void* context) { relAir.on(); tRelAir.after(airDuration*1000L, wRelAirOff, (void*)0); }
void wRelAirOff(void* context) { relAir.off(); }

void wRelFan1On(void* context) { relFan1.on(); tRelFan.after(fan1Duration*1000L, wRelFan1Off, (void*)0); }
void wRelFan1Off(void* context) { relFan1.off(); }
void wRelFan2On(void* context) { relFan2.on(); tRelFan.after(fan2Duration*1000L, wRelFan2Off, (void*)0); }
void wRelFan2Off(void* context) { relFan2.off(); }

void wRelEng1On(void* context) { relEngine1.on(); tRelEng.after(engine1Duration*1000L, wRelEng1Off, (void*)0); }
void wRelEng1Off(void* context) { relEngine1.off(); }
void wRelEng2On(void* context) { relEngine2.on(); tRelEng.after(engine2Duration*1000L, wRelEng2Off, (void*)0); }
void wRelEng2Off(void* context) { relEngine2.off(); }

/*
void wRelEng3On(void* context) { relEngine3.on(); tRelEng.after(engine3Duration*1000L, wRelEng3Off, (void*)0); }
void wRelEng3Off(void* context) { relEngine3.off(); } */

void loop() {

  //-- check for tcp data
  pos = ether.packetLoop(ether.packetReceive());
  
    if (pos) {
      char* data = (char *) Ethernet::buffer + pos;
      cur=0;

      //-- if nothing was specified, send default page
      if (strncmp("GET / ", data, 6) == 0) {
        sendfiles("index.htm");
      } //-- send update data for dashboard page
      else if (strncmp("GET /dashboard.ags", data, 18) == 0) {
        Serial.print("Requested dashboard update... ");
  
        BufferFiller bfill = ether.tcpOffset();
  
        //-- print header & JSON opening
        bfill.emit_p(PSTR("HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nPragma: no-cache\r\nAccess-Control-Allow-Origin: *\r\n\r\n"));
        bfill.emit_p(PSTR("{ "));
  
        //-- print gauge data
        bfill.emit_p(PSTR("\"ga-temp-water\":\"$D\",\n"),(int)tempWater);
        bfill.emit_p(PSTR("\"ga-temp-air\":\"$D\",\n"),(int)dht.getCValue());
        bfill.emit_p(PSTR("\"ga-hum\":\"$D\",\n"),(int)dht.getHValue());
        bfill.emit_p(PSTR("\"ga-lum\":\"$D\",\n"),luminosity.getValue());
        bfill.emit_p(PSTR("\"ga-level\":\"$D\",\n"),waterLevel.getValue());
        bfill.emit_p(PSTR("\"ga-ph\":\"$D\"\n"),(int)(ph.getValue()*100));
  
        //-- close JSON tag
        bfill.emit_p(PSTR(" }"));
                                       
        ether.httpServerReply(bfill.position());
  
        Serial.println("sent");
      } //-- Send update data for status page
      else if (strncmp("GET /status.ags", data, 15) == 0) {
        Serial.print("Requested status update... ");
  
        BufferFiller bfill = ether.tcpOffset();
  
        //-- print header & JSON opening
        bfill.emit_p(PSTR("HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nPragma: no-cache\r\nAccess-Control-Allow-Origin: *\r\n\r\n"));
        bfill.emit_p(PSTR("{ "));
  
        //-- print engine status
        bfill.emit_p(PSTR("\"st-engine1\":\"$D\",\n"),relEngine1.getState());
        bfill.emit_p(PSTR("\"st-engine2\":\"$D\",\n"),relEngine2.getState());
        bfill.emit_p(PSTR("\"st-engine3\":\"$D\",\n"),relEngine3.getState());
  
        //-- print periphery status
        bfill.emit_p(PSTR("\"st-air\":\"$D\",\n"),relAir.getState());
        bfill.emit_p(PSTR("\"st-fan1\":\"$D\",\n"),relFan1.getState());
        bfill.emit_p(PSTR("\"st-fan2\":\"$D\",\n"),relFan2.getState());
        bfill.emit_p(PSTR("\"st-heat\":\"$D\",\n"),relHeat.getState());
  
        //-- print nutrients status
        bfill.emit_p(PSTR("\"st-nutrients\":\"$D\" \n"),relNut.getState());

        //-- close JSON
        bfill.emit_p(PSTR(" }"));
                                       
        ether.httpServerReply(bfill.position());
  
        Serial.println("sent");
      } //-- Send update data for system page
      else if (strncmp("GET /system.ags", data, 15) == 0) {
        Serial.print("Requested system update... ");
  
        BufferFiller bfill = ether.tcpOffset();
  
        //-- print header & JSON opening
        bfill.emit_p(PSTR("HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nPragma: no-cache\r\nAccess-Control-Allow-Origin: *\r\n\r\n"));
        bfill.emit_p(PSTR("{ "));
  
        //-- print MAC
        bfill.emit_p(PSTR("\"mac-addr\":\""));
        for(int i=0; i<6; i++) { bfill.emit_p(PSTR("$D"),mymac[i]); if(i<5) bfill.emit_p(PSTR("::")); }
        bfill.emit_p(PSTR("\",\n"));
  
        //-- print IP
        bfill.emit_p(PSTR("\"ip-addr\":\""));
        for(int i=0; i<4; i++) { bfill.emit_p(PSTR("$D"),ether.myip[i]); if(i<3) bfill.emit_p(PSTR(".")); }
        bfill.emit_p(PSTR("\",\n"));
  
        //-- print subnet
        bfill.emit_p(PSTR("\"ip-sub\":\""));
        for(int i=0; i<4; i++) { bfill.emit_p(PSTR("$D"),ether.netmask[i]); if(i<3) bfill.emit_p(PSTR(".")); }
        bfill.emit_p(PSTR("\",\n"));
  
        //-- print gateway
        bfill.emit_p(PSTR("\"ip-gate\":\""));
        for(int i=0; i<4; i++) { bfill.emit_p(PSTR("$D"),ether.gwip[i]); if(i<3) bfill.emit_p(PSTR(".")); }
        bfill.emit_p(PSTR("\",\n"));
  
        //-- print DNS
        bfill.emit_p(PSTR("\"ip-dns\":\""));
        for(int i=0; i<4; i++) { bfill.emit_p(PSTR("$D"),ether.dnsip[i]); if(i<3) bfill.emit_p(PSTR(".")); }
        bfill.emit_p(PSTR("\",\n"));
  
        //-- print DHCP status
        bfill.emit_p(PSTR("\"ip-dhcp\":\"$D\",\n"),0);
  
        //-- print version information
        bfill.emit_p(PSTR("\"ha-ver-cor\":\"$S\",\n"),HA_VER);
  
        //--print uptime information
        bfill.emit_p(PSTR("\"ha-uptime\":\"$Dmin"),millis()/1000/60);
        bfill.emit_p(PSTR(" $Ds"),millis()/1000%60);
        bfill.emit_p(PSTR("\", \n"));

        bfill.emit_p(PSTR("\"ha-ram\":\"$D\" \n"),freeRam());
  
        //-- close JSON packet
        bfill.emit_p(PSTR(" }"));
                                       
        ether.httpServerReply(bfill.position());
  
        Serial.println("sent");
      }
      else if (strncmp("GET /", data, 5) == 0) { //serve anything on sd card 
        int i =0;  
        char temp[15]=""; //here will be the name of requested file
        while (data[i+5]!=32) {temp[i]=data[i+5];i++;}//search the end
        sendfiles((char*) temp);
      }  
  }

  //-- timer updates
  tRelAir.update();
  tRelFan.update();
  tRelEng.update();
  tSen.update();


  //-- debug
  //Serial.println(analogRead(A0));
  //delay(1000);
}

//-- Function to check and send a file from SD card
void not_found() { //content not found 
  cur=0;
  streamfile ("404.hea",TCP_FLAGS_FIN_V); 
  Serial.println("not found");
}

//-- send file from SD card
byte streamfile (char* name , byte lastflag) { //send a file to the buffer 
  if (!file.exists(name)) {return 0;}
  
  Serial.print("Sending: ");
  Serial.println(name);
  Serial.println();
  
  res=file.openFile(name, FILEMODE_BINARY);
  int  car=512;
  while (car==512) {
    car=file.readBinary();
    for(int i=0;i<car;i++) {
    cur++;
    Ethernet::buffer[cur+53]=file.buffer[i];
    }
if (cur>=512) {
      ether.httpServerReply_with_flags(cur,TCP_FLAGS_ACK_V);
      cur=0;
    }  else {
 
  if (lastflag==TCP_FLAGS_FIN_V) {
    ether.httpServerReply_with_flags(cur,TCP_FLAGS_ACK_V+TCP_FLAGS_FIN_V);
  }
    }
}
  file.closeFile();
  return 1;
}

//-- find correct header and send file from SD card
byte sendfiles(char* name) {
  ether.httpServerReplyAck();
  int i =0;  
  char dtype[13]=""; 
  while (name[i]!=0) {
    i++;
  }//search the end
  int b=i-1;
  while ((name[b]!=46)&&(b>0)) {
    b--;
  }//search the point
  int a=b+1;
  while (a<i) {
    dtype[a-b-1]=name[a];
    a++;
  }
  dtype[a-b-1]='.';
  dtype[a-b]='h';
  dtype[a-b+1]='e';
  dtype[a-b+2]='a';        
  //Serial.println(dtype); // print the requested header file
  if (streamfile ((char *)dtype,0)==0) {
    streamfile ("txt.hea",0);
  }
    //Serial.println(name); // print the requested file
  if (streamfile ((char *)name,TCP_FLAGS_FIN_V)==0) {
    cur=0;
    not_found();
  }
  
  Serial.print("content send to ");
  for(int i=30; i<34; i++) {
    Serial.print(Ethernet::buffer[i]);
    if (i<33) Serial.print(".");
  }
  Serial.println(" ");
  
}

//-- Calling 0 breaks the processor causing it to soft reset
void(* resetFunc) (void) = 0;

int freeRam () 
{
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
};
