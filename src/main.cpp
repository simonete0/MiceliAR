#include <Arduino.h>
#include "FirebaseDATOS.h"
#include "sensorDHT11.h"
#include "MQ2Sensor.h"


// Configuraciones
#define WIFI_SSID "WAB 2.4"
#define WIFI_PASSWORD "NOLAESCRIBAS"
#define FIREBASE_URL "https://reinomicelio-e4a69-default-rtdb.firebaseio.com/"
#define FIREBASE_API_KEY "AIzaSyBoaIXH4BWwvUB1VAkPqkI3DZ9SgK0b728"
#define FIREBASE_EMAIL "admin@admin.com"
#define FIREBASE_PASSWORD "admin1234"

#define MQ2_PIN A0    // ADC para sensor MQ135
#define RELAY2 16        // D0 rele 2 (Calefactor)
#define I2C_SDA 4       // GPIO2 (D2) para I2C SDA
#define I2C_SCL 5       // GPIO1 (D1) para I2C SCL
#define SW_encoder 2    // D4 para switch encoder
#define DHTPIN 14        // GPIO5 (D5) para sensor DHT11
#define DHTTYPE DHT11
#define CLOCK_encoder 12 // D6 encoder clock
#define DT_encoder 13    // D7 encoder data
#define RELAY1 15        // D8 rele 1 (Ventilador)


FirebaseDatos firebase;
sensorDHT dhtSensor(DHTPIN);
MQ2Sensor gasSensor(MQ2_PIN);
// Enumeración de modos de sistema
enum ModoSistema {
  MODO_FUNCIONAMIENTO,
  MODO_MENU
};
// Variables globales
ModoSistema modoSistema = MODO_MENU;  // Se inicia en modo menú
float co2;
void LeerCO();

void setup() {
   // Fuerza modo correcto para GPIO14
    pinMode(DHTPIN, INPUT_PULLUP);
    delay(100);
    
    // Inicializa DHT
    dhtSensor.iniciar();
    delay(2000); // Espera crítica para DHT11

    Serial.begin(115200);
      // Inicializar el sensor MQ-2
    gasSensor.begin();
    ////sensor dht11/////////
    dhtSensor.iniciar();
    ///base de datos/////////
    firebase.begin(WIFI_SSID, WIFI_PASSWORD, 
                  FIREBASE_API_KEY, FIREBASE_URL,
                  FIREBASE_EMAIL, FIREBASE_PASSWORD);


}

void loop() {
    // Testeo del pin D5/GPIO14
    pinMode(DHTPIN, INPUT_PULLUP);
    Serial.print("Estado pin D5: ");
    Serial.println(digitalRead(DHTPIN)); // Debe ser 1 (HIGH)
    
    dhtSensor.leerValores();
    float humedad = dhtSensor.getHumedad();
    float temperatura = dhtSensor.getTemperatura();
    
    if (isnan(humedad) || isnan(temperatura)) {
        Serial.println("¡Fallo lectura DHT11!");
        delay(2000);
    }
  // Mostrar y enviar datos
    Serial.printf("Temperatura: %.1f°C, Humedad: %.1f%%, CO2: %.1fppm\n", 
      temperatura, humedad, co2 >= 0 ? co2 : 0);
          
      

    if(firebase.sendData(temperatura, humedad, co2)) {
        Serial.println("Datos enviados a Firebase");
    } else {
        Serial.println("Sin cambios significativos");
    }
   LeerCO();
  delay(5000);
  return;
}

void LeerCO() {
    if (!gasSensor.isReady()) {
        Serial.println("Calentando sensor MQ2...");
        delay(10000);
        return;
    }

    float co = gasSensor.leerCO();
    co2 = gasSensor.leerCO2();

    Serial.print("CO estimado: ");
    Serial.print(co);
    Serial.println(" ppm");

    Serial.print("CO2 simulado: ");
    Serial.print(co2);
    Serial.println(" ppm");
}
