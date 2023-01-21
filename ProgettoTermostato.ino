/*********************  ESP32 SERVER CENTRALE  ********************************/

#include <NullSpaceLib.h>
#ifdef ESP8266 
       #include <ESP8266WiFi.h>
#endif 
#ifdef ESP32   
       #include <WiFi.h>
#endif
#include <ArduinoJson.h> //configuratore: https://arduinojson.org/v6/assistant/#/step1
//DHT sensor library:1.4.4
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <RTClib.h> //Adafruit RTClib per DS3231 e DateTime
#include "EasyNextionLibrary.h"  //EasyNextionLibrary
#include <ArduinoWebsockets.h> //Downloading ArduinoWebsockets@0.5.3
#include <vector>
#include <algorithm>    // std::remove_if
//mqtt
//Installing AsyncMQTT_Generic@1.8.0
extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
}
#include <AsyncMqtt_Generic.h>
AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

//PUBS
const String full_topic_pub_temperature("central/thermostat/actual/temperature");
const String full_topic_pub_humidity("central/thermostat/actual/humidity");
const String full_topic_pub_boiler_actual_onoff("central/thermostat/boiler/actual/onoff");
const String full_topic_pub_actual_setpoint_temperature("central/thermostat/actual/setpoint/temperature");
//SUBS
const String full_topic_sub_setpoint_temperature("central/thermostat/cmd/setpoint/temperature");
const String full_topic_sub_boiler_cmd_onoff("central/thermostat/boiler/cmd/onoff");

#include "SinricPro.h"
#include "SinricProThermostat.h"
#define APP_KEY           "2c5cff91-ef92-4f81-b673-e51f9d790679"      // Should look like "de0bxxxx-1x3x-4x3x-ax2x-5dabxxxxxxxx"
#define APP_SECRET        "ecfb65ba-b370-481e-b45c-8fa7a298a02a-dc548921-639d-414e-910f-bfbeccc593ec"   // Should look like "5f36xxxx-x3x7-4x3x-xexe-e86724a9xxxx-4c4axxxx-3x3x-x5xe-x9x3-333d65xxxxxx"
#define THERMOSTAT_ID     "63ad377a90b51ab26f7def4f"    // Should look like "5dc1564130xxxxxxxxxxxxxx"

#define PIN_LED_BOILER_ON 25

#define DHTPIN 14     // Digital pin connected to the DHT sensor
// Uncomment the type of sensor in use:
#define DHTTYPE    DHT11     // DHT 11
//#define DHTTYPE    DHT22     // DHT 22 (AM2302)
//#define DHTTYPE    DHT21     // DHT 21 (AM2301)
DHT_Unified dht(DHTPIN, DHTTYPE);
sensors_event_t g_dht_event;
TimerC timer_sampling_temp_hum;
uint32_t sampling_time_temp_hum;

const float temperature_control_hysteresis=0.5; //Celsius

//Serial2
//gpio17 = TX  - - > RX NEXTION CAVO GIALLO
//gpio16 = RX  - - > TX NEXTION CAVO BLU
EasyNex myNex(Serial2);
TimerC timer_sampling_read_from_nextion;
#define sampling_time_read_from_nextion 500//ms
TimerC timer_sampling_write_to_nextion;
#define sampling_time_write_to_nextion 500//ms
TimerC timer_sampling_write_to_sinricpro;
#define sampling_write_to_sinricpro 10000//ms

#define PIN_RELE 27

class DataHolder
{
  public:
    DataHolder(){}
    float central_temperature;
    float central_humidity;
    int anno;
    int mese;
    int giorno;
    int ore;
    int minuti;
    int secondi;
    int dayoftheweek;
    int From[5];
    int To[5];
    int wFrom[5];
    int wTo[5];
    int boilerManAuto;//0=Man 1=Auto
    int boilerManOnOff;//0=ManOFF 1=ManON
    bool BoilerOutput;//false=OFF true=ON
    float setpoint_temperature;

    const char* nomecitta_it;
    const char* nomecitta_en;
    float lat;
    float lon;
    const char* principale_meteo;
    const char* descrizione_meteo;
    float temperatura_esterna;
    float umidita_esterna;
    int id_icona_meteo;
    const char* country_id;
    float temperatura_percepita;//°C
    float temperatura_massima;
    float temperatura_minima;
    float pressione;//hPa
    float velocita_vento; //m/s
    int direzione_vento;// °
    float visibilita; //m
    String getDateTime()
    {
      char dt[20];
      sprintf(dt, "%02d/%02d/%04d %02d:%02d:%02d", this->giorno, this->mese,this->anno,this->ore,this->minuti,this->secondi);
      return String(dt);      
    }
    String getTimeOnly()
    {
      char soloora[10];
      sprintf(soloora, "%02d:%02d:%02d", this->ore,this->minuti,this->secondi);
      return String(soloora);
    }
    String getDateOnly()
    {
      char solodata[10];
      sprintf(solodata, "%02d/%02d/%04d", this->giorno, this->mese,this->anno);
      return String(solodata);
    }
    bool checkErrorReadTimeZones()
    {
      for (int i=0; i<5; ++i) 
      {
        if (From[i]==777777)
          return false;
        if (wFrom[i]==777777)
          return false;
        if (To[i]==777777)
          return false;
        if (wTo[i]==777777)
          return false;
      }
      return true;
    }
    void printTimeZones()
    {
      for (int i=0; i<5; ++i) 
      {
        Serial.print(From[i]);Serial.print(" ");Serial.print(To[i]);Serial.print("\t");Serial.print(wFrom[i]);Serial.print(" ");Serial.println(wTo[i]);
      }
    }
};
enum fase_del_di_enum
{
  giorno=0,
  notte
};

