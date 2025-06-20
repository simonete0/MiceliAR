#include <Arduino.h>
#include <Wire.h>
#include "LCD_I2C.h"
#include <EEPROM.h>

// ETIQUETA: Librerias de 'prueba encoder'
#include "FirebaseDATOS.h"
#include "sensorDHT11.h"
#include "MQ2Sensor.h"

#define EEPROM_SIZE 40

// Pines I2C (para ESP8266) - Comunes en ambos, mantenemos la definicion principal
#define I2C_SDA 4
#define I2C_SCL 5

// Pines del encoder (para ESP8266 NodeMCU/Wemos D1 Mini)
#define CLOCK_ENCODER 12
#define DT_ENCODER 13
#define SW_ENCODER 2

// ETIQUETA: Configuraciones de 'prueba encoder'
#define WIFI_SSID "WAB 2.4"
#define WIFI_PASSWORD "NOLAESCRIBAS"
#define FIREBASE_URL "https://reinomicelio-e4a69-default-rtdb.firebaseio.com/"
#define FIREBASE_API_KEY "AIzaSyBoaIXH4BWwvUB1VAkPqkI3DZ9SgK0b728"
#define FIREBASE_EMAIL "admin@admin.com"
#define FIREBASE_PASSWORD "admin1234"

#define MQ2_PIN A0       // ADC para sensor MQ135
#define RELAY2 16        // D0 rele 2 (Calefactor)
#define DHTPIN 14        // GPIO5 (D5) para sensor DHT11
#define DHTTYPE DHT11
#define RELAY1 15        // D8 rele 1 (Ventilador)

// ---------------- CONFIGURACIÓN LCD ----------------
#define LCD_ADDRESS 0x27
#define LCD_COLUMNS 20
#define LCD_ROWS 4
LCD_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);

// ETIQUETA: Objetos de 'prueba encoder'
FirebaseDatos firebase;
sensorDHT dhtSensor(DHTPIN);
MQ2Sensor gasSensor(MQ2_PIN);

// ---------------- VARIABLES GLOBALES ----------------
// Variables para el debounce del encoder (usando millis())
unsigned long tiempoUltimaLecturaCLK = 0;
const unsigned long RETARDO_ANTI_REBOTE_ENCODER = 10;

// Variables para el debounce del switch (usando millis())
unsigned long tiempoUltimaPresionSW = 0;
const unsigned long RETARDO_ANTI_REBOTE_SW = 200; // AUMENTADO A 200ms

volatile int valorEncoder = 0;
volatile bool estadoClk;
volatile bool estadoDt;
volatile bool estadoClkAnterior;
volatile bool swFuePresionadoISR = false;

// Variables del menú
int indiceMenuPrincipal = 0;
const int TOTAL_OPCIONES_MENU_PRINCIPAL = 3;
const int TOTAL_OPCIONES_SUBMENU_SETPOINTS = 4;
const int TOTAL_OPCIONES_SUBMENU_ALARMAS = 3;
int indiceSubMenuSetpoints = 0;
int indiceSubMenuAlarmas = 0;
int indiceEditarAlarmas = 0;
const int TOTAL_OPCIONES_EDITAR_ALARMAS = 7;

int indiceConfirmacion = 0;
const int TOTAL_OPCIONES_CONFIRMACION = 2; // SI, NO

// Setpoints
float setpointTemperatura = 25.0;
float setpointHumedad = 70.0;
int setpointCO2 = 6000; // Valor por defecto para etapa de Incubación

// Alarmas
float alarmaTempMin = 18.0;
float alarmaTempMax = 30.0;
float alarmaHumMin = 50.0;
float alarmaHumMax = 90.0;
int alarmaCO2Min = 600; // Estos valores deben ser revisados para la incubación
int alarmaCO2Max = 1200; // Estos valores deben ser revisados para la incubación

// Rangos de edición y pasos (Setpoints)
const float TEMP_MIN = 10.0;
const float TEMP_MAX = 60.0;
const float TEMP_PASO = 0.5;

const float HUM_MIN = 20.0;
const float HUM_MAX = 100.0;
const float HUM_PASO = 1.0;

const int CO2_MIN = 300; // Ampliado para permitir valores altos de incubación
const int CO2_MAX = 10000; // Ampliado para permitir valores altos de incubación
const int CO2_PASO = 50; // Paso más grande para CO2

// Rangos de edición y pasos (Alarmas)
const float ALARMA_TEMP_MIN_RANGO = 5.0;
const float ALARMA_TEMP_MAX_RANGO = 55.0;
const float ALARMA_TEMP_PASO = 0.5;

const float ALARMA_HUM_MIN_RANGO = 10.0;
const float ALARMA_HUM_MAX_RANGO = 95.0;
const float ALARMA_HUM_PASO = 1.0;

const int ALARMA_CO2_MIN_RANGO = 300; // Valores a revisar con el usuario
const int ALARMA_CO2_MAX_RANGO = 8000; // Valores a revisar con el usuario
const int ALARMA_CO2_PASO = 50; // Paso más grande para CO2


// Opciones del menú principal
String opcionesMenuPrincipal[] = {
  "Setpoints",
  "Alarmas",
  "Modo Funcionamiento"
};

int desplazamientoScroll = 0;
const int OPCIONES_VISIBLES_PANTALLA = 4;

enum EstadoApp {
  ESTADO_MENU_PRINCIPAL,
  ESTADO_EDITAR_SETPOINTS,
  ESTADO_ALARMAS_SUBMENU,
  ESTADO_MOSTRAR_ALARMAS,
  ESTADO_EDITAR_ALARMAS,
  ESTADO_CONFIRMACION_MODO_FUNCIONAMIENTO,
  ESTADO_MODO_FUNCIONAMIENTO,
  ESTADO_CONFIRMACION_VOLVER_MENU, // Nuevo estado de confirmación
  ESTADO_EDITAR_TEMP,
  ESTADO_EDITAR_HUM,
  ESTADO_EDITAR_CO2,
  ESTADO_EDITAR_ALARMA_TEMP_MIN,
  ESTADO_EDITAR_ALARMA_TEMP_MAX,
  ESTADO_EDITAR_ALARMA_HUM_MIN,
  ESTADO_EDITAR_ALARMA_HUM_MAX,
  ESTADO_EDITAR_ALARMA_CO2_MIN,
  ESTADO_EDITAR_ALARMA_CO2_MAX
};
EstadoApp estadoActualApp = ESTADO_MENU_PRINCIPAL;

bool necesitaRefrescarLCD = true;

// ETIQUETA: Variables para 'Modo Funcionamiento'
float currentTemp, currentHum, currentCO2;
float lastDisplayedTemp = -999.0; // Inicializar a un valor imposible para forzar la primera visualización
float lastDisplayedHum = -999.0;
float lastDisplayedCO2 = -999.0;

// Umbrales para actualizar la pantalla (ajustar según necesidad)
const float TEMP_DISPLAY_THRESHOLD = 0.2; // Cambio de 0.2 C
const float HUM_DISPLAY_THRESHOLD = 1.0;  // Cambio de 1%
const int CO2_DISPLAY_THRESHOLD = 20;     // Cambio de 20 ppm (según la última indicación del usuario)

unsigned long modoFuncionamientoStartTime = 0; // Para rastrear el tiempo de entrada para el mensaje inicial
bool initialMessageDisplayed = false; // Bandera para asegurar que el mensaje inicial se muestre solo una vez

// ---------------- VARIABLES DE CONTROL DE RELÉS Y VENTILADOR ----------------
// Histéresis para calefactor (Relay 2)
const float HISTERESIS_TEMP_ENCENDER_CALEFACTOR = 2.0; // Encender cuando Temp <= Setpoint - 2C
const float HISTERESIS_TEMP_APAGAR_CALEFACTOR = 1.5;   // Apagar cuando Temp >= Setpoint + 1.5C

