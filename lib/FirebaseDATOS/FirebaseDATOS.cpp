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
    //json.set("hora", currentTime);
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
        String fechaHora = getCurrentDate() + " " + getCurrentTime();
        Firebase.RTDB.setString(&fbdo, "/ultima_lectura/FechaHora", fechaHora);
        return true;
    }
    return false;
}
void FirebaseDatos::guardarSetpointsFirebase(float settemp, float sethum, int setco2) {
    // Solo guarda si cambió respecto a lo que hay en Firebase
    // (puedes leer primero y comparar, o guardar siempre)
    Firebase.RTDB.setFloat(&fbdo, "/Parametros/Setpoints/Temp", settemp);
    Firebase.RTDB.setFloat(&fbdo, "/Parametros/Setpoints/Hum", sethum);
    Firebase.RTDB.setInt(&fbdo, "/Parametros/Setpoints/CO2", setco2);
    String fechaHora = getCurrentDate() + " " + getCurrentTime();
    Firebase.RTDB.setString(&fbdo, "/Parametros/Setpoints/FechaHora", fechaHora);

}

void FirebaseDatos::guardarAlarmasFirebase(float tmin, float tmax, float hmin, float hmax, int co2min, int co2max) {
    Firebase.RTDB.setFloat(&fbdo, "/Parametros/Alarmas/TempMin", tmin);
    Firebase.RTDB.setFloat(&fbdo, "/Parametros/Alarmas/TempMax", tmax);
    Firebase.RTDB.setFloat(&fbdo, "/Parametros/Alarmas/HumMin", hmin);
    Firebase.RTDB.setFloat(&fbdo, "/Parametros/Alarmas/HumMax", hmax);
    Firebase.RTDB.setInt(&fbdo, "/Parametros/Alarmas/CO2Min", co2min);
    Firebase.RTDB.setInt(&fbdo, "/Parametros/Alarmas/CO2Max", co2max);
    String fechaHora = getCurrentDate() + " " + getCurrentTime();
    Firebase.RTDB.setString(&fbdo, "/Parametros/Alarmas/FechaHora", fechaHora);

}

void FirebaseDatos::guardarUltimoEstadoFirebase(const String& estado) {
    Firebase.RTDB.setString(&fbdo, "/Parametros/UltimoEstado/Estado", estado);
    String fechaHora = getCurrentDate() + " " + getCurrentTime();
    Firebase.RTDB.setString(&fbdo, "/Parametros/UltimoEstado/FechaHora", fechaHora);
}

bool FirebaseDatos::leerSetpointsFirebase(float &settemp, float &sethum, int &setco2) {
    bool ok = true;
    float t = settemp, h = sethum;
    int c = setco2;

    if (Firebase.RTDB.getFloat(&fbdo, "/Parametros/Setpoints/Temp")) {
        t = fbdo.floatData();
    } else {
        ok = false;
    }
    if (Firebase.RTDB.getFloat(&fbdo, "/Parametros/Setpoints/Hum")) {
        h = fbdo.floatData();
    } else {
        ok = false;
    }
    if (Firebase.RTDB.getInt(&fbdo, "/Parametros/Setpoints/CO2")) {
        c = fbdo.intData();
    } else {
        ok = false;
    }
    settemp = t;
    sethum = h;
    setco2 = c;
    
    Serial.printf("Setpoints: T=%.1f H=%.1f CO2=%d\n", settemp, sethum, setco2);
    return ok;
}

bool FirebaseDatos::leerAlarmasFirebase(float &tmin, float &tmax, float &hmin, float &hmax, int &co2min, int &co2max) {
    bool ok = true;
    float tmi = tmin, tma = tmax, hmi = hmin, hma = hmax;
    int cmi = co2min, cma = co2max;

    if (Firebase.RTDB.getFloat(&fbdo, "/Parametros/Alarmas/TempMin")) {
        tmi = fbdo.floatData();
    } else {
        ok = false;
    }
    if (Firebase.RTDB.getFloat(&fbdo, "/Parametros/Alarmas/TempMax")) {
        tma = fbdo.floatData();
    } else {
        ok = false;
    }
    if (Firebase.RTDB.getFloat(&fbdo, "/Parametros/Alarmas/HumMin")) {
        hmi = fbdo.floatData();
    } else {
        ok = false;
    }
    if (Firebase.RTDB.getFloat(&fbdo, "/Parametros/Alarmas/HumMax")) {
        hma = fbdo.floatData();
    } else {
        ok = false;
    }
    if (Firebase.RTDB.getInt(&fbdo, "/Parametros/Alarmas/CO2Min")) {
        cmi = fbdo.intData();
    } else {
        ok = false;
    }
    if (Firebase.RTDB.getInt(&fbdo, "/Parametros/Alarmas/CO2Max")) {
        cma = fbdo.intData();
    } else {
        ok = false;
    }
    tmin = tmi;
    tmax = tma;
    hmin = hmi;
    hmax = hma;
    co2min = cmi;
    co2max = cma;
    
    Serial.printf("Alarmas: Tmin=%.1f Tmax=%.1f Hmin=%.1f Hmax=%.1f CO2min=%d CO2max=%d\n",
    tmin, tmax, hmin, hmax, co2min, co2max);
    return ok;
}

bool FirebaseDatos::leerUltimoEstadoFirebase(String &estado) {
    if (Firebase.RTDB.getString(&fbdo, "/Parametros/UltimoEstado/Estado")) {
        estado = fbdo.stringData();
    }
    return true;
}