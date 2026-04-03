// QMX+ Web Remote Control - 3 April 2026 - https://github.com/Dake100/QMX-Web-interface-and-remote-controle
// This project is based on Randy Rollins’s (KD7RR) inspiring project, where he created a remote control for the QMX+ using a small inexpensive ESP32 module, 
// a TFT display a rotary knob and a few buttons. 
// Published here on February 7 – 2026 an on Github:
// https://github.com/rrollins72/QMX-Serial-remote 
// In my new project I have added a web interface.
// Randy Rollins has given me permission to reuse his sketch (code) in this new project and has given me permission to publish it. Many thanks Randy!
// In this new project, I and Claude.AI, have added a WiFi-based web interface, allowing the transceiver to be monitored and controlled from any web 
// browser on the local network — on a desktop or laptop computer, tablet, or smartphone. This new web interface mirrors the information that is shown on the 
// TFT display in the previous project, plus a little bit more, together with full remote control of all key functions on QMX+. With the mouse wheel, you can 
// adjust the frequency and scroll through the band.



// Edit the network settings below - line 110

///////////////////////////////////////////////////////////////////////
// The original information about the QMX Remote display feb. 2026 - by Randy Rollins
///////////////////////////////////////////////////////////////////////
//	QMX Remote display
// This sketch using an ESP32-S3 processor and ST7735 display receiving
// serial data on TX/RX UART2 and using a rotary encoder to change frequency
// of the QRP Labs QMX+ tranceiver. It was done as an experiment to see how
// well the ESP32-S3 could handle the load, and it proved that is it quite
// capable.
// A ESP32-S3 Zero platform was used. Readily available for about $5 from Aliexpres
// The display is a 1.8" 160x120 TFT LCD with a ST7735 driver. 
// The Arduino IDE was used for this development. 
//	The esp32 board manager needs to be installed. The Waveshare ESP32-S3-Zero board is selected.
//	The driver for the display is the ST7735_LTSM by Gavin Lyons. Fonts are from that installation.
//		This driver is much faster than the ucg driver, so is necessary for this implementation 
//
//Operation summary
//The remote display will connect to the QMX+ via the AUX serial port. The serial interface baud rate is set to 19200, with 8N1. 
//This will need to be set up on the QMX using a USB serial port to control the menu system.
//Go to Main menu>Configuration>System config>GPS & Ser. Ports: Serial 1 on AUX: ENABLED, Serial 1 baud: 19200.
//(Note: I tried using higher baud settings, but that resulted in a lot of missed data, so settled on 19200).
//
//There are 3 controls on the display:
//The rotary knob with button on press, button A (left) and button B (right).
//Functions:
//Rotary knob will normally adjust the frequency depending on the selected step size. 
//The step size is changed by a short press on the rotary knob. The step sizes are 10Hz, 100Hz, 1kHz, 10kHz. 
//The band can also be changed by a double click on the rotary knob (it must be ~400ms or less). 
//The band will be shown in the lowest display area. 
//The rotary knob can be turned to select the band desired, then the knob pressed to verify 
//and the band will be changed to the selected. 
//(This process needs some refinement since it required a change to the VFOA frequency to make the change, 
//so previous VFOA settings are lost).
//The rotary knob can also be used to adjust the volume with a long press (>600ms).
//The volume (Analog Gain) is shown in the lower LCD and can be adjusted to a desired listening level,
//then a short press on the knob will set it to that and return to frequency adjust.
//Button A is on the left and below the rotary control. It simply controls the selected VFOA or B. 
//Button B is on the right and below the rotary control. It controls the selected mode. 
//The modes are cycled through: CW, DIGI, USB, LSB (same as QMX).
//
//Display updates occur on a 1 second increment so there can be some visible lag on some data presented on the display. 
//The updates from the rotary encoder are done during the main loop and are near real time. 
//The current build is set up for displaying MST from the clock, but this can be turned off or adjusted
//
//  
//	Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
//  This permission notice shall be included in all copies or substantial portions of the Software.
//
//////////////////////////////////////////////////////////////////////////
//
// The signals on the ST7735 and hook up to the ESP32-S3 Zero are as follows:
// ST7735   S3
//  GND     GND (pin 2)
//  VDD     5V(pin 1) The display has a 3.3V regulator on it-needs 5v.
//  SCL     SCK (GP12)
//  SDA     MOSI (GP11)
//  RST     GP9 
//  DC(RS)  GP8      
//  CS(SS)  GP10 
//  BKLT    to 3.3V on pin 1
//
/////////************** NOTE *******************************************
//  NOTE: To map the SPI to available pins, a change to the file in this location is necessary: 
//  C:\Users\xxxxx\AppData\Local\Arduino15\packages\esp32\hardware\esp32\3.x.x\variants\waveshare_esp32_s3_zero\pins_arduino.h:
//   Mapping based on the ESP32S3 data sheet - alternate for SPI2
//    static const uint8_t SS = 10;    // FSPICS0
//    static const uint8_t MOSI = 11;  // FSPID
//    static const uint8_t MISO = 13;  // FSPIQ
//    static const uint8_t SCK = 12;   // FSPICLK
//
// IF THERE IS AN UPDATE TO THE ESP32 PACKAGE THEN THIS WILL CHANGE, so need to be aware of it
//
/////////////////////////////////////////////////////////////////////////
/////////****************************************************************
// The serial interface will be based on the OnReceive demo structure-it is clean works well
// See QMX CAT document for information
////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////
// Here starts the things that is importent for the web interface
//////////////////////////////////////////////////////////////////////////
//
// libraries for web server
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>

/////////////////////////////////
// Edit ssid name and password //
/////////////////////////////////
// --- Your WiFi credentials ---
const char* ssid     = "NETWORK NAME";
const char* password = "PASS_WORD";
// mDNS name -> http://qmx.local/
const char* mdnsName = "qmx";

// Webserver on port 80
AsyncWebServer server(80);

////////////////////////////////////////
// End of settings wor the web server //
////////////////////////////////////////

// libraries for display
#include "ST7735_LTSM.hpp"
// Included Fonts 
#include <fonts_LTSM/FontArialRound_LTSM.hpp>
#include <fonts_LTSM/FontRetro_LTSM.hpp>
#include <fonts_LTSM/FontGroTesk_LTSM.hpp>

extern const char WEB_PAGE_HTML[];

using namespace std;

////Display initialize
ST7735_LTSM myTFT;
bool bhardwareSPI = true; // true for hardware spi, false for software

/////// EC-11 rotary and buttons use ezButton
#include <ezButton.h>  // the library to use for SW button and other buttons
#include <Arduino.h>  // included for general serial use

/////// Onebutton ////////////
#include <OneButton.h>


///// Defines for IO pins used
#define CLK_PIN 1  // ESP32 pin gp1
#define DT_PIN 2   // ESP32 pin gp2
#define SW_PIN 4   // ESP32 pin gp4
#define BUTTON1 5  // GP5   general purpose button inputs
#define BUTTON2 6  // GP6
#define BUTTON3 3  // GP3

#define DIRECTION_CW 0   // clockwise direction
#define DIRECTION_CCW 1  // counter-clockwise direction

#define BAUD      19200 // Should be a reasonable speed for comms?
#define RXPIN     44     // GPIO 44 => RX for Serial2
#define TXPIN     43     // GPIO 43 => TX for Serial2

//////////////////// general declarations for global use and constants ///////////////////
// Rotary control globals
volatile int counter = 0;
volatile int direction = DIRECTION_CW;
volatile unsigned long last_time;  // for debouncing
volatile int statecnt = 0;

// Thresholds for rotary button press detects(milliseconds)
const int ShortPressTop  = 250;
const int LongPressBase   = 800;
const int DoubleClickGap = 400;
// Variables to track timing
unsigned long pressedTime  = 0;
unsigned long releasedTime = 0;
bool isWaitingForDouble    = false;
bool longPressDetected     = false;
bool VolProc = false;
int bstate = 0;		//button state is 0 to idle, 1 for long, 2 for double

int prev_counter = 0;
// The ezButton library is helpful for button management
//ezButton button(SW_PIN);	// create ezButton objects that attach to pins;
ezButton button1(BUTTON1);	//button for vfo selection
ezButton button2(BUTTON2);	//button for mode selection
ezButton button3(BUTTON3);  //button backwards button 2

OneButton button(SW_PIN, true); //active low true

// for S3 zero with RGB LED the IO is 21
#define RGB_BUILTIN 21
#define DEBUG 0

// Timer controls
unsigned long prevmils = 0;
unsigned long prevmils1 = 0;

