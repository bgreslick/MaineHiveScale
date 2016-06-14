///////////////////////////////
// Maine Hive Scale Project  //
///////////////////////////////
// Created by Ben Greslick
//
// http://www.greslick.com/honey
//
// GPL v2
//
// Description: This project uses an ESP8266 board programmed with Arduino
//      more specifically, the Sparkfun Thing (not the dev board). I'm using the
//      thing, mostly because it's easy to program and can charge a battery 
//   You also need an HX711, combinator board, DS18B20 (temp), battery,  solar panel,
//   and 4x50kg 3-wire half bridge load cells
//
// Version .8.1   Date 6/x/2016
//
// Release Notes:
//   Version 8: Working on the Webserver interface to set SSID, pass, API, and set scale
//       and save to EEPROM
//   Version 7: hardcoded SSID, API, scale...
//  
//  TODO:
//     - Read dip/switch to see if it's in setup or run mode
//     - Deep Sleep
//     - read from 2 scales
//     - read temp
//
// Pinout:
//   0/4 = Scale 1 DOUT/CLK
//   A0 = SW_SETUP switch to ground
//   13 = temperature 
//   SDA(2)/SCL(14) Scale 2 DOUT/CLK
//   XPD to DTR = disconnect for programming, connect when running for deep sleep.
///////////////////////////////

// SoftwareSerial is required (even you don't intend on using it).
#include <SoftwareSerial.h> 
#include <ESP8266WiFi.h>
#include "HX711.h"
#include <EEPROM.h>

const int EEPROM_OFFSET = 1;

struct CustomConfigObject {
  int version;  
  float set_scale1;
  int scale1_offset;
  float set_scale2;
  int scale2_offset;
  char wifi[25];
  char wifi_pass[25];
  char pubkey[25];
  char privkey[25];
  int sleep;
  char comments[50];
};

CustomConfigObject customconfig;

// 1 = Setup mode (become WAP) .  0 = run normally, connect to WiFi
const int ETH_SETUP = 0;
int SW_SETUP;

const int VERSION = 2;


// HX711 definitions
#define DOUT1  0
#define CLK1  4

// scale(DOUT, Clock)
HX711 scale(DOUT1,CLK1);

// ESP SDA=2 SCL=14
//HX711 scale(2,14);


WiFiServer server(80);
const char APNAME[] = "HiveScale";

/////////////////////
// Pin Definitions //
/////////////////////
const int ESP8266_LED = 5; // Thing's onboard, green LED
const int LED_PIN = 5; // Thing's onboard, green LED
const int ANALOG_PIN = A0; // The only analog pin on the Thing
const int DIGITAL_PIN = 12; // Digital pin to be read
 
//////////////////////////////
// WiFi Network Definitions //
//////////////////////////////
// Replace these two character strings with the name and
// password of your WiFi network.
//const char mySSID[] = "HoneyBees";
//const char myPSK[] = "password";

//const String pubkey = "7JXdzQJO47UKL51A9Wv7";
//const String privkey = "mzwRrPzZ0YsAYPvDB97J";
// View at https://data.sparkfun.com/streams/<pubkey>

//////////////////
// HTTP Strings //
//////////////////
const char destServer[] = "data.sparkfun.com";


// All functions called from setup() are defined below the
// loop() function. They modularized to make it easier to
// copy/paste into sketches of your own.
void setup() 
{

   //int sensorValue = analogRead(A0);
   // if A0 is grounded, YES go into setup mode.
   if ( analogRead(A0) > 25 )
   {
      SW_SETUP=0;
   } else {
      SW_SETUP=1;
   }

  pinMode(ESP8266_LED, OUTPUT);

  // Serial Monitor is used to control the demo and view
  // debug information.
  //Serial.begin(9600);
  EEPROM.begin(256);
  EEPROM.get(EEPROM_OFFSET,customconfig);
  
  if ( customconfig.version != VERSION )
  {
    load_customconfig();
  }

  // Set Scale
  scale.set_scale(10.f);
  float scale_offset=70;


  digitalWrite(ESP8266_LED, HIGH); // LED off
  delay(500);
  digitalWrite(ESP8266_LED, LOW); // LED on
  delay(500);
   digitalWrite(ESP8266_LED, HIGH); // LED off
  delay(500);
  digitalWrite(ESP8266_LED, LOW); // LED on
  delay(900);

  if ( ETH_SETUP == 1 ) {
    // Create an AP
    ESP8266_APServer();
  } else {
    // connectESP8266() connects to the defined WiFi network.
    connectESP8266();
  }


}

