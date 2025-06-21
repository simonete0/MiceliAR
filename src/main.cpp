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

// ETIQUETA: Umbrales de proximidad para validación de alarmas (Nuevas)
const float UMBRAL_PROXIMIDAD_TEMP = 0.5; // La alarma no puede estar a menos de 0.5°C del setpoint
const float UMBRAL_PROXIMIDAD_HUM = 1.0;  // La alarma no puede estar a menos de 1% de la humedad del setpoint
const int UMBRAL_PROXIMIDAD_CO2 = 50;    // La alarma no puede estar a menos de 50 ppm del CO2 del setpoint

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

// ETIQUETA: Banderas de estado de alarmas (Nuevas)
bool banderaAlarmaTempMinActiva = false;
bool banderaAlarmaTempMaxActiva = false;
bool banderaAlarmaHumMinActiva = false;
bool banderaAlarmaHumMaxActiva = false;
bool banderaAlarmaCO2MinActiva = false;
bool banderaAlarmaCO2MaxActiva = false;
bool algunaAlarmaActiva = false; // Flag general para saber si hay alguna alarma activa


// Variable para el estado guardado en EEPROM
// 0: Modo Menu, 1: Modo Funcionamiento
uint8_t lastAppStateFlag = 0;
const int EEPROM_APP_STATE_ADDR = EEPROM_SIZE - sizeof(uint8_t); // Último byte de la EEPROM

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

// ETIQUETA: Prototipo de funcion para actualizar el estado de las alarmas (Nueva)
void actualizarEstadoAlarmas();

// Funciones para EEPROM
void guardarSetpointsEEPROM();
void cargarSetpointsEEPROM();
void guardarEstadoAppEEPROM();
void cargarEstadoAppEEPROM();

// Funciones auxiliares para la prioridad de ventilación
bool estaEnExcesoTemperatura();
bool estaEnExcesoHumedad();
bool estaEnExcesoCO2();
bool estaEnDefectoCO2();

// ETIQUETA: Prototipo de funcion para mostrar mensajes de alarma en el display (Modificada)
void mostrarMensajeAlarmaEnDisplay(String tipoAlarma);


// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);

  // ETIQUETA: Configuracion de pines de 'prueba encoder'
  // *** Asegurar que los relés estén APAGADOS desde el inicio ***
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  digitalWrite(RELAY1, HIGH);  // Apaga relé 1 (Ventilador) - HIGH
  digitalWrite(RELAY2, HIGH);  // Apaga relé 2 (Calefactor) - HIGH
  delay(100);  // Pequeña espera para estabilización

  pinMode(CLOCK_ENCODER, INPUT_PULLUP);
  pinMode(DT_ENCODER, INPUT_PULLUP);
  pinMode(SW_ENCODER, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(CLOCK_ENCODER), leerEncoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(SW_ENCODER), leerSwitchISR, FALLING);

  estadoClkAnterior = digitalRead(CLOCK_ENCODER);

  Wire.begin(I2C_SDA, I2C_SCL);
  lcd.begin();
  lcd.backlight(true); // Asegurar que la luz de fondo esté encendida
  // *** Mensaje inicial en el LCD para indicar que está iniciando ***
  lcd.clear();
  lcd.print("Iniciando sistema...", 0, 0);
  lcd.print("Cargando datos...", 0, 1);
  delay(2000); // Dar tiempo para leer el mensaje

  cargarSetpointsEEPROM();
  cargarEstadoAppEEPROM(); // Cargar el último estado de la aplicación

  // ETIQUETA: Inicializacion de sensores y Firebase de 'prueba encoder'
  pinMode(DHTPIN, INPUT_PULLUP); // Fuerza modo correcto para GPIO14
  delay(100);
  dhtSensor.iniciar();
  delay(2000); // Espera crítica para DHT11
  gasSensor.begin();
  firebase.begin(WIFI_SSID, WIFI_PASSWORD,
                FIREBASE_API_KEY, FIREBASE_URL,
                FIREBASE_EMAIL, FIREBASE_PASSWORD);

  // Si el último estado guardado fue "Modo Funcionamiento", inicia en ese estado
  if (lastAppStateFlag == 1) {
      estadoActualApp = ESTADO_MODO_FUNCIONAMIENTO;
  } else {
      estadoActualApp = ESTADO_MENU_PRINCIPAL;
  }

  necesitaRefrescarLCD = true;
  // No resetear indiceMenuPrincipal si se carga de EEPROM y se va a Modo Funcionamiento
  if (estadoActualApp == ESTADO_MENU_PRINCIPAL) {
      indiceMenuPrincipal = 0;
  }

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
                guardarEstadoAppEEPROM(); // Guardar el estado de Modo Funcionamiento en EEPROM
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

    case ESTADO_CONFIRMACION_VOLVER_MENU:
      // Nuevo caso para confirmar el regreso al menú
      if (deltaEncoderActual != 0) {
        indiceConfirmacion += deltaEncoderActual;
        if (indiceConfirmacion < 0) indiceConfirmacion = TOTAL_OPCIONES_CONFIRMACION - 1;
        if (indiceConfirmacion >= TOTAL_OPCIONES_CONFIRMACION) indiceConfirmacion = 0;
        necesitaRefrescarLCD = true;
      }
      if (switchPulsadoActual) {
        if (indiceConfirmacion == 0) { // SI
          proximoEstado = ESTADO_MENU_PRINCIPAL;
          guardarEstadoAppEEPROM(); // Guardar el estado de Menu Principal en EEPROM
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
      if (deltaEncoderActual != 0) {
        alarmaTempMin += (float)deltaEncoderActual * ALARMA_TEMP_PASO;
        if (alarmaTempMin < ALARMA_TEMP_MIN_RANGO) alarmaTempMin = ALARMA_TEMP_MIN_RANGO;
        if (alarmaTempMin > ALARMA_TEMP_MAX_RANGO) alarmaTempMin = ALARMA_TEMP_MAX_RANGO;
        necesitaRefrescarLCD = true;
      }
      if (switchPulsadoActual) {
        // VALIDACION: Alarma Temp Min no puede ser mayor o muy cercana al Setpoint de Temperatura
        if (alarmaTempMin >= setpointTemperatura - UMBRAL_PROXIMIDAD_TEMP) {
          lcd.clear();
          lcd.print("Alarma fuera de rango", 0, 0);
          lcd.print("Set Temp = " + String(setpointTemperatura, 1) + "C", 0, 1);
          delay(2000); // Muestra el mensaje por 2 segundos
          necesitaRefrescarLCD = true; // Forzar redibujado del menú de edición de alarma
        } else {
          proximoEstado = ESTADO_EDITAR_ALARMAS;
          necesitaRefrescarLCD = true;
        }
      }
      break;

    case ESTADO_EDITAR_ALARMA_TEMP_MAX:
      if (deltaEncoderActual != 0) {
        alarmaTempMax += (float)deltaEncoderActual * ALARMA_TEMP_PASO;
        if (alarmaTempMax < ALARMA_TEMP_MIN_RANGO) alarmaTempMax = ALARMA_TEMP_MIN_RANGO;
        if (alarmaTempMax > ALARMA_TEMP_MAX_RANGO) alarmaTempMax = ALARMA_TEMP_MAX_RANGO;
        necesitaRefrescarLCD = true;
      }
      if (switchPulsadoActual) {
        // VALIDACION: Alarma Temp Max no puede ser menor o muy cercana al Setpoint de Temperatura
        if (alarmaTempMax <= setpointTemperatura + UMBRAL_PROXIMIDAD_TEMP) {
          lcd.clear();
          lcd.print("Alarma fuera de rango", 0, 0);
          lcd.print("Set Temp = " + String(setpointTemperatura, 1) + "C", 0, 1);
          delay(2000);
          necesitaRefrescarLCD = true;
        } else {
          proximoEstado = ESTADO_EDITAR_ALARMAS;
          necesitaRefrescarLCD = true;
        }
      }
      break;

    case ESTADO_EDITAR_ALARMA_HUM_MIN:
      if (deltaEncoderActual != 0) {
        alarmaHumMin += (float)deltaEncoderActual * ALARMA_HUM_PASO;
        if (alarmaHumMin < ALARMA_HUM_MIN_RANGO) alarmaHumMin = ALARMA_HUM_MIN_RANGO;
        if (alarmaHumMin > ALARMA_HUM_MAX_RANGO) alarmaHumMin = ALARMA_HUM_MAX_RANGO;
        necesitaRefrescarLCD = true;
      }
      if (switchPulsadoActual) {
        // VALIDACION: Alarma Hum Min no puede ser mayor o muy cercana al Setpoint de Humedad
        if (alarmaHumMin >= setpointHumedad - UMBRAL_PROXIMIDAD_HUM) {
          lcd.clear();
          lcd.print("Alarma fuera de rango", 0, 0);
          lcd.print("Set Hum = " + String(setpointHumedad, 0) + "%", 0, 1);
          delay(2000);
          necesitaRefrescarLCD = true;
        } else {
          proximoEstado = ESTADO_EDITAR_ALARMAS;
          necesitaRefrescarLCD = true;
        }
      }
      break;

    case ESTADO_EDITAR_ALARMA_HUM_MAX:
      if (deltaEncoderActual != 0) {
        alarmaHumMax += (float)deltaEncoderActual * ALARMA_HUM_PASO;
        if (alarmaHumMax < ALARMA_HUM_MIN_RANGO) alarmaHumMax = ALARMA_HUM_MIN_RANGO;
        if (alarmaHumMax > ALARMA_HUM_MAX_RANGO) alarmaHumMax = ALARMA_HUM_MAX_RANGO;
        necesitaRefrescarLCD = true;
      }
      if (switchPulsadoActual) {
        // VALIDACION: Alarma Hum Max no puede ser menor o muy cercana al Setpoint de Humedad
        if (alarmaHumMax <= setpointHumedad + UMBRAL_PROXIMIDAD_HUM) {
          lcd.clear();
          lcd.print("Alarma fuera de rango", 0, 0);
          lcd.print("Set Hum = " + String(setpointHumedad, 0) + "%", 0, 1);
          delay(2000);
          necesitaRefrescarLCD = true;
        } else {
          proximoEstado = ESTADO_EDITAR_ALARMAS;
          necesitaRefrescarLCD = true;
        }
      }
      break;

    case ESTADO_EDITAR_ALARMA_CO2_MIN:
      if (deltaEncoderActual != 0) {
        alarmaCO2Min += deltaEncoderActual * ALARMA_CO2_PASO;
        if (alarmaCO2Min < ALARMA_CO2_MIN_RANGO) alarmaCO2Min = ALARMA_CO2_MIN_RANGO;
        if (alarmaCO2Min > ALARMA_CO2_MAX_RANGO) alarmaCO2Min = ALARMA_CO2_MAX_RANGO;
        necesitaRefrescarLCD = true;
      }
      if (switchPulsadoActual) {
        // VALIDACION: Alarma CO2 Min no puede ser mayor o muy cercana al Setpoint de CO2
        if (alarmaCO2Min >= setpointCO2 - UMBRAL_PROXIMIDAD_CO2) {
          lcd.clear();
          lcd.print("Alarma fuera de rango", 0, 0);
          lcd.print("Set CO2 = " + String(setpointCO2) + "ppm", 0, 1);
          delay(2000);
          necesitaRefrescarLCD = true;
        } else {
          proximoEstado = ESTADO_EDITAR_ALARMAS;
          necesitaRefrescarLCD = true;
        }
      }
      break;

    case ESTADO_EDITAR_ALARMA_CO2_MAX:
      if (deltaEncoderActual != 0) {
        alarmaCO2Max += deltaEncoderActual * ALARMA_CO2_PASO;
        if (alarmaCO2Max < ALARMA_CO2_MIN_RANGO) alarmaCO2Max = ALARMA_CO2_MIN_RANGO;
        if (alarmaCO2Max > ALARMA_CO2_MAX_RANGO) alarmaCO2Max = ALARMA_CO2_MAX_RANGO;
        necesitaRefrescarLCD = true;
      }
      if (switchPulsadoActual) {
        // VALIDACION: Alarma CO2 Max no puede ser menor o muy cercana al Setpoint de CO2
        if (alarmaCO2Max <= setpointCO2 + UMBRAL_PROXIMIDAD_CO2) {
          lcd.clear();
          lcd.print("Alarma fuera de rango", 0, 0);
          lcd.print("Set CO2 = " + String(setpointCO2) + "ppm", 0, 1);
          delay(2000);
          necesitaRefrescarLCD = true;
        } else {
          proximoEstado = ESTADO_EDITAR_ALARMAS;
          necesitaRefrescarLCD = true;
        }
      }
      break;
  }

  if (proximoEstado != estadoActualApp) {
    estadoActualApp = proximoEstado;
    necesitaRefrescarLCD = true;
  }

  // Manejo de la lógica del display LCD
  if (necesitaRefrescarLCD) {
    lcd.clear();
    switch (estadoActualApp) {
      case ESTADO_MENU_PRINCIPAL:
        manejarMenuPrincipal(deltaEncoderActual, switchPulsadoActual);
        break;
      case ESTADO_EDITAR_SETPOINTS:
        manejarEditarSetpoints(deltaEncoderActual, switchPulsadoActual);
        break;
      case ESTADO_ALARMAS_SUBMENU:
        manejarSubMenuAlarmas(deltaEncoderActual, switchPulsadoActual);
        break;
      case ESTADO_MOSTRAR_ALARMAS:
        manejarMostrarAlarmas(deltaEncoderActual, switchPulsadoActual);
        break;
      case ESTADO_EDITAR_ALARMAS:
        manejarEditarAlarmas(deltaEncoderActual, switchPulsadoActual);
        break;
      case ESTADO_CONFIRMACION_MODO_FUNCIONAMIENTO:
        lcd.print("Entrar en Modo?", 0, 0);
        lcd.print(indiceConfirmacion == 0 ? "> SI" : "  SI", 0, 1);
        lcd.print(indiceConfirmacion == 1 ? "> NO" : "  NO", 0, 2);
        break;
      case ESTADO_MODO_FUNCIONAMIENTO:
        // La lógica de mostrar aquí se maneja dentro de la función
        manejarModoFuncionamiento(deltaEncoderActual, switchPulsadoActual);
        break;
      case ESTADO_CONFIRMACION_VOLVER_MENU:
        lcd.print("Volver al Menu?", 0, 0);
        lcd.print(indiceConfirmacion == 0 ? "> SI" : "  SI", 0, 1);
        lcd.print(indiceConfirmacion == 1 ? "> NO" : "  NO", 0, 2);
        break;
      case ESTADO_EDITAR_TEMP:
        manejarEditarTemperatura(deltaEncoderActual, switchPulsadoActual);
        break;
      case ESTADO_EDITAR_HUM:
        manejarEditarHumedad(deltaEncoderActual, switchPulsadoActual);
        break;
      case ESTADO_EDITAR_CO2:
        manejarEditarCO2(deltaEncoderActual, switchPulsadoActual);
        break;
      case ESTADO_EDITAR_ALARMA_TEMP_MIN:
        manejarEditarAlarmaTempMin(deltaEncoderActual, switchPulsadoActual);
        break;
      case ESTADO_EDITAR_ALARMA_TEMP_MAX:
        manejarEditarAlarmaTempMax(deltaEncoderActual, switchPulsadoActual);
        break;
      case ESTADO_EDITAR_ALARMA_HUM_MIN:
        manejarEditarAlarmaHumMin(deltaEncoderActual, switchPulsadoActual);
        break;
      case ESTADO_EDITAR_ALARMA_HUM_MAX:
        manejarEditarAlarmaHumMax(deltaEncoderActual, switchPulsadoActual);
        break;
      case ESTADO_EDITAR_ALARMA_CO2_MIN:
        manejarEditarAlarmaCO2Min(deltaEncoderActual, switchPulsadoActual);
        break;
      case ESTADO_EDITAR_ALARMA_CO2_MAX:
        manejarEditarAlarmaCO2Max(deltaEncoderActual, switchPulsadoActual);
        break;
    }
    necesitaRefrescarLCD = false;
  }

  // --- Lógica de control de actuadores y lectura de sensores en Modo Funcionamiento ---
  // Se ejecutará continuamente sin depender de necesitaRefrescarLCD,
  // pero el display solo se actualizará cuando 'necesitaRefrescarLCD' sea true.
  if (estadoActualApp == ESTADO_MODO_FUNCIONAMIENTO) {
    // Estas funciones ahora se llaman dentro de manejarModoFuncionamiento para controlar el refresco del LCD
    // leerSensores();
    // actualizarEstadoAlarmas(); // Llamar a esta función regularmente

    // CONTROL DE RELÉS CON CONSIDERACIÓN DE ALARMAS
    // Control del Calefactor (Relay 2)
    // Si la temperatura está por debajo del setpoint - histeresis Y NO hay alarma de temp max activa
    if (currentTemp <= setpointTemperatura - HISTERESIS_TEMP_ENCENDER_CALEFACTOR && !banderaAlarmaTempMaxActiva) {
      digitalWrite(RELAY2, LOW); // Enciende calefactor
    } else if (currentTemp >= setpointTemperatura + HISTERESIS_TEMP_APAGAR_CALEFACTOR || banderaAlarmaTempMaxActiva) { // O si hay alarma de temp max activa, apágalo
      digitalWrite(RELAY2, HIGH); // Apaga calefactor
    }

    // Control del Ventilador (Relay 1)
    bool activarVentiladorPorCondicion = false;
    // Prioridad: Exceso de CO2 > Exceso Humedad > Exceso Temperatura
    if (estaEnExcesoCO2()) {
      activarVentiladorPorCondicion = true;
      razonActivacionVentilador = CO2_ALTO;
    } else if (estaEnExcesoHumedad()) {
      activarVentiladorPorCondicion = true;
      razonActivacionVentilador = HUMEDAD_ALTA;
    } else if (estaEnExcesoTemperatura()) {
      activarVentiladorPorCondicion = true;
      razonActivacionVentilador = TEMPERATURA_ALTA;
    } else {
      razonActivacionVentilador = NINGUNA;
    }

    // Comprobación de condiciones de ALARMA para evitar encendido del ventilador
    if (banderaAlarmaTempMinActiva || banderaAlarmaHumMinActiva || banderaAlarmaCO2MinActiva) {
        activarVentiladorPorCondicion = false; // Override: No enciendas el ventilador si hay alarma de mínimo.
    }

    // Lógica para el estado del ventilador
    switch (estadoVentiladorActual) {
      case VENTILADOR_INACTIVO:
        if (activarVentiladorPorCondicion) {
          digitalWrite(RELAY1, LOW); // Enciende ventilador
          temporizadorActivoVentilador = millis();
          estadoVentiladorActual = VENTILADOR_ENCENDIDO_3S;
        }
        break;

      case VENTILADOR_ENCENDIDO_3S:
        if (millis() - temporizadorActivoVentilador >= 3 * 1000) {
          digitalWrite(RELAY1, HIGH); // Apaga ventilador
          temporizadorActivoVentilador = millis();
          estadoVentiladorActual = VENTILADOR_APAGADO_5S;
        }
        break;

      case VENTILADOR_APAGADO_5S:
        if (millis() - temporizadorActivoVentilador >= 5 * 1000) {
          digitalWrite(RELAY1, LOW); // Enciende ventilador
          temporizadorActivoVentilador = millis();
          estadoVentiladorActual = VENTILADOR_ENCENDIDO_5S;
        }
        break;

      case VENTILADOR_ENCENDIDO_5S:
        if (millis() - temporizadorActivoVentilador >= 5 * 1000) {
          digitalWrite(RELAY1, HIGH); // Apaga ventilador
          temporizadorActivoVentilador = millis();
          estadoVentiladorActual = VENTILADOR_ESPERA_ESTABILIZACION;
        }
        break;

      case VENTILADOR_ESPERA_ESTABILIZACION:
        // El ventilador está apagado y esperamos 15 segundos para que los valores se estabilicen
        if (millis() - temporizadorActivoVentilador >= 15 * 1000) {
          // Chequear si la condición que activó el ventilador aún persiste
          bool condicionPersiste = false;
          if (razonActivacionVentilador == TEMPERATURA_ALTA && currentTemp > setpointTemperatura + UMBRAL_TEMP_APAGAR_VENTILADOR) {
            condicionPersiste = true;
          }
          if (razonActivacionVentilador == HUMEDAD_ALTA && currentHum > setpointHumedad + UMBRAL_HUM_APAGAR_VENTILADOR) {
            condicionPersiste = true;
          }
          if (razonActivacionVentilador == CO2_ALTO && currentCO2 > setpointCO2 + UMBRAL_CO2_APAGAR_VENTILADOR) {
            condicionPersiste = true;
          }

          if (condicionPersiste) {
            estadoVentiladorActual = VENTILADOR_INACTIVO; // Vuelve a INACTIVO para re-evaluar y potencialmente encender
          } else {
            estadoVentiladorActual = VENTILADOR_INACTIVO; // La condición mejoró, permanece inactivo
          }
        }
        break;
    }

    // Lógica para ventilación programada de CO2
    if (millis() - ultimoTiempoVentilacionProgramada >= INTERVALO_VENTILACION_PROGRAMADA && !ventilacionProgramadaActiva) {
      // Activa ventilación programada solo si no hay condiciones de alarma por mínimos
      if (!banderaAlarmaTempMinActiva && !banderaAlarmaHumMinActiva && !banderaAlarmaCO2MinActiva) {
        digitalWrite(RELAY1, LOW); // Enciende ventilador
        ventilacionProgramadaActiva = true;
        ultimoTiempoVentilacionProgramada = millis(); // Reinicia el temporizador para la duración de la ventilación
        Serial.println("Ventilación programada de CO2 iniciada.");
      }
    }

    if (ventilacionProgramadaActiva && (millis() - ultimoTiempoVentilacionProgramada >= DURACION_VENTILACION_PROGRAMADA)) {
      digitalWrite(RELAY1, HIGH); // Apaga ventilador
      ventilacionProgramadaActiva = false;
      ultimoTiempoVentilacionProgramada = millis(); // Reinicia el temporizador para el próximo intervalo
      Serial.println("Ventilación programada de CO2 finalizada.");
    }

  } // Fin de if (estadoActualApp == ESTADO_MODO_FUNCIONAMIENTO)

  delay(10); // Pequeño delay para estabilidad del sistema
}