/*************************************/
volatile size_t received_bytes = 0;
unsigned int fifoFull = 38;   // max data to receive
char Rcvbuf1[40];	// buffer to receive data
bool Rcvbufavail = 0;
bool Serconnavail = 0;	//set when a serial connection is valid
char FreqA[12] = "00014047500";
char FreqB[12] = "00007050000";
char Frcv[2] = "0";
char Ftrx[2] = "0";
char freqbuf[15];
char freqbuf1[15];
char AudGn[6] = "0180";
char timedat[9] = "12:00:00";     //time data hh:mm:ss
// char timedat1[3];     // for am or pm
char timedat1[1];     // 
char timehr[3];       //HR portion
int AudGnVol = 180;	//base audio volume
int timehrdat;
int timelocalhr;
int rxtxchk = 0;   //receive/transmit state rx=0, tx=1
// For displaying local time offset from UTC from TM command
// const int timelocal = -7;     //GMT offset for local MST
// const bool TwelveHour = 1;	// For displaying 12 (=3) or 24 hour format
const int timelocal = 1;     //GMT offset for local MST
const bool TwelveHour = 0;	// For displaying 12 or 24 hour format //*******************************************************************************
char smeterdat[6] = "00.1";   //smeter data in db xx.x 
char swrdat[6] = "1.00";      //swr reading x.xx:1
char lcddat[20] = {};     // 20 - lcd data-second line starts at 15 (16 bytes in)
//char infodat[36] = {};	  //info response 
char modedat[4] = {};     //mode info 1=LSB, 2=USB, 3=CW, 6=DIGI (FSK), 7=CWR
unsigned int modex = 2;	  // mode number as above
unsigned int frecvfo = 0; //receive vfo =0 for A, 1 for B, 3 for split
unsigned int ftrxvfo = 0; //same as receive vfo
unsigned int vfoaorb = 0;	//0=VFOA 1=VFOB
long Vfreqa = 0;
long Vfreqb = 0;
long stepset = 10;	//Step size in Hz for rotary step
volatile bool freqUpdateNeeded = false;

int bandsel = 5;	//11 bands. 0=160m..10=6m 5=20m
bool bandselproc = 0;	//if in band select process

// Display constants
const char modLSB[] = "LSB  ";
const char modUSB[] = "USB  ";
const char modCW[] = "CW   ";
const char modCWR[] = "CWR  ";
const char modDIG[] = "DIGI ";
const char modDIGR[] = "DIGIR";
const char vfoAstr[] = "VFO:A";
const char vfoBstr[] = "VFO:B";
const char vfoSplit[] = "SPLIT";
const char stepHZ[] = "Hz  ";
const char stepkHZ[]= "kHz";
const char txstr[] = "TX";
const char rxstr[] = "RX";

// band set strings
const char bandtxt[11][10] = {"160m band"," 80m band"," 60m band"," 40m band"," 30m band"," 20m band"," 17m band"," 15m band"," 12m band"," 10m band","  6m band"};

// Here you can set the start frequenses for VFO A - Now the recommended CW QRP frequenses for each band
const char bandsets[11][12] = {
  "00001836000",
  "00003560000",
  "00005357000",  // 60m  5.357 MHz (FT8 på 60m)
  "00007030000",
  "00010116000",
  "00014060000",
  "00018086000",
  "00021060000",
  "00024906000",
  "00028060000",
  "00050060000" 
  };

// Command constants
const char LCcmd[] = "LC;";   //LCD command-receive 35 chars
const char FAcmd[] = "FA;";   //Freq VFOA command-receive 14 chars, send 14 chars
const char FBcmd[] = "FB;";   //Freq VFOB command-receive 14 chars, send 14 chars
const char FRcmd[] = "FR;";   //Receive VFO command-receive 3 chars, send 3 chars
const char FTcmd[] = "FT;";   //Transmit VFO command-receive 3 chars, send 3 chars
const char MDcmd[] = "MD;";   //Mode command-receive 4 chars or send 4 chars
const char SMcmd[] = "SM;";   //Smeter command-receive 6 chars
const char TMcmd[] = "TM;";   //Time command-receive 9 chars
const char TQcmd[] = "TQ;";   //Tx/Rx command-receive 3 chars
const char SWcmd[] = "SW;";   //SWR during transmit-receive 6 chars
const char AGcmd[] = "AG;";   //Audio Gain command-receive 6 chars

///////////////////////////////////////////////////////////////
// This is called from the main loop to detect rotation changes
// There is sufficient main loop timing to make this work
///////////////////////////////////////////////////////////////
void rotloop () {
    if (statecnt == 0){
      if ((digitalRead(CLK_PIN) == LOW)&&(digitalRead(DT_PIN) == LOW)){
        statecnt = 1;
      }
    }
    if (statecnt == 1){
      if (digitalRead(CLK_PIN) == HIGH){
        statecnt = 2;
        last_time = millis();
      }
    }
    if (statecnt == 2){
      if ((millis() - last_time) > 2){
          if ((digitalRead(CLK_PIN) == HIGH)&&(digitalRead(DT_PIN) == HIGH)){
          // the encoder is rotating in counter-clockwise direction => decrease the counter
          counter--;
          direction = DIRECTION_CCW;
          statecnt = 0;
          }
          if ((digitalRead(CLK_PIN) == HIGH)&&(digitalRead(DT_PIN) == LOW)) {
          // the encoder is rotating in counter-clockwise direction => decrease the counter
          counter++;
          direction = DIRECTION_CW;
          statecnt = 0;
          }         
      }
    }
}

void RotButtonCheck(){
	button.tick(); // Monitor button state
  if (bstate == 1){	//long press
	  VolProc = true;
	    Serial2.write(AGcmd);	//request current audio gain on long press
		delay(5);  
  }
  else if (bstate == 2){ //double press
	    bandselproc = 1;  //enter band select
  }
}

// SHORT PRESS: Acts as the stop or regular press
void handleClick() {
  if (bstate != 0) {
    bstate = 0;
	if (bandselproc) {
		BandChgSnd();	//if in bandsel state, clear it and send command
		bandselproc = 0;
	}
	VolProc = 0;
  } 
  else {
    Serial.println("Short Press: Standard Action.");
	  if (stepset == 10) stepset = 100;
    else if (stepset == 100) stepset = 1000;
    else if (stepset == 1000) stepset = 10000;
    else if (stepset == 10000) stepset = 10;
    Botline();  // update display
  }


}
// DOUBLE PRESS: Starts the double routine
void handleDoubleClick() {
  if (bstate == 0) {
    bstate = 2;
  }
}

// LONG PRESS: Starts the long routine
void handleLongPressStart() {
  if (bstate == 0) {
    bstate = 1;
  }

  
}


////////////// Top line of the display ///////////////////
///////// Print time if receiving data from QMX, otherwise just show no connection /////
void Topline (){
  if (Serconnavail) {
    myTFT.setFont(FontRetro);
    myTFT.setTextColor(myTFT.C_WHITE, myTFT.C_BLUE);
    myTFT.setCursor(0,5);
    if (vfoaorb == 0) myTFT.print("VFO:A               ");	//which vfo to show - if (vfoaorb == 0) myTFT.print(vfoAstr);	
    if (vfoaorb == 1) myTFT.print("VFO:B               ");
    myTFT.setCursor(65,5);
    if (rxtxchk == 0) {
      myTFT.print(rxstr);
      myTFT.setCursor(95,5);
      myTFT.print("        ");
    }    
    else if (rxtxchk == 1) {
      myTFT.print(txstr);
      myTFT.setCursor(95,5);
      myTFT.print("SWR ");
      myTFT.print(swrdat);
    }    
    
    if (!TwelveHour){
      // adding difference to get lokal time
      timehrdat = atoi(timehr);     //to convert to local time for display

      // Printing GMT
      //
      myTFT.setFont(FontRetro);
      myTFT.setCursor(5,110);
      // myTFT.print(ctime);
      myTFT.print ("   GMT: ");
      myTFT.print (timedat);

      
      // printing local time
      myTFT.setFont(FontArialRound);
      timelocalhr = timehrdat+timelocal;  // Local offset from UTC
      if (timelocalhr < 0) timelocalhr = timelocalhr+24;
      itoa (timelocalhr,timehr,10);
      if (timelocalhr < 10) {   // can pad with a 0 or space if leading 0 not desired
        // if (TwelveHour) timedat[0] = ' ';
        // else 
        timedat[0] = '0';
        timedat[1] = timehr[0];
      }
      else {
        timedat[0] = timehr[0];
        timedat[1] = timehr[1];
      }

      myTFT.setCursor(13,22);
      myTFT.print (timedat);

     

    }
    /*
    else {
    // This converts the hour to local time for display
      timehrdat = atoi(timehr);     //to convert to local time for display
      timelocalhr = timehrdat+timelocal;  // Local offset from UTC
      if (timelocalhr < 0) timelocalhr = timelocalhr+24;
      if (TwelveHour) {
        if (timelocalhr >= 12) {
          if (timelocalhr > 12) timelocalhr = timelocalhr-12;
        timedat1[0] = 'p';
        }
        else {
          if (timelocalhr == 0) timelocalhr = 12;	
          timedat1[0] = 'a';
        }
        timedat1[1] = 'm';   // 'm'
        timedat1[2] = 0;
      }
      itoa (timelocalhr,timehr,10);
      if (timelocalhr < 10) {   // can pad with a 0 or space if leading 0 not desired
        if (TwelveHour) timedat[0] = ' ';
        else timedat[0] = '0';
        timedat[1] = timehr[0];
      }
      else {
        timedat[0] = timehr[0];
        timedat[1] = timehr[1];
      }
      myTFT.setCursor(0,22);    
      myTFT.print (timedat);
      myTFT.print (timedat1);    
    }
    */
  }
  else{
    myTFT.setFont(FontRetro);   //  myTFT.setFont(FontArialRound)
    myTFT.setCursor(0,5);  // (0,22)
    myTFT.print ("No contact with QMX+");
    myTFT.setCursor(0,22);
    myTFT.print ("");
  }
}

