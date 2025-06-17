#include "LCD_I2C.h"

LCD_I2C::LCD_I2C(uint8_t address, uint8_t columns, uint8_t rows) 
    : _lcd(address, columns, rows), _columns(columns), _rows(rows) {}

void LCD_I2C::begin() {
     Wire.begin(4, 5);  // Pines SDA=4, SCL=5 para ESP8266
    _lcd.init();
    _lcd.backlight();
    _lcd.clear();
    
}

void LCD_I2C::clear() {
    _lcd.clear();
}

void LCD_I2C::print(String text, uint8_t col, uint8_t row) {
    _lcd.setCursor(col, row);
    _lcd.print(text);
}

void LCD_I2C::backlight(bool state) {
    if (state) {
        _lcd.backlight(); // Enciende
    } else {
        _lcd.noBacklight(); // Apaga
    }
}
