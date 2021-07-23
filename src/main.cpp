#include <ArduinoOTA.h>

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ModbusMaster.h>
#include "secrets.h"

/******************************************************************
Secrets, please change these in the secrets.h file
*******************************************************************/
const char* wifi_ssid                  = WIFI_SSID;
const char* wifi_password              = WIFI_PASSWORD;
const char* wifi_hostname              = WIFI_HOSTNAME;

const char* mqtt_server                = MQTT_SERVER;
const int   mqtt_port                  = MQTT_PORT;
const char* mqtt_username              = MQTT_USERNAME;
const char* mqtt_password              = MQTT_PASSWORD;



#define MAXDATASIZE 256
char data[MAXDATASIZE];

unsigned long next_poll = 0;

/******************************************************************
Instantiate modbus and mqtt libraries
*******************************************************************/
ModbusMaster node;
int slave_id_growatt = 1;
WiFiClient mqtt_wifi_client;
PubSubClient mqtt_client(mqtt_server, mqtt_port, mqtt_wifi_client);


void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, WIFI_PASSWORD);

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

void reconnect() {
  // Loop until we're reconnected
  while (!mqtt_client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt_client.connect("GrowattClient", mqtt_username, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

/******************************************************************
Setup, only ran on application start
*******************************************************************/
void setup() {  
  Serial.begin(9600);

  setup_wifi();

    // Port defaults to 8266
  ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(wifi_hostname);

  ArduinoOTA.onStart([]() {
  });
  ArduinoOTA.onEnd([]() {
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {

  });
  ArduinoOTA.onError([](ota_error_t error) {

  });
  ArduinoOTA.begin();
}

/******************************************************************
Publish float var to specified sub topic
*******************************************************************/
void publishFloat(char * topic, float f) {
  String value_str = String(f, 1);
  char value_char[32] = "";
  value_str.toCharArray(value_char, 40);

  mqtt_client.publish(topic, value_char); 
}

/******************************************************************
Publish int var to specified sub topic
*******************************************************************/
void publishInt(char * topic, int i) {
  String value_str = String(i);
  char value_char[32] = "";
  value_str.toCharArray(value_char, 40);
  
  mqtt_client.publish(topic, value_char); 
}

/******************************************************************
Create float using values from multiple regsiters
*******************************************************************/
float glueFloat(unsigned int d1, unsigned int d0) {
  unsigned long t;
  t = d1 << 16;
  t += d0;

  float f;
  f = t;
  f = f / 10;
  return f;
}

/******************************************************************
Growatt
*******************************************************************/
void update_growatt() {
  static uint32_t i;
  uint8_t result;
  
  i++;

  String tmp;
  char value[40] = "";
  
  // instantiate modbusmaster with slave id growatt
  node.begin(slave_id_growatt, Serial);
  
  result = node.readInputRegisters(0, 32);
  // do something with data if read is successful
  if (result == node.ku8MBSuccess){
    publishInt("energy/growatt/status", node.getResponseBuffer(0));
    
    publishFloat("energy/growatt/Ppv", glueFloat(node.getResponseBuffer(1), node.getResponseBuffer(2)));    
    publishFloat("energy/growatt/Vpv1", glueFloat(0, node.getResponseBuffer(3)));    
    publishFloat("energy/growatt/PV1Curr", glueFloat(0, node.getResponseBuffer(4)));    
    publishFloat("energy/growatt/Pac", glueFloat(node.getResponseBuffer(11), node.getResponseBuffer(12)));
    publishFloat("energy/growatt/Fac", glueFloat(0, node.getResponseBuffer(13))/10 );  
  
    publishFloat("energy/growatt/Vac1", glueFloat(0, node.getResponseBuffer(14)));  
    publishFloat("energy/growatt/Iac1", glueFloat(0, node.getResponseBuffer(15)));
    publishFloat("energy/growatt/Pac1", glueFloat(node.getResponseBuffer(16), node.getResponseBuffer(17)));
  
    publishFloat("energy/growatt/Etoday", glueFloat(node.getResponseBuffer(26), node.getResponseBuffer(27)));
    publishFloat("energy/growatt/Etotal", glueFloat(node.getResponseBuffer(28), node.getResponseBuffer(29)));
    publishFloat("energy/growatt/ttotal", glueFloat(node.getResponseBuffer(30), node.getResponseBuffer(31)));
    publishFloat("energy/growatt/Tinverter", glueFloat(0, node.getResponseBuffer(32)));
  } else {
    tmp = String(result, HEX);
    tmp.toCharArray(value, 40);
    
    publishInt("energy/growatt/status", -1);
  }
  node.clearResponseBuffer();
}

void loop() {
  // Handle OTA first.
  ArduinoOTA.handle();

  if (!mqtt_client.connected()) {
    reconnect();
  }
  mqtt_client.loop();

  if(millis() >= next_poll) {
    update_growatt();

    next_poll = millis() + 5000;
  }
}