//////////// lower display area, for mode and step and Smeter //////////
void Botline (){
	myTFT.setFont(FontRetro);
	myTFT.setCursor(2,85);
  switch (modex){
      case 1:
          myTFT.print (modLSB);
          break;
      case 2:
          myTFT.print (modUSB);
          break;
      case 3:
          myTFT.print (modCW);
          break;
      case 6:
          myTFT.print (modDIG);
          break;
      case 7:
          myTFT.print (modCWR);
          break;
      case 9:
          myTFT.print (modDIGR);
          break;
  }
  myTFT.setCursor(50,85);
  switch (stepset){
    case 1:
      myTFT.print("  ");    
      myTFT.print(stepset);
      break;
    case 10:
      myTFT.print(" ");   
      myTFT.print(stepset);
      break;
    case 100:
      myTFT.print(stepset);
      break;  
    case 1000:
      myTFT.print("  ");      
      myTFT.print(stepset/1000);
      break;
    case 10000:
      myTFT.print(" ");
      myTFT.print(stepset/1000);
      break;
    case 100000:
      myTFT.print(stepset/1000);
      break;
  }
  myTFT.setCursor(80,85);
  if (stepset < 1000) myTFT.print(stepHZ);
  else myTFT.print (stepkHZ);
  myTFT.setCursor(110,85);
  myTFT.print ("S");
  myTFT.print (smeterdat);
}

/////////////// Bottom line where QMX LCD info is displayed //////
void LCbotline() {
	myTFT.setFont(FontRetro);
	myTFT.setCursor(5,110);
	if (bandselproc){
		myTFT.print(bandtxt[bandsel]);
		myTFT.print("     ");
	} 
  else if (VolProc) {
    myTFT.print("Vol ");
    myTFT.print(AudGnVol);
		myTFT.print("     ");    
  }
  else  {
    myTFT.setCursor(5,110);
    // myTFT.print(ctime);
    // myTFT.print(lcddat);
    // myTFT.print(timedat2);
  }
}

/////////// When a frequency change occurs, send from here ///////
void FreqChgsend(){
  if (vfoaorb == 0) {  //VFOA
      sprintf(freqbuf,"%ld", Vfreqa);
      Serial2.write("FA");
      Serial2.write(freqbuf);
      Serial2.write(";");
      Serial2.write(FBcmd); //update VFOB
      delay(10);
  }
  if (vfoaorb == 1) { //VFOB
      sprintf(freqbuf,"%ld", Vfreqb);
      Serial2.write("FB");
      Serial2.write(freqbuf);
      Serial2.write(";");
      Serial2.write(FAcmd);	//update VFOA
      delay(10);
  }
  Serial2.write("FR");
    if (!vfoaorb) Serial2.write("0");
    else Serial2.write("1");
    Serial2.write(";");
}

/////////// Frequency display update  ///////////
void Frequpdate(){
    static unsigned long lastFreqUpdate = 0;
  if (millis() - lastFreqUpdate < 100) return;  // max 10 gånger per sekund
  lastFreqUpdate = millis();
  long lvalue = 1;
  if (vfoaorb == 0) lvalue = Vfreqa;  //VFOA
  else if (vfoaorb == 1) lvalue = Vfreqb;  //VFOB
  sprintf(freqbuf1, "%ld", lvalue);
  if (lvalue >= 10000000){
  strncpy(freqbuf, freqbuf1, 2);
  freqbuf[2] = '.'; // Insert the character
  strcpy(freqbuf + 3, freqbuf1 + 2); // Copy elements after the insertion point
  strncpy(freqbuf1, freqbuf, 6);
  freqbuf1[6] = '.';
  strcpy( freqbuf1+7,freqbuf+6);
  }
  else if (lvalue < 10000000){
  strncpy(freqbuf, freqbuf1, 1);
  freqbuf[1] = '.'; // Insert the character
  strcpy(freqbuf + 2, freqbuf1 + 1); // Copy elements after the insertion point
  strncpy(freqbuf1, freqbuf, 6);
  freqbuf1[5] = '.';
  strcpy( freqbuf1+6,freqbuf+5);    
  }
  freqbuf1[10] = 0;  // begränsa till exakt 10 tecken
	myTFT.setFont(FontGroTesk);
	myTFT.setTextColor(myTFT.C_WHITE, myTFT.C_BLUE);
  myTFT.setCursor(0,45);
  if (lvalue < 10000000) myTFT.print("0");
  myTFT.print(freqbuf1);	//which vfo to show
  
}

