#ifndef MQ135_SENSOR_H
#define MQ135_SENSOR_H

#include <Arduino.h>

class MQ135Sensor {
  public:
    // Constructor: recibe el pin analógico y el tiempo de calentamiento en ms
    MQ135Sensor(uint8_t pin, unsigned long warmupTime = 60000);
    unsigned long getStartTime() const { return _startTime; }
    // Inicializa el sensor
    void begin();
    
    // Lee la concentración de CO2 en ppm
    float readCO2();
    
    // Verifica si el sensor está caliente y listo
    bool isReady() const;
    
    // Calibra el sensor (debe hacerse en aire limpio)
    void calibrate();

  private:
    uint8_t _pin;
    float _r0;  // Resistencia en aire limpio
    unsigned long _warmupTime;
    unsigned long _startTime;
    bool _calibrated;
    
    // Calcula la resistencia del sensor
    float readResistance();
    
    // Calcula el ratio RS/R0
    float readRatio();
};

#endif