/*********************  ESP8688 CLIENT NODO REMOTO  ********************************/

#include <NullSpaceLib.h>
#ifdef ESP8266 
       #include <ESP8266WiFi.h>
#endif 
#ifdef ESP32   
       #include <WiFi.h>
#endif
#include "SinricPro.h"
#include "SinricProTemperaturesensor.h"
#include "SinricProLight.h"
#include <ArduinoJson.h> //configuratore: https://arduinojson.org/v6/assistant/#/step1
//DHT sensor library:1.4.4
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <RTClib.h> //Adafruit RTClib per DS3231 e DateTime
#include <ArduinoWebsockets.h> //Downloading ArduinoWebsockets@0.5.3
#include <Adafruit_NeoPixel.h>

//Installing AsyncMQTT_Generic@1.8.0
#include <Ticker.h>
#include <AsyncMqtt_Generic.h>
AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;

Ticker wifiReconnectTimer;

#define REMOTE_NODE_ID 2
const String topic_node_name("bedroom");
//const String topic_node_name("bathroom");

const String full_topic_pub_temperature("node/"+topic_node_name+"/actual/temperature");
const String full_topic_pub_humidity("node/"+topic_node_name+"/actual/humidity");
const String full_topic_pub_lamp_state_onoff("node/"+topic_node_name+"/lamp/actual/onoff");
const String full_topic_pub_lamp_state_color("node/"+topic_node_name+"/lamp/actual/color");

const String full_topic_sub_lamp_onoff("node/"+topic_node_name+"/lamp/cmd/onoff");
const String full_topic_sub_lamp_color("node/"+topic_node_name+"/lamp/cmd/color");

const char* mqtt_server = "192.168.178.44";
const uint16_t mqtt_port = 1883;
WiFiClient espClient;

TimerC timer_mqtt_publisher;
#define sampling_time_mqtt_publisher 1000 //ms

#define DHTPIN 2      // Digital pin connected to the DHT sensor GPIO2=D4
// Uncomment the type of sensor in use:
#define DHTTYPE    DHT11     // DHT 11
//#define DHTTYPE    DHT22     // DHT 22 (AM2302)
//#define DHTTYPE    DHT21     // DHT 21 (AM2301)
DHT_Unified dht(DHTPIN, DHTTYPE);
sensors_event_t g_dht_event;
TimerC timer_sampling_temp_hum;
uint32_t sampling_time_temp_hum;

Adafruit_NeoPixel strip = Adafruit_NeoPixel(5, 5, NEO_GRB + NEO_KHZ800); //4 leds gpio3=RX

const char* ssid = "FRITZ!Box 7530 JB"; //Enter SSID
const char* password = "alessandrotoscana8024"; //Enter Password
const char* websockets_server_host = "192.168.178.102"; //Enter server adress
const uint16_t websockets_server_port = 8080; // Enter server port
#define APP_KEY           "2c5cff91-ef92-4f81-b673-e51f9d790679"      // Should look like "de0bxxxx-1x3x-4x3x-ax2x-5dabxxxxxxxx"
#define APP_SECRET        "ecfb65ba-b370-481e-b45c-8fa7a298a02a-dc548921-639d-414e-910f-bfbeccc593ec"   // Should look like "5f36xxxx-x3x7-4x3x-xexe-e86724a9xxxx-4c4axxxx-3x3x-x5xe-x9x3-333d65xxxxxx"
#define TEMP_SENSOR_ID    "63ac16e2fdf12ea3f97e1dfe"    // Should look like "5dc1564130xxxxxxxxxxxxxx"
#define LIGHT_ID          "63ac164b90b51ab26f7cffbc"    // Should look like "5dc1564130xxxxxxxxxxxxxx"
#define EVENT_WAIT_TIME   60000                // send event every 60 seconds
TimerC timer_syncpro_send;

SinricProLight &myLight = SinricPro[LIGHT_ID];
// ColorController
void updateColor(byte r, byte g, byte b) {
  myLight.sendColorEvent(r, g, b);
}
// ToggleController
void updateToggleState(bool state) {
  myLight.sendPowerStateEvent(state);
}

class DataHolder
{
  public:
    DataHolder()
    {
      this->led_color = Adafruit_NeoPixel::Color(255,255,255);
      this->led_brightness = 100; //range 0-100
    }
    float node_temperature;
    float node_humidity;
    bool deviceTemperatureIsOn;
    bool deviceLedIsOn;
    uint8_t led_brightness;
    uint32_t led_color;
};
DataHolder g_DataHolder;

