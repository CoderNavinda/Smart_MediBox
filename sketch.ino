#include <WiFi.h>
#include "DHTesp.h"
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP32Servo.h>


#define DHT_PIN 15
#define BUZZER 12
#define LDR 35

const float GAMMA = 0.7;
const float R = 50;
int theta_offset = 30;
float ctrl_factor = 0.75;

Servo serv;
WiFiClient espClient;
PubSubClient mqttClient(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

char lightAr[10];
char tempAr[6];

DHTesp dhtSensor;
const int servopin = 21;
bool isScheduledON = false;
unsigned long scheduledOnTime;

void setup() {
  Serial.begin(115200);
  setupWifi();
  setupMqtt();
  dhtSensor.setup(DHT_PIN, DHTesp::DHT22); 
  timeClient.begin();
  timeClient.setTimeOffset(5.5 * 3600);
  pinMode(BUZZER, OUTPUT);
  pinMode(LDR, INPUT);
  digitalWrite(BUZZER, LOW); 
  serv.attach(servopin, 500, 2400);
}

void loop() {
  if (!mqttClient.connected()) {
    connectToBroker();
  }
  mqttClient.loop();
  updateTemperature();
  Serial.println(tempAr);
  mqttClient.publish("ENTC-ADMIN-TEMP1", tempAr);
  LightIntensity();
  Serial.println(lightAr);
  mqttClient.publish("LIGHT-INTENSITY", lightAr);
  serv.write(theta_offset+(180-theta_offset)*atof(lightAr)*ctrl_factor);
  checkSchedule();
  delay(1000);
  
}

void buzzerOn(bool on){
   if (on) {
    tone (BUZZER, 256);
  } else {
    noTone (BUZZER);
  }
}



void receiveCallback(char* topic, byte* payload, unsigned int length) {

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  char payloadCharAr[length];
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    payloadCharAr[i] = (char)payload[i];
  }

  Serial.print("");

  if (strcmp(topic, "ENTC-ADMIN-MAIN-ON-OFF1") == 0) {
    buzzerOn(payloadCharAr[0] == '1');
  }  else if (strcmp(topic, "ENTC-ADMIN-SCH-ON1") ==  0) {
    if (payloadCharAr[0] == 'N') {
      isScheduledON = false;
    } else {
      isScheduledON = true;
      scheduledOnTime = atol(payloadCharAr);
    } 
  } else if (strcmp(topic, "MIN-ANGLE") ==  0){

    theta_offset = atoi(payloadCharAr);
    Serial.print("THETA-OFFSET: ");
    Serial.println(theta_offset);

  } else if (strcmp(topic, "CONTROL-FACTOR") ==  0){

    ctrl_factor = atof(payloadCharAr);
    Serial.print("GAMMA: ");
    Serial.println(ctrl_factor);

  }

}


void connectToBroker() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect("ESP32-123456454541")) {
      Serial.println("connected");
      //Subscribe
      mqttClient.subscribe("ENTC-ADMIN-MAIN-ON-OFF1");
      mqttClient.subscribe("ENTC-ADMIN-SCH-ON1");
       mqttClient.subscribe("MIN-ANGLE");
      mqttClient.subscribe("CONTROL-FACTOR");
    } else {
      Serial.print("failed ");
      Serial.print(mqttClient.state());
      delay(5000);
    }
  }
}

void setupMqtt() {
    mqttClient.setServer("test.mosquitto.org", 1883);
    mqttClient.setCallback(receiveCallback);
}


void updateTemperature() {
    TempAndHumidity data = dhtSensor.getTempAndHumidity();
    String(data.temperature, 2).toCharArray(tempAr, 6);
}
 



void setupWifi() {
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println("Wokwi-GUEST");
  WiFi.begin("Wokwi-GUEST", "");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

unsigned long getTime() {
  timeClient.update();
  return timeClient.getEpochTime();
}


void checkSchedule()   {
  if (isScheduledON) {
    unsigned long currentTime = getTime();
    if (currentTime > scheduledOnTime) {
      buzzerOn(true);
      isScheduledON = false;
      mqttClient.publish("ENTC-ADMIN-MAIN-ON-OFF-ESP", "1");
      mqttClient.publish("ENTC-ADMIN-TEMP1", "0");
      Serial.println("Scheduled ON");
    }
  }
}

void LightIntensity() {
  float v = (analogRead(LDR)/4) / 1024. * 5;
  float resistance = 2000 * v / (1 - v / 5);
  float lux = pow(R * 1e3 * pow(10, GAMMA) / resistance, (1 / GAMMA))/100916.59;
  String(lux, 6).toCharArray(lightAr, 10);
}