void loop() {

  if ( ETH_SETUP == 1 ) {
    // WiFi Server
    wifi_server();
  } else {

    // Set Scale
    //serial_view_scale();

    // Send Scale
     serial_send_scale();

     digitalWrite(ESP8266_LED, LOW); // LED off
     ESP.deepSleep(customconfig.sleep * 60 * 1000000);
  }

  digitalWrite(ESP8266_LED, HIGH); // LED off
  delay(500);
  digitalWrite(ESP8266_LED, LOW); // LED on
  delay(200);
}


void serial_view_scale() {
  while ( 1 == 1 ) {
   // Serial.print("read: ");
   // Serial.println(scale.read());
   // Serial.print("average: ");
   // Serial.println(scale.read_average(10));
    Serial.print("get 5: ");
    float weight=(scale.get_units(5) * .0022046)-69;
    Serial.println(weight,1);
    Serial.println("");
    
    delay(3000);
  }
}

void serial_send_scale() {
 // while ( 1 == 1 ) {
   // Serial.print("read: ");
   // Serial.println(scale.read());
   // Serial.print("average: ");
   // Serial.println(scale.read_average(10));

    digitalWrite(ESP8266_LED,HIGH);
    Serial.print("weight: ");
    float weight=(scale.get_units(5) * .0022046)-69;
    Serial.print(weight,1);
    Serial.println(" lbss");
    Serial.println("");
    
    send_http_sparkfun(weight);

    digitalWrite(ESP8266_LED,LOW);

//    delay(60000);
 // }
}

void setup_scale() {
  Serial.println("Set scale float");
  Serial.println(" ");
  scale.set_scale();
  scale.tare();
  Serial.println("Quick, place a weight");
  delay(5000);
  Serial.println("Weighing");
  Serial.println(scale.get_units(10));
  delay(10000);
}



