#include "settings.h"
#include <myCredentials.h>        // oh yeah. there is myCredentials.zip on the root of this repository. include it as a library and then edit the file with your onw ips and stuff

#include "setupWifi.h"
#include "OTA.h"

#ifdef USING_SERIALOTA
#include "SerialOTA.h"
#endif

#include <FS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

#include <ArtnetESP32.h>

#include "FastLED.h"
FASTLED_USING_NAMESPACE

//The following has to be adapted to your specifications
#define NUM_LEDS LED_WIDTH*LED_HEIGHT
CRGB leds[NUM_LEDS];

#ifdef USING_LED_BUFFER
CRGB ledsBuffer[NUM_LEDS];
#endif

int maxCurrent = MAX_CURRENT;         // in milliwatts. can be changed later on with mqtt commands. be careful with this one. it might be best to disable this funvtionality altogether
int universeSize = UNIVERSE_SIZE;

ArtnetESP32 artnet;


char primarySsid[64];
char primaryPsk[64];
char hostname[64] = HOSTNAME;


void displayfunction()
{  
  // this is here so that we don't call Fastled.show() too fast. things froze if we did that
  // perhaps I should use microseconds here. I could shave off a couple of milliseconds
  static unsigned long expectedTime = LED_HEIGHT * 24 * 12 / 8 + 500;     // 500 us for the reset pulse and (takes 50 us. better safe than sorry) also added 20 % extra just to be on the safe side

  
  static unsigned long oldMicros = 0;
  unsigned long frameTime = micros() - oldMicros;
  static unsigned long biggestFrameTime = 0;
  if (biggestFrameTime < frameTime) biggestFrameTime = frameTime;
  else if (biggestFrameTime == -1) biggestFrameTime = 0;
  
  static unsigned long delay;
  if (frameTime > 6000000) delayMicroseconds(expectedTime);
  else if (frameTime < expectedTime)
  {
    delay = expectedTime - frameTime;
    delayMicroseconds(delay);
  }
  
  #ifdef USING_LED_BUFFER
  memcpy(&leds[0], &ledsBuffer[0], sizeof(CRGB) * NUM_LEDS);
  #endif
  
  oldMicros = micros();
  FastLED.show();
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
    SerialOTA.println();
    SerialOTA.println(String("I'm running on core ") + xPortGetCoreID());
    SerialOTA.println(String("FastLED.show() took ") + biggestDelta + " microseconds");
    SerialOTA.println(String("Delay was ") + delay + " microseconds");
    SerialOTA.println(String("frameTime was ") + biggestFrameTime + " microseconds");
    SerialOTA.printf("nb frames read: %d  nb of incomplete frames:%d lost:%.2f %%\n\r",artnet.frameslues,artnet.lostframes,(float)(artnet.lostframes*100)/artnet.frameslues);
    //here the buffer is the led array hence a simple FastLED.show() is enough to display the array
    biggestDelta = 0;
    biggestFrameTime = -1;
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

  primarySsid[0] = 0;
  primaryPsk[0] = 0; 
  
  loadConfig();
  
  setupWifi(primarySsid, primaryPsk);
  
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  setupOTA(hostname);
  
  #ifdef USING_SERIALOTA
  setupSerialOTA(hostname);
  #endif

  FastLED.addLeds<NEOPIXEL, PIN_0>(leds, 0*LED_HEIGHT, LED_HEIGHT);
  
  randomSeed(esp_random());
  set_max_power_in_volts_and_milliamps(5, maxCurrent);   // in my current setup the maximum current is 50A
  
  artnet.setFrameCallback(&displayfunction); //set the function that will be called back a frame has been received
  
  #ifndef USING_LED_BUFFER
  artnet.setLedsBuffer((uint8_t*)leds); //set the buffer to put the frame once a frame has been received
  #endif
  
  #ifdef USING_LED_BUFFER
  artnet.setLedsBuffer((uint8_t*)ledsBuffer); //set the buffer to put the frame once a frame has been received
  #endif
  
  artnet.begin(NUM_LEDS, universeSize); //configure artnet
  
}

void loop()
{
  reconnectToWifiIfNecessary();
  SerialOTAhandle();
  ArduinoOTA.handle();
  
  artnet.readFrame(); //ask to read a full frame
}
