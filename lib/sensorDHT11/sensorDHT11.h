#ifndef sensorDHT11_h
#define sensorDHT11_h
#include <Arduino.h>
#include <DHT.h>
#define DHTTYPE DHT11  // Especifica el tipo de sensor DHT 

class sensorDHT
{
    public:
         // Constructor
    sensorDHT(int p);
         // Método para inicializar el sensor
    void iniciar();
        // Método para leer los valores del sensor y almacenarlos 
    void leerValores();
        // Método para obtener la temperatura
    float getTemperatura();
        // Método para obtener la humedad
    float getHumedad();

    private:
        int _pin;
        DHT _dht;
        float _temperatura;
        float _humedad;
};
#endif