void ESP8266_APServer() {
 
 Serial.begin(115200);
  pinMode(DIGITAL_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  // Don't need to set ANALOG_PIN as input, 
  // that's all it can be.


  WiFi.mode(WIFI_AP);

 /* // Do a little work to get a unique-ish name. Append the
  // last two bytes of the MAC (HEX'd) to "Thing-":
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.softAPmacAddress(mac);
  String macID = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) +
                 String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
  macID.toUpperCase();
  String AP_NameString = "ESP8266 Thing " + macID;

  char AP_NameChar[AP_NameString.length() + 1];
  memset(AP_NameChar, 0, AP_NameString.length() + 1);

  for (int i=0; i<AP_NameString.length(); i++)
    AP_NameChar[i] = AP_NameString.charAt(i);  

//  WiFi.softAP(AP_NameChar, WiFiAPPSK);  */
    WiFi.softAP("HiveScale");

  server.begin();

}



void  wifi_server() {

  String req_val,shiftstring,pubkey,privkey;
  float sscale;
 // Check if a client has connected
  WiFiClient client = server.available();
  if (!client) {
    return;
  }

  // Read the first line of the request
  String req = client.readStringUntil('\r');
  Serial.println(req);
  client.flush();

  // Match the request
  int val = -1; // We'll use 'val' to keep track of both the
                // request type (read/set) and value if set.
  if (req.indexOf("/scale") != -1)
    val = 0; // view scale page
  else if (req.indexOf("/SetScaleStep1") != -1)
    val = 1; // ScaleStep1
  else if (req.indexOf("/SetScaleStep2") != -1)
    val = 2; // ScaleStep2
  else if (req.indexOf("/SetScaleStep3") != -1)
    val = 3; //   ScaleStep3 
  else if (req.indexOf("/network") != -1)
    val = 10; //   Network 
  else if (req.indexOf(" / ") != -1)
    val = 99; //   front page  
  else
    val = 99;
  // Otherwise request will be invalid. We'll say as much in HTML

  client.flush();

  // Prepare the response. Start with the common header:
  String s = "HTTP/1.1 200 OK\r\n";
  s += "Content-Type: text/html\r\n\r\n";
  s += "<!DOCTYPE HTML>\r\n<html>\r\n";

  if ( val == 0 )
  {
    float weight=(scale.get_units(5) * .0022046)-69;
    s += "<A HREF=/>&lt;-- Home</a> - HiveScale<br><br>";
    s += "Scale reads: ";
    s += String(weight);
    s += "<br><br>Set Scale: Please remove all weights from the scale and then click <A HREF=/SetScaleStep1>Step 1</a>";
  }
  else if (val == 1)
  {
    s += "<A HREF=/>&lt;-- Home</a> - HiveScale<br><br>";
    scale.set_scale();
    scale.tare();
    s += "Scale zeroed. Please place a KNOWN weight on the scale and then click ";
    s += "<A HREF=/SetScaleStep2>Step 2</a><br>";
  }
  else if (val == 2)
  {
    float weight=(scale.get_units(5) * .0022046);
    s += "<A HREF=/>&lt;-- Home</a> - HiveScale<br><br>";
    s += " The scale reads: ";
    s += String(weight);
    s += "<br> this is wrong, but to fix it please divide this number by the actual weight in lbs, ";
    s += "and enter that integer number here (no decimal place): ";
    s += "<FORM METHOD=GET ACTION=/SetScaleStep3><INPUT name=sscale>";
    s += "<INPUT type=submit>";
  }
  else if (val == 3)
  {
    s += "<A HREF=/>&lt;-- Home</a> - HiveScale<br><br>";

    if ( req.indexOf('=') )
    {
       String req_val=req;
       req_val.remove(0,req.indexOf("=")+1);
       req_val.remove(req_val.indexOf(" "));
       s += "Value entered = ";
       s += req_val;
       req_val += ".f";
    //   sscale=
       customconfig.set_scale1=req_val.toFloat();
       EEPROM.put(EEPROM_OFFSET,customconfig);
       EEPROM.commit();
       
    } else {
      s += "No value found, go home!";
    }
  }
  else if (val == 10)
  {
    String wifi,wpass;
    s += "<A HREF=/>&lt;-- Home</a> - HiveScale<br><br>";
    if ( req.indexOf('?') != -1 )
    {
       String GET_STRING=req;
       GET_STRING.remove(0,GET_STRING.indexOf("?")+1);
       GET_STRING.remove(GET_STRING.indexOf(" "));

       wifi=GET_STRING;
       wifi.remove(0,wifi.indexOf("=")+1);
       wifi.remove(wifi.indexOf("&"));

       // get Wifi Pass
       wpass=GET_STRING;
       wpass.remove(0,wpass.indexOf("&")+1);
       wpass.remove(0,wpass.indexOf("=")+1);
       shiftstring=wpass;
       wpass.remove(wpass.indexOf("&"));

       // get Pub
       pubkey=shiftstring;
       pubkey.remove(0,pubkey.indexOf("=")+1);
       shiftstring=pubkey;
       pubkey.remove(pubkey.indexOf("&"));
       
       // get Priv
       privkey=shiftstring;
       privkey.remove(0,privkey.indexOf("=")+1);
       privkey.remove(privkey.indexOf(" "));
       
       wifi.toCharArray(customconfig.wifi,25);
       wpass.toCharArray(customconfig.wifi_pass,25);
       pubkey.toCharArray(customconfig.pubkey,25);
       privkey.toCharArray(customconfig.privkey,25);
       EEPROM.put(EEPROM_OFFSET,customconfig);
       EEPROM.commit();

    } else {
      wifi=String(customconfig.wifi);
      wpass=String(customconfig.wifi_pass);
      pubkey=String(customconfig.pubkey);
      privkey=String(customconfig.privkey);
    }

    s += "<FORM METHOD=GET ACTION=/network>WiFi SSID: <INPUT name=wifi value=";
    s += String(wifi);
    s += "><br>WiFi Pass:<INPUT name=wifi_pass type=password value=";
    s += String(wpass);
    s += "><BR><BR>data.sparkfun.com Pub Key:<INPUT size=25 name=pubkey value=";
    s += String(pubkey);
    s += "><br>&nbsp;&nbsp;&nbsp;Private Key:<INPUT size=25 name=privkey value=";
    s += String(privkey);
    s += "><BR><INPUT type=SUBMIT><br>";
  }
  else if (val == 99)
  {

    s += "<FONT COLOR=BLUE SIZE=22>Welcome to the Greslick Hive Scale project</FONT><br><br>";
    s += " WiFi: ";
    s += String(customconfig.wifi);
    s += " &nbsp;&nbsp;&nbsp;&nbsp; <A HREF=/network>Setup Networking and API key</a><br>";
    s += "<br>Scale setting: ";
    s += String(customconfig.set_scale1);
    s += " &nbsp;&nbsp;&nbsp;&nbsp; <A HREF=/scale>Setup the scale</a><br>";

    s += "<br><br><br><br><br><br><br>";
    s += "<center><i><A HREF='http://www.greslick.com/honey/'>http://www.greslick.com/honey/</a><br><br>";

    int sensorValue = analogRead(A0);
    if ( sensorValue > 25 )
    {
      s += " NO GROUND : ";
    } else {
      s += " GROUND ATTACHED : ";
    }
    s += " DEBUG A0 = ";
    s += String(sensorValue);
    s += ".";

  }
  else
  {
    s += "404 Error in the URL<br><br><A HREF=/>Go HOME</a>";
  }
  s += "</body></html>\n";

  // Send the response to the client
  client.print(s);
  delay(100);
  Serial.println("Client disonnected");

  // The client will actually be disconnected 
  // when the function returns and 'client' object is detroyed

}




void load_customconfig()
{
  CustomConfigObject customconfig2 = {
    VERSION,
    0.f,
    0,
    0.f,
    0,
    "linksys",
    "",
    "pub",
    "priv",
    5,
    "comments"
  };

  EEPROM.put(EEPROM_OFFSET,customconfig2);
  EEPROM.commit();

  EEPROM.get(EEPROM_OFFSET,customconfig);

}


void connectESP8266()
{

  byte ledStatus = LOW;

  // Use esp8266.getMode() to check which mode it's in:
  // Set WiFi mode to station (as opposed to AP or AP_STA)
  WiFi.mode(WIFI_STA);

  Serial.println(F("Mode set to station"));

  // esp8266.status() indicates the ESP8266's WiFi connect
  // status.
  // A return value of 1 indicates the device is already
  // connected. 0 indicates disconnected. (Negative values
  // equate to communication errors.)

  // Use STATIC definitions
  //WiFi.begin(mySSID, myPSK);

  // use EEPROM
  WiFi.begin(customconfig.wifi,customconfig.wifi_pass);
  
  while (WiFi.status() != WL_CONNECTED)
  {
    // Blink the LED
    digitalWrite(ESP8266_LED, ledStatus); // Write LED high/low
    ledStatus = (ledStatus == HIGH) ? LOW : HIGH;

    // Delays allow the ESP8266 to perform critical tasks
    // defined outside of the sketch. These tasks include
    // setting up, and maintaining, a WiFi connection.
    delay(100);
    // Potentially infinite loops are generally dangerous.
    // Add delays -- allowing the processor to perform other
    // tasks -- wherever possible.
  }
  Serial.println("WiFi connected");  
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}



void send_http_sparkfun(float myweight)
{

  // To use the ESP8266 as a TCP client, use the 
  // ESP8266Client class. First, create an object:
  WiFiClient client;

  Serial.println("about to set mhttpreq");

 String httpReq = String("GET /input/" + String(customconfig.pubkey) +"?private_key=" + String(customconfig.privkey) + "&weight=" + String(myweight) + " HTTP/1.1\nHost: data.sparkfun.com\n+Connection: close\n\n");

  Serial.println("");
  Serial.println(" Sending Headerr: ");
  Serial.println(httpReq); 

  Serial.print("Trying to connect to");
   Serial.println(destServer );

  // ESP8266Client connect([server], [port]) is used to 
  // connect to a server (const char * or IPAddress) on
  // a specified port.
  // Returns: 1 on success, 2 on already connected,
  // negative on fail (-1=TIMEOUT, -3=FAIL).

  // DNS entry
  int retVal = client.connect(destServer, 80);
  // IP if DNS does not work
  // int retVal = client.connect("54.86.132.254", 80);
  if (retVal <= 0)
  {
    Serial.println(F("Failed to connect to server."));
    return;
  }

  Serial.println("sending httpreq");
 
  // print and write can be used to send data to a connected
  // client connection.
  client.print(httpReq);

  // available() will return the number of characters
  // currently in the receive buffer.
  while (client.available())
    Serial.write(client.read()); // read() gets the FIFO char
  
  // connected() is a boolean return value - 1 if the 
  // connection is active, 0 if it's closed.
  if (client.connected())
    client.stop(); // stop() closes a TCP connection.
}




