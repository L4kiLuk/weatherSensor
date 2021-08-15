#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <Ticker.h>
#include <DNSServer.h>
#include <ESP8266HTTPClient.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <sstream>

#define LED_Mode 14

String ssid = "weathersensor";
String password;
String server;
int mqttPort;
String sensorId;
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
boolean setupmode = false;
WiFiClient wificlient;
PubSubClient pubSubClient(wificlient);
Adafruit_BME280 bme; // I2C

String mqttuser = "sensor";
String mqttpassword = "1234";
String mqttTopic = "sensor/";
int sleeptime = 15;


void toggleLED()
{
  Serial.println("toggle");
  digitalWrite(LED_BUILTIN, !ledstate);
}

void statusLED(int frequency)
{
  ticker.attach((1 / frequency) / 2, toggleLED);
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



void sendData()
{
  //send data of measurement over mqtt
  client.publish("/home/data", "Hello MQTTY");
}

void stringToEEPROM(int addr, String s)
{
  for (int i = 0; i < s.length(); i++)
  {
    EEPROM.write(addr + i, s[i]);
  }
  EEPROM.commit();
}

void saveConfig(String aid, String assid, String apass, String aserver)
{
  //load all params to classattributes
  sensorId = aid;
  ssid = assid;
  password = apass;
  server = aserver;
  //save all params to storage
  Serial.println("write to EEPROM:");
  Serial.println(aid);
  Serial.println(assid);
  Serial.println(apass);
  Serial.println(aserver);
  for (int i = 0; i < 512; i++)
  {
    EEPROM.write(i, 0);
    EEPROM.commit();
  }
  stringToEEPROM(0, aid);
  stringToEEPROM(37, assid);
  stringToEEPROM(69, apass);
  stringToEEPROM(133, aserver);
}

String stringFromEEPROM(int addr, int length)
{
  String s;
  for (int i = 0; i < length; i++)
  {
    char c =char(EEPROM.read(addr + i));
    if(c==NULL){
      Serial.println("null");
      break;
    }
    s += c;
  }
  return s;
}

void loadConfig()
{
  //load all parameters from storage
  sensorId = stringFromEEPROM(0, 36);
  ssid = stringFromEEPROM(37, 32);
  password = stringFromEEPROM(69, 64);
  server = stringFromEEPROM(133, 100);
  //server="gamlo-cloud.de";
  Serial.println("id: " + sensorId);
  Serial.println("ssid: " + ssid);
  Serial.println("password: " + password);
  Serial.println("server: " + server);
  mqttPort = 1883;
}

void setupWiFi()
{
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  randomSeed(micros());
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void normSetup()
{
  setupWiFi();
}

void scanNetworks()
{
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
      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
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
    st += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*";
    st += "</li>";
  }
  st += "</ol>";
}

void regist()
{
  //regist sensor over restAPI and save id#
  Serial.println("regist");
  HTTPClient http;
  http.begin(espClient,"https://" + server + "/registerWeatherSensor");
  http.addHeader("Content-Type", "text/plain");
  int httpcode = http.POST("newSensor");
  Serial.println(httpcode);
  Serial.println(http.getString());
  http.end();
}

void createWebServer()
{

  webServer.onNotFound([]() {
    scanNetworks();
    IPAddress ip = WiFi.softAPIP();
    String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
    content = "<!DOCTYPE HTML>\r\n<html>Hello from ESP8266 at ";
    content += ipStr;
    content += "<p>";
    content += st;
    content += "</p><form method='get' action='setting'><label>SSID: </label><input name='ssid' length=32><br><label>Password: </label><input name='pass' length=64><br><label>Server: </label><input name='server' length=100><br><label>SensorID: </label><input name='sensorId' length=36><input type='submit'></form>";
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
    String quuid = webServer.arg("sensorId");
    if (qsid.length() > 0 && qpass.length() > 0)
    {

      WiFi.begin(qsid, qpass);
      while (WiFi.status() != WL_CONNECTED)
      {
        delay(500);
        Serial.print(".");
      }
      Serial.println("Connected to: " + qsid + "with pass: " + qpass);
      saveConfig(quuid, qsid, qpass, qserver);
      content = "<!DOCTYPE HTML>\r\n<html>Successfully connect to WiFi";
      statusCode = 200;
    }
    else
    {
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

void configure()
{
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("weathersensor");
  dnsServer.start(DNS_PORT, "*", apIP);
  createWebServer();
  Serial.println(WiFi.softAPIP());
  webServer.begin();
}

void setup()
{
  // put your setup code here, to run once:
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_Mode, INPUT);
  Serial.begin(9600);
  EEPROM.begin(512);
  statusLED(1);
  if (digitalRead(LED_Mode) == HIGH)
  {
    setupmode = true;
    Serial.println("Setupmode");
    configure();
  }
  else
  {
    setupmode = false;
    Serial.println("NormalMode");
    loadConfig();
    Serial.println("Password" + ssid);
    Serial.println(password);

    //connectWIFI
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    int i = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
      i++;
      if (i > 10)
      {
        ESP.deepSleep(5 * 60e6);
      }
    }
    Serial.println("Connected to: ");
    Serial.println(ssid);
    Serial.println(WiFi.localIP());

    //connectMQTT();
    client.setServer(server.c_str(), mqttPort);
    while (!client.connected())
    {
      Serial.print("Connecting...");
      if (!client.connect("ESP8266Client", mqttuser.c_str(), mqttpassword.c_str()))
      {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" retrying in 5 seconds");
        delay(5000);
      }
    }
    //measurement
    bme.begin(0x76);
    float temp = bme.readTemperature();
    float pressure = bme.readPressure() / 100.0F;
    float humidity = bme.readHumidity();
    Serial.println(temp);
    Serial.println(pressure);
    Serial.println(humidity);

    //Publish
    //to joel id:cb430239-18d0-432e-818a-cb36c1c44100
    //to Homeassistant
    //client.publish(mqttTopic.c_str(), String("{\"temperature\": " + String(temp) + ",\"pressure\": " + String(pressure) + ", \"humidity\": " + String(humidity) + " }").c_str(), true);
    
    client.publish(String(mqttTopic+sensorId+"/temperature").c_str(), String(temp).c_str(), true);
    client.publish(String(mqttTopic+sensorId+"/pressure").c_str(), String(pressure).c_str(), true);
    client.publish(String(mqttTopic+sensorId+"/humidity").c_str(), String(humidity).c_str(), true);
    client.disconnect();

    ESP.deepSleep(sleeptime * 60e6);
  }
}

void loop()
{
  // put your main code here, to run repeatedly:
  if (setupmode)
  {
    dnsServer.processNextRequest();
    webServer.handleClient();
  }
  else
  {
  }
}