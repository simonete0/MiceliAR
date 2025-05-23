#ifndef FIREBASE_DATOS_H
#define FIREBASE_DATOS_H

#include <Arduino.h>
#include <Firebase_ESP_Client.h>

class FirebaseDatos {
public:
    FirebaseDatos();
    void begin(const char* ssid, const char* password, 
              const char* apiKey, const char* databaseUrl,
              const char* email, const char* passwordAuth);
    
    bool isReady();
    bool sendData(float temperature, float humidity);
    bool shouldUpdate(float newTemp, float newHumidity);
    
private:
    FirebaseData _fbdo;
    FirebaseAuth _auth;
    FirebaseConfig _config;
    
    float _lastTemp = 0;
    float _lastHumidity = 0;
    
    bool _getLastValues();  // Cambiado a público temporalmente para pruebas
    static void _tokenStatusCallback(TokenInfo info);
};

#endif