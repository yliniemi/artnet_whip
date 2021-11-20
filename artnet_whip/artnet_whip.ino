/*
Persistence of vision led whiprella
Copyright (C) 2021 Antti Yliniemi

This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License version 3 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details. 

https://www.gnu.org/licenses/agpl-3.0.txt
*/

/*
I use these two libraries by Yvez Basin, He was also kind enough to help me with this source code.
https://github.com/hpwit/artnetESP32
https://github.com/hpwit/I2SClockBasedLedDriver
*/

#include "settings.h"
#include "myCredentials.h"        // oh yeah. there is myCredentials.zip on the root of this repository. include it as a library and then edit the file with your onw ips and stuff

#include "setupWifi.h"
#include "OTA.h"

#ifdef USING_SERIALOTA
#include "SerialOTA.h"
#endif

#include <FS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

#include <ArtnetESP32.h>

//#include "FastLED.h"
#define NUMSTRIPS 1
#include "I2SClocklessLedDriver.h"
//FASTLED_USING_NAMESPACE

//The following has to be adapted to your specifications
#define NUM_LEDS LED_WIDTH*LED_HEIGHT
//CRGB ledStrip[LED_HEIGHT];
I2SClocklessLedDriver driver;

volatile uint8_t ledsArtnet[2*NUM_LEDS*3];
int pins[NUMSTRIPS]={PIN_0};

volatile int buffer_number=0;
volatile bool newFrame = false;

int maxCurrent = MAX_CURRENT;         // in milliwatts. can be changed later on with mqtt commands. be careful with this one. it might be best to disable this funvtionality altogether
int universeSize = UNIVERSE_SIZE;

ArtnetESP32 artnet;

TaskHandle_t task1, task2,task3;

char primarySsid[64];
char primaryPsk[64];
char hostname[64] = HOSTNAME;
int OTArounds = OTA_ROUNDS;


void cycleLedStrips(void* parameter)
{


  static int currentStrip = 0;
  
  while(1)
  {
    driver.showPixels((uint8_t*)ledsArtnet+(buffer_number*NUM_LEDS+currentStrip * LED_HEIGHT)*3);
    // vTaskDelay(1);
    currentStrip=(currentStrip+1)%LED_WIDTH;
    
    if(newFrame && currentStrip == 0)
    {
      buffer_number=(buffer_number+1)%2;
      
      newFrame=false;
      xTaskNotifyGive(task3);
    }
    
  }
  
}


void readFrame(void* parameter)
{
  for(;;)
  {
    artnet.setLedsBuffer((uint8_t*)ledsArtnet+((buffer_number+1)%2)*NUM_LEDS*3);
    artnet.readFrame(); //ask to read a full frame
    newFrame=true;
     ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  }
}


void displayFunction()
{
  newFrame = true;
}


/*
void displayFunction()
{  
  // this is here so that we don't call Fastled.show() too fast. things froze if we did that
  // perhaps I should use microseconds here. I could shave off a couple of milliseconds
  static unsigned long expectedTime = LED_HEIGHT * 24 * 12 / 8 + 500;     // 500 us for the reset pulse and (takes 50 us. better safe than sorry) also added 20 % extra just to be on the safe side

  
  static unsigned long oldMicros = 0;
  unsigned long frameTime = micros() - oldMicros;
  static unsigned long biggestFrameTime = 0;
  static unsigned long delay = 0;

  if (biggestFrameTime < frameTime) biggestFrameTime = frameTime;
  else if (biggestFrameTime == -1) biggestFrameTime = 0;
  
  if (frameTime < expectedTime)
  {
    delay = expectedTime - frameTime;
  }
  
  oldMicros = micros();
  unsigned long delta = micros() - oldMicros;
  static unsigned long biggestDelta = 0;
  if (biggestDelta < delta) biggestDelta = delta;
  if (artnet.frameslues%1000==0)
  {
    Serial.println();
    Serial.println(String("I'm running on core ") + xPortGetCoreID());
    Serial.println(String("FastLED.show() took ") + biggestDelta + " microseconds");
    Serial.println(String("Delay was ") + delay + " microseconds");
    Serial.println(String("frameTime was ") + biggestFrameTime + " microseconds");
    Serial.printf("nb frames read: %d  nb of incomplete frames:%d lost:%.2f %%\n\r",artnet.frameslues,artnet.lostframes,(float)(artnet.lostframes*100)/artnet.frameslues);
    #ifdef USING_SERIALOTA
    SerialOTA.println();
    SerialOTA.println(String("I'm running on core ") + xPortGetCoreID());
    SerialOTA.println(String("FastLED.show() took ") + biggestDelta + " microseconds");
    SerialOTA.println(String("Delay was ") + delay + " microseconds");
    SerialOTA.println(String("frameTime was ") + biggestFrameTime + " microseconds");
    SerialOTA.printf("nb frames read: %d  nb of incomplete frames:%d lost:%.2f %%\n\r",artnet.frameslues,artnet.lostframes,(float)(artnet.lostframes*100)/artnet.frameslues);
    #endif
    biggestDelta = 0;
    biggestFrameTime = -1;
  }
}
*/