// ---------------- DEFINICIÓN DE FUNCIONES ----------------

// ISRs para el encoder y switch
void IRAM_ATTR leerEncoderISR() {
  unsigned long tiempoActual = millis();
  if (tiempoActual - tiempoUltimaLecturaCLK > RETARDO_ANTI_REBOTE_ENCODER) {
    estadoClk = digitalRead(CLOCK_ENCODER);
    estadoDt = digitalRead(DT_ENCODER);
    if (estadoClk != estadoClkAnterior) {
      if (estadoDt != estadoClk) {
        valorEncoder++; // Gira en sentido horario
      } else {
        valorEncoder--; // Gira en sentido antihorario
      }
    }
    estadoClkAnterior = estadoClk;
    tiempoUltimaLecturaCLK = tiempoActual;
  }
}

void IRAM_ATTR leerSwitchISR() {
  unsigned long tiempoActual = millis();
  if (tiempoActual - tiempoUltimaPresionSW > RETARDO_ANTI_REBOTE_SW) {
    swFuePresionadoISR = true;
    tiempoUltimaPresionSW = tiempoActual;
  }
}

// Funciones de manejo de estados del menú
void manejarMenuPrincipal(int deltaEncoder, bool pulsadoSwitch) {
  lcd.print("MENU PRINCIPAL", 0, 0);
  for (int i = 0; i < TOTAL_OPCIONES_MENU_PRINCIPAL; ++i) {
    String opcion = opcionesMenuPrincipal[i];
    if (i == indiceMenuPrincipal) {
      opcion = "> " + opcion;
    } else {
      opcion = "  " + opcion;
    }
    lcd.print(opcion, 0, i + 1);
  }
}