// Umbrales de activación para ventilador (Relay 1)
const float UMBRAL_TEMP_VENTILADOR = 2.0; // Encender si Temp > Setpoint + 2C
const float UMBRAL_HUM_VENTILADOR = 8.0;  // Encender si Hum > Setpoint + 8%
const int UMBRAL_CO2_VENTILADOR = 200;    // Encender si CO2 > Setpoint + 200 ppm (ej: 6000+200=6200ppm)

// Umbrales de apagado/estabilización para ventilador (cuando la condición mejora)
const float UMBRAL_TEMP_APAGAR_VENTILADOR = 0.5; // Apagar si Temp <= Setpoint + 0.5C
const float UMBRAL_HUM_APAGAR_VENTILADOR = 2.0;  // Apagar si Hum <= Setpoint + 2%
const int UMBRAL_CO2_APAGAR_VENTILADOR = 100;    // Apagar si CO2 <= Setpoint + 100 ppm (ej: 6000+100=6100ppm)

// Estados para el control del ventilador
enum EstadoVentilador {
  VENTILADOR_INACTIVO,                 // Ventilador apagado, esperando condiciones
  VENTILADOR_ENCENDIDO_3S,                // Ventilador encendido por 3 segundos (primer pulso)
  VENTILADOR_APAGADO_5S,               // Ventilador apagado por 5 segundos
  VENTILADOR_ENCENDIDO_5S,                // Ventilador encendido por 5 segundos (pulsos subsiguientes)
  VENTILADOR_ESPERA_ESTABILIZACION    // Ventilador apagado, esperando 15 segundos para chequeo de estabilización
};
EstadoVentilador estadoVentiladorActual = VENTILADOR_INACTIVO;
unsigned long temporizadorActivoVentilador = 0; // Para medir la duración de los estados del ventilador

// Razón por la que el ventilador está activo, para saber qué umbral de apagado usar
enum RazonActivacionVentilador { NINGUNA, TEMPERATURA_ALTA, HUMEDAD_ALTA, CO2_ALTO };
RazonActivacionVentilador razonActivacionVentilador = NINGUNA;

// Variables para ventilación programada de CO2
unsigned long ultimoTiempoVentilacionProgramada = 0;
const unsigned long INTERVALO_VENTILACION_PROGRAMADA = 3UL * 60 * 60 * 1000; // 3 horas en ms
const unsigned long DURACION_VENTILACION_PROGRAMADA = 10 * 1000; // 10 segundos en ms
bool ventilacionProgramadaActiva = false; // Bandera para indicar si la ventilación programada está en curso

// Nueva bandera para CO2 bajo
bool banderaBajoCO2 = false;

// ---------------- PROTOTIPOS DE FUNCIONES ----------------
void IRAM_ATTR leerEncoderISR();
void IRAM_ATTR leerSwitchISR();

void manejarMenuPrincipal(int deltaEncoder, bool pulsadoSwitch);
void manejarEditarSetpoints(int deltaEncoder, bool pulsadoSwitch);
void manejarSubMenuAlarmas(int deltaEncoder, bool pulsadoSwitch);
void manejarMostrarAlarmas(int deltaEncoder, bool pulsadoSwitch);
void manejarEditarAlarmas(int deltaEncoder, bool pulsadoSwitch);
void manejarConfirmacionModoFuncionamiento(int deltaEncoder, bool pulsadoSwitch);
void manejarModoFuncionamiento(int deltaEncoder, bool pulsadoSwitch);
void manejarConfirmacionVolverMenu(int deltaEncoder, bool pulsadoSwitch);
void manejarEditarTemperatura(int deltaEncoder, bool pulsadoSwitch);
void manejarEditarHumedad(int deltaEncoder, bool pulsadoSwitch);
void manejarEditarCO2(int deltaEncoder, bool pulsadoSwitch);

void manejarEditarAlarmaTempMin(int deltaEncoder, bool pulsadoSwitch);
void manejarEditarAlarmaTempMax(int deltaEncoder, bool pulsadoSwitch);
void manejarEditarAlarmaHumMin(int deltaEncoder, bool pulsadoSwitch);
void manejarEditarAlarmaHumMax(int deltaEncoder, bool pulsadoSwitch);
void manejarEditarAlarmaCO2Min(int deltaEncoder, bool pulsadoSwitch);
void manejarEditarAlarmaCO2Max(int deltaEncoder, bool pulsadoSwitch);

// ETIQUETA: Prototipo de funcion para leer sensores
void leerSensores();

// Funciones para EEPROM
void guardarSetpointsEEPROM();
void cargarSetpointsEEPROM();

// Funciones auxiliares para la prioridad de ventilación
bool estaEnExcesoTemperatura();
bool estaEnExcesoHumedad();
bool estaEnExcesoCO2();
bool estaEnDefectoCO2();


// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);

  // ETIQUETA: Configuracion de pines de 'prueba encoder'
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  digitalWrite(RELAY1, HIGH);  // Apaga relé 1 (Ventilador) - HIGH
  digitalWrite(RELAY2, HIGH);  // Apaga relé 2 (Calefactor) - HIGH
  delay(500);  // Espera a que se estabilice la alimentación

  pinMode(CLOCK_ENCODER, INPUT_PULLUP);
  pinMode(DT_ENCODER, INPUT_PULLUP);
  pinMode(SW_ENCODER, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(CLOCK_ENCODER), leerEncoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(SW_ENCODER), leerSwitchISR, FALLING);

  estadoClkAnterior = digitalRead(CLOCK_ENCODER);

  Wire.begin(I2C_SDA, I2C_SCL);
  lcd.begin();
  lcd.backlight(true); // Asegurar que la luz de fondo esté encendida
  cargarSetpointsEEPROM();

  // ETIQUETA: Inicializacion de sensores y Firebase de 'prueba encoder'
  pinMode(DHTPIN, INPUT_PULLUP); // Fuerza modo correcto para GPIO14
  delay(100);
  dhtSensor.iniciar();
  delay(2000); // Espera crítica para DHT11
  gasSensor.begin();
  firebase.begin(WIFI_SSID, WIFI_PASSWORD,
                FIREBASE_API_KEY, FIREBASE_URL,
                FIREBASE_EMAIL, FIREBASE_PASSWORD);

  necesitaRefrescarLCD = true;
  indiceMenuPrincipal = 0;

  // Inicializar el tiempo de la última ventilación programada
  ultimoTiempoVentilacionProgramada = millis();
}

