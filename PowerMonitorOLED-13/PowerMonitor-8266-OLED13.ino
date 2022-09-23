
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <Arduino_JSON.h>

/***
*
* Arduino Power Monitor
* This sketch is for AZ-Delivery ESP8266 + 1.3 inches OLED display
* Author: Maurizio Giunti https://mauriziogiunti.it
*
***/


#ifndef STASSID
#define STASSID "---SSIDNAME---"
#define STAPSK  "---SSIDPWD---"
#endif

// Shelly EM API URL
const char* shellyapiurl = "http://192.168.10.50/status";

#define NCYCLE    10 // Secondi tra le rilevazioni potenza
#define MAXPWP  4500 // Max power contatore 4.5kW
#define MAXFVP  5100 // Max power fotovoltaico 5.1kW

// Oled dimensions
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

const char* ssid = STASSID;
const char* password = STAPSK;

// INIT display
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);


// Init other vars
int cycle=0;
int overpower=0;

void setup() {
  Serial.begin(115200);
  Serial.println("");
  Serial.println("Startup");

  
  // Setup display and clear it  
  u8g2.begin();
  lcdPrepare();
  
  // Clear the buffer  
  lcdClear();
  
  // And startup
  lcdPrintln(0,"Init WiFi net"); 
  
  // Init WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  for(int i=0;i<30;i++) {
    if(WiFi.waitForConnectResult() == WL_CONNECTED) break; 
    delay(1000);
  }
  if(WiFi.waitForConnectResult() != WL_CONNECTED) {
   Serial.println("Connection Failed! Rebooting...");   
   lcdPrintln(2,"Connection Failed! Rebooting...");  
   delay(3000);
   ESP.restart();
  }    
  
  // Ready
  Serial.println("Ready");
  Serial.print("IP address: ");
  String localIP = WiFi.localIP().toString().c_str();
  Serial.println(localIP);
  
  lcdPrintln(2,"Ready"); 
  lcdPrintln(3,localIP);    
}


void loop() {
  // Ogni NCYCLE secondi chiama API Shelly per rilevare potenza
  if( (cycle%(2*NCYCLE))==1 ) {
    // Call API Shelly EM
    String js = getShellyData();    
    JSONVar data = JSON.parse(js);
    
    // Stampa sul display
    String p0=JSON.stringify(data["emeters"][0]["power"]); // FV
    String p1=JSON.stringify(data["emeters"][1]["power"]); // POW

    // Verifico di aver ricevuto qualcosa da Shelly EM
    if(p1!="null") {
      double dp0=p0.toDouble(); // Inverto il segno perché dal sensore la produzione mi arriva in negativo
      double dp1=p1.toDouble();
      if(dp0<0) dp0=0; // FV non deve andare sotto zero
      drawScreen(dp0,dp1);
             
      // Blink if near overpower
      if(dp1>MAXPWP) {
        overpower++;
      }
      else {
        overpower=0;
      }
    }
    else {
      Serial.println("Cannot connect to ShellyEM");
      lcdClear();              
      lcdPrintln(1,"ERROR:");
      lcdPrintln(2,"Cannot reach ShellyEM");       
     
      // Attendo 10 secondi e riprovo
      delay(10000);
    }
 
  }

  // Blink display overpower error
  if(overpower>0) {
    drawAlert(overpower);
    overpower++;
  }
    
  cycle++;
  delay(500);
}

/**
 * Polls ShellyEM status API
 */
String getShellyData() {
  String ret; 
  WiFiClient wifiClient;
  HTTPClient http;
  http.begin(wifiClient,shellyapiurl);
  int statusCode = http.GET();
  ret=http.getString();
  //Serial.println(ret);
  http.end();

  return ret;
}


/**
 * Handle display
 */

void lcdPrepare() {
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setFontRefHeightExtendedText();
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  u8g2.setFontDirection(0);
}

void lcdClear() {
  u8g2.clearBuffer();
  u8g2.sendBuffer();
}

/**
 * Prints line of text on oled display
 */
void lcdPrintln(int posy, String txt) {
  int py=posy*12;
  u8g2.drawStr(0, py, txt.c_str());
  u8g2.sendBuffer();
}

void drawProgressbar(int x,int y, int width,int height, int progress)
{   
   progress = progress > 100 ? 100 : progress; // set the progress value to 100
   progress = progress < -100 ? -100 : progress; // set the progress value to 100
   float bar = ((float)(width-1) / 100.0) * progress;
   u8g2.drawFrame(x, y, width, height); // Cornice
   if(bar >= 0.0) {
    u8g2.drawBox(x+2, y+2, bar , height-4); // initailize the graphics fillRect(int x, int y, int width, int height)
   }
   else {
    u8g2.drawBox(x+width+bar-1, y+2, -bar , height-4); // initailize the graphics fillRect(int x, int y, int width, int height)
   }
}
 
void drawScreen(float fvP,float pwP) {
  // 
  char buffer[128];
  int y=0;
  u8g2.clearBuffer();
  u8g2.drawStr(0, y, "--| POWER MONITOR |--");
  y+=14;

  // FV
  sprintf(buffer,"    fv: %.2fW",fvP);  
  u8g2.drawStr(0, y, buffer);
  y+=12;
  int fvPerc=(int)(100.0*fvP/MAXFVP);
  drawProgressbar(2,y, 124, 10, fvPerc);
  y+=12;
  
  // Pad
  y+=4;
  
  // PW
  sprintf(buffer,"   pow: %.2fW",pwP);  
  u8g2.drawStr(0, y, buffer);
  y+=12;
  int pwPerc=(int)(100.0*pwP/MAXPWP);
  if(pwPerc<100) {
    drawProgressbar(2, y, 124,10, pwPerc);
  }
  else {    
    u8g2.drawStr(0, y, " **** OVER POWER ****");
  }
  y+=12;
  
  // OK print
  u8g2.sendBuffer();
}

/**
 * Draw blinking OVERPOWER alert
 */
void drawAlert(int c) {
  u8g2.clearBuffer();
  if((c%2)==1) {
    u8g2.drawStr(0, 24, "**** OVER POWER ****");      
  }
  u8g2.sendBuffer();    
}