DataHolder g_DataHolder;


class RemoteNodeInfo
{
  public:
    RemoteNodeInfo(){}
    String node_id;
    String node_temperature;
    String node_humidity;
};

std::vector<RemoteNodeInfo> vecRemoteNodes;

//WEBSOCKET SERVER
using namespace websockets;
WebsocketsServer server;
std::vector<WebsocketsClient> allWSClients;
TimerC timer_websocket_server;
#define sampling_time_websocket_server 1000 //ms

//Inizializza il WiFi
WiFiClient wifiClient;
const char* ssid = "FRITZ!Box 7530 JB";                          
const char* password = "alessandrotoscana8024"; 
//STATIC IP OF THE CENTRAL NODE
// Set your Static IP address
IPAddress local_IP(192, 168, 178, 102);
// Set your Gateway IP address
IPAddress gateway(192, 168, 178, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);   //optional
IPAddress secondaryDNS(8, 8, 4, 4); //optional

//mqtt
const char* mqtt_server = "192.168.178.44";
const uint16_t mqtt_port = 1883;
TimerC timer_mqtt_publisher;
#define sampling_time_mqtt_publisher 1000 //ms

TimerC httpGetTimer;
//Ogni httpGetInterval viene fatta la Get per le info sul METEO
const unsigned long httpGetInterval = 30L * 1000L;  //=30000ms = 30s in millisecondi
//API-KEY OTTENUTA DAL SERVIZIO: https://openweathermap.org/api
const String API_KEY = "1ea6759cf4475050fc89dde08ec30abf";
//NOME E COUNTRY CODE DELLA CITTA DA MONITORARE
String CITY_NAME = "milano";
String CITY_NAME_OLD = "milano";
const String COUNTRY_CODE = "it";
bool first_scan=true;
bool aggiorna=false;
bool write_to_sinricpro_first_scan=true;

//indica se attualemte è giorno o notte
fase_del_di_enum fase_del_di = giorno;

SinricProThermostat &myThermostat = SinricPro[THERMOSTAT_ID];

/********* CALLBACK DEL TERMOSTATO ***********/
bool onPowerStateThermostat(const String &deviceId, bool &state) {
  
  if(g_DataHolder.boilerManAuto==false)
  {
    Serial.printf("Thermostat %s turned %s\r\n", deviceId.c_str(), state?"on":"off");
    //manuale
    //i can only switch on/off the thermostat in manual
    //set
    g_DataHolder.boilerManOnOff = state;
    //send to nextion
    if(g_DataHolder.boilerManOnOff)
    {
      myNex.writeNum("page2.swOnOffBoiler.val", 1);
    } 
    else
    {
      myNex.writeNum("page2.swOnOffBoiler.val", 0);
    }
      
  }
  else
  {
    //automatico
    Serial.printf("Thermostat is in Auto, you cannot change its state. Please set thermostat to manual");
    return false;
  }
  return true; // request handled properly
}
bool onTargetTemperatureThermostat(const String &deviceId, float &temperature) {
  Serial.printf("Thermostat %s set temperature to %f\r\n", deviceId.c_str(), temperature);
  g_DataHolder.setpoint_temperature = temperature;
  //send to nextion
  myNex.writeNum("page2.setpoint_temp.val", static_cast<uint32_t>(g_DataHolder.setpoint_temperature*10.0));
  return true;
}
bool onAdjustTargetTemperatureThermostat(const String & deviceId, float &temperatureDelta) {
  g_DataHolder.setpoint_temperature += temperatureDelta;  // calculate absolut temperature
  Serial.printf("Thermostat %s changed temperature about %f to %f", deviceId.c_str(), temperatureDelta, g_DataHolder.setpoint_temperature);
  temperatureDelta = g_DataHolder.setpoint_temperature; // return absolut temperature
  //send to nextion
  myNex.writeNum("page2.setpoint_temp.val", static_cast<uint32_t>(g_DataHolder.setpoint_temperature*10.0));
  return true;
}
bool onThermostatModeThermostat(const String &deviceId, String &mode) {
  Serial.printf("Thermostat %s set to mode %s\r\n", deviceId.c_str(), mode.c_str());
  //TODO
  return true;
}
void setupSinricPro() {
  
  myThermostat.onPowerState(onPowerStateThermostat);
  myThermostat.onTargetTemperature(onTargetTemperatureThermostat);
  myThermostat.onAdjustTargetTemperature(onAdjustTargetTemperatureThermostat);
  myThermostat.onThermostatMode(onThermostatModeThermostat);

  // setup SinricPro
  SinricPro.onConnected([](){ Serial.printf("Connected to SinricPro\r\n"); }); 
  SinricPro.onDisconnected([](){ Serial.printf("Disconnected from SinricPro\r\n"); });
  SinricPro.begin(APP_KEY, APP_SECRET);
}

void setStaticIP()
{
   // Configures static IP address
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }
}

