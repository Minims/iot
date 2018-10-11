#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Syslog.h>
#include <PubSubClient.h>
#include <Bounce2.h>
#include <EEPROM.h>
#include <DHT.h>
#include <Wire.h>
#include "SSD1306.h"

// WIFI
const char* ssid        = "*********";
const char* password    = "*********";
WiFiClient espClient;
PubSubClient client(espClient);

// HOSTNAME
const char* DEVICE_HOSTNAME  = "ESP-Thermostat";

// MQTT
const char* mqtt_server   = "*********";
const char* mqtt_user     = "";
const char* mqtt_password = "";

// SYSLOG
const char* SYSLOG_SERVER   = "*********";
int SYSLOG_PORT             =  514;
const char* APP_NAME        = "ESP-Thermostat";
WiFiUDP udpClient;
Syslog syslog(udpClient, SYSLOG_PROTO_IETF);

// PINS
#define DHTTYPE DHT22
#define DHTPIN  D4
DHT dht(DHTPIN, DHTTYPE, 11); // 11 works fine for ESP8266
int SWITCH = D2;
int BYPASS = D7;
int HEATER = D6;


// SSD1306
SSD1306  display(0x3c, D3, D5);

// MQTT TOPICS
const char* ByPass_Status  = "/ESP8266/ByPass/ByPass";
const char* ByPass_Command = "/ESP8266/ByPass/ByPass/Set";
const char* Heater_Status  = "/ESP8266/Thermostat/Thermostat";
const char* Heater_Command = "/ESP8266/Thermostat/Thermostat/Set";
const char* Temperature    = "/ESP8266/DHT22/Temperature";
const char* Humidity       = "/ESP8266/DHT22/Humidity";
const char* TargetTemp     = "/ESP8266/Thermostat/TargetTemp";

// SKETCH VARS
char message[100];
bool BYPASS_STATE = HIGH;
bool HEATER_STATE = HIGH;
float humidity, temperature;
unsigned long previousMillis = 0;
const long interval = 5000;
String target_temp = "";

Bounce debouncer = Bounce();

void setup_ota() {
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(DEVICE_HOSTNAME);

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
    Serial.println("Start updating " + type);
    syslog.log(LOG_INFO, "Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
}

void setup_syslog() {
  syslog.server(SYSLOG_SERVER, SYSLOG_PORT);
  syslog.deviceHostname(DEVICE_HOSTNAME);
  syslog.appName(APP_NAME);
  syslog.defaultPriority(LOG_KERN);
}

void setup_wifi() {

  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    extButton();
    for(int i = 0; i<500; i++){
      extButton();
      delay(1);
    }
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  int i = 0;
  for (i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    message[i] = payload[i];
  }
  message[i] = '\0';
  Serial.println();

  if ( String(topic) == ByPass_Command ) {
    if ((char)payload[0] == '0') {
      // Set Heater OFF First
      digitalWrite(HEATER, HIGH);
      Serial.println("HEATER -> OFF");
      HEATER_STATE = HIGH;
      EEPROM.write(1, HEATER_STATE);
      EEPROM.commit();
      client.publish(Heater_Status, "0");
      // Then ByPass
      digitalWrite(BYPASS, HIGH);
      Serial.println("BYPASS -> OFF");
      BYPASS_STATE = HIGH;
      EEPROM.write(0, BYPASS_STATE);
      EEPROM.commit();
      client.publish(ByPass_Status, "0");
    } else if ((char)payload[0] == '1') {
      digitalWrite(BYPASS, LOW);
      Serial.println("BYPASS -> ON");
      BYPASS_STATE = LOW;
      EEPROM.write(0, BYPASS_STATE);
      EEPROM.commit();
      client.publish(ByPass_Status, "1");
    } else if ((char)payload[0] == '2') {
      BYPASS_STATE = !BYPASS_STATE;
      digitalWrite(BYPASS, BYPASS_STATE);  // Turn off by making the voltage HIGH
      Serial.print("BYPASS -> switched to ");
      Serial.println(BYPASS_STATE);
      EEPROM.write(0, BYPASS_STATE);
      EEPROM.commit();
    }
  } else if ( String(topic) == Heater_Command ) {
    if ((char)payload[0] == '0') {
      digitalWrite(HEATER, HIGH);
      Serial.println("HEATER -> OFF");
      HEATER_STATE = HIGH;
      EEPROM.write(1, HEATER_STATE);
      EEPROM.commit();
      client.publish(Heater_Status, "0");
    } else if ((char)payload[0] == '1') {
      digitalWrite(BYPASS, LOW);
      Serial.println("BYPASS -> ON");
      BYPASS_STATE = LOW;
      EEPROM.write(0, BYPASS_STATE);
      EEPROM.commit();
      client.publish(ByPass_Status, "1");
      digitalWrite(HEATER, LOW);
      Serial.println("HEATER -> ON");
      HEATER_STATE = LOW;
      EEPROM.write(1, HEATER_STATE);
      EEPROM.commit();
      client.publish(Heater_Status, "1");
    } else if ((char)payload[0] == '2') {
      HEATER_STATE = !HEATER_STATE;
      digitalWrite(HEATER, HEATER_STATE);
      Serial.print("HEATER -> switched to ");
      Serial.println(HEATER_STATE);
      EEPROM.write(1, HEATER_STATE);
      EEPROM.commit();
    }
  } else if ( String(topic) == TargetTemp ) {
    Serial.println(String(message));
    target_temp = String(message);
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      client.subscribe(ByPass_Command);
      client.subscribe(Heater_Command);
      client.subscribe(TargetTemp);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      for(int i = 0; i<5000; i++){
        extButton();
        delay(1);
      }
    }
  }
}

