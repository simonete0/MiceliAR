
/**
 * Created by K. Suwatchai (Mobizt)
 *
 * Email: k_suwatchai@hotmail.com
 *
 * Github: https://github.com/mobizt/Firebase-ESP8266
 *
 * Copyright (c) 2023 mobizt
 *
 */

/** This example shows the basic RTDB usage with external Client.
 * This example used ESP8266 and WIZnet W5500 module.
 *
 * For ESP8266 native Ethernet usage without external Ethernet client required, see examples/RTB/BasicEthernet/ESP8266/ESP8266.ino.
 *
 * To use external Ethernet client with ESP8266 as in this example, the following macro in src/CustomFirebaseFS.h or build flag
 * should be assigned to disable the native internet inclusion that conflicts with Ethernet.h
 *
 * FIREBASE_DISABLE_NATIVE_ETHERNET
 */

#include <Firebase_ESP_Client.h>

// Provide the token generation process info.
#include <addons/TokenHelper.h>

// Provide the RTDB payload printing info and other helper functions.
#include <addons/RTDBHelper.h>

#include <Ethernet.h>

// For the following credentials, see examples/Authentications/SignInAsUser/EmailPassword/EmailPassword.ino

/* 1. Define the API Key */
#define API_KEY "API_KEY"

/* 2. Define the RTDB URL */
#define DATABASE_URL "URL" //<databaseName>.firebaseio.com or <databaseName>.<region>.firebasedatabase.app

/* 3. Define the user Email and password that alreadey registerd or added in your project */
#define USER_EMAIL "USER_EMAIL"
#define USER_PASSWORD "USER_PASSWORD"

/* 4. Defined the Ethernet module connection */
#define WIZNET_RESET_PIN 5 // Connect W5500 Reset pin to GPIO 5 (D1) of ESP8266 (-1 for no reset pin assigned)
#define WIZNET_CS_PIN 4    // Connect W5500 CS pin to GPIO 4 (D2) of ESP8266
#define WIZNET_MISO_PIN 12 // Connect W5500 MISO pin to GPIO 12 (D6) of ESP8266
#define WIZNET_MOSI_PIN 13 // Connect W5500 MOSI pin to GPIO 13 (D7) of ESP8266
#define WIZNET_SCLK_PIN 14 // Connect W5500 SCLK pin to GPIO 14 (D5) of ESP8266

/* 5. Define MAC */
uint8_t Eth_MAC[] = {0x02, 0xF0, 0x0D, 0xBE, 0xEF, 0x01};

/* 6. Define the static IP (Optional)
IPAddress localIP(192, 168, 1, 104);
IPAddress subnet(255, 255, 0, 0);
IPAddress gateway(192, 168, 1, 1);
IPAddress dnsServer(8, 8, 8, 8);
bool optional = false; // Use this static IP only no DHCP
Firebase_StaticIP staIP(localIP, subnet, gateway, dnsServer, optional);
*/

// Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;

int count = 0;

EthernetClient eth;

// Define the callback function to handle server connection

void setup()
{

    Serial.begin(115200);

    Serial_Printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

    /* Assign the api key (required) */
    config.api_key = API_KEY;

    /* Assign the user sign in credentials */
    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;

    /* Assign the RTDB URL (required) */
    config.database_url = DATABASE_URL;

    /* Assign the callback function for the long running token generation task */
    config.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h

    /* Assign the pointer to global defined Ethernet Client object */
    fbdo.setEthernetClient(&eth, Eth_MAC, WIZNET_CS_PIN, WIZNET_RESET_PIN); // The staIP can be assigned to the fifth param

     // Comment or pass false value when WiFi reconnection will control by your code or third party library
    Firebase.reconnectWiFi(true);

    // required for large file data, increase Rx size as needed.
    fbdo.setBSSLBufferSize(4096 /* Rx buffer size in bytes from 512 - 16384 */, 1024 /* Tx buffer size in bytes from 512 - 16384 */);

    Firebase.setDoubleDigits(5);

    Firebase.begin(&config, &auth);
}

void loop()
{

    // Firebase.ready() should be called repeatedly to handle authentication tasks.

    if (Firebase.ready() && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0))
    {
        sendDataPrevMillis = millis();

        Serial_Printf("Set bool... %s\n", Firebase.RTDB.setBool(&fbdo, F("/test/bool"), count % 2 == 0) ? "ok" : fbdo.errorReason().c_str());

        count++;
    }
}