/* MQTT */
//_______________
void connectToWifi()
{
  //Set Static IP
  setStaticIP();
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);
}
void connectToMqtt()
{
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}
void WiFiEvent(WiFiEvent_t event)
{
  switch (event)
  {
#if USING_CORE_ESP32_CORE_V200_PLUS

    case ARDUINO_EVENT_WIFI_READY:
      Serial.println("WiFi ready");
      break;

    case ARDUINO_EVENT_WIFI_STA_START:
      Serial.println("WiFi STA starting");
      break;

    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("WiFi STA connected");
      break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.println("WiFi connected");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      connectToMqtt();
      break;

    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
      Serial.println("WiFi lost IP");
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("WiFi lost connection");
      xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
      xTimerStart(wifiReconnectTimer, 0);
      break;
#else

    case SYSTEM_EVENT_STA_GOT_IP:
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      connectToMqtt();
      break;

    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println("WiFi lost connection");
      xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
      xTimerStart(wifiReconnectTimer, 0);
      break;
#endif

    default:
      break;
  }
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
  uint16_t packetIdSub = mqttClient.subscribe(full_topic_sub_boiler_cmd_onoff.c_str(), 0);
  Serial.print("Subscribing "+full_topic_sub_boiler_cmd_onoff+" at QoS 0, packetId: ");
  Serial.println(packetIdSub);
  packetIdSub = mqttClient.subscribe(full_topic_sub_setpoint_temperature.c_str(), 0);
  Serial.print("Subscribing "+full_topic_sub_setpoint_temperature+" at QoS 0, packetId: ");
  Serial.println(packetIdSub);
  printSeparationLine();
}
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  (void) reason;

  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected())
  {
    xTimerStart(mqttReconnectTimer, 0);
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

  /* SUB setpoint temperature */
  if (String(topic) == full_topic_sub_setpoint_temperature) 
  {
    Serial.print("[MQTT] Changing setpoint temperature to: ");
    float set_temp = messageTemp.toFloat();
    Serial.println(set_temp);
    g_DataHolder.setpoint_temperature = set_temp;
    //send to nextion
    myNex.writeNum("page2.setpoint_temp.val", static_cast<uint32_t>(g_DataHolder.setpoint_temperature*10.0));
    //send to sinricpro
    myThermostat.sendTargetTemperatureEvent(g_DataHolder.setpoint_temperature);
  }

  /* SUB cmd ON-OFF Boiler */
  if (String(topic) == full_topic_sub_boiler_cmd_onoff) 
  {
    if(g_DataHolder.boilerManAuto==false)
    {
      //in manual. we can change the thermostat state
      Serial.print("[MQTT] Changing boiler state to: ");
      if(messageTemp == "on"){
        Serial.println("on");
        //set
        g_DataHolder.boilerManOnOff=true;       
      }
      else if(messageTemp == "off"){
        Serial.println("off");
        //set
        g_DataHolder.boilerManOnOff=false;
      }     
      //send to nextion
      if(g_DataHolder.boilerManOnOff)
      {
        myNex.writeNum("page2.swOnOffBoiler.val", 1);
      } 
      else
      {
        myNex.writeNum("page2.swOnOffBoiler.val", 0);
      }
      //SEND TO SINRICPRO
      myThermostat.sendPowerStateEvent(g_DataHolder.boilerManOnOff);
      
    }
    else
    {
      //automatico
      //DO NOTHING. NO MANULA COMMANDS ARE ALLOWED !!
      Serial.printf("Thermostat is in Auto, you cannot change its state. Please set thermostat to manual");
    }
    
  }


}
void onMqttPublish(const uint16_t& packetId)
{
  Serial.println("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}
//_______________


void setup() {
  Serial.begin(115200);
  delay(300);
  //NEXTION HMI inizializzazione seriale
  myNex.begin(115200);
  delay(300);

  pinMode(PIN_RELE, OUTPUT);
  pinMode(PIN_LED_BOILER_ON, OUTPUT);

  // Initialize device.
  dht.begin();
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  dht.humidity().getSensor(&sensor);
  // Set delay between sensor readings based on sensor details.
  sampling_time_temp_hum = sensor.min_delay / 1000; //ms

  Serial.println(ASYNC_MQTT_GENERIC_VERSION);

  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0,
                                    reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0,
                                    reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));

  WiFi.onEvent(WiFiEvent);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(mqtt_server, mqtt_port);
  connectToWifi();

  //WEBSOCKET
  server.listen(8080);
  Serial.print("Is WS server live? ");
  Serial.println(server.available());

  setupSinricPro();


  timer_sampling_temp_hum.start();
  timer_sampling_read_from_nextion.start();
  timer_sampling_write_to_nextion.start();
  httpGetTimer.start();
  timer_websocket_server.start();
  timer_sampling_write_to_sinricpro.start();
  timer_mqtt_publisher.start();
}

