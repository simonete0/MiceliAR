#include "FirebaseDATOS.h"
#include "addons/TokenHelper.h"

// Implementación del callback estático
void FirebaseDatos::_tokenStatusCallback(TokenInfo info) {
    if (info.status == token_status_error) {
        Serial.printf("Token error: %s\n", getTokenError(info).c_str());
    }
}

FirebaseDatos::FirebaseDatos() {
    // Constructor
}

void FirebaseDatos::begin(const char* ssid, const char* password, 
                         const char* apiKey, const char* databaseUrl,
                         const char* email, const char* passwordAuth) {
    // Configura WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(300);
    }

    // Configura Firebase
    _config.api_key = apiKey;
    _config.database_url = databaseUrl;
    _auth.user.email = email;
    _auth.user.password = passwordAuth;
    _config.token_status_callback = _tokenStatusCallback;
    
    Firebase.begin(&_config, &_auth);
    while (!Firebase.ready()) {
        delay(300);
    }
    
    _getLastValues();
}

bool FirebaseDatos::isReady() {
    return Firebase.ready();
}

bool FirebaseDatos::_getLastValues() {
    if(Firebase.RTDB.getFloat(&_fbdo, "/ultima_lectura/temperatura")) {
        _lastTemp = _fbdo.floatData();
    }
    
    if(Firebase.RTDB.getFloat(&_fbdo, "/ultima_lectura/humedad")) {
        _lastHumidity = _fbdo.floatData();
    }
    
    return true;
}

bool FirebaseDatos::shouldUpdate(float newTemp, float newHumidity) {
    return (abs(newTemp - _lastTemp) >= 1.0) || 
           (abs(newHumidity - _lastHumidity) >= 2.0);
}

bool FirebaseDatos::sendData(float temperature, float humidity) {
    if(!shouldUpdate(temperature, humidity)) {
        return false;
    }

    FirebaseJson json;
    json.set("temperatura", temperature);
    json.set("humedad", humidity);
    json.set("timestamp", millis());

    String path = "lecturas/" + String(millis());
    
    if(Firebase.RTDB.setJSON(&_fbdo, path, &json)) {
        _lastTemp = temperature;
        _lastHumidity = humidity;
        Firebase.RTDB.setJSON(&_fbdo, "/ultima_lectura", &json);
        return true;
    }
    return false;
}