void manejarEditarSetpoints(int deltaEncoder, bool pulsadoSwitch) {
  lcd.print("EDITAR SETPOINTS", 0, 0);
  String opciones[] = {
    "Temp: " + String(setpointTemperatura, 1) + "C",
    "Hum: " + String(setpointHumedad, 0) + "%",
    "CO2: " + String(setpointCO2) + "ppm",
    "Volver"
  };
  for (int i = 0; i < TOTAL_OPCIONES_SUBMENU_SETPOINTS; ++i) {
    String opcion = opciones[i];
    if (i == indiceSubMenuSetpoints) {
      opcion = "> " + opcion;
    } else {
      opcion = "  " + opcion;
    }
    lcd.print(opcion, 0, i + 1);
  }
}

void manejarSubMenuAlarmas(int deltaEncoder, bool pulsadoSwitch) {
  lcd.print("SUBMENU ALARMAS", 0, 0);
  String opciones[] = {
    "Mostrar Alarmas",
    "Editar Alarmas",
    "Volver"
  };
  for (int i = 0; i < TOTAL_OPCIONES_SUBMENU_ALARMAS; ++i) {
    String opcion = opciones[i];
    if (i == indiceSubMenuAlarmas) {
      opcion = "> " + opcion;
    } else {
      opcion = "  " + opcion;
    }
    lcd.print(opcion, 0, i + 1);
  }
}