void loop() 
{

  SinricPro.handle();
  
  bool ok_temp_centrale = read_temp_hum_central_station(g_dht_event);

  if(ok_temp_centrale)
  {
    //MQTT PUB
    if(timer_mqtt_publisher.getET()>=sampling_time_mqtt_publisher)
    {
      timer_mqtt_publisher.reset();
      //topic, QoS, retain,payload
      mqttClient.publish(full_topic_pub_temperature.c_str(), 0, false, String(g_DataHolder.central_temperature,1).c_str());
      mqttClient.publish(full_topic_pub_humidity.c_str(), 0, false, String(g_DataHolder.central_humidity,0).c_str());
      mqttClient.publish(full_topic_pub_boiler_actual_onoff.c_str(), 0, false, g_DataHolder.BoilerOutput ? "on": "off");
      mqttClient.publish(full_topic_pub_actual_setpoint_temperature.c_str(), 0, false, String(g_DataHolder.setpoint_temperature,1).c_str());


    }
  }

  if(timer_sampling_write_to_sinricpro.getET()>=sampling_write_to_sinricpro || write_to_sinricpro_first_scan)  
  {
    write_to_sinricpro_first_scan=false;
    timer_sampling_write_to_sinricpro.reset();
    myThermostat.sendTemperatureEvent(g_DataHolder.central_temperature,g_DataHolder.central_humidity);
    myThermostat.sendPowerStateEvent(g_DataHolder.boilerManOnOff);
    myThermostat.sendTargetTemperatureEvent(g_DataHolder.setpoint_temperature);
  }

  bool ok_read_nextion = read_from_nextion_core_logic_boiler();
 // Serial.print("ok_temp_centrale: ");Serial.println(ok_temp_centrale);
  //Serial.print("ok_read_nextion: ");Serial.println(ok_read_nextion);
  if(ok_read_nextion)
  {
    write_to_nextion();
    GetWeatherInfo();
  }

  if(ok_temp_centrale && ok_read_nextion)
  {
    BoilerControlLogic();
  }
  else
  {
    //ERROR
    //TODO: maybe stop Boiler?!?
  }

  //WEBSOCKET
  checkNewWsClientAndMsgs();

  //led on off
  if(g_DataHolder.BoilerOutput)
  {
    //led on
    digitalWrite(PIN_LED_BOILER_ON,true);
  }
  else
  {
    //led off
    digitalWrite(PIN_LED_BOILER_ON,false);
  }


  //FUNZIONE NECESSARIA DA CHIAMARE NEL LOOP PER GESTIONE RICEZIONE DATI DAL NEXTION
  //myNex.NextionListen();

}

bool IsClientAvailabe (WebsocketsClient& client) { return !client.available(); } //se true rimuovi client

// this method goes thrugh every client and polls for new messages and events
void pollAllClients() {
  
  for(auto& client : allWSClients) {
    client.poll();
  }
  //check and remove from list clients not available
  allWSClients.erase( std::remove_if(allWSClients.begin(), allWSClients.end(), IsClientAvailabe), allWSClients.end());
  //Serial.println("N. Connected Clients: "+ String(allWSClients.size()));

}
// this callback is common for all clients, the client that sent that
// message is the one that gets the echo response
void onMessage(WebsocketsClient& client, WebsocketsMessage message) {
  //std::cout << "Got Message: `" << message.data() << "`, Sending Echo." << std::endl;
  //client.send("Echo: " + message.data());
  Serial.println("Msg From Client: "+ message.data());
  String msg = String(message.data());
  if(msg[0]=='!')
  {
    int firstcomma = msg.indexOf(',');
    int secondcomma = msg.indexOf(',',firstcomma+1);
    String client_node_id = msg.substring(1,firstcomma);//remove "!"
    String client_node_temperature = msg.substring(firstcomma+1,secondcomma);
    String client_node_humidity = msg.substring(secondcomma+1);
    //Serial.println(client_node_id);Serial.println(client_node_temperature);Serial.println(client_node_humidity);
    
    bool client_present=false;
    //if array non empty search for a given id if present
    for (auto& node : vecRemoteNodes) {
        if(node.node_id == client_node_id)
        {
          //update node info
          node.node_temperature = client_node_temperature;
          node.node_humidity = client_node_humidity;
          client_present=true;
          break;
        }
    }

    if(!client_present)
    {
      //empty array of client non-present. add new client node info
      RemoteNodeInfo nr;
      nr.node_id = client_node_id;
      nr.node_temperature = client_node_temperature;
      nr.node_humidity = client_node_humidity;
      vecRemoteNodes.push_back(nr);
    }

    //Serial.println("Num Remote Nodes: " + String(vecRemoteNodes.size()));
    

  }
  
}

void checkNewWsClientAndMsgs()
{
  if(timer_websocket_server.getET()>=sampling_time_websocket_server)
  {
    timer_websocket_server.reset();  
    //while the server is alive
    if(server.available())
    {
      
      // if there is a client that wants to connect
      if(server.poll()) {
        //accept the connection and register callback
        Serial.println("Accepting a new client!");
        WebsocketsClient client = server.accept();
        client.onMessage(onMessage);
        allWSClients.push_back(client);
        Serial.println("Num Clients Connected: "+String(allWSClients.size()));

      }
      // check for updates in all clients
      pollAllClients();
    }
  }
}

void isDayOrNight()
{
    DateTime dt_now(g_DataHolder.anno,g_DataHolder.mese,g_DataHolder.giorno,g_DataHolder.ore,g_DataHolder.minuti,g_DataHolder.secondi);
    //determinazione del giorno o notte
    //dopo le 19 è notte
    DateTime soglia_notte(dt_now.year(),dt_now.month(),dt_now.day(),19,0,0);
    //dopo le 6am è giorno
    DateTime soglia_giorno(dt_now.year(),dt_now.month(),dt_now.day(),6,0,0);

    if(dt_now>=soglia_giorno && dt_now<=soglia_notte)
    {
      fase_del_di=giorno;      
    }
    else
    {
      fase_del_di=notte;
    }
}

bool read_temp_hum_central_station(sensors_event_t& dht_event)
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
      //Serial.print(F("Temperature: "));
      //Serial.print(g_dht_event.temperature);
      //Serial.println(F("°C"));
      //TEMPERATURA STAZIONE CENTRALE
      g_DataHolder.central_temperature = dht_event.temperature;
      //myNex.writeStr("Tc.txt", String(dht_event.temperature,1));
    }
    // Get humidity event and print its value.
    dht.humidity().getEvent(&dht_event);
    if (isnan(dht_event.relative_humidity)) {
      //Serial.println(F("Error reading humidity!"));
      return false;
    }
    else {
      //Serial.print(F("Humidity: "));
      //Serial.print(g_dht_event.relative_humidity);
      //Serial.println(F("%"));
      //UMIDITA' STAZIONE CENTRALE
      g_DataHolder.central_humidity = dht_event.relative_humidity;
      //myNex.writeStr("Hc.txt", String(dht_event.relative_humidity,0));
    }
  }
  return true;
}