///////////////////////////////////////////////////////////////////////
// This is a UART callback function that will be activated on UART RX events
///////////////////////////////////////////////////////////////////////
void onReceiveFunction(void) {
  size_t available = Serial2.available();
  received_bytes = received_bytes + available;
//  Serial.printf("%d bytes rcvd: ", available);
  int i=0;
  while (available--) {
	Rcvbuf1[i] = (char)Serial2.read();
//    Serial.print(Rcvbuf1[i]);
    i++;
  }
  Rcvbufavail = 1;
//  Serial.println();
  ParseRcv();
}
///////////////////////////////////////////
///////// Parse the received data    //////
// Note: Since the arrays are not large, not using a for loop is much faster
////////////////////////////////////////////////////////////////////////////
void ParseRcv (){
	switch (Rcvbuf1[0])
	{
	  case 'A':	//AG Audio Gain
      if (Rcvbuf1[1] == 'G'){
        AudGn[0] = Rcvbuf1[2];
        AudGn[1] = Rcvbuf1[3];
        AudGn[2] = Rcvbuf1[4];
        AudGn[3] = Rcvbuf1[5];
		    AudGn[4] = 0;
		  AudGnVol = atoi(AudGn);
		}
		break;
		
      case 'F':	//FA,FB,FR or FT strings
      	if (Rcvbuf1[1] == 'A'){
          FreqA[0] = Rcvbuf1[2];
          FreqA[1] = Rcvbuf1[3];
          FreqA[2] = Rcvbuf1[4];
          FreqA[3] = Rcvbuf1[5];
          FreqA[4] = Rcvbuf1[6];
          FreqA[5] = Rcvbuf1[7];
          FreqA[6] = Rcvbuf1[8];
          FreqA[7] = Rcvbuf1[9];
          FreqA[8] = Rcvbuf1[10];
          FreqA[9] = Rcvbuf1[11];
          FreqA[10] = Rcvbuf1[12];
          FreqA[11] = 0;
          Vfreqa = strtol(FreqA, NULL, 10);
          if (!bandselproc) bandsel = freqToBand(Vfreqa); 
        }
   		  else if (Rcvbuf1[1] == 'B'){
          FreqB[0] = Rcvbuf1[2];
          FreqB[1] = Rcvbuf1[3];
          FreqB[2] = Rcvbuf1[4];
          FreqB[3] = Rcvbuf1[5];
          FreqB[4] = Rcvbuf1[6];
          FreqB[5] = Rcvbuf1[7];
          FreqB[6] = Rcvbuf1[8];
          FreqB[7] = Rcvbuf1[9];
          FreqB[8] = Rcvbuf1[10];
          FreqB[9] = Rcvbuf1[11];
          FreqB[10] = Rcvbuf1[12];
          FreqB[11] = 0;
          Vfreqb = strtol(FreqB, NULL, 10);
          if (vfoaorb == 1 && !bandselproc) bandsel = freqToBand(Vfreqb);  
      }
      else if (Rcvbuf1[1] == 'R'){
          Frcv[0] = Rcvbuf1[2];
          frecvfo = Rcvbuf1[2] - '0';
          vfoaorb = frecvfo;
      }    
      else if (Rcvbuf1[1] == 'T'){
          Frcv[0] = Rcvbuf1[2];
          ftrxvfo = Rcvbuf1[2] - '0';
      }          
      break;

		case 'T':	//Time response or TQ response
			if (Rcvbuf1[1] == 'M'){
					timedat[0] = Rcvbuf1[2];
          timehr[0] = Rcvbuf1[2];
					timedat[1] = Rcvbuf1[3];
          timehr[1] = Rcvbuf1[3];
          timehr[3] = 0;
					timedat[2] = ':';
					timedat[3] = Rcvbuf1[4];
					timedat[4] = Rcvbuf1[5];
					timedat[5] = ':';
					timedat[6] = Rcvbuf1[6];
					timedat[7] = Rcvbuf1[7];
					timedat[8] = 0;
      }
      else if (Rcvbuf1[1] == 'Q'){
          rxtxchk = Rcvbuf1[2] - '0';   //convert character to integer
      }
			break;	
		case 'L':	//LCD response-buffer up to 17 is top line plus spaces
			if (Rcvbuf1[1] == 'C'){
					lcddat[0] = Rcvbuf1[17];  // still one space..
					lcddat[1] = Rcvbuf1[18];
          lcddat[2] = Rcvbuf1[19];
					lcddat[3] = Rcvbuf1[20];
					lcddat[4] = Rcvbuf1[21];
					lcddat[5] = Rcvbuf1[22];
					lcddat[6] = Rcvbuf1[23];
					lcddat[7] = Rcvbuf1[24];
					lcddat[8] = Rcvbuf1[25];
					lcddat[9] = Rcvbuf1[26];
					lcddat[10] = Rcvbuf1[27];
					lcddat[11] = Rcvbuf1[28];
					lcddat[12] = Rcvbuf1[29];
          lcddat[13] = Rcvbuf1[30];
					lcddat[14] = Rcvbuf1[31];
					lcddat[15] = Rcvbuf1[32];
					lcddat[16] = Rcvbuf1[33];
          lcddat[17] = 0;
				}
			break;	
		case 'M':	//Mode response
			if (Rcvbuf1[1] == 'D'){
          modex = Rcvbuf1[2] - '0';   //convert character to integer
      }
			break;	
		case 'S': 	//S meter response or SWR response
			if (Rcvbuf1[1] == 'M'){
				smeterdat[0] = Rcvbuf1[2];
				smeterdat[1] = Rcvbuf1[3];
				smeterdat[2] = '.';
				smeterdat[3] = Rcvbuf1[4];
				smeterdat[4] = 0;
				}
      else if (Rcvbuf1[1] == 'W'){
        swrdat[0] = Rcvbuf1[2];
        swrdat[1] = '.';
        swrdat[2] = Rcvbuf1[3];
        swrdat[3] = Rcvbuf1[4];
        swrdat[4] = 0;
      }
			break;	
	}
}

/////////////////////////////////////////////////////////////////////////
// On start up, if there is no response to this command, keep looking for 1 second
// then go back to the main loop. Check each second for connection. 
/////////////////////////////////////////////////////////////////////////
void Chkserconn() {	// Use the get time command to check for a serial connection
	Serial2.write("TM;");		// get time data
	prevmils = millis();
  	while (!Rcvbufavail){
		unsigned long currentMillis = millis();	
		if (currentMillis - prevmils > 1000){ //wait for up to 1 sec for response
			Serconnavail = 0;
			return;
		}
	}
	Serconnavail = 1;
  Rcvbufavail = 0;
}

//////////// Use FA to do band change ////////////////
void BandChgSnd(){
  Serial2.write("FA");
  Serial2.write(bandsets[bandsel]);
  Serial2.write(";");
}
/////////// Send Audio Gain ////////////////////
void VolChgSnd(){
	Serial2.write("AG");
  sprintf(AudGn,"%d",AudGnVol);   //convert back to string
	Serial2.write(AudGn);	// send current audio gain (volume) level
	Serial2.write(";");
}

// ============================================================
// Functions for the web server
// ============================================================
int freqToBand(long freq) {
  if (freq >= 1800000 && freq <= 2000000) return 0;   // 160m
  if (freq >= 3500000 && freq <= 3800000) return 1;   // 80m
  if (freq >= 5300000 && freq <= 5450000) return 2;   // 60m
  if (freq >= 7000000 && freq <= 7300000) return 3;   // 40m
  if (freq >= 10100000 && freq <= 10150000) return 4; // 30m
  if (freq >= 14000000 && freq <= 14350000) return 5; // 20m
  if (freq >= 18068000 && freq <= 18168000) return 6; // 17m
  if (freq >= 21000000 && freq <= 21450000) return 7; // 15m
  if (freq >= 24890000 && freq <= 24990000) return 8; // 12m
  if (freq >= 28000000 && freq <= 29700000) return 9; // 10m
  if (freq >= 50000000 && freq <= 54000000) return 10; // 6m
  return bandsel; // behåll nuvarande om utanför band
}

void setupWebServer() {
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
    Serial.print(".");
    retries++;
  }
if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected! IP: " + WiFi.localIP().toString());
    neopixelWrite(RGB_BUILTIN, 0, RGB_BRIGHTNESS, RGB_BRIGHTNESS); // cyan = wifi ok
    if (MDNS.begin(mdnsName)) {
      Serial.println("mDNS started: http://qmx.local/");
    }
    MDNS.addService("http", "tcp", 80);
  } else {
    Serial.println("\nWiFi failed - continuing without web server");
    return;
  }

  // ---- GET /status  ----
  // Returns all radio state as JSON - polled every second by the web page
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    char json[512];

    // Format frequency nicely for display (xx.xxx.xxx Hz)
    char freqAdisp[16], freqBdisp[16];
    formatFreqDisplay(Vfreqa, freqAdisp);
    formatFreqDisplay(Vfreqb, freqBdisp);

    const char* modeStr = getModeString(modex);

    snprintf(json, sizeof(json),
      "{"
        "\"freqA\":%ld,"
        "\"freqB\":%ld,"
        "\"freqAdisp\":\"%s\","
        "\"freqBdisp\":\"%s\","
        "\"vfo\":%d,"
        "\"mode\":%d,"
        "\"modeStr\":\"%s\","
        "\"step\":%ld,"
        "\"smeter\":\"%s\","
        "\"swr\":\"%s\","
        "\"rxtx\":%d,"
        "\"volume\":%d,"
        "\"time\":\"%s\","
        "\"connected\":%d,"
        "\"band\":%d,"
        "\"bandName\":\"%s\""
      "}",
      Vfreqa, Vfreqb,
      freqAdisp, freqBdisp,
      vfoaorb,
      modex, modeStr,
      stepset,
      smeterdat, swrdat,
      rxtxchk,
      AudGnVol,
      timedat,
      Serconnavail ? 1 : 0,
      bandsel,
      bandtxt[bandsel]
    );

    request->send(200, "application/json", json);
  });

  // ---- POST /control  ----
  // Accepts commands from the web page to control the transceiver
      // ---- POST /control ----
  server.on("/control", HTTP_GET, [](AsyncWebServerRequest *request) {
  String cmd = "";
  long value = 0;
  if (request->hasParam("cmd")) cmd = request->getParam("cmd")->value();
  if (request->hasParam("value")) value = request->getParam("value")->value().toInt();
  
  Serial.println("CMD: " + cmd + " VAL: " + String(value));

  if (cmd == "stepfreq") {
    if (vfoaorb == 0) Vfreqa += value;
    else Vfreqb += value;
    FreqChgsend(); freqUpdateNeeded = true;
  } else if (cmd == "setfreq") {
    if (vfoaorb == 0) Vfreqa = value;
    else Vfreqb = value;
    FreqChgsend(); freqUpdateNeeded = true;
  } else if (cmd == "setstep") {
    if (value==10||value==100||value==1000||value==10000) stepset = value;
    Botline();
  } else if (cmd == "setmode") {
    modex = value;
    Serial2.write("MD");
    Serial2.write((int)(modex + '0'));
    Serial2.write(";");
    Botline();
  } else if (cmd == "setvfo") {
    vfoaorb = value;
    Serial2.write("FR");
    Serial2.write(vfoaorb ? "1" : "0");
    Serial2.write(";");
    Topline();
  } else if (cmd == "setvolume") {
    AudGnVol = value;
    if (AudGnVol < 1) AudGnVol = 1;
    if (AudGnVol > 799) AudGnVol = 799;
    VolChgSnd(); LCbotline();
  } else if (cmd == "setband") {
    bandsel = value;
    if (bandsel < 0) bandsel = 0;
    if (bandsel > 10) bandsel = 10;
    BandChgSnd(); LCbotline();
  }
  request->send(200, "text/plain", "OK");
});



  // ---- GET /  ----
  // Serves the main HTML page (stored as a string in flash)
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", WEB_PAGE_HTML);
  });

  server.on("/manifest.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    String manifest = "{"
      "\"name\":\"QMX+ Remote\","
      "\"short_name\":\"QMX+\","
      "\"start_url\":\"/\","
      "\"display\":\"standalone\","
      "\"background_color\":\"#0a0e1a\","
      "\"theme_color\":\"#00d4ff\","
      "\"icons\":[{"
        "\"src\":\"data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'><rect width='100' height='100' fill='%230a0e1a'/><text y='.9em' font-size='90' x='10'>📻</text></svg>\","
        "\"sizes\":\"any\","
        "\"type\":\"image/svg+xml\""
      "}]"
    "}";
    request->send(200, "application/json", manifest);
  });

  server.on("/sw.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/javascript", "self.addEventListener('fetch', e => e.respondWith(fetch(e.request)));");
  });
  
  server.begin();
  Serial.println("Web server started");
}