// ---------------- LOOP PRINCIPAL ----------------
void loop() {
  noInterrupts();
  int deltaEncoderActual = valorEncoder;
  bool switchPulsadoActual = swFuePresionadoISR;
  valorEncoder = 0;
  swFuePresionadoISR = false; // Se limpia la bandera del switch al inicio del loop
  interrupts();

  EstadoApp proximoEstado = estadoActualApp;

  switch (estadoActualApp) {
    case ESTADO_MENU_PRINCIPAL:
      if (deltaEncoderActual != 0) {
        indiceMenuPrincipal += deltaEncoderActual;
        if (indiceMenuPrincipal < 0) indiceMenuPrincipal = TOTAL_OPCIONES_MENU_PRINCIPAL - 1;
        if (indiceMenuPrincipal >= TOTAL_OPCIONES_MENU_PRINCIPAL) indiceMenuPrincipal = 0;
        necesitaRefrescarLCD = true;
      }
      if (switchPulsadoActual) {
        switch (indiceMenuPrincipal) {
          case 0: proximoEstado = ESTADO_EDITAR_SETPOINTS; break;
          case 1: proximoEstado = ESTADO_ALARMAS_SUBMENU; break;
          case 2:
              proximoEstado = ESTADO_CONFIRMACION_MODO_FUNCIONAMIENTO;
              indiceConfirmacion = 0; // Reiniciar índice a SI
              break;
        }
        necesitaRefrescarLCD = true;
      }
      break;

    case ESTADO_EDITAR_SETPOINTS:
      if (deltaEncoderActual != 0) {
        indiceSubMenuSetpoints += deltaEncoderActual;
        if (indiceSubMenuSetpoints < 0) indiceSubMenuSetpoints = TOTAL_OPCIONES_SUBMENU_SETPOINTS - 1;
        if (indiceSubMenuSetpoints >= TOTAL_OPCIONES_SUBMENU_SETPOINTS) indiceSubMenuSetpoints = 0;
        necesitaRefrescarLCD = true;
      }
      if (switchPulsadoActual) {
        switch (indiceSubMenuSetpoints) {
          case 0: proximoEstado = ESTADO_EDITAR_TEMP; break;
          case 1: proximoEstado = ESTADO_EDITAR_HUM; break;
          case 2: proximoEstado = ESTADO_EDITAR_CO2; break;
          case 3:
              guardarSetpointsEEPROM();
              proximoEstado = ESTADO_MENU_PRINCIPAL;
              break;
        }
        necesitaRefrescarLCD = true;
      }
      break;

    case ESTADO_ALARMAS_SUBMENU:
      if (deltaEncoderActual != 0) {
        indiceSubMenuAlarmas += deltaEncoderActual;
        if (indiceSubMenuAlarmas < 0) indiceSubMenuAlarmas = TOTAL_OPCIONES_SUBMENU_ALARMAS - 1;
        if (indiceSubMenuAlarmas >= TOTAL_OPCIONES_SUBMENU_ALARMAS) indiceSubMenuAlarmas = 0;
        necesitaRefrescarLCD = true;
      }
      if (switchPulsadoActual) {
        switch (indiceSubMenuAlarmas) {
          case 0: proximoEstado = ESTADO_MOSTRAR_ALARMAS; break;
          case 1:
              proximoEstado = ESTADO_EDITAR_ALARMAS;
              indiceEditarAlarmas = 0;
              break;
          case 2: proximoEstado = ESTADO_MENU_PRINCIPAL; break;
        }
        necesitaRefrescarLCD = true;
      }
      break;

    case ESTADO_EDITAR_ALARMAS:
      if (deltaEncoderActual != 0) {
        indiceEditarAlarmas += deltaEncoderActual;
        if (indiceEditarAlarmas < 0) indiceEditarAlarmas = TOTAL_OPCIONES_EDITAR_ALARMAS - 1;
        if (indiceEditarAlarmas >= TOTAL_OPCIONES_EDITAR_ALARMAS) indiceEditarAlarmas = 0;
        necesitaRefrescarLCD = true;
      }
      if (switchPulsadoActual) {
        switch (indiceEditarAlarmas) {
          case 0: proximoEstado = ESTADO_EDITAR_ALARMA_TEMP_MIN; break;
          case 1: proximoEstado = ESTADO_EDITAR_ALARMA_TEMP_MAX; break;
          case 2: proximoEstado = ESTADO_EDITAR_ALARMA_HUM_MIN; break;
          case 3: proximoEstado = ESTADO_EDITAR_ALARMA_HUM_MAX; break;
          case 4: proximoEstado = ESTADO_EDITAR_ALARMA_CO2_MIN; break;
          case 5: proximoEstado = ESTADO_EDITAR_ALARMA_CO2_MAX; break;
          case 6:
              guardarSetpointsEEPROM();
              proximoEstado = ESTADO_ALARMAS_SUBMENU;
              break;
        }
        necesitaRefrescarLCD = true;
      }
      break;

    case ESTADO_CONFIRMACION_MODO_FUNCIONAMIENTO:
        if (deltaEncoderActual != 0) {
            indiceConfirmacion += deltaEncoderActual;
            if (indiceConfirmacion < 0) indiceConfirmacion = TOTAL_OPCIONES_CONFIRMACION - 1;
            if (indiceConfirmacion >= TOTAL_OPCIONES_CONFIRMACION) indiceConfirmacion = 0;
            necesitaRefrescarLCD = true;
        }
        if (switchPulsadoActual) {
            if (indiceConfirmacion == 0) { // SI
                proximoEstado = ESTADO_MODO_FUNCIONAMIENTO;
            } else { // NO
                proximoEstado = ESTADO_MENU_PRINCIPAL;
            }
            necesitaRefrescarLCD = true;
        }
        break;

    case ESTADO_MODO_FUNCIONAMIENTO:
        // En este modo, el encoder no hace nada, solo el switch
        if (switchPulsadoActual) {
            proximoEstado = ESTADO_CONFIRMACION_VOLVER_MENU; // Pasa a preguntar si desea volver
            indiceConfirmacion = 0; // Reiniciar índice a SI
            necesitaRefrescarLCD = true; // Forzar un refresco para la nueva pantalla
        }
        // Las actualizaciones de sensores y Firebase se manejan dentro de manejarModoFuncionamiento
        // para un control más fino. No necesitamos 'necesitaRefrescarLCD = true;' aquí
        // a menos que sea para una transición de estado, lo cual ya se maneja arriba.
        break;

    case ESTADO_CONFIRMACION_VOLVER_MENU: // Nuevo caso para confirmar el regreso al menú
        if (deltaEncoderActual != 0) {
            indiceConfirmacion += deltaEncoderActual;
            if (indiceConfirmacion < 0) indiceConfirmacion = TOTAL_OPCIONES_CONFIRMACION - 1;
            if (indiceConfirmacion >= TOTAL_OPCIONES_CONFIRMACION) indiceConfirmacion = 0;
            necesitaRefrescarLCD = true;
        }
        if (switchPulsadoActual) {
            if (indiceConfirmacion == 0) { // SI
                proximoEstado = ESTADO_MENU_PRINCIPAL;
            } else { // NO
                proximoEstado = ESTADO_MODO_FUNCIONAMIENTO; // Vuelve al modo de funcionamiento
            }
            necesitaRefrescarLCD = true;
        }
        break;

    case ESTADO_EDITAR_TEMP:
    case ESTADO_EDITAR_HUM:
    case ESTADO_EDITAR_CO2:
      if (deltaEncoderActual != 0) {
        if (estadoActualApp == ESTADO_EDITAR_TEMP) {
            setpointTemperatura += (float)deltaEncoderActual * TEMP_PASO;
            if (setpointTemperatura < TEMP_MIN) setpointTemperatura = TEMP_MIN;
            if (setpointTemperatura > TEMP_MAX) setpointTemperatura = TEMP_MAX;
        } else if (estadoActualApp == ESTADO_EDITAR_HUM) {
            setpointHumedad += (float)deltaEncoderActual * HUM_PASO;
            if (setpointHumedad < HUM_MIN) setpointHumedad = HUM_MIN;
            if (setpointHumedad > HUM_MAX) setpointHumedad = HUM_MAX;
        } else if (estadoActualApp == ESTADO_EDITAR_CO2) {
            setpointCO2 += deltaEncoderActual * CO2_PASO;
            if (setpointCO2 < CO2_MIN) setpointCO2 = CO2_MIN;
            if (setpointCO2 > CO2_MAX) setpointCO2 = CO2_MAX;
        }
        necesitaRefrescarLCD = true;
      }
      if (switchPulsadoActual) {
        proximoEstado = ESTADO_EDITAR_SETPOINTS;
        necesitaRefrescarLCD = true;
      }
      break;

    case ESTADO_EDITAR_ALARMA_TEMP_MIN:
    case ESTADO_EDITAR_ALARMA_TEMP_MAX:
    case ESTADO_EDITAR_ALARMA_HUM_MIN:
    case ESTADO_EDITAR_ALARMA_HUM_MAX:
    case ESTADO_EDITAR_ALARMA_CO2_MIN:
    case ESTADO_EDITAR_ALARMA_CO2_MAX:
      if (deltaEncoderActual != 0) {
        if (estadoActualApp == ESTADO_EDITAR_ALARMA_TEMP_MIN) {
            alarmaTempMin += (float)deltaEncoderActual * ALARMA_TEMP_PASO;
            if (alarmaTempMin < ALARMA_TEMP_MIN_RANGO) alarmaTempMin = ALARMA_TEMP_MIN_RANGO;
            if (alarmaTempMin > ALARMA_TEMP_MAX_RANGO) alarmaTempMin = ALARMA_TEMP_MAX_RANGO;
        } else if (estadoActualApp == ESTADO_EDITAR_ALARMA_TEMP_MAX) {
            alarmaTempMax += (float)deltaEncoderActual * ALARMA_TEMP_PASO;
            if (alarmaTempMax < ALARMA_TEMP_MIN_RANGO) alarmaTempMax = ALARMA_TEMP_MIN_RANGO;
            if (alarmaTempMax > ALARMA_TEMP_MAX_RANGO) alarmaTempMax = ALARMA_TEMP_MAX_RANGO;
        } else if (estadoActualApp == ESTADO_EDITAR_ALARMA_HUM_MIN) {
            alarmaHumMin += (float)deltaEncoderActual * ALARMA_HUM_PASO;
            if (alarmaHumMin < ALARMA_HUM_MIN_RANGO) alarmaHumMin = ALARMA_HUM_MIN_RANGO;
            if (alarmaHumMin > ALARMA_HUM_MAX_RANGO) alarmaHumMin = ALARMA_HUM_MAX_RANGO;
        } else if (estadoActualApp == ESTADO_EDITAR_ALARMA_HUM_MAX) {
            alarmaHumMax += (float)deltaEncoderActual * ALARMA_HUM_PASO;
            if (alarmaHumMax < ALARMA_HUM_MIN_RANGO) alarmaHumMax = ALARMA_HUM_MIN_RANGO;
            if (alarmaHumMax > ALARMA_HUM_MAX_RANGO) alarmaHumMax = ALARMA_HUM_MAX_RANGO;
        } else if (estadoActualApp == ESTADO_EDITAR_ALARMA_CO2_MIN) {
            alarmaCO2Min += deltaEncoderActual * ALARMA_CO2_PASO;
            if (alarmaCO2Min < ALARMA_CO2_MIN_RANGO) alarmaCO2Min = ALARMA_CO2_MIN_RANGO;
            if (alarmaCO2Min > ALARMA_CO2_MAX_RANGO) alarmaCO2Min = ALARMA_CO2_MAX_RANGO;
        } else if (estadoActualApp == ESTADO_EDITAR_ALARMA_CO2_MAX) {
            alarmaCO2Max += deltaEncoderActual * ALARMA_CO2_PASO;
            if (alarmaCO2Max < ALARMA_CO2_MIN_RANGO) alarmaCO2Max = ALARMA_CO2_MIN_RANGO;
            if (alarmaCO2Max > ALARMA_CO2_MAX_RANGO) alarmaCO2Max = ALARMA_CO2_MAX_RANGO;
        }
        necesitaRefrescarLCD = true;
      }
      if (switchPulsadoActual) {
        proximoEstado = ESTADO_EDITAR_ALARMAS;
        necesitaRefrescarLCD = true;
      }
      break;

    case ESTADO_MOSTRAR_ALARMAS:
      if (switchPulsadoActual) {
        proximoEstado = ESTADO_ALARMAS_SUBMENU;
        necesitaRefrescarLCD = true;
      }
      break;
  }

  // ETIQUETA: Lógica de transición de estado y refresco de LCD
  if (proximoEstado != estadoActualApp || necesitaRefrescarLCD) {
      if (proximoEstado != estadoActualApp) {
          // Lógica para reiniciar índices al cambiar de estado principal
          if (proximoEstado == ESTADO_MENU_PRINCIPAL) {
              indiceMenuPrincipal = 0;
              desplazamientoScroll = 0;
          } else if (proximoEstado == ESTADO_EDITAR_SETPOINTS) {
              indiceSubMenuSetpoints = 0;
              desplazamientoScroll = 0;
          } else if (proximoEstado == ESTADO_ALARMAS_SUBMENU) {
              indiceSubMenuAlarmas = 0;
              desplazamientoScroll = 0;
          } else if (proximoEstado == ESTADO_EDITAR_ALARMAS) {
              indiceEditarAlarmas = 0;
              desplazamientoScroll = 0;
          } else if (proximoEstado == ESTADO_CONFIRMACION_MODO_FUNCIONAMIENTO || proximoEstado == ESTADO_CONFIRMACION_VOLVER_MENU) {
              indiceConfirmacion = 0; // Siempre iniciar en "SI" para confirmaciones
              desplazamientoScroll = 0;
          } else if (proximoEstado == ESTADO_MODO_FUNCIONAMIENTO) {
              initialMessageDisplayed = false; // ETIQUETA: Reiniciar bandera de mensaje inicial
              lastDisplayedTemp = -999.0; // Forzar la primera visualización de datos de sensores
              lastDisplayedHum = -999.0;
              lastDisplayedCO2 = -999.0;
              // Resetear estado del ventilador al entrar en modo funcionamiento
              estadoVentiladorActual = VENTILADOR_INACTIVO;
              digitalWrite(RELAY1, HIGH); // Asegurarse de que el ventilador esté apagado
              digitalWrite(RELAY2, HIGH); // Asegurarse de que el calefactor esté apagado
          }

          // Lógica para mantener índices al volver de una edición a un submenú
          if (estadoActualApp == ESTADO_EDITAR_TEMP && proximoEstado == ESTADO_EDITAR_SETPOINTS) indiceSubMenuSetpoints = 0;
          if (estadoActualApp == ESTADO_EDITAR_HUM && proximoEstado == ESTADO_EDITAR_SETPOINTS) indiceSubMenuSetpoints = 1;
          if (estadoActualApp == ESTADO_EDITAR_CO2 && proximoEstado == ESTADO_EDITAR_SETPOINTS) indiceSubMenuSetpoints = 2;

          if (estadoActualApp == ESTADO_MOSTRAR_ALARMAS && proximoEstado == ESTADO_ALARMAS_SUBMENU) indiceSubMenuAlarmas = 0;

          if ( (estadoActualApp == ESTADO_EDITAR_ALARMA_TEMP_MIN || estadoActualApp == ESTADO_EDITAR_ALARMA_TEMP_MAX ||
                estadoActualApp == ESTADO_EDITAR_ALARMA_HUM_MIN || estadoActualApp == ESTADO_EDITAR_ALARMA_HUM_MAX ||
                estadoActualApp == ESTADO_EDITAR_ALARMA_CO2_MIN || estadoActualApp == ESTADO_EDITAR_ALARMA_CO2_MAX) &&
                proximoEstado == ESTADO_EDITAR_ALARMAS) {
                    // Mantiene el índice en indiceEditarAlarmas donde estaba
                }

          // Ajustes de índice al volver a MENU_PRINCIPAL desde confirmación o modo funcionamiento
          if ((estadoActualApp == ESTADO_CONFIRMACION_MODO_FUNCIONAMIENTO && proximoEstado == ESTADO_MENU_PRINCIPAL) ||
              (estadoActualApp == ESTADO_CONFIRMACION_VOLVER_MENU && proximoEstado == ESTADO_MENU_PRINCIPAL)) {
              indiceMenuPrincipal = 2; // Vuelve a la opción "Modo Funcionamiento"
          }
      }

      estadoActualApp = proximoEstado;

      // ¡ESTA LÍNEA SE VUELVE A INCLUIR AQUÍ SEGÚN TU PETICIÓN!
      lcd.begin();
      lcd.backlight(true); // Asegurar que la luz de fondo esté encendida
      lcd.clear(); // Limpiar la pantalla antes de dibujar el nuevo estado

      switch (estadoActualApp) {
        case ESTADO_MENU_PRINCIPAL: manejarMenuPrincipal(0, false); break;
        case ESTADO_EDITAR_SETPOINTS: manejarEditarSetpoints(0, false); break;
        case ESTADO_ALARMAS_SUBMENU: manejarSubMenuAlarmas(0, false); break;
        case ESTADO_EDITAR_TEMP: manejarEditarTemperatura(0, false); break;
        case ESTADO_EDITAR_HUM: manejarEditarHumedad(0, false); break;
        case ESTADO_EDITAR_CO2: manejarEditarCO2(0, false); break;
        case ESTADO_MOSTRAR_ALARMAS: manejarMostrarAlarmas(0, false); break;
        case ESTADO_EDITAR_ALARMAS: manejarEditarAlarmas(0, false); break;
        case ESTADO_CONFIRMACION_MODO_FUNCIONAMIENTO: manejarConfirmacionModoFuncionamiento(0, false); break;
        case ESTADO_MODO_FUNCIONAMIENTO: manejarModoFuncionamiento(0, false); break;
        case ESTADO_CONFIRMACION_VOLVER_MENU: manejarConfirmacionVolverMenu(0, false); break;
        case ESTADO_EDITAR_ALARMA_TEMP_MIN: manejarEditarAlarmaTempMin(0, false); break;
        case ESTADO_EDITAR_ALARMA_TEMP_MAX: manejarEditarAlarmaTempMax(0, false); break;
        case ESTADO_EDITAR_ALARMA_HUM_MIN: manejarEditarAlarmaHumMin(0, false); break;
        case ESTADO_EDITAR_ALARMA_HUM_MAX: manejarEditarAlarmaHumMax(0, false); break;
        case ESTADO_EDITAR_ALARMA_CO2_MIN: manejarEditarAlarmaCO2Min(0, false); break;
        case ESTADO_EDITAR_ALARMA_CO2_MAX: manejarEditarAlarmaCO2Max(0, false); break;
      }
      necesitaRefrescarLCD = false;
  }

  // ETIQUETA: Lógica de modo de funcionamiento (ejecución continua)
  // Se llama a manejarModoFuncionamiento continuamente cuando el estado es MODO_FUNCIONAMIENTO
  if (estadoActualApp == ESTADO_MODO_FUNCIONAMIENTO) {
      manejarModoFuncionamiento(deltaEncoderActual, switchPulsadoActual);
  }

  delay(10);
}

