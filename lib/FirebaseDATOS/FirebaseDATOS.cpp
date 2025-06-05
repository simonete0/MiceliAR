#include "FirebaseDATOS.h"
#include "addons/TokenHelper.h"

FirebaseDatos::FirebaseDatos() 
    : timeClient(ntpUDP, "pool.ntp.org"), 
      lastTemp(0), 
      lastHumidity(0),
      lastCO2(-1) { // Inicializar CO2 como -1 (no disponible)
}

String FirebaseDatos::getCurrentDate() {
    timeClient.update();
    time_t epochTime = timeClient.getEpochTime();
    struct tm *ptm = gmtime(&epochTime);
    
    char dateString[11];
    strftime(dateString, sizeof(dateString), "%Y-%m-%d", ptm);
    return String(dateString);
}


String FirebaseDatos::getCurrentTime() {
    timeClient.update();
    time_t epochTime = timeClient.getEpochTime();
    struct tm *ptm = gmtime(&epochTime);
    
    char timeString[9];
    strftime(timeString, sizeof(timeString), "%H-%M-%S", ptm);
    return String(timeString);
}

void FirebaseDatos::tokenStatusCallback(TokenInfo info) {
    if (info.status == token_status_error) {
        Serial.printf("Token error: %s\n", getTokenError(info).c_str());
    }
}

void FirebaseDatos::begin(const char* ssid, const char* password, 
                         const char* apiKey, const char* databaseUrl,
                         const char* email, const char* passwordAuth) {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(300);
    }

    timeClient.begin();
    timeClient.setTimeOffset(-10800); // UTC-3 (Argentina)
    timeClient.update();

    config.api_key = apiKey;
    config.database_url = databaseUrl;
    auth.user.email = email;
    auth.user.password = passwordAuth;
    config.token_status_callback = tokenStatusCallback;
    
    Firebase.begin(&config, &auth);
    while (!Firebase.ready()) {
        delay(300);
    }
    
    getLastValues();
}

bool FirebaseDatos::isReady() {
    return Firebase.ready();
}

bool FirebaseDatos::getLastValues() {
    if(Firebase.RTDB.getFloat(&fbdo, "/ultima_lectura/temperatura")) {
        lastTemp = fbdo.floatData();
    }
    
    if(Firebase.RTDB.getFloat(&fbdo, "/ultima_lectura/humedad")) {
        lastHumidity = fbdo.floatData();
    }
    
    if(Firebase.RTDB.getFloat(&fbdo, "/ultima_lectura/co2")) {
        lastCO2 = fbdo.floatData();
    }
    return true;
}

bool FirebaseDatos::shouldUpdate(float newTemp, float newHumidity, float newCO2) {
    bool tempChanged = (abs(newTemp - lastTemp) >= 1.0);
    bool humChanged = (abs(newHumidity - lastHumidity) >= 2.0);
    bool co2Changed = (newCO2 >= 0) && (abs(newCO2 - lastCO2) >= 50.0); // Umbral de 50ppm para CO2
    
    return tempChanged || humChanged || co2Changed;
}

bool FirebaseDatos::sendData(float temperature, float humidity, float co2) {
    if(!shouldUpdate(temperature, humidity, co2)) {
        return false;
    }

    String currentDate = getCurrentDate();
    String currentTime = getCurrentTime();

    FirebaseJson json;
    json.set("hora", currentTime);
    json.set("humedad", humidity);
    json.set("temperatura", temperature);
    if(co2 >= 0) json.set("co2", co2); // Solo añadir CO2 si es válido

    String path = "lecturas/" + currentDate + "/" + currentTime;
    
    if(Firebase.RTDB.setJSON(&fbdo, path, &json)) {
        lastTemp = temperature;
        lastHumidity = humidity;
        if(co2 >= 0) lastCO2 = co2; // Actualizar solo si es válido
        
        // Actualizar última lectura
        Firebase.RTDB.setJSON(&fbdo, "/ultima_lectura", &json);
        return true;
    }
    return false;
}