bool first_run = true;

//WEBSOCKET CLIENT
using namespace websockets;
TimerC timer_websocket_client_send;
#define sampling_time_websocket_client_send 1000 //ms
TimerC timer_websocket_client_rcv;
#define sampling_time_websocket_client_rcv 500 //ms
WebsocketsClient client_ws;

/* MQTT */
//______________
void connectToWifi()
{
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);
}
void connectToMqtt()
{
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}
void onWifiConnect(const WiFiEventStationModeGotIP& event)
{
  (void) event;

  Serial.print("Connected to Wi-Fi. IP address: ");
  Serial.println(WiFi.localIP());
  connectToMqtt();
}
void onWifiDisconnect(const WiFiEventStationModeDisconnected& event)
{
  (void) event;

  Serial.println("Disconnected from Wi-Fi.");
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  wifiReconnectTimer.once(2, connectToWifi);
}
void printSeparationLine()
{
  Serial.println("************************************************");
}
void onMqttConnect(bool sessionPresent)
{
  Serial.print("Connected to MQTT broker: ");
  Serial.print(mqtt_server);
  Serial.print(", port: ");
  Serial.println(mqtt_port);

  printSeparationLine();
  Serial.print("Session present: ");
  Serial.println(sessionPresent);

  //SUBS
  uint16_t packetIdSub = mqttClient.subscribe(full_topic_sub_lamp_onoff.c_str(), 0);
  Serial.print("Subscribing "+full_topic_sub_lamp_onoff+" at QoS 0, packetId: ");
  Serial.println(packetIdSub);
  packetIdSub = mqttClient.subscribe(full_topic_sub_lamp_color.c_str(), 0);
  Serial.print("Subscribing "+full_topic_sub_lamp_color+" at QoS 0, packetId: ");
  Serial.println(packetIdSub);
  printSeparationLine();
}
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  (void) reason;

  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected())
  {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}