void manejarMostrarAlarmas(int deltaEncoder, bool pulsadoSwitch) {
  lcd.print("VALORES ALARMA", 0, 0);
  lcd.print("Temp Min: " + String(alarmaTempMin, 1) + "C", 0, 1);
  lcd.print("Temp Max: " + String(alarmaTempMax, 1) + "C", 0, 2);
  lcd.print("Hum Min: " + String(alarmaHumMin, 0) + "%", 0, 3);
  delay(100); // Pequeña pausa para que se muestre en pantalla
  if (pulsadoSwitch) {
    estadoActualApp = ESTADO_ALARMAS_SUBMENU;
    necesitaRefrescarLCD = true;
  }
}

void manejarEditarAlarmas(int deltaEncoder, bool pulsadoSwitch) {
  lcd.print("EDITAR ALARMAS", 0, 0);
  String opciones[] = {
    "Temp Min: " + String(alarmaTempMin, 1) + "C",
    "Temp Max: " + String(alarmaTempMax, 1) + "C",
    "Hum Min: " + String(alarmaHumMin, 0) + "%",
    "Hum Max: " + String(alarmaHumMax, 0) + "%",
    "CO2 Min: " + String(alarmaCO2Min) + "ppm",
    "CO2 Max: " + String(alarmaCO2Max) + "ppm",
    "Guardar y Volver"
  };
  int startIdx = 0;
  if (indiceEditarAlarmas >= OPCIONES_VISIBLES_PANTALLA - 1) {
    startIdx = indiceEditarAlarmas - (OPCIONES_VISIBLES_PANTALLA - 2);
    if (startIdx < 0) startIdx = 0;
  }

  for (int i = 0; i < OPCIONES_VISIBLES_PANTALLA -1; ++i) { // Show 3 options + title
    if (startIdx + i < TOTAL_OPCIONES_EDITAR_ALARMAS) {
      String opcion = opciones[startIdx + i];
      if ((startIdx + i) == indiceEditarAlarmas) {
        opcion = "> " + opcion;
      } else {
        opcion = "  " + opcion;
      }
      lcd.print(opcion, 0, i + 1);
    }
  }
}


void manejarConfirmacionModoFuncionamiento(int deltaEncoder, bool pulsadoSwitch) {
  // La lógica de display se maneja en el loop principal
}

void manejarConfirmacionVolverMenu(int deltaEncoder, bool pulsadoSwitch) {
  // La lógica de display se maneja en el loop principal
}

void manejarEditarTemperatura(int deltaEncoder, bool pulsadoSwitch) {
  lcd.print("Editar Temp Set", 0, 0);
  lcd.print(String(setpointTemperatura, 1) + " C", 0, 1);
  lcd.print("SW para volver", 0, 3);
}

void manejarEditarHumedad(int deltaEncoder, bool pulsadoSwitch) {
  lcd.print("Editar Hum Set", 0, 0);
  lcd.print(String(setpointHumedad, 0) + " %", 0, 1);
  lcd.print("SW para volver", 0, 3);
}

void manejarEditarCO2(int deltaEncoder, bool pulsadoSwitch) {
  lcd.print("Editar CO2 Set", 0, 0);
  lcd.print(String(setpointCO2) + " ppm", 0, 1);
  lcd.print("SW para volver", 0, 3);
}

void manejarEditarAlarmaTempMin(int deltaEncoder, bool pulsadoSwitch) {
  lcd.print("Alarma Temp Min", 0, 0);
  lcd.print(String(alarmaTempMin, 1) + " C", 0, 1);
  lcd.print("SW para volver", 0, 3);
}

void manejarEditarAlarmaTempMax(int deltaEncoder, bool pulsadoSwitch) {
  lcd.print("Alarma Temp Max", 0, 0);
  lcd.print(String(alarmaTempMax, 1) + " C", 0, 1);
  lcd.print("SW para volver", 0, 3);
}

void manejarEditarAlarmaHumMin(int deltaEncoder, bool pulsadoSwitch) {
  lcd.print("Alarma Hum Min", 0, 0);
  lcd.print(String(alarmaHumMin, 0) + " %", 0, 1);
  lcd.print("SW para volver", 0, 3);
}