// ============================================================
// WEB PAGE HTML - stored in flash memory (PROGMEM)
// This is the full single-page app served to browsers
// ============================================================

const char WEB_PAGE_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="sv">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>QMX+ Remote</title>
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-title" content="QMX+ Remote">
<link rel="manifest" href="/manifest.json">
<style>
  @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Orbitron:wght@400;700;900&display=swap');

  :root {
    --bg: #0a0e1a;
    --panel: #0f1628;
    --border: #2a4a7a;
    --border-frame: #1a6aff;
    --accent: #00d4ff;
    --accent2: #00ff88;
    --danger: #ff4455;
    --tx-color: #ff4455;
    --rx-color: #00ff88;
    --text: #c8d8f0;
    --dim: #8aaccf;
    --freq-color: #00d4ff;
    --btn-inactive-border: #6888aa;
    --btn-inactive-text: #8aaccf;
    --glow: 0 0 20px rgba(0,212,255,0.3);
  }

  * { margin:0; padding:0; box-sizing:border-box; }

  body {
    background: var(--bg);
    color: var(--text);
    font-family: 'Share Tech Mono', monospace;
    min-height: 100vh;
    display: flex;
    flex-direction: column;
    align-items: center;
    padding: 10px;
    background-image:
      radial-gradient(ellipse at 20% 20%, rgba(0,80,160,0.15) 0%, transparent 60%),
      radial-gradient(ellipse at 80% 80%, rgba(0,40,100,0.1) 0%, transparent 60%);
  }

  .container {
    width: 100%;
    max-width: 520px;
    display: flex;
    flex-direction: column;
    gap: 8px;
  }

  /* ---- Ram 1: Header ---- */
  .header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 10px 16px;
    background: var(--panel);
    border: 1px solid var(--border-frame);
    border-radius: 8px;
  }
  .header-left {
    display: flex;
    align-items: center;
    gap: 14px;
  }
  .header-title {
    font-family: 'Orbitron', sans-serif;
    font-weight: 900;
    font-size: 1.1rem;
    letter-spacing: 3px;
    color: var(--accent);
    text-shadow: var(--glow);
  }
  .rxtx-badge {
    font-family: 'Orbitron', sans-serif;
    font-weight: 700;
    font-size: 0.85rem;
    letter-spacing: 2px;
    padding: 4px 12px;
    border-radius: 4px;
    border: 1px solid currentColor;
    min-width: 48px;
    text-align: center;
    transition: all 0.2s;
  }
  .rxtx-badge.rx { color: var(--rx-color); border-color: var(--rx-color); box-shadow: 0 0 8px rgba(0,255,136,0.2); }
  .rxtx-badge.tx { color: var(--tx-color); border-color: var(--tx-color); box-shadow: 0 0 12px rgba(255,68,85,0.4); }
  .swr-value { color: var(--dim); font-size: 0.85rem; }
  .swr-value span { color: var(--text); }
  .header-right {
    display: flex;
    align-items: center;
    gap: 10px;
  }
  .status-dot {
    width: 10px; height: 10px;
    border-radius: 50%;
    background: var(--btn-inactive-border);
    transition: background 0.3s, box-shadow 0.3s;
  }
  .status-dot.connected { background: var(--accent2); box-shadow: 0 0 8px var(--accent2); }

  /* ---- Ram 3: VFO Tabs ---- */
  .vfo-tabs {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 6px;
  }
  .vfo-tab {
    padding: 8px;
    background: var(--panel);
    border: 1px solid var(--border-frame);
    border-radius: 8px;
    cursor: pointer;
    transition: all 0.2s;
    text-align: center;
  }
  .vfo-tab.active {
    border-color: var(--accent2);
    background: rgba(0,255,136,0.07);
    box-shadow: 0 0 12px rgba(0,255,136,0.2);
  }
  .vfo-label {
    font-family: 'Orbitron', sans-serif;
    font-size: 0.65rem;
    letter-spacing: 2px;
    color: var(--btn-inactive-text);
    margin-bottom: 4px;
  }
  .vfo-tab.active .vfo-label { color: var(--accent2); }
  .vfo-freq {
    font-family: 'Share Tech Mono', monospace;
    font-weight: 700;
    font-size: 1.2rem;
    color: var(--btn-inactive-text);
    letter-spacing: 1px;
  }
  .vfo-tab.active .vfo-freq { color: var(--freq-color); text-shadow: 0 0 10px rgba(0,212,255,0.5); }

  /* ---- Ram 4: Main Frequency Display ---- */
  .freq-display {
    background: var(--panel);
    border: 1px solid var(--border-frame);
    border-radius: 10px;
    padding: 14px 16px 12px;
    text-align: center;
    position: relative;
    overflow: hidden;
  }
  .freq-display::before {
    content: '';
    position: absolute;
    top: 0; left: 0; right: 0;
    height: 2px;
    background: linear-gradient(90deg, transparent, var(--accent), transparent);
    opacity: 0.6;
  }
  .freq-top-row {
    display: flex;
    justify-content: center;
    align-items: baseline;
    gap: 16px;
    margin-bottom: 10px;
  }
  .freq-main {
    font-family: 'Share Tech Mono', monospace;
    font-weight: 900;
    font-size: clamp(2.5rem, 11vw, 3.6rem);
    color: var(--freq-color);
    letter-spacing: 2px;
    text-shadow: 0 0 20px rgba(0,212,255,0.5);
    line-height: 1;
  }
  .time-display {
    color: var(--accent2);
    font-size: 0.95rem;
    letter-spacing: 1px;
    font-family: 'Share Tech Mono', monospace;
  }

  /* Tune buttons - label inside button */
  .tune-grid {
    display: grid;
    grid-template-columns: repeat(4, 1fr);
    gap: 5px;
    margin-bottom: 8px;
  }
  .tune-col { display: flex; flex-direction: column; gap: 4px; }
  .tune-btn {
    background: rgba(0,212,255,0.06);
    border: 1px solid var(--btn-inactive-border);
    color: var(--btn-inactive-text);
    font-family: 'Share Tech Mono', monospace;
    font-size: 0.72rem;
    padding: 9px 4px;
    border-radius: 5px;
    cursor: pointer;
    width: 100%;
    transition: all 0.15s;
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 1px;
    line-height: 1.2;
  }
  .tune-btn .tune-step-label {
    font-size: 0.6rem;
    color: var(--dim);
  }
  .tune-btn .tune-arrow {
    font-size: 0.85rem;
  }
  .tune-btn:hover { background: rgba(0,212,255,0.15); border-color: var(--accent); color: var(--accent); }
  .tune-btn:hover .tune-step-label { color: var(--accent); }
  .tune-btn:active { transform: scale(0.95); background: rgba(0,212,255,0.25); }

  /* Step selector */
  .step-row {
    display: flex;
    gap: 5px;
    justify-content: center;
    margin-top: 15px;
  }
  .step-btn {
    background: transparent;
    border: 1px solid var(--btn-inactive-border);
    color: var(--btn-inactive-text);
    font-family: 'Share Tech Mono', monospace;
    font-size: 0.8rem;
    padding: 8px 10px;
    border-radius: 4px;
    cursor: pointer;
    transition: all 0.15s;
  }
  .step-btn.active { border-color: var(--accent2); color: var(--accent2); background: rgba(0,255,136,0.08); }
  .step-btn:hover:not(.active) { border-color: var(--text); color: var(--text); }

  /* ---- Ram 5: Mode & S-meter ---- */
  .status-row {
    display: grid;
    grid-template-columns: auto 1fr auto;
    gap: 8px;
    align-items: center;
    background: var(--panel);
    border: 1px solid var(--border-frame);
    border-radius: 8px;
    padding: 10px 14px;
  }
  .mode-btns { display: flex; gap: 5px; }
  .mode-btn {
    background: transparent;
    border: 1px solid var(--btn-inactive-border);
    color: var(--btn-inactive-text);
    font-family: 'Share Tech Mono', monospace;
    font-size: 0.8rem;
    padding: 8px 10px;
    border-radius: 4px;
    cursor: pointer;
    letter-spacing: 1px;
    transition: all 0.15s;
    min-width: 52px;
    text-align: center;
  }
  .mode-btn.active { border-color: var(--accent2); color: var(--accent2); background: rgba(0,255,136,0.08); }
  .mode-btn:hover:not(.active) { color: var(--text); border-color: var(--text); }

  .smeter-wrap { display: flex; flex-direction: column; align-items: center; gap: 3px; }
  .smeter-bar {
    width: 85px; height: 8px;
    background: #1a3a5a;
    border-radius: 4px;
    overflow: hidden;
  }
  .smeter-fill {
    height: 100%;
    border-radius: 4px;
    background: linear-gradient(90deg, var(--accent2), #88ff00, #ffcc00, var(--danger));
    transition: width 0.3s ease;
    width: 0%;
  }
  .smeter-val { font-size: 0.7rem; color: var(--dim); }
  .smeter-val span { color: var(--accent2); }

  /* ---- Ram 6: Band Selector ---- */
  .band-section {
    background: var(--panel);
    border: 1px solid var(--border-frame);
    border-radius: 8px;
    padding: 10px 14px;
  }
  .band-rows {
    display: flex;
    flex-direction: column;
    gap: 5px;
    align-items: center;
  }
  .band-row {
    display: flex;
    gap: 5px;
    justify-content: center;
  }
  .band-btn {
    background: transparent;
    border: 1px solid var(--btn-inactive-border);
    color: var(--btn-inactive-text);
    font-family: 'Share Tech Mono', monospace;
    font-size: 0.75rem;
    padding: 8px 10px;
    border-radius: 4px;
    cursor: pointer;
    transition: all 0.15s;
    min-width: 48px;
    text-align: center;
  }
  .band-btn.wide { min-width: 58px; }
  .band-btn.active { border-color: var(--accent2); color: var(--accent2); background: rgba(0,255,136,0.08); }
  .band-btn:hover:not(.active) { color: var(--text); border-color: var(--text); }
  .band-label-btn {
    background: transparent;
    border: 1px solid transparent;
    color: var(--dim);
    font-family: 'Share Tech Mono', monospace;
    font-size: 0.7rem;
    padding: 8px 4px;
    letter-spacing: 1px;
    cursor: default;
  }

  /* ---- Ram 7: Volume ---- */
  .vol-section {
    background: var(--panel);
    border: 1px solid var(--border-frame);
    border-radius: 8px;
    padding: 10px 14px;
  }
  .vol-row-wrap {
    display: flex;
    align-items: center;
    gap: 8px;
  }
  .vol-row {
    display: flex;
    align-items: center;
    gap: 10px;
    flex: 1;
  }
  .vol-label { font-size: 0.7rem; color: var(--dim); min-width: 30px; }
  input[type=range] {
    flex: 1;
    -webkit-appearance: none;
    height: 4px;
    background: #1a3a5a;
    border-radius: 2px;
    outline: none;
  }
  input[type=range]::-webkit-slider-thumb {
    -webkit-appearance: none;
    width: 16px; height: 16px;
    border-radius: 50%;
    background: var(--btn-inactive-border);
    cursor: pointer;
    box-shadow: 0 0 4px rgba(100,150,200,0.4);
  }

  /* ---- Fullscreen button ---- */
  .fullscreen-btn {
    flex-shrink: 0;
    width: 32px; height: 32px;
    background: transparent;
    border: 1px solid var(--btn-inactive-border);
    border-radius: 5px;
    cursor: pointer;
    display: flex;
    align-items: center;
    justify-content: center;
    transition: border-color 0.2s;
    padding: 0;
  }
  .fullscreen-btn:hover { border-color: var(--accent); }
  .fullscreen-btn svg { width: 16px; height: 16px; stroke: var(--btn-inactive-border); transition: stroke 0.2s; }
  .fullscreen-btn:hover svg { stroke: var(--accent); }

  /* ---- No connection overlay ---- */
  .no-conn {
    display: none;
    position: fixed;
    inset: 0;
    background: rgba(10,14,26,0.85);
    justify-content: center;
    align-items: center;
    font-family: 'Orbitron', sans-serif;
    font-size: 1rem;
    color: var(--danger);
    letter-spacing: 3px;
    backdrop-filter: blur(4px);
    z-index: 100;
  }
  .no-conn.show { display: flex; flex-direction: column; gap: 12px; }
  .no-conn-dot { width: 12px; height: 12px; border-radius: 50%; background: var(--danger); box-shadow: 0 0 12px var(--danger); animation: pulse 1s infinite; }
  @keyframes pulse { 0%,100%{opacity:1} 50%{opacity:0.3} }

  ::-webkit-scrollbar { width: 4px; }
  ::-webkit-scrollbar-track { background: var(--bg); }
  ::-webkit-scrollbar-thumb { background: var(--border); border-radius: 2px; }
</style>
</head>
<body>

<div class="no-conn" id="noConn">
  <div class="no-conn-dot"></div>
  NO CONTACT WITH QMX+
</div>

<div class="container">

  <!-- Ram 1: Header med RX/TX -->
  <div class="header">
    <div class="header-left">
      <div class="header-title">QMX+</div>
      <div class="rxtx-badge rx" id="rxtxBadge">RX</div>
      <div class="swr-value" id="swrWrap" style="display:none">SWR <span id="swrVal">1.00</span></div>
    </div>
    <div class="header-right">
      <div style="font-size:0.7rem;color:var(--dim);letter-spacing:2px;">REMOTE</div>
      <div class="status-dot" id="statusDot"></div>
    </div>
  </div>

  <!-- Ram 3: VFO tabs -->
  <div class="vfo-tabs">
    <div class="vfo-tab active" id="vfoATab" onclick="setVFO(0)">
      <div class="vfo-label">VFO A</div>
      <div class="vfo-freq" id="vfoAFreq">--.---.---</div>
    </div>
    <div class="vfo-tab" id="vfoBTab" onclick="setVFO(1)">
      <div class="vfo-label">VFO B</div>
      <div class="vfo-freq" id="vfoBFreq">--.---.---</div>
    </div>
  </div>

  <!-- Ram 4: Main freq + tune + tid -->
  <div class="freq-display">
    <div class="freq-top-row">
      <div class="freq-main" id="freqMain">--.---.---</div>
    </div>
    <div style="text-align:center; margin-bottom:10px;">
      <div class="time-display" id="timeDisp">--:--:--</div>
    </div>

    <div class="tune-grid" id="tuneGrid"></div>

    <div class="step-row">
      <div style="font-size:0.6rem;color:var(--dim);letter-spacing:2px;line-height:30px;margin-right:2px;">STEP:</div>
      <button class="step-btn" onclick="setStep(10)">10 Hz</button>
      <button class="step-btn" onclick="setStep(100)">100 Hz</button>
      <button class="step-btn" onclick="setStep(1000)">1 kHz</button>
      <button class="step-btn" onclick="setStep(10000)">10 kHz</button>
    </div>
  </div>

  <!-- Ram 5: Mode + S-meter -->
  <div class="status-row">
    <div class="mode-btns">
      <button class="mode-btn" data-mode="2" onclick="setMode(2)">USB</button>
      <button class="mode-btn" data-mode="1" onclick="setMode(1)">LSB</button>
      <button class="mode-btn wide" data-mode="3" onclick="setMode(3)">CW</button>
      <button class="mode-btn" data-mode="6" onclick="setMode(6)">DIGI</button>
    </div>
    <div></div>
    <div class="smeter-wrap">
      <div class="smeter-bar"><div class="smeter-fill" id="smeterFill"></div></div>
      <div class="smeter-val">S <span id="smeterVal">--</span> dB</div>
    </div>
  </div>

  <!-- Ram 6: Band selector - två rader -->
  <div class="band-section">
    <div class="band-rows">
      <div class="band-row">
        <span class="band-label-btn">BAND</span>
        <button class="band-btn" data-band="0" onclick="setBand(0)">160m</button>
        <button class="band-btn" data-band="1" onclick="setBand(1)">80m</button>
        <button class="band-btn" data-band="2" onclick="setBand(2)">60m</button>
        <button class="band-btn" data-band="3" onclick="setBand(3)">40m</button>
        <button class="band-btn" data-band="4" onclick="setBand(4)">30m</button>
      </div>
      <div class="band-row">
        <button class="band-btn" data-band="5" onclick="setBand(5)">20m</button>
        <button class="band-btn" data-band="6" onclick="setBand(6)">17m</button>
        <button class="band-btn" data-band="7" onclick="setBand(7)">15m</button>
        <button class="band-btn" data-band="8" onclick="setBand(8)">12m</button>
        <button class="band-btn" data-band="9" onclick="setBand(9)">10m</button>
        <button class="band-btn" data-band="10" onclick="setBand(10)">6m</button>
      </div>
    </div>
  </div>

  <!-- Ram 7: Volume + fullscreen -->
  <div class="vol-section">
    <div class="vol-row-wrap">
      <div class="vol-row">
        <div class="vol-label">VOL</div>
        <input type="range" id="volSlider" min="1" max="799" value="180"
               oninput="onVolInput(this.value)" onchange="sendVolume(this.value)">
      </div>
      <button class="fullscreen-btn" id="fsBtn" onclick="toggleFullscreen()">
        <svg id="fsIcon" viewBox="0 0 16 16" fill="none" stroke-width="1.8" stroke-linecap="round">
          <polyline points="1,6 1,1 6,1"/>
          <polyline points="10,1 15,1 15,6"/>
          <polyline points="1,10 1,15 6,15"/>
          <polyline points="15,10 15,15 10,15"/>
        </svg>
      </button>
    </div>
  </div>

</div>

<script>
  let currentStep = 10;
  let currentVFO = 0;
  let currentMode = 2;
  let pollTimer = null;

  // ---- Tune button grid - label inside button ----
  const STEPS = [10, 100, 1000, 10000];
  function buildTuneGrid() {
    const grid = document.getElementById('tuneGrid');
    grid.innerHTML = '';
    STEPS.forEach(s => {
      const col = document.createElement('div');
      col.className = 'tune-col';
      [+s, -s].forEach(delta => {
        const btn = document.createElement('button');
        btn.className = 'tune-btn';
        const label = s < 1000 ? s + 'Hz' : (s/1000) + 'kHz';
        btn.innerHTML = `<span class="tune-step-label">${label}</span><span class="tune-arrow">${delta > 0 ? '▲' : '▼'}</span>`;
        btn.onclick = () => sendCommand('stepfreq', delta);
        col.appendChild(btn);
      });
      grid.appendChild(col);
    });
  }

  // ---- Mushjul för frekvensändring ----
  let wheelTimeout = null;
  let wheelAccum = 0;
  document.addEventListener('wheel', function(e) {
    e.preventDefault();
    wheelAccum += e.deltaY < 0 ? currentStep : -currentStep;
    if (!wheelTimeout) {
      wheelTimeout = setTimeout(() => {
        sendCommand('stepfreq', wheelAccum);
        wheelAccum = 0;
        wheelTimeout = null;
      }, 150);
    }
  }, { passive: false });

  buildTuneGrid();

  // ---- Poll /status ----
  function startPolling() {
    poll();
    pollTimer = setInterval(poll, 250);
  }

  async function poll() {
    try {
      const r = await fetch('/status');
      if (!r.ok) throw new Error();
      const d = await r.json();
      updateUI(d);
    } catch(e) {
      setConnected(false);
    }
  }

  function updateUI(d) {
    const isConnected = parseInt(d.connected) === 1;
    setConnected(isConnected);
    if (!isConnected) return;

    // Freq display
    document.getElementById('freqMain').textContent =
      d.vfo === 0 ? d.freqAdisp : d.freqBdisp;
    document.getElementById('vfoAFreq').textContent = d.freqAdisp;
    document.getElementById('vfoBFreq').textContent = d.freqBdisp;

    // VFO tabs
    document.getElementById('vfoATab').classList.toggle('active', d.vfo === 0);
    document.getElementById('vfoBTab').classList.toggle('active', d.vfo === 1);
    currentVFO = d.vfo;

    // RX/TX
    const badge = document.getElementById('rxtxBadge');
    const swrWrap = document.getElementById('swrWrap');
    if (d.rxtx === 1) {
      badge.textContent = 'TX';
      badge.className = 'rxtx-badge tx';
      swrWrap.style.display = '';
      document.getElementById('swrVal').textContent = d.swr;
    } else {
      badge.textContent = 'RX';
      badge.className = 'rxtx-badge rx';
      swrWrap.style.display = 'none';
    }

    // Time
    document.getElementById('timeDisp').textContent = d.time;

    // Mode
    currentMode = d.mode;
    document.querySelectorAll('.mode-btn').forEach(b => {
      b.classList.toggle('active', parseInt(b.dataset.mode) === d.mode);
    });

    // Step
    currentStep = d.step;
    updateStepBtns(d.step);

    // S-meter
    const sval = (parseFloat(d.smeter) || 0) * 10;
    document.getElementById('smeterVal').textContent = sval.toFixed(0);
    document.getElementById('smeterFill').style.width = Math.min(100, (sval / 80) * 100) + '%';

    // Band
    document.querySelectorAll('.band-btn').forEach(b => {
      b.classList.toggle('active', parseInt(b.dataset.band) === d.band);
    });

    // Volume
    const slider = document.getElementById('volSlider');
    if (document.activeElement !== slider) {
      slider.value = d.volume;
    }
  }

  function updateStepBtns(step) {
    document.querySelectorAll('.step-btn').forEach(b => {
      const v = parseInt(b.textContent) * (b.textContent.includes('k') ? 1000 : 1);
      b.classList.toggle('active', v === step);
    });
  }

  function setConnected(ok) {
    document.getElementById('statusDot').className = 'status-dot' + (ok ? ' connected' : '');
    document.getElementById('noConn').className = 'no-conn' + (ok ? '' : ' show');
  }

  // ---- Control functions ----
  async function sendCommand(cmd, value) {
    try {
      await fetch('/control?cmd=' + cmd + '&value=' + value);
      poll();
    } catch(e) {}
  }

  function setVFO(v) { sendCommand('setvfo', v); }
  function setMode(m) { sendCommand('setmode', m); }
  function setStep(s) { currentStep = s; updateStepBtns(s); sendCommand('setstep', s); }
  function setBand(b) { sendCommand('setband', b); }

  function onVolInput(v) {}
  function sendVolume(v) {
    sendCommand('setvolume', v);
  }

  // ---- Pop Out Window ----
  function toggleFullscreen() {
    // På mobil/tablet - använd fullskärm
    if ('ontouchstart' in window || navigator.maxTouchPoints > 0) {
      if (!document.fullscreenElement) {
        document.documentElement.requestFullscreen();
        document.getElementById('fsIcon').innerHTML = `
          <polyline points="6,1 6,6 1,6"/>
          <polyline points="10,1 10,6 15,6"/>
          <polyline points="6,15 6,10 1,10"/>
          <polyline points="10,15 10,10 15,10"/>`;
      } else {
        document.exitFullscreen();
        document.getElementById('fsIcon').innerHTML = `
          <polyline points="1,6 1,1 6,1"/>
          <polyline points="10,1 15,1 15,6"/>
          <polyline points="1,10 1,15 6,15"/>
          <polyline points="15,10 15,15 10,15"/>`;
      }
    } else {
      // På dator - öppna popup-fönster
      const w = 380;
      const h = 640;
      const left = (screen.width - w) / 2;
      const top = (screen.height - h) / 2;
      window.open(
        '/',
        'QMX Remote',
        `width=${w},height=${h},left=${left},top=${top},menubar=no,toolbar=no,location=no,status=no,scrollbars=yes`
      );
    }
  }

  // ---- PWA Service Worker ----
  if ('serviceWorker' in navigator) {
    navigator.serviceWorker.register('/sw.js').catch(() => {});
  }

  startPolling();
</script>
</body>
</html>

)rawhtml";



