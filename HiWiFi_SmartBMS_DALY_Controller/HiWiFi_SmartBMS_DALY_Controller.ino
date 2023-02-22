#include <Arduino.h>
#include <ArduinoJson.h>
#include <daly-bms-uart.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include "LittleFS.h"
#include <algorithm>  // std::min
#include <PubSubClient.h>
#include "StreamUtils.h"

/*
 * ESP-01 (1Mb) set: Generic ESP8266 module, 
 * Flash size: 1MB (FS: 64Kb, OTA:470Kb)
 * ESP <--> BMS
 * Tx ----- Red
 * Rx ----- White
 * Gnd ---- Black
 */

#define WIFI_LED    1   //WiFi Led Enable


#define BUTTON_PIN        0  
#define BUTTON_INTERVAL   500   // every 0.5 sec
unsigned long checkButtonInterval = 0;
int buttonInt = 0;
byte buttonPressed = 0;

const char* input_ssid = "ssid";
const char* input_pass = "pass";
String ssid_name = "demo";
String ssid_pass = "12345678";
byte ifConnected = 0;

const char* wifiConfigFile = "/#wifi.json";
const char* configFile = "/#config.json";

boolean restartFlag = false;

///////////////////////////////////////////////////
#define FIRMWARE_VERSION  "1.0"
#define MODEL_NAME        "DALY-BMS"

#define AP_SSID           "AP_BMS_"
#define AP_PASSWORD       "bms12345"    //8 charter !!
#define WEB_PORT  80

#define BAUD_SERIAL      9600
#define RXBUFFERSIZE     1024
#define STACK_PROTECTOR  512  // bytes
#define MAX_SRV_CLIENTS  2    // how many clients should be able to telnet to this ESP8266
#define TCPUART_DEFAULT_PORT     502

#define MQTT_DEFAULT_PORT        1883

#ifdef WIFI_LED
  #define WIFI_LED_PIN  2
  #define WIFI_LED_ON   digitalWrite(WIFI_LED_PIN, LOW)
  #define WIFI_LED_OFF  digitalWrite(WIFI_LED_PIN, HIGH)
#endif

#define WAIT_INIT_WIFI        30
#define CHECK_WIFI_INTERVAL   4000 
#define BMS_SERIAL            Serial

Daly_BMS_UART bms(BMS_SERIAL);

unsigned long previousMillis = 0;
unsigned long wifiLedInterval = 0;
byte TXflag = 0;

int wifiAPflag = 0;

const char redir15_html[] PROGMEM = R"rawliteral(<!DOCTYPE HTML><html><head><meta http-equiv = "refresh" content = "15; url = ./" /></head><body><h1>Success!</h1><h2> WiFi module will now restart...<br>This page will redirect in 15 seconds...</h2></body></html>)rawliteral";
const char redir0_html[] PROGMEM = R"rawliteral(<!DOCTYPE HTML><html><head><meta http-equiv = "refresh" content = "0; url = ./settings" /></head><body></body></html>)rawliteral";

// Створюємо вебсервер на 80 порт
AsyncWebServer serverWEB(WEB_PORT);

// Створюємо TCP-UART на 502 порт
WiFiServer serverTCP(TCPUART_DEFAULT_PORT);
WiFiClient serverTCPClients[MAX_SRV_CLIENTS];


WiFiClient serverMQTT;
PubSubClient mqttClient(serverMQTT);

unsigned long previousMillisMqtt = 0;
byte mqttUpdateConfigFlag = 0;
byte tcpuartUpdateConfigFlag = 0;

const char* input_mqtt_server = "m0";
const char* input_mqtt_port = "m1";
const char* input_mqtt_user = "m2";
const char* input_mqtt_pass = "m3";
const char* input_mqtt_topic = "m4";
const char* input_mqtt_interval = "m5";
const char* input_mqtt_status = "m6";
const char* input_tcpuart_port = "t1";
const char* input_tcpuart_status = "t2";
typedef struct {
  String mqtt_server;
  unsigned int mqtt_port;
  String mqtt_user;
  String mqtt_pass;
  String mqtt_topic;
  unsigned int mqtt_interval;
  byte mqtt_status;
  unsigned int tcpuart_port;
  byte tcpuart_status;
} conf_t;
conf_t CONF = {"0.0.0.0",MQTT_DEFAULT_PORT,"mqtt-user","mqtt-pass","smartbms",60,0,TCPUART_DEFAULT_PORT,0};


