// HOSTNAME
const char* hostname        = "RFLink";         // Hostname (used by syslog)

// WIFI
const char* ssid            = "SSID";           // SSID to connect
const char* password        = "PASSWORD";       // WIFI Password

// SYSLOG
const char* syslog_server   = "192.168.1.1";
const int   syslog_port     =  514;
const char* app_name        = hostname;

// MQTT
const char* mqtt_server     = "192.168.1.1";   // MQTT ip address
const char* mqtt_username   = "username";      // MQTT username
const char* mqtt_password   = "paswword";      // MQTT password
const char* client_name     = hostname;        // MQTT client name (must be unique)

// some testing switches
boolean testmode    = false;    // if true, then do not listen to softwareserial, but normal serial for input
boolean enableMQTT  = true;     // if false, do not transmit MQTT codes (for testing really)
boolean enableDebug = false;    // if false, do not send data to debug topic (for testing really)