void manejarEditarAlarmaHumMax(int deltaEncoder, bool pulsadoSwitch) {
  lcd.print("Alarma Hum Max", 0, 0);
  lcd.print(String(alarmaHumMax, 0) + " %", 0, 1);
  lcd.print("SW para volver", 0, 3);
}

void manejarEditarAlarmaCO2Min(int deltaEncoder, bool pulsadoSwitch) {
  lcd.print("Alarma CO2 Min", 0, 0);
  lcd.print(String(alarmaCO2Min) + " ppm", 0, 1);
  lcd.print("SW para volver", 0, 3);
}

void manejarEditarAlarmaCO2Max(int deltaEncoder, bool pulsadoSwitch) {
  lcd.print("Alarma CO2 Max", 0, 0);
  lcd.print(String(alarmaCO2Max) + " ppm", 0, 1);
  lcd.print("SW para volver", 0, 3);
}

// ETIQUETA: Implementacion de funcion para leer sensores
void leerSensores() {
  // Leer temperatura y humedad
  dhtSensor.leerValores(); // Llama a la función que actualiza los valores internos del sensor DHT
  currentTemp = dhtSensor.getTemperatura(); // Obtiene la temperatura de la librería DHT
  currentHum = dhtSensor.getHumedad();     // Obtiene la humedad de la librería DHT

  // Leer CO2 solo si el sensor MQ2 está listo
  if (gasSensor.isReady()) {
    currentCO2 = gasSensor.leerCO2(); // Asumiendo que 'leerCO2()' es la función correcta para tu MQ2
  } else {
    // Si el sensor no está listo, puedes establecer un valor por defecto o mantener el último.
    // Para evitar datos erróneos, podríamos poner 0 o -1, o simplemente no actualizarlo.
    // En este caso, lo establecemos a 0 o un valor que indique "no disponible" si la lógica de alarms lo admite
    currentCO2 = 0; // O considera un valor como -1 para indicar que no hay lectura válida
    Serial.println("Calentando sensor MQ2, CO2 no disponible.");
  }

  // Enviar datos a Firebase periódicamente (cada 1 minuto)
  static unsigned long lastFirebaseSendTime = 0;
  const unsigned long FIREBASE_SEND_INTERVAL = 60 * 1000; // 1 minuto en milisegundos

  if (millis() - lastFirebaseSendTime >= FIREBASE_SEND_INTERVAL) {
    firebase.sendData(currentTemp, currentHum, currentCO2);
    lastFirebaseSendTime = millis();
  }
}

// ETIQUETA: Implementacion de funcion para actualizar el estado de las alarmas (Nueva)
void actualizarEstadoAlarmas() {
    // Temperatura Mínima
    if (currentTemp < alarmaTempMin) {
        banderaAlarmaTempMinActiva = true;
    } else if (currentTemp > alarmaTempMin + 0.5) { // Se desactiva con pequeña histéresis
        banderaAlarmaTempMinActiva = false;
    }

    // Temperatura Máxima
    if (currentTemp > alarmaTempMax) {
        banderaAlarmaTempMaxActiva = true;
    } else if (currentTemp < alarmaTempMax - 0.5) { // Se desactiva con pequeña histéresis
        banderaAlarmaTempMaxActiva = false;
    }

    // Humedad Mínima
    if (currentHum < alarmaHumMin) {
        banderaAlarmaHumMinActiva = true;
    } else if (currentHum > alarmaHumMin + 1.0) { // Se desactiva con pequeña histéresis
        banderaAlarmaHumMinActiva = false;
    }

    // Humedad Máxima
    if (currentHum > alarmaHumMax) {
        banderaAlarmaHumMaxActiva = true;
    } else if (currentHum < alarmaHumMax - 1.0) { // Se desactiva con pequeña histéresis
        banderaAlarmaHumMaxActiva = false;
    }

    // CO2 Mínimo
    if (currentCO2 < alarmaCO2Min) {
        banderaAlarmaCO2MinActiva = true;
    } else if (currentCO2 > alarmaCO2Min + 50) { // Se desactiva con pequeña histéresis
        banderaAlarmaCO2MinActiva = false;
    }

    // CO2 Máximo
    if (currentCO2 > alarmaCO2Max) {
        banderaAlarmaCO2MaxActiva = true;
    } else if (currentCO2 < alarmaCO2Max - 50) { // Se desactiva con pequeña histéresis
        banderaAlarmaCO2MaxActiva = false;
    }

    // Actualizar bandera general de alguna alarma activa
    algunaAlarmaActiva = banderaAlarmaTempMinActiva || banderaAlarmaTempMaxActiva ||
                         banderaAlarmaHumMinActiva || banderaAlarmaHumMaxActiva ||
                         banderaAlarmaCO2MinActiva || banderaAlarmaCO2MaxActiva;
}


// ETIQUETA: Implementacion de funcion para mostrar mensajes de alarma en el display (Modificada)
void mostrarMensajeAlarmaEnDisplay(String tipoAlarma) {
    lcd.clear();
    lcd.print("ALARMA: " + tipoAlarma,0,0);

    lcd.print("T: " + String(currentTemp, 1) + "C",0,1);
    lcd.print("H: " + String(currentHum, 0) + "%",10,1);


    lcd.print("CO2: " + String(currentCO2, 0) + "ppm",0,2);

    lcd.print("SW: Volver Menu",0,3);
}