void onMqttSubscribe(const uint16_t& packetId, const uint8_t& qos)
{
  Serial.println("Subscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
  Serial.print("  qos: ");
  Serial.println(qos);
}
void onMqttUnsubscribe(const uint16_t& packetId)
{
  Serial.println("Unsubscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void onMqttMessage(char* topic, char* payload, const AsyncMqttClientMessageProperties& properties,
                   const size_t& len, const size_t& index, const size_t& total)
{

  Serial.println("Publish received.");
  Serial.print("  topic: ");
  Serial.println(topic);
  Serial.print("  qos: ");
  Serial.println(properties.qos);
  Serial.print("  dup: ");
  Serial.println(properties.dup);
  Serial.print("  retain: ");
  Serial.println(properties.retain);
  Serial.print("  len: ");
  Serial.println(len);
  Serial.print("  index: ");
  Serial.println(index);
  Serial.print("  total: ");
  Serial.println(total);
  Serial.print("  raw msg: ");
  Serial.println(payload);
  Serial.print("  msg: ");
  String messageTemp;
  for (int i = 0; i < len; i++) {
    Serial.print((char)payload[i]);
    messageTemp += (char)payload[i];
  }
  Serial.println();

  // SUB ON-OFF 
  if (String(topic) == full_topic_sub_lamp_onoff) {
    Serial.print("[MQTT] Changing lamp state to: ");
    if(messageTemp == "on"){
      Serial.println("on");
      changeState(true);
      //send update to sinricPro
      updateToggleState(true);
      
    }
    else if(messageTemp == "off"){
      Serial.println("off");
      changeState(false);
      //send update to sinricPro
      updateToggleState(false);
    }
  }

  // SUB LAMP COLOR 
  if (String(topic) == full_topic_sub_lamp_color) {
    Serial.print("[MQTT] Changing lamp color to: " + messageTemp);
    //Color as comma separated r,g,b string
    int firstcomma = messageTemp.indexOf(',');
    int secondcomma = messageTemp.indexOf(',',firstcomma+1);
    String r = messageTemp.substring(1,firstcomma);//remove "!"
    String g = messageTemp.substring(firstcomma+1,secondcomma);
    String b = messageTemp.substring(secondcomma+1);
    g_DataHolder.led_color = strip.Color(r.toInt(),g.toInt(),b.toInt());    
    changeColor();
    //update color to sinricPro
    updateColor(r.toInt(),g.toInt(),b.toInt());
  }


}

void onMqttPublish(const uint16_t& packetId)
{
  Serial.println("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}
//______________


// setup function for SinricPro
void setupSinricPro() {
  // add device to SinricPro

  //DHT11 SENSOR
  SinricProTemperaturesensor &mySensor = SinricPro[TEMP_SENSOR_ID];
  mySensor.onPowerState(onPowerStateTemperatureSensor);

  //LEDS  
  //SinricProLight &myLight = SinricPro[LIGHT_ID];
  // set callback function to device
  myLight.onPowerState(onPowerStateLed);
  myLight.onBrightness(onBrightnessLed);
  myLight.onAdjustBrightness(onAdjustBrightnessLed);
  myLight.onColor(onColorLed);

  // setup SinricPro
  SinricPro.onConnected([](){ Serial.printf("Connected to SinricPro\r\n"); }); 
  SinricPro.onDisconnected([](){ Serial.printf("Disconnected from SinricPro\r\n"); });
  //SinricPro.restoreDeviceStates(true); // Uncomment to restore the last known state from the server.
  SinricPro.begin(APP_KEY, APP_SECRET);  
}
bool onPowerStateTemperatureSensor(const String &deviceId, bool &state) {
  Serial.printf("Temperaturesensor turned %s (via SinricPro) \r\n", state?"on":"off");
  g_DataHolder.deviceTemperatureIsOn = state; // turn on / off temperature sensor
  return true; // request handled properly
}
bool changeState(bool state)
{
  g_DataHolder.deviceLedIsOn = state;
  if (state) {
    for(uint16_t i=0; i<strip.numPixels(); i++) 
    {
      strip.setPixelColor(i, g_DataHolder.led_color);
    }
    strip.setBrightness(map(g_DataHolder.led_brightness, 0, 100, 0, 255));
  } else {
    strip.clear();
  }
  strip.show();
  return true; 
}  
bool onPowerStateLed(const String &deviceId, bool &state) {
  Serial.printf("onPowerStateLed turned %s (via SinricPro) \r\n" , state?"on":"off");
  Serial.printf("g_DataHolder.led_color %d (via SinricPro) \r\n", g_DataHolder.led_color);
  Serial.printf("g_DataHolder.led_brightness %d (via SinricPro) \r\n", g_DataHolder.led_brightness);
  return changeState(state);
}
bool changeColor()
{ 
  if(!g_DataHolder.deviceLedIsOn) return false;
  for(uint16_t i=0; i<strip.numPixels(); i++) {
  strip.setPixelColor(i, g_DataHolder.led_color);
  }
  strip.show();
  return true;
}
bool onColorLed(const String &deviceId, byte &r, byte &g, byte &b) {
  g_DataHolder.led_color = strip.Color(r,g,b);
  return changeColor();
}
bool onBrightnessLed(const String &deviceId, int &brightness) {
  if(!g_DataHolder.deviceLedIsOn) return false;
  g_DataHolder.led_brightness=brightness;
  for(uint16_t i=0; i<strip.numPixels(); i++) 
  {
    strip.setPixelColor(i, g_DataHolder.led_color);
  }
  strip.setBrightness(map(g_DataHolder.led_brightness, 0, 100, 0, 255));
  strip.show();
  return true;
}
bool onAdjustBrightnessLed(const String &deviceId, int &brightnessDelta) {
  if(!g_DataHolder.deviceLedIsOn) return false;
  g_DataHolder.led_brightness += brightnessDelta;
  brightnessDelta = g_DataHolder.led_brightness; //update global brightness
  for(uint16_t i=0; i<strip.numPixels(); i++) 
  {
    strip.setPixelColor(i, g_DataHolder.led_color);
  }
  strip.setBrightness(map(g_DataHolder.led_brightness, 0, 100, 0, 255));
  strip.show();
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println(ASYNC_MQTT_GENERIC_VERSION);

  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(mqtt_server, mqtt_port);
  connectToWifi();


  strip.begin();
  strip.setBrightness(255); //[0-255]
  strip.clear(); // metti ad off tutti i leds
  strip.show(); // Initialize all pixels to 'off'

  // Initialize device.
  dht.begin();
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  dht.humidity().getSensor(&sensor);
  // Set delay between sensor readings based on sensor details.
  sampling_time_temp_hum = sensor.min_delay / 1000; //ms

  connectWSClientToServer();

  setupSinricPro();

  timer_sampling_temp_hum.start();
  timer_websocket_client_send.start();
  timer_websocket_client_rcv.start();
  timer_syncpro_send.start();
  timer_mqtt_publisher.start();

}

void loop() 
{

  SinricPro.handle();
  bool ok = read_temp_hum_node_station(g_dht_event);
  if(ok)
  {
    clientWSSendDataToServer();
    sendDataToSyncPro();
    //publish mqtt 
    if(timer_mqtt_publisher.getET()>=sampling_time_mqtt_publisher)
    {
      timer_mqtt_publisher.reset();
      mqttClient.publish(full_topic_pub_temperature.c_str(), 0, false, String(g_DataHolder.node_temperature,1).c_str());
      mqttClient.publish(full_topic_pub_humidity.c_str(), 0, false, String(g_DataHolder.node_humidity,0).c_str());
      mqttClient.publish(full_topic_pub_lamp_state_onoff.c_str(), 0, false, g_DataHolder.deviceLedIsOn ? "on": "off");
      mqttClient.publish(full_topic_pub_lamp_state_color.c_str(), 0, false, String(g_DataHolder.led_color).c_str());
      //Serial.println("Publishing at QoS 0");
    }
      
  }

  clientCheckForIncomingMsgs();
}


bool read_temp_hum_node_station(sensors_event_t& dht_event)
{
  if(timer_sampling_temp_hum.getET()>=sampling_time_temp_hum)
  {
    //digitalWrite(PIN_RELE,!digitalRead(PIN_RELE));
    timer_sampling_temp_hum.reset();
    dht.temperature().getEvent(&dht_event);
    if (isnan(dht_event.temperature)) {
      //Serial.println(F("Error reading temperature!"));
      return false;
    }
    else {
      g_DataHolder.node_temperature = dht_event.temperature;

      //Serial.print(F("Temperature: "));
      //Serial.print(g_dht_event.temperature);
      //Serial.println(F("°C"));
    }
    // Get humidity event and print its value.
    dht.humidity().getEvent(&dht_event);
    if (isnan(dht_event.relative_humidity)) {
      //Serial.println(F("Error reading humidity!"));
      return false;
    }
    else {
      g_DataHolder.node_humidity = dht_event.relative_humidity;
      //Serial.print(F("Humidity: "));
      //Serial.print(g_dht_event.relative_humidity);
      //Serial.println(F("%"));      
    }
  }
  return true;
}

void connectWSClientToServer()
{
    // try to connect to Websockets server
    bool connected = client_ws.connect(websockets_server_host, websockets_server_port, "/");
    if(connected) {
        Serial.println("Connecetd!");
        //client_ws.send("Hello Server");
    } else {
        Serial.println("Not Connected!");
    }
    
    // run callback when messages are received
    client_ws.onMessage([&](WebsocketsMessage message) {
        Serial.print("Got Message: ");
        Serial.println(message.data());
    });
}

void clientWSSendDataToServer()
{
  if(timer_websocket_client_send.getET() >= sampling_time_websocket_client_send)
  {
    timer_websocket_client_send.reset();
    //send data over websocket to server
    //TODO: BETTER TO USE JSON
    //I USE MY COSTUM STRING PROTOCOL
    char msg[20];
    sprintf(msg,"!%d,%s,%d", REMOTE_NODE_ID,String(g_DataHolder.node_temperature,1),static_cast<int>(g_DataHolder.node_humidity));
    client_ws.send(msg);
    
  }
}

void sendDataToSyncPro()
{
  if(timer_syncpro_send.getET()>=EVENT_WAIT_TIME || first_run)
  {
    first_run = false;
    timer_syncpro_send.reset();
    if(!g_DataHolder.deviceTemperatureIsOn)
      return;    
    SinricProTemperaturesensor &mySensor = SinricPro[TEMP_SENSOR_ID];  // get
    bool success = mySensor.sendTemperatureEvent(g_DataHolder.node_temperature, g_DataHolder.node_humidity); // send event
    if (success) {  // if event was sent successfuly, print temperature and humidity to serial
      Serial.printf("Temperature: %2.1f Celsius\tHumidity: %2.0f%%\r\n", g_DataHolder.node_temperature, g_DataHolder.node_humidity);
    } else {  // if sending event failed, print error message
      Serial.printf("Something went wrong...could not send Event to server!\r\n");
    }
  }
}

void clientCheckForIncomingMsgs()
{
  if(timer_websocket_client_rcv.getET() >= sampling_time_websocket_client_rcv)
  {
    timer_websocket_client_rcv.reset();
    // let the websockets client_ws check for incoming messages
    if(client_ws.available()) {
        client_ws.poll();
    }
  }
}