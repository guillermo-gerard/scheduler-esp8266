#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <string.h>
#include <Config.h>
#include <CaptiveRequestHandler.h>
#include <WifiConfigPersistor.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>

void SetupCaptivePortalMode(const char *portalName);
void ConnectToWifi();
void ConfigureServerEndpoints();

const char *filename = "/config.txt";
WifiConfigPersistor wifiConfigPersistor(filename);
AsyncWebServer server(80);
DNSServer dnsServer;

const uint8_t portalButton = D1; //TODO: confirm the location and logic of this button with Sebastian



void setup(){
Serial.begin(115200);

  if (!LittleFS.begin())
  {
    Serial.println(F("An Error has occurred while mounting LittleFS"));
    return;
  }

  //The captive portal will be set only if the button is pressed when booting
  //after the config file is saved, the esp will be restarted automatically
  if (digitalRead(portalButton))
  {
    SetupCaptivePortalMode("scheduler-config");
    while (true)
    {
      //The only way to get out of this will be a manual reset OR to fill in the credentials (it will reboot automatically)
      yield();
      dnsServer.processNextRequest();
    }
  }
  ConnectToWifi();
}

void loop(){
}


void ConnectToWifi()
{
  WiFi.mode(WIFI_STA);
  Config wifiConfig = wifiConfigPersistor.Retrieve();
  WiFi.begin(wifiConfig.ssid, wifiConfig.pass);
  Serial.println("");
  Serial.print("trying to connect to Wifi");
 
  if (WiFi.waitForConnectResult(30000) != WL_CONNECTED)
  {
    Serial.println("WiFi connection failed!");
    delay(2000);
  }
  else
  {
    Serial.println("WiFi connection succeed!");
    ConfigureServerEndpoints();
    server.begin();
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    if (!MDNS.begin("scheduler")) {
        Serial.println("Error setting up MDNS responder!");
    }
    
    delay(2000);
  }
}

void ConfigureServerEndpoints()
{
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              Serial.println(request->url());
              request->send(LittleFS, "/dashboard.html", String(), false);
            });

  server.on("/bootstrap.css", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              Serial.println(request->url());
              request->send(LittleFS, "/bootstrap.css", "text/css");
            });
}

void SetupCaptivePortalMode(const char *portalName)
{
  WiFi.softAP(portalName);
  dnsServer.start(53, "*", WiFi.softAPIP());
  server.addHandler(new CaptiveRequestHandler(&wifiConfigPersistor)).setFilter(ON_AP_FILTER); //only when requested from AP
  server.begin();
}

