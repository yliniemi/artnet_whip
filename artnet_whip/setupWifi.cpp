#define SETUPWIFI_CPP
#include "setupWifi.h"

char* ssid;
char* psk;


/*
void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
  // Serial.println("Connection Lost! Rebooting...");
  Serial.println("Disconnected from WiFi access point");
  Serial.print("WiFi lost connection. Reason: ");
  Serial.println(info.disconnected.reason);
  Serial.println("Trying to Reconnect");
  WiFi.reconnect();
  delay(5000);
  // ESP.restart();
}
*/

void reconnectToWifiIfNecessary()
{
  static bool beenConnected = false;
  static unsigned long oldMillis = 0;
  unsigned long newMillis = millis();
  static unsigned long previousTime = 0;
  if ((millis() - previousTime > 10000) || (millis() < previousTime))
  {
    static String reconnectedAt = "";
    static bool beenDisconnected = false;
    previousTime = millis();
    Serial.println();
    Serial.println(reconnectedAt);
    Serial.print("WiFi connections status: ");
    Serial.println(WiFi.status());
    Serial.print(String("I have been on for ") + previousTime / (1000 * 60 * 60) + " hours, ");
    Serial.print(String((previousTime / (1000 * 60)) % 60) + " minutes and ");
    Serial.println(String((previousTime / 1000) % 60) + " seconds");
    Serial.println();

    #ifdef USING_SERIALOTA
    SerialOTA.println();
    SerialOTA.println(reconnectedAt);
    SerialOTA.print("WiFi connections status: ");
    SerialOTA.println(WiFi.status());
    SerialOTA.print(String("I have been on for ") + previousTime / (1000 * 60 * 60) + " hours, ");
    SerialOTA.print(String((previousTime / (1000 * 60)) % 60) + " minutes and ");
    SerialOTA.println(String((previousTime / 1000) % 60) + " seconds");
    SerialOTA.println();
    #endif
    
    static int tryNumber = 0;
    if (WiFi.status() != WL_CONNECTED)
    {
      tryNumber++;
      Serial.print("Try number: ");
      Serial.println(tryNumber);
      reconnectedAt += WiFi.status();   // im doing this to differentiate regular temporary WiFi outage and a long one
      if (tryNumber > TRY_DISCONNECTING)
      {
        if (tryNumber > TIME_TO_REBOOT)
        {
          Serial.println("Couldn't reconnect to WiFi no matter what. Giving up and rebooting");
          delay(1000);
          ESP.restart();
        }
        else
        {
          Serial.println("WiFi has been down for too long. Trying to force disconnect.");
          WiFi.disconnect();
          delay(1000);
          WiFi.mode(WIFI_STA);
          WiFi.begin(ssid, psk);
          delay(1000);
        }
      }
      else
      {
        Serial.println("God damn it! WiFi is lost. Trying to reconnect.");
        
        WiFi.reconnect();
        // WiFi.reconnect() resulted in a crash half the time
        // using events or WiFi.setAutoReconnect(true) didn't help. everything just kept crashing anyways
        // i finally cracked the problem
        // the solution is to have core 1 do nothing for some time. there is delay(1000) right now but i will reduce it when i do more testing
        // the problem was FastLED.show() starting right after WiFi.reconnect(). core 1 didn't like that
        // even though WiFi is run on core 0, core 1 does something really important right after connection is established
        delay(1000);   // maybe this will make reconnecting more reliable. who knows???   IT DID!!!!!   False alarm. It didn't
        
        beenDisconnected = true;        
      }
    }
    else if (beenDisconnected)
    {
      reconnectedAt += String(" WiFi reconnected at: ") + previousTime / (1000 * 60 * 60) + ":" + previousTime / (1000 * 60) % 60 + ":" + previousTime / 1000 % 60;      
      reconnectedAt += String(" after ") + tryNumber + " tries\r\n";
      beenDisconnected = false;
      tryNumber = 0;
    }
  }
}

void setupWifi(char* primarySsid, char* primaryPsk)
{
  
  if (primarySsid[0] != 0)
  {
    WiFi.disconnect();
    delay(500);
    ssid = primarySsid;
    psk = primaryPsk;
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, psk);
    Serial.println();
    Serial.print("Trying to connect to ");
    Serial.println(ssid);
    delay(5000);
  }
  
  for (int i = 0; WiFi.waitForConnectResult() != WL_CONNECTED && wifiArray[i][0] != 0; i++)
  {
    WiFi.disconnect();
    delay(500);
    ssid = wifiArray[i][0];
    psk = wifiArray[i][1];
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, psk);
    Serial.println();
    Serial.print("Trying to connect to ");
    Serial.println(ssid);
    delay(5000);
  }
  
  if (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.println();
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  Serial.print("Connection to ");
  Serial.print(ssid);
  Serial.println(" succeeded!");
  // WiFi.setAutoReconnect(true);  // this didn't work well enough. i had to do this another way
  WiFi.persistent(false);          // we don't want to save the credentials on the internal filessytem of the esp32
  // WiFi.onEvent(WiFiStationDisconnected, SYSTEM_EVENT_STA_DISCONNECTED);   // this one was problematic too because you shouldn't do much right after trying to reconnect. perhaps fastled disabling interrupts does something nefarious
}

void setupWifi()
{
  setupWifi((char) 0, (char) 0);
}
