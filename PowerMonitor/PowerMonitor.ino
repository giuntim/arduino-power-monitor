
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Arduino_JSON.h>
#include <LCD_I2C.h>

/***
*
* Arduino Power Monitor
* This sketch is for AZ-Delivery D1 + 16x2 LCD 
* Author: Maurizio Giunti https://mauriziogiunti.it
*
***/



#ifndef STASSID
#define STASSID "---SSIDNAME---"
#define STAPSK  "---SSIDPWD---"
#endif

#define NCYCLE    10 // Secondi tra le rilevazioni potenza
#define BLITSECS  10 // Secondi di retroilluminazione
#define PWLIMIT 4400 // Limite allarme potenza
#define BUTTONPIN 16 // Pin bottone retroilluminazione


const char* ssid = STASSID;
const char* password = STAPSK;

// Shelly EM API URL
const char* shellyapiurl = "http://192.168.10.50/status";


// INIT display
LCD_I2C lcd(0x27, 16, 2); // Default address of most PCF8574 modules, change according
int cycle=0;
int backlit=0;
int blink=0;
int buttonState;


void setup() {
    Serial.begin(115200);
    Serial.println("");
    Serial.println("Startup");

    // Setta input per pulsante
    pinMode(BUTTONPIN, INPUT);

    // Init display
    lcd.begin(); // If you are using more I2C devices using the Wire library use lcd.begin(false)
                 // this stop the library(LCD_I2C) from calling Wire.begin()
    lcd.backlight();
    lcd.noBlink();    
    backlit=1;    
    blink=0;    
    lcd.setCursor(0, 0); 
    lcd.print("Init WiFi net"); 

    // Init WiFi
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    for(int i=0;i<30;i++) {
      if(WiFi.waitForConnectResult() == WL_CONNECTED) break; 
      delay(1000);
    }
    if(WiFi.waitForConnectResult() != WL_CONNECTED) {
     Serial.println("Connection Failed! Rebooting...");
     lcd.clear(); 
     lcd.print("Connection Failed! Rebooting...");  
     delay(3000);
     ESP.restart();
  }    
  
  // Ready
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  lcd.clear(); 
  lcd.setCursor(0, 0); 
  lcd.print("Ready");  
  lcd.setCursor(0, 1); 
  lcd.print(WiFi.localIP());
    
}

void loop() {

  // Pulsante per retroilluminazione display
  buttonState = digitalRead(BUTTONPIN);
  //Serial.println(buttonState);
  if(buttonState != 0) {
    backlit=1;
    lcd.backlight();
  }
    
  
  // Ogni NCYCLE secondi chiama API Shelly per rilevare potenza
  if( (cycle%(2*NCYCLE))==1 ) {
    // Call API Shelly EM
    String js = getShellyData();    
    JSONVar data = JSON.parse(js);
    
    // Stampa sul display
    String p0=JSON.stringify(data["emeters"][0]["power"]); // FV
    String p1=JSON.stringify(data["emeters"][1]["power"]); // POW
    
    if(p1!="null") {
      double dp0=p0.toDouble();
      double dp1=p1.toDouble();
      if(dp0<1) p0="OFF 0";
      
      char buffer1[32];
      char buffer2[32];
      sprintf(buffer1, "Fv: %sW", p0);      
      sprintf(buffer2, "Pow: %sW", p1); 
      lcd.clear();
      lcd.setCursor(0, 0);   
      lcd.print(buffer1);  
      lcd.setCursor(0, 1);   
      lcd.print(buffer2);  
       
      // Blink if near overpower
      if(dp1>PWLIMIT) {
        blink++;
      }
      else {
        blink=0;
      }
    }
    else {
      lcd.clear();
      lcd.setCursor(0, 0);   
      lcd.print("Error:");  
      lcd.setCursor(0, 1);   
      lcd.print("ShellyEM no conn");  
      Serial.println("Cannot connect to ShellyEM");
    }
 
  }

  // Backlit off automatico
  if(backlit>0) {
    backlit++;
    if(backlit>(2*BLITSECS)) {
      lcd.noBacklight();  
      backlit=0;
    }
  }
  else {
    // Blink display code
    if(blink>0) {
      if((blink%2)==1) {
        lcd.backlight();        
      }
      else {
        lcd.noBacklight();
      }     
      blink++;
    }
  }
  
  cycle++;
  delay(500);
}

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