String processor(const String& var){
  if(var == "S0") { return ssid_name; }
  if(var == "S1") { if(WiFi.status() == WL_CONNECTED) return ssid_name; else return "not connect"; }
   if(var == "M0") { return CONF.mqtt_server; }
   if(var == "M1") { return (String)CONF.mqtt_port; }
   if(var == "M2") { return CONF.mqtt_user; }
   if(var == "M3") { return CONF.mqtt_pass; }
   if(var == "M4") { return CONF.mqtt_topic; }
   if(var == "M5") { return (String)CONF.mqtt_interval; }
   if(var == "M6") { return (String)CONF.mqtt_status; }
   if(var == "T1") { return (String)CONF.tcpuart_port; }
   if(var == "T2") { return (String)CONF.tcpuart_status; }
  return String();
}


/*
 ****** Functions ******
*/

void readWifiConfig(){
 File file = LittleFS.open(wifiConfigFile,"r");
 if(file){
  String fileContent;
  while(file.available()){ fileContent = file.readStringUntil('\n'); break; }
  DynamicJsonDocument json(1024);
  auto deserializeError = deserializeJson(json, fileContent);
  if(!deserializeError){
   ssid_name = (String)json[0];
   ssid_pass = (String)json[1];
  }
  file.close();
 }
}

void writeWifiConfig(){
 DynamicJsonDocument json(1024);
 json[0] = ssid_name;
 json[1] = ssid_pass;
 File file = LittleFS.open(wifiConfigFile,"w");
 if(file){
  serializeJson(json, file);
  file.close();
 }
}

void readConfig(){
 File file = LittleFS.open(configFile,"r");
 if(file){
  String fileContent;
  while(file.available()){ fileContent = file.readStringUntil('\n'); break; }
  DynamicJsonDocument json(1024);
  auto deserializeError = deserializeJson(json, fileContent);
  if(!deserializeError){
   CONF.mqtt_server = (String)json[0];
   CONF.mqtt_port = json[1];
   CONF.mqtt_user = (String)json[2];
   CONF.mqtt_pass = (String)json[3];
   CONF.mqtt_topic = (String)json[4];
   CONF.mqtt_interval = json[5];
   CONF.mqtt_status = json[6];
   CONF.tcpuart_port = json[7];
   CONF.tcpuart_status = json[8];
  }
  file.close();
 }
}
void writeConfig(){
 DynamicJsonDocument json(1024);
 json[0] = CONF.mqtt_server;
 json[1] = CONF.mqtt_port;
 json[2] = CONF.mqtt_user;
 json[3] = CONF.mqtt_pass;
 json[4] = CONF.mqtt_topic;
 json[5] = CONF.mqtt_interval;
 json[6] = CONF.mqtt_status; 
 json[7] = CONF.tcpuart_port; 
 json[8] = CONF.tcpuart_status; 
 File file = LittleFS.open(configFile,"w");
 if(file){
  serializeJson(json, file);
  file.close();
 }
}