bool read_from_nextion_core_logic_boiler()
{
  if(timer_sampling_read_from_nextion.getET()>=sampling_time_read_from_nextion)
  {
    timer_sampling_read_from_nextion.reset();

    String NomeCittaInputato = myNex.readStr("page1.InsNomeCitta.txt");
    if(NomeCittaInputato=="ERROR")
    {
      //errore
      return false;
    }
    CITY_NAME_OLD = CITY_NAME;
    CITY_NAME = NomeCittaInputato;
    if(CITY_NAME_OLD!=CITY_NAME)
    {
      aggiorna=true;
      Serial.println("Vecchio nome: "+CITY_NAME_OLD+" Nuovo nome: "+CITY_NAME);
    }

    /* READ DATE-TIME */
    g_DataHolder.anno=myNex.readNumber("rtc0");
    g_DataHolder.mese=myNex.readNumber("rtc1");
    g_DataHolder.giorno=myNex.readNumber("rtc2");
    g_DataHolder.ore=myNex.readNumber("rtc3");
    g_DataHolder.minuti=myNex.readNumber("rtc4");
    g_DataHolder.secondi=myNex.readNumber("rtc5");
    g_DataHolder.dayoftheweek=myNex.readNumber("rtc6");
    if (g_DataHolder.anno==777777 || g_DataHolder.mese==777777 || g_DataHolder.giorno==777777 || g_DataHolder.ore==777777|| g_DataHolder.minuti==777777|| g_DataHolder.secondi==777777 || g_DataHolder.dayoftheweek==777777)
    {
      //errore
      //Serial.println("error");
      return false;
    }

    isDayOrNight();

    //Serial.println(buffer);

    /* READ TIME ZONES */
    //Week
    g_DataHolder.From[1]=myNex.readNumber("page2.h1From.val");
    g_DataHolder.From[2]=myNex.readNumber("page2.h2From.val");
    g_DataHolder.From[3]=myNex.readNumber("page2.h3From.val");
    //g_DataHolder.From[4]=myNex.readNumber("page2.h4From.val");
    g_DataHolder.To[1]=myNex.readNumber("page2.h1To.val");
    g_DataHolder.To[2]=myNex.readNumber("page2.h2To.val");
    g_DataHolder.To[3]=myNex.readNumber("page2.h3To.val");
    g_DataHolder.To[4]=myNex.readNumber("page2.h4To.val");
    //WeekEnd
    g_DataHolder.wFrom[1]=myNex.readNumber("page2.h1wFrom.val");
    g_DataHolder.wFrom[2]=myNex.readNumber("page2.h2wFrom.val");
    g_DataHolder.wFrom[3]=myNex.readNumber("page2.h3wFrom.val");
    //g_DataHolder.wFrom[4]=myNex.readNumber("page2.h4wFrom.val");
    g_DataHolder.wTo[1]=myNex.readNumber("page2.h1wTo.val");
    g_DataHolder.wTo[2]=myNex.readNumber("page2.h2wTo.val");
    g_DataHolder.wTo[3]=myNex.readNumber("page2.h3wTo.val");
    g_DataHolder.wTo[4]=myNex.readNumber("page2.h4wTo.val");
    bool ok =  g_DataHolder.checkErrorReadTimeZones();
    //g_DataHolder.printTimeZones();
    if(!ok)
      return false;

    /* TEMPERATURE SETPOINT */
    uint32_t set_temp =myNex.readNumber("page2.setpoint_temp.val");
    //Serial.print("setpoint temp: ");Serial.println(set_temp);
   // Serial.println(set_temp);
    if (set_temp==777777)
    {
      return false;
    }
    g_DataHolder.setpoint_temperature = set_temp*0.1;

    /* READ IF BOILER IN AUTO O MAN */
    g_DataHolder.boilerManAuto=myNex.readNumber("page2.swAutoMan.val");
    if (g_DataHolder.boilerManAuto==777777)
    {
      return false;
    }
    /* READ BOILER MANUAL CONTROL STATE */
    g_DataHolder.boilerManOnOff=myNex.readNumber("page2.swOnOffBoiler.val");
    if (g_DataHolder.boilerManOnOff==777777)
    {
      return false;
    }

  }


  return true;
    
}

void write_to_nextion()
{
  if(timer_sampling_write_to_nextion.getET()>=sampling_time_write_to_nextion)
  {
    timer_sampling_write_to_nextion.reset();
    myNex.writeStr("page0.solodata.txt",g_DataHolder.getDateOnly());
    myNex.writeStr("page0.soloora.txt",g_DataHolder.getTimeOnly());
    myNex.writeStr("page0.TempInterna.txt",String(g_DataHolder.central_temperature,1));
    myNex.writeStr("page0.HumInside.txt",String(g_DataHolder.central_humidity,0));
    myNex.writeStr("page2.barra_data.txt",g_DataHolder.getDateOnly());
    myNex.writeStr("page2.barra_ora.txt",g_DataHolder.getTimeOnly());
    myNex.writeStr("page2.barra_t_cent.txt",String(g_DataHolder.central_temperature,1));
    myNex.writeStr("page2.barra_h_cent.txt",String(g_DataHolder.central_humidity,0));
    if(g_DataHolder.BoilerOutput)
    {
      myNex.writeNum("page2.barra_stato_ca.pic", 28); //blue led image
      myNex.writeNum("page0.barra_stato_ca.pic", 28); //blue led image
    }    
    else
    {
      myNex.writeNum("page2.barra_stato_ca.pic", 27); //white led image
      myNex.writeNum("page0.barra_stato_ca.pic", 27); //white led image
    }
    //WRITE REMOTE NODES INFO
    for (auto& node : vecRemoteNodes) {
        //myNex.writeStr("page3.node"+node.node_id+"_id.txt",node.node_id);
        myNex.writeStr("page3.node"+node.node_id+"_temp.txt",node.node_temperature);
        myNex.writeStr("page3.node"+node.node_id+"_hum.txt",node.node_humidity);

        //page 1 template bar info
        myNex.writeStr("page0.node"+node.node_id+"_temp.txt",node.node_temperature);
        myNex.writeStr("page0.node"+node.node_id+"_hum.txt",node.node_humidity);
    }
    //central station
    myNex.writeStr("page3.central_temp.txt",String(g_DataHolder.central_temperature,1));
    myNex.writeStr("page3.central_hum.txt",String(g_DataHolder.central_humidity,0));

    
  }
    
}