// ---------------- FUNCIONES DE INTERRUPCIÓN (ISR) ----------------
void IRAM_ATTR leerEncoderISR() {
  unsigned long ahora = millis();
  if (ahora - tiempoUltimaLecturaCLK > RETARDO_ANTI_REBOTE_ENCODER) {
    estadoClk = digitalRead(CLOCK_ENCODER);
    estadoDt = digitalRead(DT_ENCODER);

    if (estadoClk != estadoClkAnterior) {
      if (estadoDt != estadoClk) {
        valorEncoder++;
      } else {
        valorEncoder--;
      }
    }
    estadoClkAnterior = estadoClk;
    tiempoUltimaLecturaCLK = ahora;
  }
}

void IRAM_ATTR leerSwitchISR() {
  unsigned long ahora = millis();
  if (ahora - tiempoUltimaPresionSW > RETARDO_ANTI_REBOTE_SW) {
    if (digitalRead(SW_ENCODER) == LOW) {
      swFuePresionadoISR = true;
      // necesitaRefrescarLCD = true; // No lo activamos aquí para no saturar en el modo funcionamiento
                                    // La lógica de refresco para la salida de modo funcionamiento está en el loop principal
    }
    tiempoUltimaPresionSW = ahora;
  }
}

// ---------------- FUNCIONES DE MANEJO DE ESTADO (Solo dibujan) ----------------