DynamicJsonDocument bmsToJson(){
  DynamicJsonDocument json(1024);
  json["model"] = MODEL_NAME;
  json["fw"] = FIRMWARE_VERSION;
  json["uptime"] = millis() / 1000;
  json["heap"] = ESP.getFreeHeap();
  json["ssid"] = WiFi.SSID();
  json["rssi"] = WiFi.RSSI();
  if(mqttClient.connected()) json["mqtt"] = "1"; else json["mqtt"] = "0";
  json["voltage"] = (String)bms.get.packVoltage;
  json["current"] = (String)bms.get.packCurrent;
  json["soc"] = (String)bms.get.packSOC;
  json["temp"] = (String)bms.get.tempAverage;
  json["cells"] = (String)bms.get.numberOfCells;
  json["tSensors"] = (String)bms.get.numOfTempSensors;
  json["cycles"] = (String)bms.get.bmsCycles;
  json["bh"] = (String)bms.get.bmsHeartBeat;   // BMS life(0~255 cycles)
  json["dFet"] = (String)bms.get.disChargeFetState;
  json["cFet"] = (String)bms.get.chargeFetState;
  json["capacity"] = bms.get.resCapacitymAh;
  json["maxCV"] = (int)bms.get.maxCellmV;
  json["minCV"] = (int)bms.get.minCellmV;
  json["difCV"] = (int)bms.get.cellDiff;
  json["status"] = (String)bms.get.chargeDischargeStatus;
  json["balancer"] = (String)bms.get.cellBalanceActive;
  for (size_t i = 0; i < size_t(bms.get.numberOfCells); i++) {
   json["cellsV"][i] = bms.get.cellVmV[i];
   json["cellsB"][i] = (String)bms.get.cellBalanceState[i];
  }
  return json;
}



void setup(){
 
  bms.Init();
  //Serial.swap();
  //Serial.flush();

  LittleFS.begin();
  readWifiConfig();
  
  #ifdef WIFI_LED
    pinMode(WIFI_LED_PIN, OUTPUT);
  #endif
  pinMode(BUTTON_PIN, INPUT_PULLUP);


// ***** Wifi AP/STA Init ***** 
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid_name, ssid_pass);
  int i=0;
  while (WiFi.status() != WL_CONNECTED) {  //try to connect STA wifi WAIT_INIT_WIFI seconds...
    #ifdef WIFI_LED
      WIFI_LED_ON; delay(500);
      WIFI_LED_OFF; delay(500);
    #endif
    #ifndef WIFI_LED
      delay(1000);
    #endif
    i++;
    if(i == WAIT_INIT_WIFI) break;
  }
  if(i == WAIT_INIT_WIFI) {  //WiFi client not connected. AP turns ON
   ifConnected = 0;
   WiFi.disconnect(true);   //reset networking
   WiFi.mode(WIFI_AP);      //run WiFi AP
   WiFi.softAP(AP_SSID + (String)ESP.getChipId(), AP_PASSWORD);
   delay(100);
  }
  if(i < WAIT_INIT_WIFI) {     //WiFi client is connected
    ifConnected = 1;
  }
// ***** Wifi AP/STA Init ***** 


