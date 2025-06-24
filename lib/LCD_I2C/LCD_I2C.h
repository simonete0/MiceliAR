#ifndef LCD_I2C_H
#define LCD_I2C_H

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

class LCD_I2C {
public:
    LCD_I2C(uint8_t address, uint8_t columns, uint8_t rows); // Constructor
    void begin(); // Inicialización
    void clear(); // Limpiar pantalla
    void print(String text, uint8_t col = 0, uint8_t row = 0); // Escribir en posición
    void backlight(bool state); // true = ON, false = OFF

private:
    LiquidCrystal_I2C _lcd;
    uint8_t _columns;
    uint8_t _rows;
};

#endif