void manejarMenuPrincipal(int deltaEncoder, bool pulsadoSwitch) {
  if (indiceMenuPrincipal < desplazamientoScroll) {
      desplazamientoScroll = indiceMenuPrincipal;
  } else if (indiceMenuPrincipal >= desplazamientoScroll + OPCIONES_VISIBLES_PANTALLA) {
      desplazamientoScroll = indiceMenuPrincipal - OPCIONES_VISIBLES_PANTALLA + 1;
  }

  String opcionesMenuPrincipal[] = {
    "Setpoints",
    "Alarmas",
    "Modo Funcionamiento"
  };

  for (int i = 0; i < OPCIONES_VISIBLES_PANTALLA; i++) {
    int indiceOpcion = desplazamientoScroll + i;

    if (indiceOpcion < TOTAL_OPCIONES_MENU_PRINCIPAL) {
      String textoOpcion = opcionesMenuPrincipal[indiceOpcion];
      String lineaMostrada = (indiceOpcion == indiceMenuPrincipal ? "> " : "  ") + textoOpcion;

      int len = lineaMostrada.length();
      if (len > LCD_COLUMNS) {
          lineaMostrada = lineaMostrada.substring(0, LCD_COLUMNS);
      } else {
          for(int k=0; k < (LCD_COLUMNS - len); k++) {
              lineaMostrada += " ";
          }
      }
      lcd.print(lineaMostrada, 0, i);
    } else {
      lcd.print("                    ", 0, i);
    }
  }
}

void manejarEditarSetpoints(int deltaEncoder, bool pulsadoSwitch) {
  if (indiceSubMenuSetpoints < desplazamientoScroll) {
      desplazamientoScroll = indiceSubMenuSetpoints;
  } else if (indiceSubMenuSetpoints >= desplazamientoScroll + OPCIONES_VISIBLES_PANTALLA) {
      desplazamientoScroll = indiceSubMenuSetpoints - OPCIONES_VISIBLES_PANTALLA + 1;
  }

  String opcionesSubMenuSetpoints[] = {
    "T: " + String(setpointTemperatura, 1) + " C",
    "H: " + String(setpointHumedad, 0) + " %",
    "CO2: " + String(setpointCO2) + " ppm",
    "Guardar y Volver"
  };

  for (int i = 0; i < OPCIONES_VISIBLES_PANTALLA; i++) {
    int indiceOpcion = desplazamientoScroll + i;

    if (indiceOpcion < TOTAL_OPCIONES_SUBMENU_SETPOINTS) {
      String linea = (indiceOpcion == indiceSubMenuSetpoints ? "> " : "  ") + opcionesSubMenuSetpoints[indiceOpcion];

      int len = linea.length();
      if (len > LCD_COLUMNS) {
          linea = linea.substring(0, LCD_COLUMNS);
      } else {
          for(int k=0; k < (LCD_COLUMNS - len); k++) {
              linea += " ";
          }
      }
      lcd.print(linea, 0, i);
    } else {
      lcd.print("                    ", 0, i);
    }
  }
}

