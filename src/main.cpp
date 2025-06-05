#include <Arduino.h>
#include "FirebaseDATOS.h"
#include "sensorDHT11.h"
#include "MQ135Sensor.h"



// Configuraciones
#define WIFI_SSID "WAB 2.4"
#define WIFI_PASSWORD "NOLAESCRIBAS"
#define FIREBASE_URL "https://reinomicelio-e4a69-default-rtdb.firebaseio.com/"
#define FIREBASE_API_KEY "AIzaSyBoaIXH4BWwvUB1VAkPqkI3DZ9SgK0b728"
#define FIREBASE_EMAIL "admin@admin.com"
#define FIREBASE_PASSWORD "admin1234"
#define DHTPIN 5      // GPIO5 (D5) para sensor DHT11
#define I2C_SDA 2     // GPIO2 (D2) para I2C SDA
#define I2C_SCL 1     // GPIO1 (D1) para I2C SCL
#define SWITCH_PIN 0  // D0 para llave palanca (interrupt)
#define ENCODER_CLK 6 // D6 encoder clock
#define ENCODER_DT 7  // D7 encoder data
#define ENCODER_SW 9  // D9 encoder switch
#define LED_ALARM 3   // D3 led alarma
#define MQ135_PIN A0  // ADC para sensor MQ135
#define RELAY1 4      // D4 rele 1 (Ventilador)
#define RELAY2 8      // D8 rele 2 (Calefactor)

FirebaseDatos firebase;
sensorDHT dhtSensor(DHTPIN);
MQ135Sensor co2Sensor(A0, 60000);// Crear instancia del sensor en el pin A0 con 60 segundos de calentamiento
// Enumeración de modos de sistema
enum ModoSistema {
  MODO_FUNCIONAMIENTO,
  MODO_MENU
};
// Variables globales
ModoSistema modoSistema = MODO_MENU;  // Se inicia en modo menú


void setup() {
    Serial.begin(115200);
    ////sensor dht11/////////
    dhtSensor.iniciar();
    ///base de datos/////////
    firebase.begin(WIFI_SSID, WIFI_PASSWORD, 
                  FIREBASE_API_KEY, FIREBASE_URL,
                  FIREBASE_EMAIL, FIREBASE_PASSWORD);
    //sensor mq135///////////
    co2Sensor.begin();
    Serial.println("Calentando sensor MQ135...");  // Mostrar mensaje de calentamiento
    //

}

void loop() {
    Leer_Sensores;
   
    
}


// Función para leer el estado de la llave
void leerModoSistema() {
  if (digitalRead(SWITCH_PIN) == HIGH) {
    modoSistema = MODO_FUNCIONAMIENTO;
  } else {
    modoSistema = MODO_MENU;
  }
}
// Lógica del modo funcionamiento
void loopFuncionamiento() {
  // Aquí irá la lógica para controlar sensores, relés y alarmas según setpoints
  Serial.println("Modo: FUNCIONAMIENTO");
  // ejemplo: digitalWrite(RELAY1, HIGH); si temp < setpoint
}
// Lógica del modo menú
void loopMenu() {
  // Aquí irá la navegación del menú usando el encoder y su botón
  Serial.println("Modo: MENU");
  // ejemplo: mostrar "Setpoint Humedad" y permitir ajustar
}


void Leer_Sensores() {
    delay(4000);
    
    // Leer DHT11
    dhtSensor.leerValores();
    float temperatura = dhtSensor.getTemperatura();
    float humedad = dhtSensor.getHumedad();
    
    if (isnan(humedad) || isnan(temperatura)) {
        Serial.println("Error en lectura DHT11");
        return;
    }

    // Leer MQ135
    static unsigned long lastCO2Read = 0;
    float co2 = -1; // Valor por defecto (no disponible)
    
    if (co2Sensor.isReady() && millis() - lastCO2Read >= 30000) {
        lastCO2Read = millis();
        co2 = co2Sensor.readCO2();
        if (co2 >= 0) {
            Serial.print("CO2: ");
            Serial.print(co2);
            Serial.println(" ppm");
        }
    }

    // Mostrar y enviar datos
    Serial.printf("Temperatura: %.1f°C, Humedad: %.1f%%, CO2: %.1fppm\n", 
                 temperatura, humedad, co2 >= 0 ? co2 : 0);
                 
    if(firebase.sendData(temperatura, humedad, co2)) {
        Serial.println("Datos enviados a Firebase");
    } else {
        Serial.println("Sin cambios significativos");
    }
}