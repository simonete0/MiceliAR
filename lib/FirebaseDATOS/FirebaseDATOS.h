#ifndef FIREBASE_DATOS_H
#define FIREBASE_DATOS_H

#include <Arduino.h>
#include <Firebase_ESP_Client.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

class FirebaseDatos {
public:
    FirebaseDatos();
    
    void begin(const char* ssid, const char* password, 
              const char* apiKey, const char* databaseUrl,
              const char* email, const char* passwordAuth);
    
    bool isReady();
    bool sendData(float temperature, float humidity, float co2 = -1);
    bool shouldUpdate(float newTemp, float newHumidity, float newCO2 = -1);
    
private:
    WiFiUDP ntpUDP;
    NTPClient timeClient;
    FirebaseData fbdo;
    FirebaseAuth auth;
    FirebaseConfig config;
    
    float lastTemp;
    float lastHumidity;
    float lastCO2;
    
    bool getLastValues();
    static void tokenStatusCallback(TokenInfo info);
    
    String getCurrentDate();
    String getCurrentTime();
};

#endif