void manejarSubMenuAlarmas(int deltaEncoder, bool pulsadoSwitch) {
  if (indiceSubMenuAlarmas < desplazamientoScroll) {
      desplazamientoScroll = indiceSubMenuAlarmas;
  } else if (indiceSubMenuAlarmas >= desplazamientoScroll + OPCIONES_VISIBLES_PANTALLA) {
      desplazamientoScroll = indiceSubMenuAlarmas - OPCIONES_VISIBLES_PANTALLA + 1;
  }

  String opcionesSubMenuAlarmas[] = {
    "Mostrar Alarmas",
    "Editar Alarmas",
    "Volver al Menu"
  };

  for (int i = 0; i < OPCIONES_VISIBLES_PANTALLA; i++) {
    int indiceOpcion = desplazamientoScroll + i;

    if (indiceOpcion < TOTAL_OPCIONES_SUBMENU_ALARMAS) {
      String linea = (indiceOpcion == indiceSubMenuAlarmas ? "> " : "  ") + opcionesSubMenuAlarmas[indiceOpcion];

      int len = linea.length();
      if (len > LCD_COLUMNS) {
          linea = linea.substring(0, LCD_COLUMNS);
      } else {
          for(int k=0; k < (LCD_COLUMNS - len); k++) {
              linea += " ";
          }
      }
      lcd.print(linea, 0, i);
    } else {
      lcd.print("                    ", 0, i);
    }
  }
}

void manejarEditarAlarmas(int deltaEncoder, bool pulsadoSwitch) {
  if (indiceEditarAlarmas < desplazamientoScroll) {
      desplazamientoScroll = indiceEditarAlarmas;
  } else if (indiceEditarAlarmas >= desplazamientoScroll + OPCIONES_VISIBLES_PANTALLA) {
      desplazamientoScroll = indiceEditarAlarmas - OPCIONES_VISIBLES_PANTALLA + 1;
  }

  String opcionesEditarAlarmas[] = {
    "T Min: " + String(alarmaTempMin, 1) + " C",
    "T Max: " + String(alarmaTempMax, 1) + " C",
    "H Min: " + String(alarmaHumMin, 0) + " %",
    "H Max: " + String(alarmaHumMax, 0) + " %",
    "CO2 Min: " + String(alarmaCO2Min) + " ppm",
    "CO2 Max: " + String(alarmaCO2Max) + " "
"ppm",
    "Guardar y Volver"
  };

  for (int i = 0; i < OPCIONES_VISIBLES_PANTALLA; i++) {
    int indiceOpcion = desplazamientoScroll + i;

    if (indiceOpcion < TOTAL_OPCIONES_EDITAR_ALARMAS) {
      String linea = (indiceOpcion == indiceEditarAlarmas ? "> " : "  ") + opcionesEditarAlarmas[indiceOpcion];

      int len = linea.length();
      if (len > LCD_COLUMNS) {
          linea = linea.substring(0, LCD_COLUMNS);
      } else {
          for(int k=0; k < (LCD_COLUMNS - len); k++) {
              linea += " ";
          }
      }
      lcd.print(linea, 0, i);
    } else {
      lcd.print("                    ", 0, i);
    }
  }
}

void manejarMostrarAlarmas(int deltaEncoder, bool pulsadoSwitch) {
  lcd.print("Mostrar Alarmas     ", 0, 0);
  lcd.print("T Min:" + String(alarmaTempMin, 1) + " Max:" + String(alarmaTempMax, 1), 0, 1);
  lcd.print("H Min:" + String(alarmaHumMin, 0) + " Max:" + String(alarmaHumMax, 0), 0, 2);
  lcd.print("CO2 Min:" + String(alarmaCO2Min) + " Max:" + String(alarmaCO2Max), 0, 3);
}

void manejarConfirmacionModoFuncionamiento(int deltaEncoder, bool pulsadoSwitch) {
    lcd.print("Cambiar a M. Fun?", 0, 0);
    lcd.print("                    ", 0, 1);

    String opcionesConfirmacion[] = {"SI", "NO"};

    for (int i = 0; i < TOTAL_OPCIONES_CONFIRMACION; i++) {
        String linea = (i == indiceConfirmacion ? "> " : "  ") + opcionesConfirmacion[i];
        lcd.print(linea, 0, 2 + i);
    }
}

void manejarConfirmacionVolverMenu(int deltaEncoder, bool pulsadoSwitch) {
    lcd.print("Volver al Menu?", 0, 0);
    lcd.print("                    ", 0, 1);

    String opcionesConfirmacion[] = {"SI", "NO"};

    for (int i = 0; i < TOTAL_OPCIONES_CONFIRMACION; i++) {
        String linea = (i == indiceConfirmacion ? "> " : "  ") + opcionesConfirmacion[i];
        lcd.print(linea, 0, 2 + i);
    }
}

// ETIQUETA: Implementacion de la funcion para leer sensores
void leerSensores() {
  dhtSensor.leerValores();
  float temp = dhtSensor.getTemperatura();
  float hum = dhtSensor.getHumedad();

  // Solo leer CO2 si el sensor esta listo
  float mq2Co2 = -1.0;
  if (gasSensor.isReady()) {
    mq2Co2 = gasSensor.leerCO2(); // Asumiendo que esta es la funcion para leer CO2 de MQ2
  } else {
    Serial.println("Calentando sensor MQ2, CO2 no disponible.");
  }

  // Actualizar valores actuales solo si son validos
  if (!isnan(temp) && !isnan(hum)) {
    currentTemp = temp;
    currentHum = hum;
  }
  if (mq2Co2 >= 0) { // MQ2Sensor.leerCO2() devuelve 0 si no esta listo, o un valor estimado
    currentCO2 = mq2Co2;
  }

  // Lógica para la bandera de CO2 bajo
  if (estaEnDefectoCO2()) {
      banderaBajoCO2 = true;
      Serial.println("ALARMA: CO2 BAJO DETECTADO. POSIBLE INCUBADORA MAL CERRADA.");
  } else {
      banderaBajoCO2 = false;
  }
}