if(!ifConnected) {
 // Стрворюємо стартову сторінку Wifimanager
  serverWEB.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/wifi.html", "text/html", false, processor);
  });

} else {
  
  readConfig();

  // Стрворюємо стартову сторінку
  serverWEB.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html", false, processor);
  });

  serverWEB.on("/setup", HTTP_GET, [](AsyncWebServerRequest *request) {
   request->send(LittleFS, "/setup.html", "text/html", false, processor);
  });

  serverWEB.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/settings.html", "text/html", false, processor);
  });
  
  serverWEB.on("/settings", HTTP_POST, [](AsyncWebServerRequest *request) {
   int params = request->params();
   for(int i=0;i<params;i++) {
    AsyncWebParameter* p = request->getParam(i);
    if(p->isPost()) {
      if (p->name() == input_mqtt_server) CONF.mqtt_server = p->value().c_str();
      if (p->name() == input_mqtt_port) CONF.mqtt_port = p->value().toInt();
      if (p->name() == input_mqtt_user) CONF.mqtt_user = p->value().c_str();
      if (p->name() == input_mqtt_pass) CONF.mqtt_pass = p->value().c_str();
      if (p->name() == input_mqtt_topic) CONF.mqtt_topic = p->value().c_str();
      if (p->name() == input_mqtt_interval) CONF.mqtt_interval = p->value().toInt();
      if (p->name() == input_mqtt_status) CONF.mqtt_status = p->value().toInt();
      if (p->name() == input_tcpuart_port) {
        if( CONF.tcpuart_port != p->value().toInt() ) tcpuartUpdateConfigFlag = 1;
        CONF.tcpuart_port = p->value().toInt();
      }
      if (p->name() == input_tcpuart_status) {
        if( CONF.tcpuart_status != p->value().toInt() ) tcpuartUpdateConfigFlag = 1;
        CONF.tcpuart_status = p->value().toInt();
      }
     }
    }
    writeConfig();
    mqttUpdateConfigFlag = 1;
    if (tcpuartUpdateConfigFlag == 1) {
     request->send(200, "text/html", redir15_html);
     restartFlag = true;
    } else {
     request->send(200, "text/html", redir0_html);
    }
  });


  //передача параметрів до БМС
  serverWEB.on("/bms", HTTP_POST, [](AsyncWebServerRequest *request) {
    int params = request->params();
    for(int i=0;i<params;i++){
     AsyncWebParameter* p = request->getParam(i);
     if(p->isPost()) {
      if (p->name() == "dFet") {
       if (p->value() == "true") bms.setDischargeMOS(true);
       if (p->value() == "false") bms.setDischargeMOS(false);      
      } 
      if (p->name() == "cFet") {
       if (p->value() == "true") bms.setChargeMOS(true);
       if (p->value() == "false") bms.setChargeMOS(false);
      }
      if (p->name() == "resbms") {
        bms.setBmsReset();
      }
      if (p->name() == "resesp") {
        restartFlag = true;
      }
     }
    }
    request->send(200, "text/plain", "OK");
  });

  
  // Стрворюємо сторінку для JSON запитів
  serverWEB.on("/json", HTTP_GET, [](AsyncWebServerRequest *request) {
    if(TXflag == 0) TXflag = 1;
    if(TXflag == 1) bms.update();
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    DynamicJsonDocument json(1024);
    json = bmsToJson();
    serializeJson(json, *response);
    request->send(response);
    TXflag = 0;
  });

 serverWEB.serveStatic("/assets/", LittleFS, "/assets/");
}

 serverWEB.on("/setup", HTTP_POST, [](AsyncWebServerRequest *request) {
   int params = request->params();
   for(int i=0;i<params;i++) {
    AsyncWebParameter* p = request->getParam(i);
    if(p->isPost()) {
      if (p->name() == "ssid") ssid_name = p->value().c_str();
      if (p->name() == "pass") ssid_pass = p->value().c_str();
     }
    }
    if(ssid_name != "" && ssid_pass != "") writeWifiConfig();
    request->send(200, "text/html", redir15_html);
    restartFlag = true;
  });

  // Сторінка сканування WiFi мереж
  // Перший запит поверне 0 результатів, якщо ви не почнете сканування з іншого місця (цикл/налаштування)
  // Не запитуйте частіше ніж 3-5 секунд
  serverWEB.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request){
   AsyncResponseStream *response = request->beginResponseStream("application/json");
   DynamicJsonDocument json(1024);
   int n = WiFi.scanComplete();
   if(n == -2){
    WiFi.scanNetworks(true);
   } else if(n) {
    for (int i = 0; i < n; ++i){
      json[i]["rssi"] = String(WiFi.RSSI(i));
      json[i]["ssid"] = WiFi.SSID(i);
      json[i]["channel"] = String(WiFi.channel(i));
    }
    WiFi.scanDelete();
    if(WiFi.scanComplete() == -2){
     WiFi.scanNetworks(true);
    }
   }
   serializeJson(json, *response);
   request->send(response);
  });

  // Start ElegantOTA
  AsyncElegantOTA.begin(&serverWEB);

  // start WEB server
  serverWEB.begin();


  // start TCP server
  if(CONF.tcpuart_status == 1){
   serverTCP.begin(CONF.tcpuart_port);
   serverTCP.setNoDelay(true);
  }
  
}
 
