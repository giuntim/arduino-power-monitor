
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino_JSON.h>

/***
*
* Arduino Power Monitor
* This sketch is for AZ-Delivery ESP8266 + 0.96 inches OLED display
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
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET); // 0.96 pollici


// Init other vars
int cycle=0;
int overpower=0;

void setup() {
  Serial.begin(115200);
  Serial.println("");
  Serial.println("Startup");

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  
  // Clear the buffer  
  display.clearDisplay();  
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
   display.clearDisplay();
   lcdPrintln(0,"Connection Failed! Rebooting...");  
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
      double dp0=p0.toDouble();
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
      display.clearDisplay();              
      lcdPrintln(1,"ERROR:\nCannot reach ShellyEM");       
      Serial.println("Cannot connect to ShellyEM");

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

/* No more used
void lcdWrite(int posx, int posy, String txt) {
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(WHITE); // Draw white text
  display.setCursor(posx, posy);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
  display.print(txt);
  display.display();
}
*/

/**
 * Prints line of text on oled display
 */
void lcdPrintln(int posy, String txt) {
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(WHITE); // Draw white text
  int py=posy*12;
  display.setCursor(0, py);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
  display.print(txt);
  display.display();
}

/**
 * Draws progress bar
 */
void drawProgressbar(int x,int y, int width,int height, int progress)
{   
   progress = progress > 100 ? 100 : progress; // set the progress value to 100
   progress = progress < -100 ? -100 : progress; // set the progress value to 100
   float bar = ((float)(width-1) / 100.0) * progress;
   display.drawRect(x, y, width, height, WHITE); // Cornice
   if(bar >= 0.0) {
    display.fillRect(x+2, y+2, bar , height-4, WHITE); // initailize the graphics fillRect(int x, int y, int width, int height)
   }
   else {
    display.fillRect(x+width+bar-1, y+2, -bar , height-4, WHITE); // initailize the graphics fillRect(int x, int y, int width, int height)
   }
   display.display();
}

/**
 * Handle display
 */
void drawScreen(float fvP,float pwP) {
  // 
  char buffer[128];
  int y=0;
  display.clearDisplay();
  display.setTextSize(1);      
  display.setTextColor(WHITE); 
  display.setCursor(0, y);
  display.print("--| POWER MONITOR |--");
  y+=14;

  // FV
  sprintf(buffer,"    fv: %.2fW",fvP);  
  display.setCursor(0, y);
  display.setTextSize(1);
  display.print(buffer);
  y+=12;
  int fvPerc=(int)(100.0*fvP/MAXFVP);
  drawProgressbar(2,y, 124, 10, fvPerc);
  y+=12;
  
  // Pad
  y+=4;
  
  // PW
  sprintf(buffer,"   pow: %.2fW",pwP);  
  display.setCursor(0, y);
  display.setTextSize(1);
  display.print(buffer);
  y+=12;
  int pwPerc=(int)(100.0*pwP/MAXPWP);
  if(pwPerc<100) {
    drawProgressbar(2, y, 124,10, pwPerc);
  }
  else {    
    display.setCursor(0, y);
    display.print(" **** OVER POWER ****");
  }
  y+=12;
  
  // OK print
  display.display();
}

/**
 * Draw blinking OVERPOWER alert
 */
void drawAlert(int c) {
    display.clearDisplay();
    if((c%2)==1) {
      display.setCursor(0, 24);
      display.setTextSize(2);
      display.print("OVERPOWER!");
    }
    
    display.display();
}
