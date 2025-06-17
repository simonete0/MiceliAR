#include "MQ2Sensor.h"

MQ2Sensor::MQ2Sensor(uint8_t analogPin, unsigned long tiempoCalentamientoMs) {
    _pin = analogPin;
    _tiempoCalentamiento = tiempoCalentamientoMs;
}

void MQ2Sensor::begin() {
    _startTime = millis();
}

bool MQ2Sensor::isReady() {
    return (millis() - _startTime >= _tiempoCalentamiento);
}

unsigned long MQ2Sensor::getStartTime() {
    return _startTime;
}

void MQ2Sensor::calibrate() {
    // Aquí podrías almacenar una referencia si lo deseas
    _coReferencia = analogRead(_pin);
}

float MQ2Sensor::leerCO() {
    int lectura = analogRead(_pin);
    // Suponiendo que 1023 = 1000 ppm de CO (calibración simple y empírica)
    float ppm = (lectura / 1023.0) * 1000.0;
    return ppm;
}

float MQ2Sensor::leerCO2() {
    float co = leerCO();
    // Simulación simple: se estima CO2 como 2x CO (ajustable)
    float co2 = co * 2.0;
    return co2;
}