////////////////////////////////////////////////////////////
// End of HTML
////////////////////////////////////////////////////////////

// ---- Helper: format frequency as "14.074.000" ----
void formatFreqDisplay(long freq, char* out) {
  if (freq >= 10000000) {
    // xx.xxx.xxx
    sprintf(out, "%ld", freq);
    // Insert dots: pos 2 and 6
    char tmp[16];
    sprintf(tmp, "%c%c.%c%c%c.%c%c%c",
      out[0], out[1], out[2], out[3], out[4], out[5], out[6], out[7]);
    strcpy(out, tmp);
  } else {
    // x.xxx.xxx
    sprintf(out, "%ld", freq);
    char tmp[16];
    sprintf(tmp, "%c.%c%c%c.%c%c%c",
      out[0], out[1], out[2], out[3], out[4], out[5], out[6]);
    strcpy(out, tmp);
  }
}

// ---- Helper: mode number to string ----
const char* getModeString(int m) {
  switch(m) {
    case 1: return "LSB";
    case 2: return "USB";
    case 3: return "CW";
    case 6: return "DIGI";
    case 7: return "CWR";
    case 9: return "DIGIR";
    default: return "---";
  }
}


//////////////////////// Setup function-start things up /////////////

void setup() {

  Serial.begin(115200);
  // configure encoder and button pins as inputs
  pinMode(CLK_PIN, INPUT);
  pinMode(DT_PIN, INPUT);
  pinMode(BUTTON1, INPUT_PULLUP);
  pinMode(BUTTON2, INPUT_PULLUP);
  pinMode(BUTTON3, INPUT_PULLUP);


  //  button.setDebounceTime(20); // set debounce time milliseconds
  button1.setDebounceTime(20); // set debounce time milliseconds
  button2.setDebounceTime(20); // set debounce time milliseconds
  button3.setDebounceTime(20); // set debounce time milliseconds
  
  // Register callback functions
  button.attachClick(handleClick);
  button.attachDoubleClick(handleDoubleClick);
  button.attachLongPressStart(handleLongPressStart);
  
  Serial2.begin(BAUD, SERIAL_8N1, RXPIN, TXPIN);  // start serial handler
  
  ////////////////////////////////////////////////////////////
  // TFT setup for 7735
  //*************** USER OPTION 1 SPI_SPEED + TYPE ***********
  int8_t DC_TFT  = 8;
  int8_t RST_TFT = 9;
	int8_t CS_TFT  = 10;  
	if (bhardwareSPI == true) { // hw spi
		uint32_t TFT_SCLK_FREQ = 20000000;  // Spi freq in Hertz
		myTFT.setupGPIO_SPI(TFT_SCLK_FREQ, RST_TFT, DC_TFT, CS_TFT); 
  }



  //********************************************************
  // ****** USER OPTION 2 Screen Setup ****** 
	uint8_t OFFSET_COL = 0;  // 2, These offsets can be adjusted for any issues->
	uint8_t OFFSET_ROW = 0; // 3, with screen manufacture tolerance/defects
	uint16_t TFT_WIDTH = 128;// Screen width in pixels
	uint16_t TFT_HEIGHT = 160; // Screen height in pixels
	myTFT.TFTInitScreenSize(OFFSET_COL, OFFSET_ROW , TFT_WIDTH , TFT_HEIGHT);
  // ******************************************
  // ******** USER OPTION 3 PCB_TYPE  **************************
	myTFT.TFTInitPCBType(myTFT.TFT_ST7735S_Black); // pass enum,4 choices,see README from library
  //**********************************************************

	myTFT.setRotation(myTFT.Degrees_90);    // Change as needed for orientation of display
  // use blue as background for SOLID mode
	myTFT.fillScreen(myTFT.C_BLUE);		//fill blue background
	myTFT.setFont(FontArialRound);
	myTFT.setTextColor(myTFT.C_WHITE, myTFT.C_BLUE);
  myTFT.setCursor(0,45);
  myTFT.print("QMX remote");
  // myTFT.setCursor(15,70);
  // myTFT.print("");  
  	delay(3000);
 	myTFT.fillScreen(myTFT.C_BLUE);		//fill blue background

  neopixelWrite(RGB_BUILTIN,0,0,RGB_BRIGHTNESS); // Blue
  Topline();    // set up top line
  Botline();    // set up bottom line

  Serial2.flush();                                       // wait Serial FIFO to be empty and then spend almost no time processing it
  Serial2.setRxFIFOFull(fifoFull);                      // testing different result based on FIFO Full setup
  Serial2.onReceive(onReceiveFunction, false);  // sets a RX callback function for Serial 2 with timeout or fifofull
   delay (1000);
  neopixelWrite(RGB_BUILTIN,0,RGB_BRIGHTNESS,0); // green

  long lvalue = 1;
    lvalue = strtol(FreqA, NULL, 10);
    Vfreqa = lvalue;
    lvalue = strtol(FreqB, NULL, 10);
    Vfreqb = lvalue;
    Frequpdate(); // write out init freq disp
    prevmils = millis();

  // ============================================================
  // Starting Web Server
  // ============================================================
  setupWebServer();
}

