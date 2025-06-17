#ifndef MQ2SENSOR_H
#define MQ2SENSOR_H

#include <Arduino.h>

class MQ2Sensor {
public:
    MQ2Sensor(uint8_t analogPin, unsigned long tiempoCalentamientoMs = 60000);
    void begin();
    bool isReady();
    unsigned long getStartTime();
    void calibrate(); // opcional
    float leerCO();   // devuelve CO estimado
    float leerCO2();  // simula un valor de CO₂
    float co;
    float co2;

private:
    uint8_t _pin;
    unsigned long _startTime;
    unsigned long _tiempoCalentamiento;
    float _coReferencia = 200.0;  // ppm estimada de referencia CO
};

#endif
