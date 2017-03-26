/*
Compiled primarily from examples.
Wemos SHT3X / SHT30_Shield
Thingspeak / WriteVoltage
Added wifi manager with parameters
Added deepsleep to prevent SHT from heating up
Sets all values that are to be sent before sending to minimize ontime.

ThingSpeak ( https://www.thingspeak.com ) is an analytic IoT platform service that allows you to aggregate, visualize and analyze live data streams in the cloud.
On how to add values sent från Thingsspeak:
http://community.thingspeak.com/forum/thingspeak-apps/thinghttp-ifttt-maker/

  *****************************************************************************************
  **** Visit https://www.thingspeak.com to sign up for a free account and create
  **** a channel.  The video tutorial http://community.thingspeak.com/tutorials/thingspeak-channels/ 
  **** has more information. You need to change this to your channel, and your write API key
  **** IF YOU SHARE YOUR CODE WITH OTHERS, MAKE SURE YOU REMOVE YOUR WRITE API KEY!!
  *****************************************************************************************

Connections:
D0 -- RST     

*/

#include <FS.h>                   //this needs to be first, or it all crashes and burns...

// sleep for this many seconds
const int sleepSeconds = 60;

#include "ThingSpeak.h"           //https://thingspeak.com/
#include <WEMOS_SHT3X.h>          //https://github.com/wemos/WEMOS_SHT3x_Arduino_Library
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

//define your default values here, if there are different values in config.json, they are overwritten.
// If blank shows help text
char THINGSPEAK_CHANNEL_NUMBER[30] = "";
char THINGSPEAK_WRITE_API_KEY[26] = "";

unsigned long myChannelNumber; // Needed later to convert from char to unsigned long

bool shouldSaveConfig = false; //flag for saving data

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

int status = WL_IDLE_STATUS;  // Needed? Cant find it in code

WiFiClient  client;

SHT3X sht30(0x45);

void setup() {

  Serial.begin(115200);
  Serial.println("\n\nWake up");

  sht30.get();
  Serial.print("Temperature in Celsius : ");
  Serial.println(sht30.cTemp);
  Serial.print("Temperature in Fahrenheit : ");
  Serial.println(sht30.fTemp);
  Serial.print("Relative Humidity : ");
  Serial.println(sht30.humidity);
  Serial.println();
  delay(100);

  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(THINGSPEAK_CHANNEL_NUMBER, json["THINGSPEAK_CHANNEL_NUMBER"]);
          strcpy(THINGSPEAK_WRITE_API_KEY, json["THINGSPEAK_WRITE_API_KEY"]); 

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name     placeholder/prompt     default     length
  WiFiManagerParameter custom_THINGSPEAK_CHANNEL_NUMBER("THINGSPEAK_CHANNEL_NUMBER", "Thingspeak channel number", THINGSPEAK_CHANNEL_NUMBER, 30);
  WiFiManagerParameter custom_THINGSPEAK_WRITE_API_KEY("THINGSPEAK_WRITE_API_KEY", "Thingspeak write API key", THINGSPEAK_WRITE_API_KEY, 26); // La till själv

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  // defines the header text
  WiFiManagerParameter custom_text2("<body> <br> <br><b>Thingspeak data write</b></body>");  // Defines The text to be displayed
  
  wifiManager.addParameter(&custom_text2);  // Adds the text  
  wifiManager.addParameter(&custom_THINGSPEAK_CHANNEL_NUMBER);
  wifiManager.addParameter(&custom_THINGSPEAK_WRITE_API_KEY); 

  //reset settings - for testing
  //wifiManager.resetSettings();

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();
  
  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("AutoConnectAP", "password")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  //read updated parameters
  strcpy(THINGSPEAK_CHANNEL_NUMBER, custom_THINGSPEAK_CHANNEL_NUMBER.getValue());
  strcpy(THINGSPEAK_WRITE_API_KEY, custom_THINGSPEAK_WRITE_API_KEY.getValue()); 

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["THINGSPEAK_CHANNEL_NUMBER"] = THINGSPEAK_CHANNEL_NUMBER;
    json["THINGSPEAK_WRITE_API_KEY"] = THINGSPEAK_WRITE_API_KEY; 

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  //Serial.println("local ip");
  //Serial.println(WiFi.localIP());
   

  // Connect D0 to RST to wake up
  pinMode(D0, WAKEUP_PULLUP);
  
  myChannelNumber = long(THINGSPEAK_CHANNEL_NUMBER);  // Convert to correct format

  ThingSpeak.begin(client);
  delay(100); //Needed?

  
  // Write to ThingSpeak. There are up to 8 fields in a channel, allowing you to store up to 8 different
  // pieces of information in a channel.  Here, we write to field 4 and 5 one at a time.
  // ThingSpeak.writeField(myChannelNumber, 4, sht30.cTemp, THINGSPEAK_WRITE_API_KEY);
  // delay(16000);  // Min 150000 due to 15 seconds min between values
  // ThingSpeak.writeField(myChannelNumber, 5, sht30.humidity, THINGSPEAK_WRITE_API_KEY);
  // But that is not efficient so...

  // Set all values and write once. Better, less time active and thus less heating of the SHT on Wemos
  // Read the inputs and set each field to be sent to ThingSpeak.
  ThingSpeak.setField(4,sht30.cTemp);
  ThingSpeak.setField(5,sht30.humidity);
  // Write the fields that you've set all at once.
  ThingSpeak.writeFields(myChannelNumber, THINGSPEAK_WRITE_API_KEY);  
  delay(500);  
  Serial.printf("Sleep for %d seconds\n\n", sleepSeconds);

  // convert to microseconds
  ESP.deepSleep(sleepSeconds * 1000000);
}

void loop() {}





