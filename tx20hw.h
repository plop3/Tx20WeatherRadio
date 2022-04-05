/*
	Station météo ESP8266 Anémomètre/girouette TX20
  Envoi des données par MQTT
	Serge CLAUS
	GPL V3
	Version 4.0
	12/03/2020
  03/04/2022

*/

//----------------------------------------
#define DATAPIN     D7  // Anémomètre Lacrosse TX20 (GND, 3.3V, Data)
#define INTERV      60  // Intervalle d'envoi des données anémomètre (Intervalle=INTERV*2)
#define WINDMIN     10  // Vitesse minimum du vent en m/s*10 pour prise en compte de la girouette 

//OTA
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// MQTT
#include <PubSubClient.h>
WiFiClient espClient;
PubSubClient client(espClient);

//#include "WiFiP.h"



//----------------------------------------

// TX20 anémomètre
volatile boolean TX20IncomingData = false;
unsigned char chk;
unsigned char sa, sb, sd, se;
unsigned int sc, sf, pin;
String tx20RawDataS = "";
unsigned int WindMoy = 0;      // Vitesse moyenne du vent 
unsigned int WindMax = 0;   // Vitess maximum du vent (rafales)
unsigned int WindMin = 999; // Vitesse minimum du vent
unsigned int DirTab[17];    // Tableau des directions de la girouette (16 éléments)

String DirT[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
int NbMesures = 0;  // Nombre de mesures
int NbDir =0 ;      // Nombre de mesures de direction valides (vitesse du vent > 1 m/s)

//----------------------------------------

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  // OTA
  WiFi.mode(WIFI_STA);
  IPAddress ip(192, 168, 0, 203);
  IPAddress subnet(255, 255, 255, 0);
  IPAddress dns(192, 168, 0, 254);
  IPAddress gateway(192, 168, 0, 254);
  WiFi.config(ip, gateway, subnet, dns);
  WiFi.begin(ssid, password);
  byte i = 5;
  while ((WiFi.waitForConnectResult() != WL_CONNECTED) && (i > 0)) {
    Serial.println("Connection Failed...");
    delay(5000);
    //i--; // La station doit être connectée pour fonctionner
  }
  ArduinoOTA.setHostname("anemometre");
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
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
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // TX20 anémomètre
  pinMode(DATAPIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(DATAPIN), isTX20Rising, RISING);

  // MQTT
  client.setServer(mqtt_server, 1883);
}

//----------------------------------------

void loop() {
  // OTA
  ArduinoOTA.handle();
  // TX20 anémomètre
  // Des données sont présentes ?
  if (TX20IncomingData) {
    // Données OK ?
    if (readTX20()) {
      // Data OK
      WindMoy += sf;                     // Ajout de la mesure du vent (calcul de la moyenne)
      if (sf > WindMax) WindMax=sf;   // Vitesse maximum
      if (sf < WindMin) WindMin=sf;   // Vitesse minimun
      if (sf > WINDMIN) {             // Vitesse du vent suffisante pour prise en compte de la girouette
        DirTab[sb]++;                 // Stockage de la direction actuelle
        NbDir++;                      // Incrément du nombre de mesures valides pour la girouette
    }
      NbMesures++;                    // Incrément du nombre de mesures du vent effectuées
      if (NbMesures >= INTERV) {      // Nombre de mesures atteint ?
        envoiMQTT();                  // Envoi des mesures
        for (int i=0;i<16;i++) {      // Réinitialisation du tableau de la girouette
          DirTab[i]=0;
        }
        WindMoy=0;                     // Initialisation des mesures
        WindMax=0;
        WindMin=999;
        NbMesures=0;
        NbDir=0;
      }
    }
    if (!client.connected()) {        // MQTT
      reconnect();
    }
    client.loop();
  }
}


ICACHE_RAM_ATTR void isTX20Rising() {
  // Interruption, arrivée d'une mesure
  if (!TX20IncomingData) {
    TX20IncomingData = true;
  }
}

boolean readTX20() {
  // Lecture des données de l'anémomètre
  int bitcount = 0;

  sa = sb = sd = se = 0;
  sc = 0; sf = 0;
  tx20RawDataS = "";

  for (bitcount = 41; bitcount > 0; bitcount--) {
    pin = (digitalRead(DATAPIN));
    if (!pin) {
      tx20RawDataS += "1";
    } else {
      tx20RawDataS += "0";
    }
    if ((bitcount == 41 - 4) || (bitcount == 41 - 8) || (bitcount == 41 - 20)  || (bitcount == 41 - 24)  || (bitcount == 41 - 28)) {
      tx20RawDataS += " ";
    }
    if (bitcount > 41 - 5) {
      // start, inverted
      sa = (sa << 1) | (pin ^ 1);
    } else if (bitcount > 41 - 5 - 4) {
      // wind dir, inverted
      sb = sb >> 1 | ((pin ^ 1) << 3);
    } else if (bitcount > 41 - 5 - 4 - 12) {
      // windspeed, inverted
      sc = sc >> 1 | ((pin ^ 1) << 11);
    } else if (bitcount > 41 - 5 - 4 - 12 - 4) {
      // checksum, inverted
      sd = sd >> 1 | ((pin ^ 1) << 3);
    } else if (bitcount > 41 - 5 - 4 - 12 - 4 - 4) {
      // wind dir
      se = se >> 1 | (pin << 3);
    } else {
      // windspeed
      sf = sf >> 1 | (pin << 11);
    }

    delayMicroseconds(1220);
  }
  chk = ( sb + (sc & 0xf) + ((sc >> 4) & 0xf) + ((sc >> 8) & 0xf) ); chk &= 0xf;
  delayMicroseconds(2000);  // just in case
  TX20IncomingData = false;

  if (sa == 4 && sb == se && sc == sf && sd == chk) {
    return true;
  } else {
    return false;
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    // If you do not want to use a username and password, change next line to
    // if (client.connect("Anemometre")) {
    if (client.connect("Anemometre", mqtt_user, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void envoiMQTT() {
  //Serial.println("Envoi données");
  // Anémomètre
  ///  http.begin(client, "http://192.168.0.4:8082/json.htm?type=command&param=udevice&idx=11&nvalue=0&svalue=" + String(Dir) + ";" + DirT[DirS] + ";" + String(Wind) + ";" + String(Gust) + ";" + String(Tp) + ";" + String(WindChild));
  client.publish("anemometre/windspeed", String(WindMoy/10.0/NbMesures).c_str(), true); // Vitesse moyenne du vent
  client.publish("anemometre/gust", String(WindMax/10.0).c_str(), true);                // Rafales
  client.publish("anemometre/min", String(WindMin/10.0).c_str(), true);                 // Vitesse mini du vent
  // Recherche de la direction privilégiée
  if (NbDir) {                    // Au moins une mesure de la girouette valide ?
  int priv, nbpriv=0;             // priv: Recherche de l'orientation priviligiée. nbpriv: Orientation priviligiée
    for (int i=0; i<16; i++) {
      if (DirTab[i]> priv) {      // Direction priviligiée ?
        priv=DirTab[i];           // On enregistre la direction priviligiée (nombre d'occurences)
      nbpriv=i;                   // Direction priviligiée actuellement
      }

    }
    // Si dess données girouette sont valides, envoi au broker MQTT
    client.publish("anemometre/winddir", String(nbpriv*22.5).c_str(), true);
    client.publish("anemometre/winddirs", DirT[nbpriv].c_str(), true);
  }
}