// ETIQUETA: Implementacion de manejarModoFuncionamiento
void manejarModoFuncionamiento(int deltaEncoder, bool pulsadoSwitch) {
    unsigned long currentMillis = millis();

    // Mostrar mensaje inicial por 3 segundos
    if (!initialMessageDisplayed) {
        lcd.clear();
        lcd.print("Inicializando", 0, 0);
        lcd.print("sistema...", 0, 1);
        modoFuncionamientoStartTime = currentMillis;
        initialMessageDisplayed = true;
    }

    // Si todavía estamos dentro del tiempo del mensaje inicial, no hacemos nada más
    if (currentMillis - modoFuncionamientoStartTime < 3000) {
        return;
    }

    // Leer sensores
    leerSensores();

    // --- Control del Calefactor (Relé 2 - D0) ---
    // Enciende si la temperatura cae por debajo del setpoint - 2°C
    if (currentTemp <= setpointTemperatura - HISTERESIS_TEMP_ENCENDER_CALEFACTOR) {
        digitalWrite(RELAY2, LOW); // Enciende el calefactor
    }
    // Apaga si la temperatura sube por encima del setpoint + 1.5°C
    else if (currentTemp >= setpointTemperatura + HISTERESIS_TEMP_APAGAR_CALEFACTOR) {
        digitalWrite(RELAY2, HIGH); // Apaga el calefactor
    }

    // --- Control del Ventilador (Relé 1 - D8) ---

    // Lógica para la ventilación programada de CO2
    // PRIORIDAD: Solo activar ventilación programada si el ventilador está inactivo
    // y no hay condiciones de exceso activando el ventilador por umbrales.
    if (!ventilacionProgramadaActiva &&
        (currentMillis - ultimoTiempoVentilacionProgramada >= INTERVALO_VENTILACION_PROGRAMADA) &&
        (estadoVentiladorActual == VENTILADOR_INACTIVO) &&
        !estaEnExcesoTemperatura() && !estaEnExcesoHumedad() && !estaEnExcesoCO2() )
    {
        ventilacionProgramadaActiva = true;
        digitalWrite(RELAY1, LOW); // Enciende el ventilador para pulso programado
        temporizadorActivoVentilador = currentMillis; // Reiniciar temporizador del ventilador para la duración programada
        Serial.println("Ventilación programada de CO2 iniciada.");
    }

    if (ventilacionProgramadaActiva) {
        if (currentMillis - temporizadorActivoVentilador >= DURACION_VENTILACION_PROGRAMADA) {
            digitalWrite(RELAY1, HIGH); // Apaga el ventilador
            ventilacionProgramadaActiva = false;
            ultimoTiempoVentilacionProgramada = currentMillis; // Actualizar el tiempo de la última ventilación
            Serial.println("Ventilación programada de CO2 finalizada.");
        }
        // Mientras la ventilación programada está activa, la lógica de histeresis del ventilador se pausará.
        // Después de que la ventilación programada finalice, el sistema volverá a evaluar las condiciones por umbral.
    } else {
        // Lógica de control por histeresis (solo si no hay ventilación programada activa)
        switch (estadoVentiladorActual) {
            case VENTILADOR_INACTIVO:
                // Verificar si alguna condición de activación se cumple
                if (estaEnExcesoTemperatura()) {
                    razonActivacionVentilador = TEMPERATURA_ALTA;
                    estadoVentiladorActual = VENTILADOR_ENCENDIDO_3S;
                    temporizadorActivoVentilador = currentMillis;
                    digitalWrite(RELAY1, LOW); // Enciende el ventilador
                    Serial.println("Ventilador ON (Temp alta)");
                } else if (estaEnExcesoHumedad()) {
                    razonActivacionVentilador = HUMEDAD_ALTA;
                    estadoVentiladorActual = VENTILADOR_ENCENDIDO_3S;
                    temporizadorActivoVentilador = currentMillis;
                    digitalWrite(RELAY1, LOW); // Enciende el ventilador
                    Serial.println("Ventilador ON (Humedad alta)");
                } else if (estaEnExcesoCO2()) {
                    razonActivacionVentilador = CO2_ALTO;
                    estadoVentiladorActual = VENTILADOR_ENCENDIDO_3S;
                    temporizadorActivoVentilador = currentMillis;
                    digitalWrite(RELAY1, LOW); // Enciende el ventilador
                    Serial.println("Ventilador ON (CO2 alto)");
                }
                break;

            case VENTILADOR_ENCENDIDO_3S:
                if (currentMillis - temporizadorActivoVentilador >= 3000) { // 3 segundos
                    digitalWrite(RELAY1, HIGH); // Apaga el ventilador
                    estadoVentiladorActual = VENTILADOR_APAGADO_5S;
                    temporizadorActivoVentilador = currentMillis;
                    Serial.println("Ventilador OFF (después de 3s)");
                }
                break;

            case VENTILADOR_APAGADO_5S:
                if (currentMillis - temporizadorActivoVentilador >= 5000) { // 5 segundos
                    estadoVentiladorActual = VENTILADOR_ENCENDIDO_5S; // Pasar al siguiente pulso
                    temporizadorActivoVentilador = currentMillis;
                    digitalWrite(RELAY1, LOW); // Enciende el ventilador de nuevo
                    Serial.println("Ventilador ON (pulso de 5s)");
                }
                break;

            case VENTILADOR_ENCENDIDO_5S:
                if (currentMillis - temporizadorActivoVentilador >= 5000) { // 5 segundos
                    digitalWrite(RELAY1, HIGH); // Apaga el ventilador
                    estadoVentiladorActual = VENTILADOR_ESPERA_ESTABILIZACION;
                    temporizadorActivoVentilador = currentMillis;
                    Serial.println("Ventilador OFF (después de 5s)");
                }
                break;

            case VENTILADOR_ESPERA_ESTABILIZACION:
                if (currentMillis - temporizadorActivoVentilador >= 15000) { // 15 segundos
                    // Evaluar si las condiciones han mejorado
                    bool condicionesMejoraron = false;
                    switch (razonActivacionVentilador) {
                        case TEMPERATURA_ALTA:
                            if (currentTemp <= setpointTemperatura + UMBRAL_TEMP_APAGAR_VENTILADOR) {
                                condicionesMejoraron = true;
                            }
                            break;
                        case HUMEDAD_ALTA:
                            if (currentHum <= setpointHumedad + UMBRAL_HUM_APAGAR_VENTILADOR) {
                                condicionesMejoraron = true;
                            }
                            break;
                        case CO2_ALTO:
                            if (currentCO2 <= setpointCO2 + UMBRAL_CO2_APAGAR_VENTILADOR) {
                                condicionesMejoraron = true;
                            }
                            break;
                        case NINGUNA: // No debería llegar aquí si el ventilador está activo
                            condicionesMejoraron = true;
                            break;
                    }

                    if (condicionesMejoraron) {
                        estadoVentiladorActual = VENTILADOR_INACTIVO; // Vuelve a inactivo, condiciones mejoraron
                        razonActivacionVentilador = NINGUNA;
                        Serial.println("Ventilador IDLE (condiciones mejoraron)");
                    } else {
                        estadoVentiladorActual = VENTILADOR_ENCENDIDO_5S; // Repite el ciclo de 5s, condiciones no mejoraron
                        temporizadorActivoVentilador = currentMillis;
                        digitalWrite(RELAY1, LOW); // Enciende el ventilador de nuevo
                        Serial.println("Ventilador ON (reinicio ciclo, condiciones no mejoraron)");
                    }
                }
                break;
        }
    }


    // Comprobar cambios y actualizar LCD
    bool changed = false;
    if (abs(currentTemp - lastDisplayedTemp) > TEMP_DISPLAY_THRESHOLD || lastDisplayedTemp == -999.0) {
        lastDisplayedTemp = currentTemp;
        changed = true;
    }
    if (abs(currentHum - lastDisplayedHum) > HUM_DISPLAY_THRESHOLD || lastDisplayedHum == -999.0) {
        lastDisplayedHum = currentHum;
        changed = true;
    }
    if (abs(currentCO2 - lastDisplayedCO2) > CO2_DISPLAY_THRESHOLD || lastDisplayedCO2 == -999.0) {
        lastDisplayedCO2 = currentCO2;
        changed = true;
    }

    if (changed) {
        lcd.clear(); // Limpiar solo cuando hay nuevos datos para imprimir
        lcd.print("T: " + String(lastDisplayedTemp, 1) + " C", 0, 0);
        lcd.print("H: " + String(lastDisplayedHum, 0) + " %", 0, 1);
        lcd.print("CO2: " + String((int)lastDisplayedCO2) + " ppm", 0, 2);
        lcd.print("SW para Menu        ", 0, 3); // Mantener el mensaje para salir
    }

    // Envío de datos de sensores a Firebase (se basa en la lógica interna de sendData para cambios significativos)
    if(firebase.sendData(currentTemp, currentHum, currentCO2)) {
        Serial.println("Datos enviados a Firebase");
    } else {
        Serial.println("Sin cambios significativos (o error de Firebase)");
    }
}


// ---------------- FUNCIONES EEPROM ----------------
void guardarSetpointsEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  int addr = 0;
  EEPROM.put(addr, setpointTemperatura); addr += sizeof(float);
  EEPROM.put(addr, setpointHumedad);     addr += sizeof(float);
  EEPROM.put(addr, setpointCO2);         addr += sizeof(int);

  EEPROM.put(addr, alarmaTempMin);       addr += sizeof(float);
  EEPROM.put(addr, alarmaTempMax);       addr += sizeof(float);
  EEPROM.put(addr, alarmaHumMin);        addr += sizeof(float);
  EEPROM.put(addr, alarmaHumMax);        addr += sizeof(float);
  EEPROM.put(addr, alarmaCO2Min);        addr += sizeof(int);
  EEPROM.put(addr, alarmaCO2Max);        addr += sizeof(int);

  EEPROM.commit();
  EEPROM.end();
  Serial.println("Setpoints y Alarmas guardados en EEPROM.");
}

void cargarSetpointsEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  int addr = 0;
  EEPROM.get(addr, setpointTemperatura); addr += sizeof(float);
  EEPROM.get(addr, setpointHumedad);     addr += sizeof(float);
  EEPROM.get(addr, setpointCO2);         addr += sizeof(int);

  EEPROM.get(addr, alarmaTempMin);       addr += sizeof(float);
  EEPROM.get(addr, alarmaTempMax);       addr += sizeof(float);
  EEPROM.get(addr, alarmaHumMin);        addr += sizeof(float);
  EEPROM.get(addr, alarmaHumMax);        addr += sizeof(float);
  EEPROM.get(addr, alarmaCO2Min);        addr += sizeof(int);
  EEPROM.get(addr, alarmaCO2Max);        addr += sizeof(int);

  EEPROM.end();

  // Validaciones para Setpoints
  if (setpointTemperatura < TEMP_MIN || setpointTemperatura > TEMP_MAX || isnan(setpointTemperatura)) {
    setpointTemperatura = 25.0;
  }
  if (setpointHumedad < HUM_MIN || setpointHumedad > HUM_MAX || isnan(setpointHumedad)) {
    setpointHumedad = 70.0;
  }
  if (setpointCO2 < CO2_MIN || setpointCO2 > CO2_MAX) { // Usar los nuevos rangos ampliados
    setpointCO2 = 6000; // Nuevo valor por defecto
  }

  // Validaciones para Alarmas
  if (alarmaTempMin < ALARMA_TEMP_MIN_RANGO || alarmaTempMin > ALARMA_TEMP_MAX_RANGO || isnan(alarmaTempMin)) {
      alarmaTempMin = 18.0;
  }
  if (alarmaTempMax < ALARMA_TEMP_MIN_RANGO || alarmaTempMax > ALARMA_TEMP_MAX_RANGO || isnan(alarmaTempMax)) {
      alarmaTempMax = 30.0;
  }
  if (alarmaHumMin < ALARMA_HUM_MIN_RANGO || alarmaHumMin > ALARMA_HUM_MAX_RANGO || isnan(alarmaHumMin)) {
      alarmaHumMin = 50.0;
  }
  if (alarmaHumMax < ALARMA_HUM_MIN_RANGO || alarmaHumMax > ALARMA_HUM_MAX_RANGO || isnan(alarmaHumMax)) {
      alarmaHumMax = 90.0;
  }
  if (alarmaCO2Min < ALARMA_CO2_MIN_RANGO || alarmaCO2Min > ALARMA_CO2_MAX_RANGO) { // Usar nuevos rangos
      alarmaCO2Min = 600; // Por revisar con el usuario
  }
  if (alarmaCO2Max < ALARMA_CO2_MIN_RANGO || alarmaCO2Max > ALARMA_CO2_MAX_RANGO) { // Usar nuevos rangos
      alarmaCO2Max = 1200; // Por revisar con el usuario
  }

  Serial.println("Setpoints y Alarmas cargados de EEPROM:");
  Serial.printf("Temp: %.1f, Hum: %.1f, CO2: %d\n", setpointTemperatura, setpointHumedad, setpointCO2);
  Serial.printf("Alarma Temp Min: %.1f, Max: %.1f\n", alarmaTempMin, alarmaTempMax);
  Serial.printf("Alarma Hum Min: %.1f, Max: %.1f\n", alarmaHumMin, alarmaHumMax);
  Serial.printf("Alarma CO2 Min: %d, Max: %d\n", alarmaCO2Min, alarmaCO2Max);
}

void manejarEditarTemperatura(int deltaEncoder, bool pulsadoSwitch) {
  lcd.print("Temperatura         ", 0, 0);
  lcd.print(String(setpointTemperatura, 1) + " C        ", 0, 1);
  lcd.print("                    ", 0, 2);
  lcd.print("SW para volver      ", 0, 3);
}

void manejarEditarHumedad(int deltaEncoder, bool pulsadoSwitch) {
  lcd.print("Humedad             ", 0, 0);
  lcd.print(String(setpointHumedad, 0) + " %        ", 0, 1);
  lcd.print("                    ", 0, 2);
  lcd.print("SW para volver      ", 0, 3);
}

void manejarEditarCO2(int deltaEncoder, bool pulsadoSwitch) {
  lcd.print("CO2                 ", 0, 0);
  lcd.print(String(setpointCO2) + " ppm       ", 0, 1);
  lcd.print("                    ", 0, 2);
  lcd.print("SW para volver      ", 0, 3);
}

void manejarEditarAlarmaTempMin(int deltaEncoder, bool pulsadoSwitch) {
  lcd.print("Alarma Temp Min     ", 0, 0);
  lcd.print(String(alarmaTempMin, 1) + " C        ", 0, 1);
  lcd.print("                    ", 0, 2);
  lcd.print("SW para volver      ", 0, 3);
}

void manejarEditarAlarmaTempMax(int deltaEncoder, bool pulsadoSwitch) {
  lcd.print("Alarma Temp Max     ", 0, 0);
  lcd.print(String(alarmaTempMax, 1) + " C        ", 0, 1);
  lcd.print("                    ", 0, 2);
  lcd.print("SW para volver      ", 0, 3);
}

void manejarEditarAlarmaHumMin(int deltaEncoder, bool pulsadoSwitch) {
  lcd.print("Alarma Hum Min      ", 0, 0);
  lcd.print(String(alarmaHumMin, 0) + " %        ", 0, 1);
  lcd.print("                    ", 0, 2);
  lcd.print("SW para volver      ", 0, 3);
}

void manejarEditarAlarmaHumMax(int deltaEncoder, bool pulsadoSwitch) {
  lcd.print("Alarma Hum Max      ", 0, 0);
  lcd.print(String(alarmaHumMax, 0) + " %        ", 0, 1);
  lcd.print("                    ", 0, 2);
  lcd.print("SW para volver      ", 0, 3);
}

void manejarEditarAlarmaCO2Min(int deltaEncoder, bool pulsadoSwitch) {
  lcd.print("Alarma CO2 Min      ", 0, 0);
  lcd.print(String(alarmaCO2Min) + " ppm       ", 0, 1);
  lcd.print("                    ", 0, 2);
  lcd.print("SW para volver      ", 0, 3);
}

void manejarEditarAlarmaCO2Max(int deltaEncoder, bool pulsadoSwitch) {
  lcd.print("Alarma CO2 Max      ", 0, 0);
  lcd.print(String(alarmaCO2Max) + " ppm       ", 0, 1);
  lcd.print("                    ", 0, 2);
  lcd.print("SW para volver      ", 0, 3);
}

// ---------------- FUNCIONES AUXILIARES DE ESTADO DEL SISTEMA ----------------
bool estaEnExcesoTemperatura() {
    return currentTemp > (setpointTemperatura + UMBRAL_TEMP_VENTILADOR);
}

bool estaEnExcesoHumedad() {
    return currentHum > (setpointHumedad + UMBRAL_HUM_VENTILADOR);
}

bool estaEnExcesoCO2() {
    return currentCO2 > (setpointCO2 + UMBRAL_CO2_VENTILADOR);
}

bool estaEnDefectoCO2() {
    return currentCO2 < alarmaCO2Min; // Usa la alarmaCO2Min para detectar CO2 bajo
}