#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <Ticker.h>
#include <DNSServer.h>
#include <ESP8266HTTPClient.h>

#define LED_Mode 14

String ssid = "weathersensor";
String password;
String server;
int mqttPort;
String uuid;
int sleeptime;
int statusCode;
int ledstate = LOW;
String content;
String st;
WiFiClient espClient;
PubSubClient client(espClient);
ESP8266WebServer webServer(80);
Ticker ticker;
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 1, 1);
DNSServer dnsServer;
boolean setupmode =false;
WiFiClient wificlient;
PubSubClient pubSubClient(wificlient);

void toggleLED(){
  Serial.println("toggle");
  digitalWrite(LED_BUILTIN,!ledstate);
}

void statusLED(int frequency){
  ticker.attach((1/frequency)/2,toggleLED);
  
}

void callback(char *topic, byte *payload, unsigned int length)
{
   Serial.print("Message arrived in topic: ");
   Serial.println(topic);
   std::string payload_s = "";
      for (int i = 0; i < length; i++)
      {
         payload_s.append(1, (char)payload[i]);
      }
      Serial.println(payload_s.c_str());
}

void connectMQTT(){
  client.setServer(server.c_str(),mqttPort);
  while (!client.connected()) {
        Serial.print("Connecting...");
        if (!client.connect("ESP8266Client")) {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" retrying in 5 seconds");
            delay(5000);
        }
    }
  client.setCallback(callback);
  client.subscribe("LED");
  client.subscribe("sensor/#");
}

void measurement(){
  // do a measurement
}

void sendData(){
  //send data of measurement over mqtt 
  client.publish("/home/data", "Hello MQTTY");
}

void stringToEEPROM(int addr,String s){
  for(int i =0;i<s.length();i++){
    EEPROM.write(addr+i,s[i]);
  }
  EEPROM.commit();
}

void saveConfig(String aid,String assid,String apass,String aserver){
  //load all params to classattributes
  uuid=aid;
  ssid=assid;
  password=apass;
  server=aserver;
  //save all params to storage
  Serial.println("write to EEPROM:");
  Serial.println(aid);
  Serial.println(assid);
  Serial.println(apass);
  Serial.println(aserver);
  for(int i=0;i<512;i++){
    EEPROM.write(i,0);
    EEPROM.commit();
  }
  stringToEEPROM(0,aid);
  stringToEEPROM(37,assid);
  stringToEEPROM(69,apass);
  stringToEEPROM(133,aserver);
}

String stringFromEEPROM(int addr,int length){
String s;
for(int i=0;i<length;i++){
  s+= char(EEPROM.read(addr+i));
}
return s;
}

void loadConfig(){
  //load all parameters from storage
uuid= stringFromEEPROM(0,36);
ssid=stringFromEEPROM(37,32);
password=stringFromEEPROM(69,64);
server=stringFromEEPROM(133,100);
//server="gamlo-cloud.de";
Serial.println("id: "+ uuid);
Serial.println("ssid: "+ssid);
Serial.println("password: "+password);
Serial.println("server: "+server);
mqttPort = 1883;
sleeptime =1;
}

void setupWiFi() {
    delay(10);
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    randomSeed(micros());
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}


void normSetup(){
  setupWiFi();
}


void scanNetworks(){
  //WiFi.mode(WIFI_STA);
  //WiFi.disconnect(); 
  
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0)
    Serial.println("no networks found");
  else
  {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i)
     {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*");
      delay(10);
     }
  }
  Serial.println(""); 
  st = "<ol>";
  for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      st += "<li>";
      st += WiFi.SSID(i);
      st += " (";
      st += WiFi.RSSI(i);
      st += ")";
      st += (WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*";
      st += "</li>";
    }
  st += "</ol>";
  
  }

void regist(){
  //regist sensor over restAPI and save id#
  Serial.println("regist");
  HTTPClient http;
  http.begin("https://"+server+"/registerWeatherSensor");
  http.addHeader("Content-Type", "text/plain");
  int httpcode = http.POST("newSensor");
  Serial.println(httpcode);
  Serial.println(http.getString());
  http.end();
}