int TemperatureControl(float hysteresis_value=0.3)
{
  //temperature control based on central unit temperature sensor
 // Serial.print("central_temperature: ");Serial.println(g_DataHolder.central_temperature);
 // Serial.print("set+hist: ");Serial.println(g_DataHolder.setpoint_temperature+hysteresis_value);
 // Serial.print("set-hist: ");Serial.println(g_DataHolder.setpoint_temperature-hysteresis_value);
  if (g_DataHolder.central_temperature > g_DataHolder.setpoint_temperature+hysteresis_value)
  {
    //switch off
    return 0;
  }
  else if (g_DataHolder.central_temperature < g_DataHolder.setpoint_temperature-hysteresis_value)
  {
    return 1;
  }
  
  return -1;
}

void BoilerControlLogic()
{
  if(g_DataHolder.boilerManAuto==0)
  {
    //Boiler in manuale
    if(g_DataHolder.boilerManOnOff==0)
    {
      //manuale OFF
      g_DataHolder.BoilerOutput=false;
    }else
    {
      //manuale ON
      //temperature control based on central unit temperature sensor
      int idcontrol = TemperatureControl(temperature_control_hysteresis);
      if(idcontrol==1)
        g_DataHolder.BoilerOutput=true;
      else if(idcontrol==0)
        g_DataHolder.BoilerOutput=false;

    }
  }else
  {
    bool inzone=false;  
    //Boiler in Automatico
    if(g_DataHolder.dayoftheweek==0 || g_DataHolder.dayoftheweek==6)
    {
      //weekend domenica e sabato
      if(g_DataHolder.ore>=g_DataHolder.wFrom[1] && g_DataHolder.ore<=g_DataHolder.wTo[1])
      {
        //prima fascia del week end
        //g_DataHolder.BoilerOutput=true;
        inzone=true;
      }else if(g_DataHolder.ore>=g_DataHolder.wFrom[2] && g_DataHolder.ore<=g_DataHolder.wTo[2])
      {
       //g_DataHolder.BoilerOutput=true;
       inzone=true;
      }else if(g_DataHolder.ore>=g_DataHolder.wFrom[3] && g_DataHolder.ore<=g_DataHolder.wTo[3])
      {
        //g_DataHolder.BoilerOutput=true;
        inzone=true;
      }
      /*else if(g_DataHolder.ore>=g_DataHolder.wFrom[4] && g_DataHolder.ore<=g_DataHolder.wTo[4])
      {
        //g_DataHolder.BoilerOutput=true;
        inzone=true;
      }*/
      else
      {
        g_DataHolder.BoilerOutput=false;
        inzone=false;
      }
    }else
    {
      //dentro la settimana
      if(g_DataHolder.ore>=g_DataHolder.From[1] && g_DataHolder.ore<=g_DataHolder.To[1])
      {
        //prima fascia del week end
       //g_DataHolder.BoilerOutput=true;
       inzone=true;
      }else if(g_DataHolder.ore>=g_DataHolder.From[2] && g_DataHolder.ore<=g_DataHolder.To[2])
      {
        //g_DataHolder.BoilerOutput=true;
        inzone=true;
      }else if(g_DataHolder.ore>=g_DataHolder.From[3] && g_DataHolder.ore<=g_DataHolder.To[3])
      {
        inzone=true;
        //g_DataHolder.BoilerOutput=true;
      }/*else if(g_DataHolder.ore>=g_DataHolder.From[4] && g_DataHolder.ore<=g_DataHolder.To[4])
      {
        //g_DataHolder.BoilerOutput=true;
        inzone=true;
      }*/
      else
      {
        g_DataHolder.BoilerOutput=false;
        inzone=false;
      }
    }
    if(inzone)
    {
      //temperature control based on central unit temperature sensor
      //g_DataHolder.BoilerOutput=TemperatureControl(temperature_control_hysteresis);
      int idcontrol = TemperatureControl(temperature_control_hysteresis);
      if(idcontrol==1)
        g_DataHolder.BoilerOutput=true;
      else if(idcontrol==0)
        g_DataHolder.BoilerOutput=false;
    }
    else
    {
      g_DataHolder.BoilerOutput=false;
    }
  }

  digitalWrite(PIN_RELE, g_DataHolder.BoilerOutput);
}

