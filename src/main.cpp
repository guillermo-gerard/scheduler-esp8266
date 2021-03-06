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
#include <NTPClient.h>

void SetupCaptivePortalMode(const char *portalName);
void ConnectToWifi();
void ConfigureServerEndpoints();

const char *filename = "/config.json";
WifiConfigPersistor wifiConfigPersistor(filename);
AsyncWebServer server(80);
DNSServer dnsServer;

const uint8_t portalButton = D1; // TODO: confirm the location and logic of this button with Sebastian

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

AsyncWebSocket ws("/ws");
AsyncWebSocketClient* wsClient;
 
void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  if(type == WS_EVT_CONNECT){
    wsClient = client;
  } else if(type == WS_EVT_DISCONNECT){
    wsClient = nullptr;
  }
}

void setup()
{
    Serial.begin(115200);

    if (!LittleFS.begin())
    {
        Serial.println(F("An Error has occurred while mounting LittleFS"));
        return;
    }

    // The captive portal will be set only if the button is pressed when booting
    // after the config file is saved, the esp will be restarted automatically
    if (digitalRead(portalButton))
    {
        SetupCaptivePortalMode("scheduler-config");
        while (true)
        {
            // The only way to get out of this will be a manual reset OR to fill in the credentials (it will reboot automatically)
            yield();
            dnsServer.processNextRequest();
        }
    }
    ConnectToWifi();
    timeClient.setTimeOffset(-10800);

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);
}

void loop()
{
    if(wsClient != nullptr && wsClient->canSend()) {
    // .. send hello message :-)
    wsClient->text(String("Hello client" + String(millis())));
  }
  
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
        if (!MDNS.begin("esp8266"))
        {
            Serial.println("Error setting up MDNS responder!");
        }
        timeClient.begin();
        delay(2000);
    }
}

String processor(const String &var)
{
    if (var == "HORA")
    {
        timeClient.update();
        return timeClient.getFormattedTime();
    }
    if (var == "TEMPERATURA")
    {
        return String(9.0);
    }
    return String();
}

void ConfigureServerEndpoints()
{
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              {
              Serial.println(request->url());
              request->send(LittleFS, "/dashboard.html", String(), false, processor); });

    server.on("/ajax_info_hora", HTTP_GET, [](AsyncWebServerRequest *request)
              {
              Serial.println(request->url());
              timeClient.update();
         
              request->send(200, "text/plain", timeClient.getFormattedTime()); });

    server.on("/bootstrap.css", HTTP_GET, [](AsyncWebServerRequest *request)
              {
              Serial.println(request->url());
              request->send(LittleFS, "/bootstrap.css", "text/css"); });
}

void SetupCaptivePortalMode(const char *portalName)
{
    WiFi.softAP(portalName);
    dnsServer.start(53, "*", WiFi.softAPIP());
    server.addHandler(new CaptiveRequestHandler(&wifiConfigPersistor)).setFilter(ON_AP_FILTER); // only when requested from AP
    server.begin();
}
