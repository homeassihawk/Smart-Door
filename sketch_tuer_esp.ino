////////////////////////////////////////////////
///                 EXPLANATION              ///
////////////////////////////////////////////////
/*
Red cable from the doorbell connects to Pin 5 on the ESP32 (GPIO 46). Brown wire connects to ground pin.

Doorbell 
  - Red     -> 3.3 V from LED
  - Brown   -> Ground (Blinking)
  - Blue    -> Telephone icon button
  - Yellow  -> Key icon button 
*/


// For a connection via I2C using the Arduino Wire include:
#include <Wire.h>               
#include "HT_SSD1306Wire.h"
#include "images.h"

// For the MQTT implementation
#include<Arduino.h>
#include<WiFi.h>
#include<PubSubClient.h>
#include<mat.h>
#include<HaMqttEntities.h>

// GPIO
#include "driver/gpio.h"

#define PIN5 46 // Input <- LED +
#define PIN6 45 // Output -> Telephone button / Accept doorbell
#define PIN7 42 // Output -> Key button / Open door
#define GND  41 // Switchable GROUND


////////////////////////////////////////////////
///           MQTT AND WIFI SETUP            ///
////////////////////////////////////////////////

#define WIFI_SSID     "nub022025"
#define WIFI_PASSWORD "nub022025"
#define MQTT_SERVER   "10.108.2.179"
#define MQTT_PORT     1883
#define MQTT_USER     "MQTTUser"
#define MQTT_PASSWORD "admin"

WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);


////////////////////////////////////////////////
///          HOMEASSISTANT PARTS             ///
////////////////////////////////////////////////

// HA Parts
#define ENTITIES_COUNT 2
#define HA_DEVICE_ID   "SmartDoor"
#define HA_DEVICE_FRIENDLY_NAME "SmartDoor"


////////////////////////////////////////////////
///             SENSORS FOR MQTT             ///
////////////////////////////////////////////////

HADevice ha_device = HADevice(HA_DEVICE_ID,HA_DEVICE_FRIENDLY_NAME,"1.0");

HASensorBinary ha_sensor_doorbell = HASensorBinary("sensor_doorbell", "Doorbell", ha_device);
HAButton ha_door_button = HAButton("door_button", "Door button", ha_device);

void ha_callback(HAEntity *entity, char *topic, byte *payload, unsigned int length);

////////////////////////////////////////////////
///           Variables                      ///
////////////////////////////////////////////////

static SSD1306Wire  display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED); // addr , freq , i2c group , resolution , rst

int pin46 = 0;
bool doorbell_triggered = false;
bool last_doorbell_state = LOW;
bool doorbell_state = LOW;
unsigned long lastLowTime = 0;
const unsigned long resetTime = 5000; // 5 sekunden
bool open_door_state = false;


////////////////////////////////////////////////
///                 ESP32 SETUP              ///
////////////////////////////////////////////////

void setup() {
  pinMode(PIN5, INPUT);
  pinMode(PIN6, OUTPUT);
  pinMode(PIN7, OUTPUT);

  digitalWrite(PIN6, LOW);
  digitalWrite(PIN7, LOW);

  // MQTT Setup
  mqtt_client.setServer(MQTT_SERVER, MQTT_PORT);

  HAMQTT.begin(mqtt_client,ENTITIES_COUNT);
  HAMQTT.addEntity(ha_sensor_doorbell);
  HAMQTT.addEntity(ha_door_button);
  HAMQTT.setCallback(ha_callback);

  // WiFi Setup 
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.begin(115200);
  Serial.println();
  Serial.println();
  VextON();
  delay(100);

  // Initialising the UI will init the display too.
  display.init();
}

void openDoor() 
{
  // Answer door
  digitalWrite(PIN6, HIGH);
  delay(1000);
  digitalWrite(PIN6, LOW);

  delay(3000);

  // Open door
  digitalWrite(PIN7, HIGH);
  delay(1000);
  digitalWrite(PIN7, LOW);

  // Cancel call
  digitalWrite(PIN6, HIGH);
  delay(1000);
  digitalWrite(PIN6, LOW);

  // Reset
  open_door_state = false;
}

void drawHelloNotice() 
{
    // Font Demo1
    // create more fonts at http://oleddisplay.squix.ch/
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_16);
    display.drawString(60, 20, "Hello :)");
}

void drawDingDong() 
{
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(60, 20, "Ding Dong!");
}

void drawOpeningDoor()
{
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(60, 20, "Opening door.");
}

void VextON(void)
{
  pinMode(Vext,OUTPUT);
  digitalWrite(Vext, LOW);
}

void VextOFF(void) //Vext default OFF
{
  pinMode(Vext,OUTPUT);
  digitalWrite(Vext, HIGH);
}


////////////////////////////////////////////////
///                 ESP32 LOOP               ///
////////////////////////////////////////////////

void loop() {
  HAMQTT.loop();

  if(!HAMQTT.connected() && !HAMQTT.connect(HA_DEVICE_ID,MQTT_USER,MQTT_PASSWORD)) 
    {
      Serial.println("HAMQTT not connected");
      delay(1000);
    }

  doorbell_state = digitalRead(PIN5);
  //Serial.println("doorbell_state : ");
  //Serial.println(pin46);

  unsigned long now = millis();

  if (doorbell_state == LOW && last_doorbell_state == HIGH && !doorbell_triggered) 
  {
    Serial.println("Doorbell!");
    doorbell_triggered = true;
    ha_sensor_doorbell.setState(doorbell_triggered);
    //Serial.println("Set ha_sensor_doorbell to : ");
    //Serial.println(doorbell_triggered);
  }

  if (doorbell_state == HIGH) 
  {
    if (last_doorbell_state == LOW) 
    {
      lastLowTime = now;
     // Serial.println("Timer started..");
    }
    if (now - lastLowTime > resetTime) 
    {
      doorbell_triggered = false;
      ha_sensor_doorbell.setState(doorbell_triggered);
    }
  }

if (last_doorbell_state != doorbell_state) 
{
  last_doorbell_state = doorbell_state;
}
  
  // clear the display
  display.clear();

  if (doorbell_triggered == false) 
  {
    drawHelloNotice();
  } 
  else 
  {
    drawDingDong();
  }

  if (open_door_state == true) 
  {
    openDoor();
    drawOpeningDoor();
  }

  // write the buffer to the display
  display.display();

  delay(10);
}

void ha_callback(HAEntity *entity, char *topic, byte *payload,
    unsigned int length) {

    if(entity == &ha_door_button)
    {
      open_door_state = !open_door_state;
    }

}