#include "LCD_I2C.h"
// No es necesario incluir LiquidCrystal_I2C.h aquí, ya se incluye en LCD_I2C.h

LCD_I2C::LCD_I2C(uint8_t address, uint8_t columns, uint8_t rows)
    : _lcd(address, columns, rows), _columns(columns), _rows(rows) {}

void LCD_I2C::begin() {
    // ESTA LÍNEA ES CLAVE: Wire.begin() DEBE ESTAR AQUÍ O EN EL SETUP, PERO NO EN AMBOS.
    // Lo dejaremos aquí para que tu librería lo maneje.
    Wire.begin(4, 5);  // Pines SDA=4, SCL=5 para ESP8266

    _lcd.init();       // Inicializa el controlador del LCD
    _lcd.backlight();  // Enciende la luz de fondo
    _lcd.clear();      // Limpia el display
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
        _lcd.backlight();
    } else {
        _lcd.noBacklight();
    }
}