bool GetCityLatLon(String NomeCitta, DataHolder& Dati)
{

  //DATO IL NOME CITTA E IL COUNTRY CODE SI OTTENGOLO LAT E LON
  bool ok=false;

  String GET_URI_LATLON_BY_NAME = "GET /geo/1.0/direct?q="+NomeCitta+","+COUNTRY_CODE+"&limit=1&appid="+API_KEY+" HTTP/1.1";//solo la prima città limit=1;
  
  if (wifiClient.connect("api.openweathermap.org", 80)) 
  {
    
    // INVIO RICHIESTA HTTP GET AL SERVER:
    wifiClient.println(GET_URI_LATLON_BY_NAME);
    wifiClient.println("Host: api.openweathermap.org");
    wifiClient.println("Connection: close");
    wifiClient.println();

    // CONTROLLO DELLO STATO HTTP
    char status[32] = {0};
    wifiClient.readBytesUntil('\r', status, sizeof(status));
    // DOVREBBE ESSERE "HTTP/1.0 200 OK" oppure "HTTP/1.1 200 OK"
    if (strcmp(status + 9, "200 OK") != 0) 
    {
      return ok;
    }

    // SALTA GLI HTTP headers
    char endOfHeaders[] = "\r\n\r\n";
    if (!wifiClient.find(endOfHeaders)) 
    {
      //Serial.println(F("Invalid response"));
      return ok;
    }
    //IN RISPOSTA ABBIaMO UNA STRINGA JSON
    //CONVERSIONE DELLA STRINGA NEL OGGETO JSON
    //TRAMITE LA LIBRERIA https://arduinojson.org/v6/assistant/#/step1
    //LE DIMENSIONE DEL JSON (ESEMPIO 3072) sono state calcolate usando il link sopra
    //copiando la risposta json dal sito https://openweathermap.org/current#geo
    DynamicJsonDocument doc(3072);
    DeserializationError error = deserializeJson(doc, wifiClient);
    if (error) {
      //Serial.print(F("deserializeJson() failed: "));
      //Serial.println(error.f_str());
      return ok;
    }

    //UNPACK
    JsonObject root_0 = doc[0];
    const char* root_0_name = root_0["name"]; // "London"
    JsonObject root_0_local_names = root_0["local_names"];
    const char* root_0_local_names_it = root_0_local_names["it"]; // "Londra"

    float root_0_lat = root_0["lat"]; // 51.5085
    float root_0_lon = root_0["lon"]; // -0.1257
    const char* root_0_country = root_0["country"]; // "GB"

    Dati.lat = root_0_lat;
    Dati.lon = root_0_lon;
    Dati.country_id = root_0_country;
    //Dati.nomecitta_it = root_0_local_names_it;
    Dati.nomecitta_en = root_0_name;
    //Serial.println(String(Dati.nomecitta_it)+" lat: "+String(Dati.lat,2)+" lon: "+String(Dati.lon,2)+" "+String(Dati.country_id));

    ok=true;
    return ok;
  } 
  else 
  {
    //CONNESSIONE FALLITA
    return ok;
  }
}

bool updateInfoTempo(DataHolder& Dati)
{

  //DATI LAT E LON SI OTTENGONO LE INFORMAZIONI SUL METEO
  bool ok=false;
  String GET_URI_WEATHER_BY_LATLON = "GET /data/2.5/weather?lat="+String(Dati.lat,2)+"&lon="+String(Dati.lon,2)+"&units=metric&lang=it&appid="+API_KEY+" HTTP/1.1";
  //https://api.openweathermap.org/data/2.5/weather?lat=44.34&lon=10.99&appid={API key}

  
  //se connessione è ok
  if (wifiClient.connect("api.openweathermap.org", 80)) 
  {
    // INVIO RICHIESTA HTTP GET AL SERVER:
    wifiClient.println(GET_URI_WEATHER_BY_LATLON);
    wifiClient.println("Host: api.openweathermap.org");
    wifiClient.println("Connection: close");
    wifiClient.println();

    // CONTROLLO DELLO STATO HTTP
    char status[32] = {0};
    wifiClient.readBytesUntil('\r', status, sizeof(status));
    // It should be "HTTP/1.0 200 OK" or "HTTP/1.1 200 OK"
    if (strcmp(status + 9, "200 OK") != 0) 
    {
      return ok;
    }

    // SALTA GLI HTTP headers
    char endOfHeaders[] = "\r\n\r\n";
    if (!wifiClient.find(endOfHeaders)) 
    {
      //Serial.println(F("Invalid response"));
      return ok;
    }

    //IN RISPOSTA ABBIAMO UNA STRINGA JSON
    //CONVERSIONE DELLA STRINGA NEL OGGETO JSON
    //TRAMITE LA LIBRERIA https://arduinojson.org/v6/assistant/#/step1
    //LE DIMENSIONE DEL JSON (ESEMPIO 1024) sono state calcolate usando il link sopra
    //copiando la risposta json dal sito https://openweathermap.org/current#geo
    StaticJsonDocument<1024> doc;

    DeserializationError error = deserializeJson(doc, wifiClient);
    
    if (error) {
     
      return ok;
    }
    
    JsonObject weather_0 = doc["weather"][0];
    Dati.id_icona_meteo = weather_0["id"]; // 501
    const char* principale_meteo=weather_0["main"];
    const char* descrizione_meteo= weather_0["description"];
    Dati.principale_meteo = principale_meteo; // "Rain"
    Dati.descrizione_meteo = descrizione_meteo; // "moderate rain"
    //const char* weather_0_icon = weather_0["icon"]; // "10d"
    //const char* base = doc["base"]; // "stations"    
    JsonObject main = doc["main"];
    Dati.temperatura_esterna = main["temp"]; // 298.48   
    Dati.temperatura_percepita= main["feels_like"]; // 298.74
    Dati.temperatura_minima = main["temp_min"]; // 297.56
    Dati.temperatura_massima = main["temp_max"]; // 300.05
    Dati.pressione = main["pressure"]; // 1015
    Dati.umidita_esterna= main["humidity"]; // 64
    //int main_sea_level = main["sea_level"]; // 1015
    //int main_grnd_level = main["grnd_level"]; // 933   
    Dati.visibilita = doc["visibility"]; // 10000
    JsonObject wind = doc["wind"];
    Dati.velocita_vento = wind["speed"]; // 0.62
    Dati.direzione_vento = wind["deg"]; // 349
    Dati.nomecitta_it = doc["name"]; // "Zocca"

    //Disconnect
    wifiClient.stop();

    ok=true;
    return ok;
  } 
  else 
  {
    //CONNESSIONE FALLITA
    return ok;
  }
}