void loop() {
 unsigned long currentMillis = millis();
 if (restartFlag) { delay(1500); ESP.restart();}
 if(wifiAPflag > 75) {   // 15 => 15 x 4sec = 1min
   restartFlag = true;   // 75 => 5min
   wifiAPflag=0;         // 150 => 10min
 }

 #ifdef WIFI_LED
   if (currentMillis - previousMillis == CHECK_WIFI_INTERVAL - wifiLedInterval) WIFI_LED_ON;
 #endif
 if (currentMillis - previousMillis >= CHECK_WIFI_INTERVAL) {
   previousMillis = currentMillis;
   if (WiFi.status() == WL_CONNECTED) {
     wifiLedInterval = 20;
     wifiAPflag=0;
   } else {
     wifiLedInterval = CHECK_WIFI_INTERVAL / 2;
     wifiAPflag++;
   }
   #ifdef WIFI_LED
    if(buttonInt>=0)WIFI_LED_OFF;
   #endif
   if (WiFi.softAPgetStationNum() > 0) { 
    wifiLedInterval = CHECK_WIFI_INTERVAL - 100;
    wifiAPflag=0;
   }
 }
 //read button
 if (currentMillis - checkButtonInterval >= BUTTON_INTERVAL) {
    checkButtonInterval = currentMillis;
    if(digitalRead(BUTTON_PIN) == LOW ) {
      buttonInt++;
    } else buttonInt = 0;
 }
 if (buttonInt > 4) { //2 sec
  buttonPressed = 1;
  wifiAPflag = 10;
  #ifdef WIFI_LED
   WIFI_LED_ON;
  #endif
  buttonInt = -100; 
 }


if(ifConnected) { //STA connected 
 if(CONF.mqtt_status == 1) {
  mqttClient.loop();
  if (currentMillis - previousMillisMqtt >= CONF.mqtt_interval * 1000 ) {
   previousMillisMqtt = currentMillis;
   if(TXflag == 0) TXflag = 2;
   if(TXflag == 2) {
    bms.update();
    if (!mqttClient.connected() || mqttUpdateConfigFlag == 1) {
     readConfig();
     mqttClient.disconnect();
     mqttClient.setServer(CONF.mqtt_server.c_str(), CONF.mqtt_port );
     mqttClient.connect("bms", CONF.mqtt_user.c_str(), CONF.mqtt_pass.c_str());
     mqttUpdateConfigFlag = 0;
    }
    DynamicJsonDocument json(1024);
    json = bmsToJson();
    mqttClient.beginPublish(CONF.mqtt_topic.c_str(), measureJson(json), false);
    BufferingPrint bufferedClient(mqttClient, 32);
    serializeJson(json, bufferedClient);
    bufferedClient.flush();
    mqttClient.endPublish();    
   }
  }  
 } else {
  if (mqttUpdateConfigFlag == 1) {
   mqttClient.disconnect();
   mqttUpdateConfigFlag = 0;
  }
 }

 if(CONF.tcpuart_status == 1) {
  if (TXflag == 0) {  // в режимі очікування будемо слухати запити через Modbus TCP
   // **** START.UART-TCP ****
   // check if there are any new clients
   if (serverTCP.hasClient()) {
    int i;
    for(i=0;i<MAX_SRV_CLIENTS;i++)if(!serverTCPClients[i]){serverTCPClients[i]=serverTCP.accept();break;}
    if(i==MAX_SRV_CLIENTS){serverTCP.accept().println("busy");}
   }
   // check TCP clients for data
   for (int i = 0; i < MAX_SRV_CLIENTS; i++)
    while (serverTCPClients[i].available() && Serial.availableForWrite() > 0) {
      Serial.write(serverTCPClients[i].read());   // working char by char is not very efficient
    }
   int maxToTcp = 0;
   for (int i = 0; i < MAX_SRV_CLIENTS; i++)
    if (serverTCPClients[i]) {
      int afw = serverTCPClients[i].availableForWrite();
      if (afw) {
        if (!maxToTcp) { maxToTcp = afw; } else { maxToTcp = std::min(maxToTcp, afw); }
      } 
    }
   // check UART for data
   size_t len = std::min(Serial.available(), maxToTcp);
   len = std::min(len, (size_t)STACK_PROTECTOR);
   if (len) {
    uint8_t sbuf[len];
    int serial_got = Serial.readBytes(sbuf, len);
    // push UART data to all connected telnet clients
    for (int i = 0; i < MAX_SRV_CLIENTS; i++)
      if (serverTCPClients[i].availableForWrite() >= serial_got) {
        size_t tcp_sent = serverTCPClients[i].write(sbuf, serial_got);
      }
   }
   // **** END.UART-TCP ****
  }
 }
} //STA connected
}
