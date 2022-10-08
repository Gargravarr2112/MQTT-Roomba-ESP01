//This version is for the Roomba 600 Series
//Connect a wire from D4 on the nodeMCU to the BRC pin on the roomba to prevent sleep mode.



#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <SimpleTimer.h>
#include <Roomba.h>


//USER CONFIGURED SECTION START//
const char* ssid = "QNetR";
const char* password = "getoffmylawn";
const char* mqtt_server = "192.168.11.10";
const int mqtt_port = 1883;
const char *mqtt_user = "mqtt";
const char *mqtt_pass = "0xT4xuB3Si0xtx7S7x1wtAdfjdGCh7o7";
const char *mqtt_client_name = "wheatley"; // Client connections can't have the same connection name
//USER CONFIGURED SECTION END//


WiFiClient espClient;
PubSubClient client(espClient);
SimpleTimer timer;
Roomba roomba(&Serial, Roomba::Baud115200);


// Variables
bool toggle = true;
const int noSleepPin = 2;
bool boot = true;
long battery_Current_mAh = 0;
long battery_Voltage = 0;
long battery_Total_mAh = 0;
long battery_percent = 0;
int chargingStatus = 0;
char battery_percent_send[50];
char battery_Current_mAh_send[50];
uint8_t tempBuf[10];
String status = "Idle";
char statusBuf[14];

//Functions

void setup_wifi() 
{
  WiFi.hostname(mqtt_client_name);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
  }
}

void reconnect() 
{
  // Loop until we're reconnected
  int retries = 0;
  while (!client.connected()) 
  {
    if(retries < 50)
    {
      // Attempt to connect
      if (client.connect(mqtt_client_name, mqtt_user, mqtt_pass, "roomba/status", 0, 0, "Dead Somewhere")) 
      {
        // Once connected, publish an announcement...
        if(boot)
        {
          boot = false;
          status = "Rebooted";
        }
        else
        {
          status = "Reconnected";
        }
        status.toCharArray(statusBuf, 14);
        client.publish("roomba/status", statusBuf);
        // ... and resubscribe
        client.subscribe("roomba/commands");
      } 
      else 
      {
        retries++;
        // Wait 5 seconds before retrying
        delay(5000);
      }
    }
    if(retries >= 50)
    {
      ESP.restart();
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) 
{
  String newTopic = topic;
  payload[length] = '\0';
  String newPayload = String((char *)payload);
  if (newTopic == "roomba/commands") 
  {
    if (newPayload == "start")
    {
      startCleaning();
    }
    else if (newPayload == "stop")
    {
      stopCleaning();
    }
    else if (newPayload == "status")
    {
      sendInfoRoomba();
    }
  }
}

void startCleaning()
{
  wakeUp();
  Serial.write(128);
  delay(50);
  Serial.write(131);
  delay(50);
  Serial.write(135);
  status = "Cleaning";
  status.toCharArray(statusBuf, 14);
  client.publish("roomba/status", statusBuf);
}

void stopCleaning()
{
  wakeUp();
  Serial.write(128);
  delay(50);
  Serial.write(131);
  delay(50);
  Serial.write(143);
  status.toCharArray(statusBuf, 14);
  client.publish("roomba/status", statusBuf);
}

void sendInfoRoomba()
{
  roomba.start(); 
  roomba.getSensors(21, tempBuf, 1);
  chargingStatus = tempBuf[0];
  delay(50);
  if (chargingStatus == 5)
  {
    status = "Charger Fault";
  }
  else if (chargingStatus > 0) //0 = Not Charging
  {
    status = "Charging";
  }
  roomba.getSensors(22, tempBuf, 1);
  battery_Voltage = tempBuf[0];
  delay(50);
  roomba.getSensors(25, tempBuf, 2);
  battery_Current_mAh = tempBuf[1]+256*tempBuf[0];
  delay(50);
  roomba.getSensors(26, tempBuf, 2);
  battery_Total_mAh = tempBuf[1]+256*tempBuf[0];
  if(battery_Total_mAh != 0)
  {
    int nBatPcent = 100*battery_Current_mAh/battery_Total_mAh;
    String temp_str2 = String(nBatPcent);
    temp_str2.toCharArray(battery_percent_send, temp_str2.length() + 1); //packaging up the data to publish to mqtt
    client.publish("roomba/battery", battery_percent_send);
  }
  if(battery_Total_mAh == 0)
  {  
    client.publish("roomba/battery", "NO DATA");
  }
  String temp_str = String(battery_Voltage);
  temp_str.toCharArray(battery_Current_mAh_send, temp_str.length() + 1); //packaging up the data to publish to mqtt
  client.publish("roomba/charge", battery_Current_mAh_send);
  status.toCharArray(statusBuf, 14);
  client.publish("roomba/status", statusBuf);
}

void wakeUp()
{
  digitalWrite(noSleepPin, LOW);
  delay(1000);
  digitalWrite(noSleepPin, HIGH);
  delay(1000);
}

void setup() 
{
  pinMode(noSleepPin, OUTPUT);
  digitalWrite(noSleepPin, HIGH);
  Serial.begin(115200);
  Serial.write(129); //Start OI
  delay(50);
  Serial.write(11); //Baud rate to 115200
  delay(50);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  timer.setInterval(5000, sendInfoRoomba);
}

void loop() 
{
  if (!client.connected()) 
  {
    reconnect();
  }
  client.loop();
  timer.run();
}