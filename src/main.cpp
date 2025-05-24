#include <Arduino.h>
#include "FirebaseDATOS.h"
#include "sensorDHT11.h"

// Configuraciones
#define WIFI_SSID "WAB 2.4"
#define WIFI_PASSWORD "NOLAESCRIBAS"
#define FIREBASE_URL "https://reinomicelio-e4a69-default-rtdb.firebaseio.com/"
#define FIREBASE_API_KEY "AIzaSyBoaIXH4BWwvUB1VAkPqkI3DZ9SgK0b728"
#define FIREBASE_EMAIL "admin@admin.com"
#define FIREBASE_PASSWORD "admin1234"
#define DHTPIN 5 // GPIO5 (D1)

FirebaseDatos firebase;
sensorDHT dhtSensor(DHTPIN);

void setup() {
    Serial.begin(115200);
    dhtSensor.iniciar();
    
    firebase.begin(WIFI_SSID, WIFI_PASSWORD, 
                  FIREBASE_API_KEY, FIREBASE_URL,
                  FIREBASE_EMAIL, FIREBASE_PASSWORD);
}

void loop() {
    delay(4000); // Espera entre lecturas
    
    dhtSensor.leerValores();
    float temperatura = dhtSensor.getTemperatura();
    float humedad = dhtSensor.getHumedad();

    if (isnan(humedad) || isnan(temperatura)) {
        Serial.println("Error en lectura del sensor");
        return;
    }

    Serial.printf("Temperatura: %.1f°C, Humedad: %.1f%%\n", temperatura, humedad);

    if(firebase.sendData(temperatura, humedad)) {
        Serial.println("Datos enviados a Firebase");
    } else {
        Serial.println("Sin cambios significativos");
    }
}