void createWebServer()
{
  
    webServer.onNotFound( []() {
        scanNetworks();
        IPAddress ip = WiFi.softAPIP();
        String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
        content = "<!DOCTYPE HTML>\r\n<html>Hello from ESP8266 at ";
        content += ipStr;
        content += "<p>";
        content += st;
        content += "</p><form method='get' action='setting'><label>SSID: </label><input name='ssid' length=32><br><label>Password: </label><input name='pass' length=64><br><label>Server: </label><input name='server' length=100><br><label>UUID: </label><input name='uuid' length=36><input type='submit'></form>";
        content += "</html>";
        webServer.send(200, "text/html", content);  
    });
    webServer.on("/", []() {
        scanNetworks();
        IPAddress ip = WiFi.softAPIP();
        String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
        content = "<!DOCTYPE HTML>\r\n<html>Hello from ESP8266 at ";
        content += ipStr;
        content += "<p>";
        content += st;
        content += "</p><form method='get' action='setting'><label>SSID: </label><input name='ssid' length=32><input name='pass' length=64><input name='server' length=100><input type='submit'></form>";
        content += "</html>";
        webServer.send(200, "text/html", content);  
    });
    webServer.on("/setting", []() {
        
        String qsid = webServer.arg("ssid");
        String qpass = webServer.arg("pass");
        String qserver = webServer.arg("server");
        String quuid = webServer.arg("uuid");
        if (qsid.length() > 0 && qpass.length() > 0) {
          
          WiFi.begin(qsid,qpass);
          while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
          }
          Serial.println("Connected to: "+qsid+"with pass: "+qpass);
          saveConfig(quuid,qsid,qpass,qserver);
          content = "<!DOCTYPE HTML>\r\n<html>Successfully connect to WiFi";
          statusCode = 200;
        } else {
          content = "{Error: Reboot in setupmode for a new try!}";
          statusCode = 404;
          Serial.println("Sending 404");
        }
        regist();
        webServer.send(statusCode, "text/html", content);
        //send mesage to reboot to webserver
        webServer.close();
        //WiFi.softAPdisconnect(true);
        
    });
    
    
  }

  


void configure(){
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("weathersensor");
  dnsServer.start(DNS_PORT, "*", apIP);
  createWebServer();
  Serial.println(WiFi.softAPIP());
  webServer.begin();

}



void setup() {
  // put your setup code here, to run once:
  pinMode(LED_BUILTIN,OUTPUT);
  pinMode(LED_Mode,INPUT);
  Serial.begin(9600);
  EEPROM.begin(512);
  statusLED(1);
  if(digitalRead(LED_Mode)==HIGH){
    setupmode=true;
    Serial.println("Setupmode");
    configure();
  }else{
    setupmode=false;
    Serial.println("NormalMode");
    loadConfig();
    Serial.println("Password"+ssid);
    Serial.println(password);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid,password);
    while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
    }
    Serial.println("Connected to: ");
    Serial.println(ssid);
    Serial.println(WiFi.localIP());  
  
  //connectMQTT();
  client.setServer(server.c_str(),1883);
  while (!client.connected()) {
        Serial.print("Reconnecting...");
        if (!client.connect("ESP8266Client","mqtt","mqtt")) {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" retrying in 5 seconds");
            delay(5000);
        }
    }
  //measurement();
  float temp=30;
  float pressure=50;
  float humidity=40;
  //sendData();
  client.publish(String("sensor/"+uuid+"/temperature").c_str(),String(temp).c_str());
  client.publish(String("sensor/"+uuid+"/pressure").c_str(),String(pressure).c_str());
  client.publish(String("sensor/"+uuid+"/humidity").c_str(),String(humidity).c_str());
  
  //client.loop();
  
  //ESP.deepSleep(sleeptime);
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  if(setupmode){
  dnsServer.processNextRequest();
  webServer.handleClient();
  }else{
  
  }
}