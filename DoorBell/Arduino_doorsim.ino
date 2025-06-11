#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Configuración de WiFi
const char* ssid = "Fablab_Torino";
const char* password = "Fablab.Torino!";

// Configuración de MQTT
const char* mqtt_server = "172.26.34.36";
const char* mqtt_topic_door1 = "Door1_topic";
const char* mqtt_username = "tu_usuario";
const char* mqtt_password = "tu_contraseña";

WiFiClient espClient;
PubSubClient client(espClient);

// Variables para el LED
bool ledStateDoor1 = false;
unsigned long ledOnTimeDoor1 = 0;

// Datos del timbre
const char* hostname = "FABLAB-MAKEIT";

// Configuración del relé
const int relayPin = 4;
unsigned long relayOnTime = 0;

void setup() {
  Serial.begin(9600);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando a WiFi...");
  }
  Serial.println("Conectado a WiFi");

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  while (!client.connected()) {
    Serial.println("Conectando a MQTT...");
    if (client.connect("ArduinoClient", mqtt_username, mqtt_password)) {
      Serial.println("Conectado a MQTT");
      client.subscribe(mqtt_topic_door1);
    } else {
      Serial.print("Error al conectar: ");
      Serial.print(client.state());
      delay(2000);
    }
  }

  pinMode(13, OUTPUT);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  controlLED(13, ledStateDoor1, ledOnTimeDoor1);

  if (millis() - relayOnTime >= 2000 && relayOnTime > 0) {
    digitalWrite(relayPin, LOW);
    Serial.println("Relé apagado");
    relayOnTime = 0;
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensaje recibido [");
  Serial.print(topic);
  Serial.print("] ");

  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  // Verifica que el topic sea el correcto
  if (strcmp(topic, mqtt_topic_door1) != 0) {
    return;
  }

  // Parsear el JSON recibido
  StaticJsonDocument<300> jsonDoc;
  DeserializationError error = deserializeJson(jsonDoc, message);

  if (error) {
    Serial.print("Error al deserializar JSON: ");
    Serial.println(error.f_str());
    return;
  }

  // Extraer campos clave
  const char* cmd = jsonDoc["cmd"];
  const char* desc = jsonDoc["desc"];

  if (cmd && desc && strcmp(cmd, "event") == 0 && strcmp(desc, "Doorbell ringing") == 0) {
    ledStateDoor1 = true;
    ledOnTimeDoor1 = millis();
    sendNotificationToPublisher("Door 1 opened");

    digitalWrite(relayPin, HIGH);
    Serial.println("Relé activado");
    relayOnTime = millis();
  } else {
    Serial.println("Mensaje recibido, pero no cumple condiciones");
  }
}

void sendNotificationToPublisher(const char* message) {
  StaticJsonDocument<200> jsonDoc;

  jsonDoc["type"] = "INFO";
  jsonDoc["message"] = message;

  String jsonString;
  serializeJson(jsonDoc, jsonString);

  client.publish("door/notifications", jsonString.c_str());
}

void reconnect() {
  while (!client.connected()) {
    Serial.println("Reconectando a MQTT...");
    if (client.connect("ArduinoClient", mqtt_username, mqtt_password)) {
      Serial.println("Conectado");
      client.subscribe(mqtt_topic_door1);
    } else {
      Serial.print("Error al reconectar: ");
      Serial.print(client.state());
      delay(5000);
    }
  }
}

void controlLED(int ledPin, bool& ledState, unsigned long& ledOnTime) {
  if (ledState && (millis() - ledOnTime >= 5000)) {
    ledState = false;
  }
  digitalWrite(ledPin, ledState);
}

