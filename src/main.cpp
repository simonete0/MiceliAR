#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include <DHT.h>
#include "FirebaseDATOS.h"
// Configuración WiFi
#define WIFI_SSID "WAB 2.4"
#define WIFI_PASSWORD "NOLAESCRIBAS"
// Configuración Firebase
#define FIREBASE_URL "https://reinomicelio-e4a69-default-rtdb.firebaseio.com/"
#define FIREBASE_API_KEY "AIzaSyBoaIXH4BWwvUB1VAkPqkI3DZ9SgK0b728"
#define FIREBASE_PROJECT_ID "reinomicelio-e4a69"
#define FIREBASE_EMAIL "admin@admin.com"
#define FIREBASE_PASSWORD "admin1234"
FirebaseDatos firebase;
//Configuracion del sensor DHT11
#define DHTPIN 5     // GPIO5 (D1)
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);
  //WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  //Serial.print("Conectando a WiFi");
  //while (WiFi.status() != WL_CONNECTED) {
  //  Serial.print(".");
  //  delay(300);
  //}
  //Serial.println("\nWiFi conectado");
  //Serial.print("IP: "); Serial.println(WiFi.localIP());
  //Inicio configuracion firebase
  // firebase.begin(WIFI_SSID, WIFI_PASSWORD, 
  //                FIREBASE_API_KEY, FIREBASE_URL,
  //                FIREBASE_EMAIL, FIREBASE_PASSWORD);

  Serial.println("Iniciando prueba del DHT11...");
  dht.begin();
}

void loop() {
  delay(2000);  // El DHT11 requiere 2 segundos entre lecturas

  float humedad = dht.readHumidity();
  float temperatura = dht.readTemperature();

  if (isnan(humedad) || isnan(temperatura)) {
    Serial.println("¡Error al leer el sensor!");
    return;
  }

  Serial.print("Humedad: ");
  Serial.print(humedad);
  Serial.print("%\t");
  Serial.print("Temperatura: ");
  Serial.print(temperatura);
  Serial.println("°C");

  if(firebase.sendData(temperatura, humedad)) {
        Serial.println("Datos enviados a Firebase");
    } else {
        Serial.println("No hubo cambios significativos");
    }
}