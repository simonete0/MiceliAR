#include "sensorDHT11.h"


// Constructor
sensorDHT::sensorDHT(int p) : _pin(p), _dht(p, DHTTYPE), _temperatura(0), _humedad(0) {}

// Método para inicializar el sensor
void sensorDHT::iniciar() {
    _dht.begin();
}

// Método para leer los valores del sensor
void sensorDHT::leerValores() {
    _humedad = _dht.readHumidity();
    _temperatura = _dht.readTemperature();
// Comprueba si alguna lectura ha fallado e imprime un mensaje de error
    if (isnan(_humedad) || isnan(_temperatura)) {
    Serial.println("Error al leer el sensor DHT11!");
    return;
  }
}

// Método para obtener la temperatura
float sensorDHT::getTemperatura() {
    return _temperatura;
}

// Método para obtener la humedad
float sensorDHT::getHumedad() {
    return _humedad;
}
