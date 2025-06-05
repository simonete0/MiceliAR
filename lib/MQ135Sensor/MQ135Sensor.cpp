#include "MQ135Sensor.h"
#include <math.h>

MQ135Sensor::MQ135Sensor(uint8_t pin, unsigned long warmupTime) 
  : _pin(pin), _warmupTime(warmupTime), _r0(76.63), _calibrated(false) {
}

void MQ135Sensor::begin() {
  pinMode(_pin, INPUT);
  _startTime = millis();
  Serial.println("Iniciando sensor MQ135...");
}

bool MQ135Sensor::isReady() const {
  return (millis() - _startTime) >= _warmupTime;
}

float MQ135Sensor::readResistance() {
  int val = analogRead(_pin);
  float voltage = val * (5.0 / 1023.0);
  return (5.0 - voltage) / voltage;  // RS para RL = 1KΩ
}

float MQ135Sensor::readRatio() {
  return readResistance() / _r0;
}

float MQ135Sensor::readCO2() {
  if (!isReady()) {
    Serial.println("Sensor no listo, aún calentando...");
    return -1.0;
  }
  
  float ratio = readRatio();
  return 116.6020682 * pow((ratio/2.5), -2.769034857);  // Fórmula para CO2
}

void MQ135Sensor::calibrate() {
  if (!isReady()) {
    Serial.println("Sensor no listo para calibración, espere...");
    return;
  }
  
  Serial.println("Calibrando sensor en aire limpio...");
  float rs = 0.0;
  const int samples = 100;
  
  for (int i = 0; i < samples; i++) {
    rs += readResistance();
    delay(100);
    Serial.print(".");
  }
  Serial.println();
  
  _r0 = rs / samples;
  _calibrated = true;
  
  Serial.print("Calibración completada. R0 = ");
  Serial.println(_r0);
}