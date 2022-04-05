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

void initAnemometer();
void reconnect();

float windSpeed=0;
float windDir=0;

struct {
  bool status;
  int direction;
  float avgSpeed;
  float minSpeed;
  float maxSpeed;
} anemometerData = {false,  0, 0.0, 0.0, 0.0};

// intermediate values to translate #rotations into wind speed
volatile float minSpeed;              // minimal wind speed since startTime
volatile float maxSpeed;              // maximal wind speed since startTime

// intermediate values to calculate an average wind direction
volatile float initial_direction; // remember the first direction measured
volatile float direction_diffs;   // and collect the diffs to build the average
volatile unsigned long startTime;     // overall start time for calculating the wind speed


// calculate the windspeed
float windspeed(unsigned long time, unsigned long startTime, unsigned int rotations) {

  // 1600 rotations per hour or 2.25 seconds per rotation
  // equals 1 mp/h wind speed (1 mp/h = 1609/3600 m/s)
  // speed (m/s) = rotations * 1135.24 / delta t

  if (time == startTime)
    return 0.0;
  else
    return (windSpeed);
}

// calculate the wind direction in degree (N = 0, E = 90, ...)
int winddirection() {
  // the wind direction is measured with a potentiometer
  volatile int direction = windDir;

  // ensure 0 <= direction <360
  if (direction >= 360)
    direction -= 360;
  else if (direction < 0)
    direction += 360;

  return direction;
}


void reset(unsigned long time) {
  startTime      = time;
  maxSpeed       = 0.0;
  minSpeed       = 9999.0;
  initial_direction = winddirection();
  direction_diffs = 0;
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
      volatile float speed = windSpeed;

      // update min and max values
      minSpeed = speed < minSpeed ? speed : minSpeed;
      maxSpeed = speed > maxSpeed ? speed : maxSpeed;

      // calculate the difference in the wind direction
      volatile int current_direction = winddirection();
      volatile int diff = initial_direction - current_direction;
      // ensure that the diff is in the range -180 < diff <= 180
      if (diff > 180) diff -= 360;
      if (diff <= -180) diff += 360;
        direction_diffs += diff;
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
  anemometerData.minSpeed = min(minSpeed, maxSpeed);
  anemometerData.maxSpeed = maxSpeed;
  /*
  if (slices > 0)
    anemometerData.direction = round(initial_direction - (direction_diffs / slices));
  else
    anemometerData.direction = initial_direction;
    */
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
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String topicStr(topic);
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  Serial.println(topicStr);

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is active low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }

}

void initAnemometer() {
  MQTTclient.setServer(MQTT_ADDRESS, 1883);
  //PubSubClient client(server, 1883, callback, ethClient);
  MQTTclient.setCallback(callback);
  anemometerData.status = true;
  // reset measuring data
  reset(millis());
}