void extButton() {
  debouncer.update();

  // Call code if Bounce fell (transition from HIGH to HIGH) :
  if ( debouncer.fell() ) {
    Serial.println("Debouncer fell");
    // Toggle bypass state :
    BYPASS_STATE = !BYPASS_STATE;
    digitalWrite(BYPASS,BYPASS_STATE);
    EEPROM.write(0, BYPASS_STATE);
    if (BYPASS_STATE == 0){
      client.publish(ByPass_Status, "1");
    }
    else if (BYPASS_STATE == 1){
      client.publish(ByPass_Status, "0");
      digitalWrite(HEATER,HIGH);
      HEATER_STATE = BYPASS_STATE;
      EEPROM.write(1, HEATER_STATE);
      client.publish(Heater_Status, "0");
    }
      EEPROM.commit();
  }
}

void setup() {

  // Init OLED SSD1603
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 0, "E-Thermostat");
  display.drawString(0, 16, "Starting...");
  display.display();

  EEPROM.begin(512);       // Begin EEPROM to store on/off state
  pinMode(BYPASS, OUTPUT); // Initialize the BYPASS as an output
  pinMode(HEATER, OUTPUT); // Initialize the HEATER as an output
  pinMode(SWITCH, INPUT);  // Initialize the SWITCH as an input
  pinMode(DHTPIN, OUTPUT); // Initialize the DHT22  as an output

  // Restore last states.
  BYPASS_STATE = EEPROM.read(0);
  HEATER_STATE = EEPROM.read(1);
  digitalWrite(BYPASS,BYPASS_STATE);
  digitalWrite(HEATER,HEATER_STATE);

  // Define Bounce2.
  debouncer.attach(SWITCH); // Use the bounce2 library to debounce the built in button
  debouncer.interval(50);   // Input must be HIGH for 50 ms

  Serial.begin(115200);
  setup_wifi();             // Connect to wifi
  setup_syslog();
  syslog.log(LOG_INFO, "Start Setup");
  setup_ota();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  dht.begin();              // initialize temperature sensor

}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  extButton();
  gettemperature();
  display.display();
  ArduinoOTA.handle();
}

void gettemperature() {
  unsigned long currentMillis = millis();
  if(currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    humidity       = dht.readHumidity();        // Read humidity (percent)
    temperature    = dht.readTemperature();     // Read temperature as Celcius
    temperature    = temperature - 0.5;         // Correct Temperautre value
    if (isnan(humidity) || isnan(temperature)) {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }
    client.publish(Temperature, String(temperature).c_str(), true);
    client.publish(Humidity, String(humidity).c_str(), true);

    display.clear();
    display.drawString(0, 0, "Hum: " + String(humidity) + "%\t");
    display.drawString(0, 16, "T°: " + String(temperature) + " / " + String(target_temp) + "°C");
    if (BYPASS_STATE == 1) {
      display.drawString(0, 32, "eThermostat: OFF");
    } else if (BYPASS_STATE == 0) {
      display.drawString(0, 32, "eThermostat: ON");
    }
    if (HEATER_STATE == 1) {
        display.drawString(0, 48, "Chaudière: OFF");
    } else if (HEATER_STATE == 0) {
        display.drawString(0, 48, "Chaudière: ON");
    }
  }
}
