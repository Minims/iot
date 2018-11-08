#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Syslog.h>
#include <PubSubClient.h>

// CUSTOM include
#include "config.h"
#include "MQ7.h"

// WIFI
WiFiClient espClient;
PubSubClient client(espClient);

// MQTT TOPICS
const char* MQ7_TOPIC = "/MQ7";
const char* ppmc      = "0.00";

// MQ7
MQ7 mq7(A0, 0.92);

// PINS
const int MQ = 12; // D6 - GPIO-12

// SYSLOG
WiFiUDP udpClient;
Syslog syslog(udpClient, SYSLOG_PROTO_IETF);

void setup_ota() {
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(hostname);

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    syslog.log(LOG_INFO, "Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    syslog.log(LOG_INFO, "\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      syslog.log(LOG_INFO, "Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      syslog.log(LOG_INFO, "Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      syslog.log(LOG_INFO, "Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      syslog.log(LOG_INFO, "Receive Failed");
    } else if (error == OTA_END_ERROR) {
      syslog.log(LOG_INFO, "End Failed");
    }
  });
  ArduinoOTA.begin();
}

void setup_syslog() {
  syslog.server(syslog_server, syslog_port);
  syslog.deviceHostname(hostname);
  syslog.appName(app_name);
  syslog.defaultPriority(LOG_KERN);
}

void setup() {
  Serial.begin(serial_speed);
  setup_wifi();
  setup_syslog();
  syslog.log(LOG_INFO, "Start Setup");
  setup_ota();
  client.setServer(mqtt_server, mqtt_port);
  pinMode(MQ, OUTPUT);
 }

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    for(int i = 0; i<500; i++){
      delay(1);
    }
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}


void reconnect() {
  while (!client.connected()) {
    syslog.log(LOG_INFO, "Attempting MQTT connection...");
    if (client.connect(mqtt_identifier, mqtt_username, mqtt_password)) {
      syslog.log(LOG_INFO, "connected");
    } else {
      syslog.log(LOG_INFO, "failed, rc=");
      Serial.print(client.state());
      syslog.log(LOG_INFO, " try again in 5 seconds");
      for(int i = 0; i<5000; i++){
        delay(1);
      }
    }
  }
}

void loop() {
  client.loop();
  readMQ7();
  ArduinoOTA.handle();
}

void readMQ7() {
  analogWrite(MQ, 1024);
  //syslog.log(LOG_INFO, "5v for 60s");
  delay(60000);
  analogWrite(MQ, 333);
  //syslog.log(LOG_INFO, "1.4v for 90s");
  delay(90000);
  digitalWrite(MQ, HIGH);
  //syslog.log(LOG_INFO, "READ @5v");
  delay (50);
  String ppm = String(mq7.getPPM());
  ppmc = (char*) ppm.c_str();
   if (!client.connected()) {
    reconnect();
  }
  client.publish(MQ7_TOPIC,  String(ppmc).c_str() );
}