// Lógica principal del Modo Funcionamiento
void manejarModoFuncionamiento(int deltaEncoder, bool pulsadoSwitch) {
  // Manejo de la lógica del switch para salir del modo de funcionamiento
  // (ya se maneja en el loop principal, solo se necesita el refresco inicial)

  // Mensaje inicial al entrar en Modo Funcionamiento
  if (!initialMessageDisplayed) {
    lcd.clear(); // Limpia la pantalla para el nuevo modo
    lcd.print("Modo: Funcionamiento", 0, 0); //
    lcd.print("Iniciando medicion", 0, 1); //
    modoFuncionamientoStartTime = millis();
    initialMessageDisplayed = true;
    necesitaRefrescarLCD = false; // Ya se refrescó
  }

  // Esperar 3 segundos para que se muestre el mensaje inicial
  if (millis() - modoFuncionamientoStartTime < 3000) {
    return; // No hacer nada más por ahora
  }

  // Leer sensores (esta función ya actualiza currentTemp, currentHum, currentCO2)
  leerSensores();

  // Comprobar si los valores de los sensores han cambiado lo suficiente para actualizar el display
  bool tempChanged = abs(currentTemp - lastDisplayedTemp) >= TEMP_DISPLAY_THRESHOLD;
  bool humChanged = abs(currentHum - lastDisplayedHum) >= HUM_DISPLAY_THRESHOLD;
  bool co2Changed = abs(currentCO2 - lastDisplayedCO2) >= CO2_DISPLAY_THRESHOLD;

  if (tempChanged || humChanged || co2Changed || necesitaRefrescarLCD) {
    lcd.clear(); // Limpia la pantalla para redibujar
    
    // Linea 0: Temperatura
    lcd.print("Temp: " + String(currentTemp, 1) + " C", 0, 0); //

    // Linea 1: Humedad
    lcd.print("Hum:  " + String(currentHum, 0) + " %", 0, 1); //

    // Linea 2: CO2
    lcd.print("CO2:  " + String(currentCO2) + " ppm", 0, 2); //

    // Linea 3: Estado del sistema o alarmas (si aplica)
    String estadoLinea = "Sistema OK";
    // Aquí puedes añadir lógica para mostrar mensajes de alarma o estado del ventilador/calefactor
    // Por ejemplo:
    // if (algunaAlarmaActiva) {
    //   estadoLinea = "¡ALARMA!";
    // } else if (digitalRead(RELAY1) == LOW) { // Si el ventilador está encendido
    //   estadoLinea = "Ventilando";
    // } else if (digitalRead(RELAY2) == LOW) { // Si el calefactor está encendido
    //   estadoLinea = "Calentando";
    // }
    
    lcd.print(estadoLinea, 0, 3); //

    // Actualizar los últimos valores mostrados
    lastDisplayedTemp = currentTemp;
    lastDisplayedHum = currentHum;
    lastDisplayedCO2 = currentCO2;
    necesitaRefrescarLCD = false; // Ya se refrescó
  }

  // --- Lógica de control de relés y ventilación (sin cambios) ---
  // Control del Calefactor (Relay 2)
  if (currentTemp < (setpointTemperatura - HISTERESIS_TEMP_ENCENDER_CALEFACTOR)) {
    digitalWrite(RELAY2, LOW); // Encender calefactor (LOW si es relé activo bajo)
  } else if (currentTemp >= (setpointTemperatura + HISTERESIS_TEMP_APAGAR_CALEFACTOR)) {
    digitalWrite(RELAY2, HIGH); // Apagar calefactor
  }

  // Control del Ventilador (Relay 1) - Implementación del ciclo de ventilación
  // Se ejecutará solo si no hay ninguna alarma de CO2 bajo activa
  if (!banderaBajoCO2) {
    switch (estadoVentiladorActual) {
      case VENTILADOR_INACTIVO:
        // Criterios de activación
        if (currentTemp > (setpointTemperatura + UMBRAL_TEMP_VENTILADOR)) {
          razonActivacionVentilador = TEMPERATURA_ALTA;
          estadoVentiladorActual = VENTILADOR_ENCENDIDO_3S;
          temporizadorActivoVentilador = millis();
        } else if (currentHum > (setpointHumedad + UMBRAL_HUM_VENTILADOR)) {
          razonActivacionVentilador = HUMEDAD_ALTA;
          estadoVentiladorActual = VENTILADOR_ENCENDIDO_3S;
          temporizadorActivoVentilador = millis();
        } else if (currentCO2 > (setpointCO2 + UMBRAL_CO2_VENTILADOR)) {
          razonActivacionVentilador = CO2_ALTO;
          estadoVentiladorActual = VENTILADOR_ENCENDIDO_3S;
          temporizadorActivoVentilador = millis();
        }
        // También considera la ventilación programada
        if (!ventilacionProgramadaActiva && (millis() - ultimoTiempoVentilacionProgramada >= INTERVALO_VENTILACION_PROGRAMADA)) {
          ventilacionProgramadaActiva = true;
          razonActivacionVentilador = NINGUNA; // O una nueva razón para programada
          estadoVentiladorActual = VENTILADOR_ENCENDIDO_3S;
          temporizadorActivoVentilador = millis();
          Serial.println("Ventilación programada iniciada.");
        }
        break;

      case VENTILADOR_ENCENDIDO_3S:
        digitalWrite(RELAY1, LOW); // Encender ventilador
        if (millis() - temporizadorActivoVentilador >= 3000) {
          estadoVentiladorActual = VENTILADOR_APAGADO_5S;
          temporizadorActivoVentilador = millis();
        }
        break;

      case VENTILADOR_APAGADO_5S:
        digitalWrite(RELAY1, HIGH); // Apagar ventilador
        if (millis() - temporizadorActivoVentilador >= 5000) {
          estadoVentiladorActual = VENTILADOR_ENCENDIDO_5S; // Pasa al siguiente pulso
          temporizadorActivoVentilador = millis();
        }
        break;

      case VENTILADOR_ENCENDIDO_5S:
        digitalWrite(RELAY1, LOW); // Encender ventilador
        if (millis() - temporizadorActivoVentilador >= 5000) {
          digitalWrite(RELAY1, HIGH); // Apagar antes de decidir el siguiente estado
          // Evaluar si la condición de activación original ha mejorado lo suficiente
          bool condicionMejorada = false;
          if (razonActivacionVentilador == TEMPERATURA_ALTA && currentTemp <= (setpointTemperatura + UMBRAL_TEMP_APAGAR_VENTILADOR)) {
            condicionMejorada = true;
          } else if (razonActivacionVentilador == HUMEDAD_ALTA && currentHum <= (setpointHumedad + UMBRAL_HUM_APAGAR_VENTILADOR)) {
            condicionMejorada = true;
          } else if (razonActivacionVentilador == CO2_ALTO && currentCO2 <= (setpointCO2 + UMBRAL_CO2_APAGAR_VENTILADOR)) {
            condicionMejorada = true;
          } else if (ventilacionProgramadaActiva && millis() - ultimoTiempoVentilacionProgramada >= DURACION_VENTILACION_PROGRAMADA) {
            condicionMejorada = true;
            ventilacionProgramadaActiva = false; // Finaliza la ventilación programada
            ultimoTiempoVentilacionProgramada = millis(); // Reiniciar el temporizador para la próxima
            Serial.println("Ventilación programada finalizada.");
          }

          if (condicionMejorada) {
            estadoVentiladorActual = VENTILADOR_ESPERA_ESTABILIZACION;
            temporizadorActivoVentilador = millis();
            razonActivacionVentilador = NINGUNA; // Resetear razón
          } else {
            estadoVentiladorActual = VENTILADOR_APAGADO_5S; // Continuar ciclo de pulsos
            temporizadorActivoVentilador = millis();
          }
        }
        break;

      case VENTILADOR_ESPERA_ESTABILIZACION:
        digitalWrite(RELAY1, HIGH); // Asegurarse de que esté apagado
        if (millis() - temporizadorActivoVentilador >= 15000) { // Espera 15 segundos
          estadoVentiladorActual = VENTILADOR_INACTIVO; // Vuelve al estado inicial para reevaluar
        }
        break;
    }
  } else {
    // Si banderaBajoCO2 es true, asegurar que el ventilador esté APAGADO
    digitalWrite(RELAY1, HIGH);
    // Y resetear el estado del ventilador a inactivo para que reevalúe cuando CO2 suba
    estadoVentiladorActual = VENTILADOR_INACTIVO;
    razonActivacionVentilador = NINGUNA;
    ventilacionProgramadaActiva = false; // Cancelar cualquier ventilación programada si CO2 está bajo
  }

  // Las alarmas de "ventilador no encendido" o "calefactor no encendido"
  // se muestran en 'mostrarMensajeAlarmaEnDisplay' si la condición de alarma es TRUE
  // y el relé correspondiente está en HIGH (apagado) cuando debería estar encendido.
  // Esto debe ser manejado en la lógica de control del actuador.
  // Por ejemplo, dentro del loop principal en el bloque ESTADO_MODO_FUNCIONAMIENTO
  // o en funciones separadas de control de reles.
  // Ya se ha implementado en la sección del loop principal donde se controla los relés.
}