void loop() {
  unsigned long currentMillis = millis();	
  rotloop();
  if (freqUpdateNeeded) {
    freqUpdateNeeded = false;
    Frequpdate();
  }
  if (prev_counter != counter) {
    if (direction == DIRECTION_CW){
      if (bandselproc){
        bandsel++;
        if (bandsel > 10) bandsel = 0;
		LCbotline();
      }
	    else if (VolProc){
		    AudGnVol++;
		    if (AudGnVol > 799) AudGnVol = 799;
        VolChgSnd();
        LCbotline();
	    }
      else if (vfoaorb == 0) {
        Vfreqa = Vfreqa+stepset;
		    FreqChgsend();
        Frequpdate();
	    }
      else if (vfoaorb == 1) {
        Vfreqb = Vfreqb+stepset;
		    FreqChgsend();
        Frequpdate();
	    }
    }
    if (direction == DIRECTION_CCW){
      if (bandselproc){
        bandsel--;
        if (bandsel < 0) bandsel = 10;
        LCbotline();
      }
      else if (VolProc){
        AudGnVol--;
        if (AudGnVol < 1) AudGnVol = 1;
        VolChgSnd();
        LCbotline();
      }
      else if (vfoaorb == 0) {
        Vfreqa = Vfreqa-stepset;
		    FreqChgsend();		
        Frequpdate();
	    }
      else if (vfoaorb == 1) {
        Vfreqb = Vfreqb-stepset;
		    FreqChgsend();
        Frequpdate();
	    }       
	  }
    prev_counter = counter;
  }
  
 // Check for rotary button
 RotButtonCheck();	// Process the rotary button pushes

   
  // Check for button 1
  button1.loop();     // This button changes the VFO from current to next
  if (button1.isPressed()) {
    if (vfoaorb == 0) vfoaorb = 1;
    else if (vfoaorb == 1) vfoaorb = 0;
    Serial2.write("FR");
    if (!vfoaorb) Serial2.write("0");
    else Serial2.write("1");
    Serial2.write(";");
  }

  // Check for button2
  button2.loop();     // This button changes the mode of the xceiver: Rotates thru CW,DIGI,USB,LSB
  if (button2.isPressed()) {
    if (modex == 2) modex = 1;
    else if (modex == 1) modex = 3;
    else if (modex == 3) modex = 6;
    else if (modex == 6) modex = 2;
    else if (modex > 6) modex = 2;
    Serial2.write("MD");
    Serial2.write(modex + '0'); // create ascii number for mode and send
    Serial2.write(";");
  }

  // Check for button3
  button3.loop();     // Stepset in opposit direction compared to RotButton
  if (button3.isPressed())  {
	  if (stepset == 10) stepset = 10000;
    else if (stepset == 100) stepset = 10;
    else if (stepset == 1000) stepset = 100;
    else if (stepset == 10000) stepset = 1000;
    Botline();  // update display
  }

  // one second updates-general status and time
  // Delays are for processing the serial data requested
  // The values used let each command to process properly based on received bytes
	if (currentMillis - prevmils1 > 1000)  { //update clock each second
    Chkserconn();   //used the TM command to look for serial connection
		Topline();
    if (Serconnavail){
      Serial2.write(FAcmd);   //get VFOA 
      delay(10);
      Serial2.write(FBcmd);   //get VFOB 
      delay(10);										 
      Frequpdate(); // Display freq for selected VFO
      Serial2.write(MDcmd);   //mode command request
      delay(4);
      Serial2.write(TQcmd);   //get rx/tx mode 
      delay(4);
      if (rxtxchk == 1){
        Serial2.write(SWcmd);
        delay(4);
      }
      Serial2.write(SMcmd);   //Smeter request
      delay(8);

      Serial2.write(FRcmd);   //get selection
      delay(4);

      Botline();
      Serial2.write(LCcmd);   //get LC data for bottom row
      delay(18);
      LCbotline();
    }
		prevmils1 = currentMillis;
	}

  
} //Main loop end
