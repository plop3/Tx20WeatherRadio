/*  Streaming functions for the TX20 anemometer measuring wind speed and
    direction:
    https://www.john.geek.nz/2011/07/la-crosse-tx20-anemometer-communication-protocol/
    https://github.com/bunnyhu/ESP8266_TX20_wind_sensor
        The wind speed is measured in m/s, the direction is measured in deg, i.e.
    N = 0 deg, E = 90 deg etc.
    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include <PubSubClient.h>
WiFiClient espClient;
PubSubClient MQTTclient(espClient);

//void initAnemometer();
//void reconnect();

float windSpeed=0;
int windDir=0;

struct {
  bool status;
  int direction;
  float avgSpeed;
  float minSpeed;
  float maxSpeed;
} anemometerData = {false,  0, 0.0, 0.0, 0.0};

// intermediate values to translate #rotations into wind speed
float minSpeed;              // minimal wind speed since startTime
float maxSpeed;              // maximal wind speed since startTime

void reset(unsigned long time) {
  maxSpeed       = 0.0;
  minSpeed       = 0.0;
}


/**
   Update anemometer counters
*/
void updateAnemometer() {
  if (anemometerData.status) {
    if (!MQTTclient.connected()) {
        reconnect();
    }
    MQTTclient.loop();
  }
  else
    initAnemometer();
}

/**
   Read out the anemometer data and reset the counters
*/
void readAnemometer() {
  updateAnemometer();
  anemometerData.avgSpeed = windSpeed;
  anemometerData.minSpeed = minSpeed;
  anemometerData.maxSpeed = maxSpeed;
   anemometerData.direction = windDir;
  reset(millis());
}

void serializeAnemometer(JsonObject &doc) {

  JsonObject data = doc.createNestedObject("TX20 Anemometer");
  data["init"] = anemometerData.status;

  if (anemometerData.status) {
    data["direction"] = anemometerData.direction;
    data["avg speed"] = anemometerData.avgSpeed;
    data["min speed"] = anemometerData.minSpeed;
    data["max speed"] = anemometerData.maxSpeed;
  }
}

String displayAnemometerParameters() {
  if (anemometerData.status == false) return "";
  
  String result = " direction: " + String(anemometerData.direction, DEC) + "\n avg speed: " + String(anemometerData.avgSpeed, 1);
  result += "\n min speed: " + String(anemometerData.minSpeed, 1) + "\n" + "\n max speed: " + String(anemometerData.maxSpeed, 1);
  return result;
}

void reconnect() {
  // Loop until we're reconnected
  if (!MQTTclient.connected()) {
    //Serial.print("Attempting MQTT connection...");
    if (MQTTclient.connect("WeatherRadio", MQTT_USER, MQTT_PASSWD)) {
      //Serial.println("connected");
      MQTTclient.publish("weatherradio","Bonjour");
      MQTTclient.subscribe("anemometre/#");
    }
    else {
      //Serial.print("failed, rc=");
      //Serial.print(MQTTclient.state());
      //Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(2000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
	payload[length] = '\0';
	if (strcmp(topic,"windspeed") == 0 {
		windSpeed = atof((char *)payload);
	}
	if (strcmp(topic,"gust") == 0 {
		maxSpeed = atof((char *)payload);
	}
	if (strcmp(topic,"min") == 0 {
		minSpeed = atof((char *)payload);
	}
	if (strcmp(topic,"winddir") == 0 {
		windDir = atoi((char *)payload);
	}
}

void initAnemometer() {
  MQTTclient.setServer(MQTT_ADDRESS, 1883);
  MQTTclient.setCallback(callback);
  reconnect();
  anemometerData.status = true;
  // reset measuring data
  reset(millis());
}
