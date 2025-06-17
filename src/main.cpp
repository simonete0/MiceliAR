#include <Arduino.h>
#include "FirebaseDATOS.h"
#include "sensorDHT11.h"
#include "MQ2Sensor.h"
#include "LCD_I2C.h"
#include <Wire.h>

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

// Configuración LCD
#define LCD_ADDRESS 0x27   // Dirección I2C común (usa 0x3F si no funciona)
#define LCD_COLUMNS 20     // 2004A = 20 columnas
#define LCD_ROWS 4         // 2004A = 4 filas

LCD_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);



void setup() {
    pinMode(RELAY1, OUTPUT);
    pinMode(RELAY2, OUTPUT);
    digitalWrite(RELAY1, HIGH);  // Apaga relé (depende de tu lógica)
    digitalWrite(RELAY2, HIGH);  // HIGH = Apagado en configuración normal
    delay(500);  // Espera a que se estabilice la alimentación
    Serial.begin(115200);
    lcd.begin();
    // Fuerza modo correcto para GPIO14
    pinMode(DHTPIN, INPUT_PULLUP);
    delay(100);
    // Inicializa DHT
    dhtSensor.iniciar();
    delay(2000); // Espera crítica para DHT11
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
    lcd.clear();         // Limpia pantalla
    lcd.begin();  // Sin argumentos
    lcd.backlight(true);
    lcd.print("¡Funciona!"); 
    lcd.clear();
    lcd.print("Temperatura: " + String(temperatura, 1), 0, 0);
    lcd.print("Humedad: " + String(humedad, 1), 0, 1);
    lcd.print("CO2: " + String(co2, 1), 0, 2);
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