void maintenance(void* parameter)
{
  while(1)
  {
    reconnectToWifiIfNecessary();
    SerialOTAhandle();
    ArduinoOTA.handle();
   // displayFunction();
    static unsigned long previousTime = 0;
  
    if ((millis() - previousTime > 60000) || (millis() < previousTime))
    {
      previousTime = millis();
      Serial.println();
      Serial.printf("nb frames read: %d  nb of incomplete frames:%d lost:%.2f %%\n\r",artnet.frameslues,artnet.lostframes,(float)(artnet.lostframes*100)/artnet.frameslues);
      #ifdef USING_SERIALOTA
      SerialOTA.println();
      SerialOTA.printf("nb frames read: %d  nb of incomplete frames:%d lost:%.2f %%\n\r",artnet.frameslues,artnet.lostframes,(float)(artnet.lostframes*100)/artnet.frameslues);
      #endif
    }
    vTaskDelay(10000);    
  }
}


bool loadConfig()
{
  //allows serving of files from SPIFFS
  Serial.println("Mounting FS...");
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return false;
  }

  File configFile = SPIFFS.open(CONFIG_FILE_NAME, "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return false;
  }

  if (configFile.size() > 1024) {
    Serial.println("Config file size is too large");
    return false;
  }

  // Allocate the memory pool on the stack.
  // Use arduinojson.org/assistant to compute the capacity.
  StaticJsonDocument<1024> jsonBuffer;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(jsonBuffer, configFile);

  // Test if parsing succeeds.
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return false;
  }

  // Copy values from the JsonObject to the Config
  
  if (jsonBuffer.containsKey("ssid"))
  {
    String stringSsid = jsonBuffer["ssid"];
    stringSsid.toCharArray(primarySsid, 64);
    Serial.println(String("primarySsid") + " = " + primarySsid);
  }
  
  if (jsonBuffer.containsKey("psk"))
  {
    String stringPsk = jsonBuffer["psk"];
    stringPsk.toCharArray(primaryPsk, 64);
    Serial.println(String("primaryPsk") + " = " + "********");
  }
  
  if (jsonBuffer.containsKey("hostname"))
  {
    String stringPsk = jsonBuffer["hostname"];
    stringPsk.toCharArray(hostname, 64);
    Serial.println(String("hostname") + " = " + hostname);
  }
  
  if (jsonBuffer.containsKey("OTArounds"))
  {
    OTArounds = jsonBuffer["OTArounds"];
    Serial.println(String("OTArounds") + " = " + OTArounds);
  }
  
  if (jsonBuffer.containsKey("maxCurrent"))
  {
    maxCurrent = jsonBuffer["maxCurrent"];
    Serial.println(String("maxCurrent") + " = " + maxCurrent);
  }
  
  if (jsonBuffer.containsKey("universeSize"))
  {
    universeSize = jsonBuffer["universeSize"];
    Serial.println(String("universeSize") + " = " + universeSize);
  }
  
  // We don't need the file anymore
  
  configFile.close();

  return true;
}


void setup()
{
  Serial.begin(115200);
  Serial.println("Booting");
  memset((uint8_t*)ledsArtnet,0,6*NUM_LEDS);

  

  primarySsid[0] = 0;
  primaryPsk[0] = 0; 
  
  loadConfig();
  
  setupWifi(primarySsid, primaryPsk);
  
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  setupOTA(hostname, OTArounds);
  
  #ifdef USING_SERIALOTA
  setupSerialOTA(hostname);
  #endif

  //FastLED.addLeds<NEOPIXEL, PIN_0>(ledStrip, 0*LED_HEIGHT, LED_HEIGHT);
   driver.initled((uint8_t*)ledsArtnet,pins,NUMSTRIPS,LED_HEIGHT,ORDER_GRB);
   driver.setBrightness(64);
  
  randomSeed(esp_random());
//  set_max_power_in_volts_and_milliamps(5, maxCurrent);   // in my current setup the maximum current is 50A
  
  // artnet.setFrameCallback(&displayFunction); //set the function that will be called back a frame has been received
 // artnet.setFrameCallback(&displayFunction); //set the function that will be called back a frame has been received
  
  //artnet.setLedsBuffer((uint8_t*)ledsArtnet+NUM_LEDS*3); //set the buffer to put the frame once a frame has been received
  
  artnet.begin(NUM_LEDS, universeSize); //configure artnet
  buffer_number=0;
  xTaskCreatePinnedToCore(
      cycleLedStrips, // Function to implement the task
      "cycleLedStrips", // Name of the task
      10000,  // Stack size in words
      NULL,  // Task input parameter
      0,  // Priority of the task
      &task2,  // Task handle.
      0); // not core 1
      

  xTaskCreatePinnedToCore(
      maintenance, // Function to implement the task
      "maintenance", // Name of the task
      10000,  // Stack size in words
      NULL,  // Task input parameter
      0,  // Priority of the task
      &task1,  // Task handle.
      0); // not core 1

  delay(2000);
  
  xTaskCreatePinnedToCore(
      readFrame, // Function to implement the task
      "readFrame", // Name of the task
      10000,  // Stack size in words
      NULL,  // Task input parameter
      0,  // Priority of the task
      &task3,  // Task handle.
      1); // Core where the task should run
  
}

void loop()
{
  //cycleLedStrips(NULL);
  vTaskDelete(NULL);
}