// Funciones EEPROM
void guardarSetpointsEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(0, setpointTemperatura);
  EEPROM.put(sizeof(float), setpointHumedad);
  EEPROM.put(2 * sizeof(float), setpointCO2);
  EEPROM.put(2 * sizeof(float) + sizeof(int), alarmaTempMin);
  EEPROM.put(3 * sizeof(float) + sizeof(int), alarmaTempMax);
  EEPROM.put(4 * sizeof(float) + sizeof(int), alarmaHumMin);
  EEPROM.put(5 * sizeof(float) + sizeof(int), alarmaHumMax);
  EEPROM.put(6 * sizeof(float) + sizeof(int), alarmaCO2Min);
  EEPROM.put(6 * sizeof(float) + 2 * sizeof(int), alarmaCO2Max);
  EEPROM.commit();
  EEPROM.end();
  Serial.println("Setpoints y Alarmas guardados en EEPROM.");
}

void cargarSetpointsEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, setpointTemperatura);
  EEPROM.get(sizeof(float), setpointHumedad);
  EEPROM.get(2 * sizeof(float), setpointCO2);
  EEPROM.get(2 * sizeof(float) + sizeof(int), alarmaTempMin);
  EEPROM.get(3 * sizeof(float) + sizeof(int), alarmaTempMax);
  EEPROM.get(4 * sizeof(float) + sizeof(int), alarmaHumMin);
  EEPROM.get(5 * sizeof(float) + sizeof(int), alarmaHumMax);
  EEPROM.get(6 * sizeof(float) + sizeof(int), alarmaCO2Min);
  EEPROM.get(6 * sizeof(float) + 2 * sizeof(int), alarmaCO2Max);
  EEPROM.end();
  Serial.println("Setpoints y Alarmas cargados de EEPROM.");
}

void guardarEstadoAppEEPROM() {
    EEPROM.begin(EEPROM_SIZE);
    lastAppStateFlag = (estadoActualApp == ESTADO_MODO_FUNCIONAMIENTO) ? 1 : 0;
    EEPROM.put(EEPROM_APP_STATE_ADDR, lastAppStateFlag);
    EEPROM.commit();
    EEPROM.end();
    Serial.print("Estado de la aplicación guardado en EEPROM: ");
    Serial.println(lastAppStateFlag);
}

void cargarEstadoAppEEPROM() {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.get(EEPROM_APP_STATE_ADDR, lastAppStateFlag);
    EEPROM.end();
    Serial.print("Estado de la aplicación cargado de EEPROM: ");
    Serial.println(lastAppStateFlag);
}


// Funciones auxiliares para la prioridad de ventilación (se mantienen igual)
bool estaEnExcesoTemperatura() {
  return currentTemp > setpointTemperatura + UMBRAL_TEMP_VENTILADOR;
}

bool estaEnExcesoHumedad() {
  return currentHum > setpointHumedad + UMBRAL_HUM_VENTILADOR;
}

bool estaEnExcesoCO2() {
  return currentCO2 > setpointCO2 + UMBRAL_CO2_VENTILADOR;
}

bool estaEnDefectoCO2() {
  return currentCO2 < setpointCO2 - UMBRAL_CO2_VENTILADOR; // O ajustar según lógica específica de defecto
}