void GetWeatherInfo()
{

  if (httpGetTimer.getET()>httpGetInterval || first_scan || aggiorna)
  {
    httpGetTimer.reset();
    first_scan=false;
    aggiorna=false;
    bool ok = GetCityLatLon(CITY_NAME,g_DataHolder);
    if(!ok)
      return;
    ok = updateInfoTempo(g_DataHolder);
    if(!ok)
      return;
    updateDatiNextion(g_DataHolder);
  }
}

bool updateDatiNextion(DataHolder& Dati)
{
 
 //SCRITTURA DEI CAMPI TESTUALI PRESENTI NALLA PAGINA PRINCIPALE DEL NEXTION (PAGE 0)
  //myNex.writeStr("dataora.txt", String(Dati.dt));
  myNex.writeStr("page0.NomeCitta.txt", String(Dati.nomecitta_it));
  myNex.writeStr("page0.DescTempo.txt", String(Dati.descrizione_meteo));
  myNex.writeStr("page0.TempEsterna.txt", String(Dati.temperatura_esterna,1));
  myNex.writeStr("page0.UmiditaEsterna.txt", String(Dati.umidita_esterna,0));
  myNex.writeStr("page0.vel_vento.txt", String(Dati.velocita_vento*3.6,1));//3.6 conversione m/s -> km/h
  
  //CONVERSIONE DEL ANGOLO DEL VENTO [0-360°) IN DIREZIONE TESTUALE
  //IN BASE ALLA DIREZIONE SI SCEGLI L'IMMAGINE DA VISUALIZZARE SUL NEXTION
  String compass = degToCompass8(Dati.direzione_vento);
  if(compass == "N")
  {
    myNex.writeNum("page0.IconaVento.pic", 9);
  }
  else if(compass == "NE")
  {
    myNex.writeNum("page0.IconaVento.pic", 10);
  }
  else if(compass == "E")
  {
    myNex.writeNum("page0.IconaVento.pic", 11);
  }
  else if(compass == "SE")
  {
    myNex.writeNum("page0.IconaVento.pic", 12);
  }
  else if(compass == "S")
  {
    myNex.writeNum("page0.IconaVento.pic", 13);
  }
  else if(compass == "SW")
  {
    myNex.writeNum("page0.IconaVento.pic", 14);
  }
  else if(compass == "W")
  {
    myNex.writeNum("page0.IconaVento.pic", 15);
  }
  else if(compass == "NW")
  {
    myNex.writeNum("page0.IconaVento.pic", 16);
  }

  //IN BASE ALL'ID METEO RICEVUTA DAL SERVIZIO https://openweathermap.org/
  //SI SCEGLIE QUALI ICONA VISUALIZZARE SUL NEXTION
  //LA CONVERSIONE id-IMMAGINE è STATA FATTA USANDO LE TABELLE AL SEGUENTE LINK
  //https://openweathermap.org/weather-conditions#How-to-get-icon-URL
  int id_icona_meteo=0;
  if(Dati.id_icona_meteo == 800)    //SERENO NESSUNA NUVOLA
  {
    
    if(fase_del_di==giorno)
    {
      id_icona_meteo=0;
    }
    else
    {
      id_icona_meteo=2;
    }
    myNex.writeNum("page0.IconaTempo.pic", id_icona_meteo);
  }
  else if (Dati.id_icona_meteo == 801)
  {
    //SOLE+NUVOLE
    if(fase_del_di==giorno)
    {
      id_icona_meteo=1;
    }
    else
    {
      id_icona_meteo=3;
    }
    myNex.writeNum("page0.IconaTempo.pic", id_icona_meteo);
  }
  else
  {
    switch(Dati.id_icona_meteo/100)//divisione fra interi
    {
      case 2:     //TEMPESTA
          id_icona_meteo=6;
          myNex.writeNum("page0.IconaTempo.pic", id_icona_meteo);
          break;
  
      case 3:     //PIOGGERELLINA
          id_icona_meteo=4;
          myNex.writeNum("page0.IconaTempo.pic", id_icona_meteo);
          break;
      case 5:     //PIOGGIA
          id_icona_meteo=5;
          myNex.writeNum("page0.IconaTempo.pic", id_icona_meteo);
          break;

      case 6:    //NEVE
          id_icona_meteo=7;
          myNex.writeNum("page0.IconaTempo.pic", id_icona_meteo);
          break;
  
      case 7:     //NEBBIA
          id_icona_meteo=8;               
          myNex.writeNum("page0.IconaTempo.pic", id_icona_meteo);
          break;
      case 8:     //NUVOLOSO
          id_icona_meteo=21;
          myNex.writeNum("page0.IconaTempo.pic", id_icona_meteo);
          break;
    }    
  }
  return true;
}
