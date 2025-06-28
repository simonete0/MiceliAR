#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
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
#define SW_ENCODER 0

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
#define RELAY3 2        // D7 rele 3 (Humificador)

// ---------------- CONFIGURACIÓN LCD ----------------
#define LCD_ADDRESS 0x27
#define LCD_COLUMNS 20
#define LCD_ROWS 4
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);

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
float setpointTemperatura;// = 25.0;
float setpointHumedad;// = 70.0;
int setpointCO2;// = 6000; // Valor por defecto para etapa de Incubación

// Alarmas
float alarmaTempMin; //= 18.0;
float alarmaTempMax;// = 30.0;
float alarmaHumMin;// = 50.0;
float alarmaHumMax;// = 90.0;
int alarmaCO2Min;// = 600; // Estos valores deben ser revisados para la incubación
int alarmaCO2Max;// = 1200; // Estos valores deben ser revisados para la incubación
unsigned long tiempoAlternarPantalla = 0;
bool mostrandoAlarmas = false;
// Rangos de edición y pasos (Setpoints)
const float TEMP_MIN = 10.0;
const float TEMP_MAX = 60.0;
const float TEMP_PASO = 0.5;

const float HUM_MIN = 10.0;
const float HUM_MAX = 100.0;
const float HUM_PASO = 1.0;

const int CO2_MIN = 50; // Ampliado para permitir valores altos de incubación
const int CO2_MAX = 10000; // Ampliado para permitir valores altos de incubación
const int CO2_PASO = 50; // Paso más grande para CO2

// Rangos de edición y pasos (Alarmas)
const float ALARMA_TEMP_MIN_RANGO = 5.0;
const float ALARMA_TEMP_MAX_RANGO = 55.0;
const float ALARMA_TEMP_PASO = 0.5;

const float ALARMA_HUM_MIN_RANGO = 10.0;
const float ALARMA_HUM_MAX_RANGO = 100.0;
const float ALARMA_HUM_PASO = 1.0;

const int ALARMA_CO2_MIN_RANGO = 50; // Valores a revisar con el usuario
const int ALARMA_CO2_MAX_RANGO = 9000; // Valores a revisar con el usuario
const int ALARMA_CO2_PASO = 50; // Paso más grande para CO2

unsigned long tiempoInicioModoFuncionamiento = 0; // Marca el inicio del modo funcionamiento
unsigned long tiempoUltimoAvisoAlarma = 0;        // Marca el inicio del ciclo de aviso/sensores
bool mostrarAvisoAlarma = false;                  // Indica si se está mostrando el aviso de alarma
bool alarmasSilenciadas = false;                  // Indica si las alarmas están silenciadas
unsigned long tiempoSilenciadoAlarmas = 0;        // Marca cuándo se silenciaron las alarmas
const unsigned long DURACION_SILENCIO_ALARMAS = 180000; // 3 minutos de silencio

// Opciones del menú principal
String opcionesMenuPrincipal[] = {
  "Setpoints",
  "Alarmas",
  "Modo Funcionamiento"
};
int lastDesplazamientoScrollAlarma = -1; // Para evitar refresco innecesario de pantalla de alarmas
int desplazamientoScroll = 0; // Desplazamiento para el scroll de opciones en pantalla
const int OPCIONES_VISIBLES_PANTALLA = 4;

enum EstadoApp {
  ESTADO_MENU_PRINCIPAL,
  ESTADO_EDITAR_SETPOINTS,
  ESTADO_EDITAR_ALARMAS,
  ESTADO_CONFIRMACION_MODO_FUNCIONAMIENTO,
  ESTADO_MODO_FUNCIONAMIENTO,
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

//FirebaseParametros
float temp, hum;
int co2;
float tmin, tmax, hmin, hmax;
int co2min, co2max;
String estado;
float settemp, sethum;
int setco2;
bool necesitaRefrescarLCD = true;
float lastTempFirebase = -999.0; // Inicializar a un valor imposible para forzar la primera visualización
float lastHumFirebase = -999.0;
float lastCO2Firebase = -999.0;


// ETIQUETA: Variables para 'Modo Funcionamiento'
float currentTemp, currentHum, currentCO2;
float lastDisplayedTemp = -999.0; // Inicializar a un valor imposible para forzar la primera visualización
float lastDisplayedHum = -999.0;
float lastDisplayedCO2 = -999.0;

// Umbrales para actualizar la pantalla (ajustar según necesidad)
const float TEMP_DISPLAY_THRESHOLD = 0.2; // Cambio de 0.2 C
const float HUM_DISPLAY_THRESHOLD = 1.0;  // Cambio de 1%
const int CO2_DISPLAY_THRESHOLD = 20;     // Cambio de 20 ppm (según la última indicación del usuario)

// EXTRAS
unsigned long modoFuncionamientoStartTime = 0; // Para rastrear el tiempo de entrada para el mensaje inicial
bool initialMessageDisplayed = false; // Bandera para asegurar que el mensaje inicial se muestre solo una vez
bool esperandoConfirmacion = false;
bool pantallaMostradaAlarma = false;
bool pantallaMostradaATMax = false; // Bandera para saber si se mostró la pantalla de alarma de temperatura maxima
bool pantallaMostradaATMin = false; // Bandera para saber si se mostró la pantalla de alarma de temperatura minima
bool pantallaMostradaAHMax = false; // Bandera para saber si se mostró la pantalla de alarma de humedad maxima
bool pantallaMostradaAHMin = false; // Bandera para saber si se mostró la pantalla de alarma de humedad minima
bool pantallaMostradaACO2Max = false; // Bandera para saber si se mostró la pantalla de alarma de CO2 maxima
bool pantallaMostradaACO2Min = false; // Bandera para saber si se mostró la pantalla de alarma de CO2 minima
bool pantallaEditTempMostrada = false; // Bandera para saber si se mostró la pantalla de edición de temperatura
bool pantallaEditHumMostrada = false; // Bandera para saber si se mostró la pantalla de edición de humedad
bool pantallaEditCO2Mostrada = false; // Bandera para saber si se mostró la pantalla de edición de CO2
bool mensajeMostrado = false; // Bandera para saber si el mensaje fijo de "Pulsa para ir a Menu" ya fue mostrado
// ---------------- VARIABLES DE CONTROL DE RELÉS Y VENTILADOR ----------------
// Histéresis para calefactor (Relay 2)
const float HISTERESIS_TEMP_ENCENDER_CALEFACTOR = 2.0; // Encender cuando Temp <= Setpoint - 2C
const float HISTERESIS_TEMP_APAGAR_CALEFACTOR = 1.5;   // Apagar cuando Temp >= Setpoint + 1.5C
const float HISTERESIS_HUM_ENCENDER_HUMIDIFICADOR = 5.0; // Encender cuando Hum <= Setpoint - 5%
const float HISTERESIS_HUM_APAGAR_HUMIDIFICADOR = 2.0;   // Apagar cuando Hum >= Setpoint + 2%

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
bool pantallaEditSetMostrada = false;
// Nueva bandera para CO2 bajo
bool banderaBajoCO2 = false;

// Variable para el estado guardado en EEPROM
// 0: Modo Menu, 1: Modo Funcionamiento
uint8_t lastAppStateFlag = 0;
const int EEPROM_APP_STATE_ADDR = EEPROM_SIZE - sizeof(uint8_t); // Último byte de la EEPROM

// ---------------- PROTOTIPOS DE FUNCIONES ----------------
void IRAM_ATTR leerEncoderISR();
void IRAM_ATTR leerSwitchISR();

void manejarMenuPrincipal(int deltaEncoder, bool pulsadoSwitch);
void manejarEditarSetpoints(int deltaEncoder, bool pulsadoSwitch);
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
void leerEncoder();
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
//funcionamiento
void obtenerAlarmasActivas(String &avisos);


// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);

  // Asegura que los relés estén APAGADOS desde el inicio
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT);
  digitalWrite(RELAY1, HIGH);  // Apaga relé 1 (Ventilador) - HIGH
  digitalWrite(RELAY2, HIGH);  // Apaga relé 2 (Calefactor) - HIGH
  digitalWrite(RELAY3, LOW);  // Apaga relé 3 (Humedad) - LOW
  delay(200);  // Pequeña espera para estabilización

  pinMode(CLOCK_ENCODER, INPUT_PULLUP);
  pinMode(DT_ENCODER, INPUT_PULLUP);
  pinMode(SW_ENCODER, INPUT_PULLUP);

   // ETIQUETA: Inicializacion de sensores y Firebase de 'prueba encoder'
  pinMode(DHTPIN, INPUT_PULLUP); // Fuerza modo correcto para GPIO14
  delay(100);
  dhtSensor.iniciar();
  delay(2000); // Espera crítica para DHT11
  gasSensor.begin();
  firebase.begin(WIFI_SSID, WIFI_PASSWORD,
                FIREBASE_API_KEY, FIREBASE_URL,
                FIREBASE_EMAIL, FIREBASE_PASSWORD);
  
  attachInterrupt(digitalPinToInterrupt(CLOCK_ENCODER), leerEncoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(SW_ENCODER), leerSwitchISR, FALLING);

  estadoClkAnterior = digitalRead(CLOCK_ENCODER);

    if (firebase.leerSetpointsFirebase(settemp, sethum, setco2)) {
        setpointTemperatura = settemp;
        setpointHumedad = sethum;
        setpointCO2 = setco2;
    }
    if (firebase.leerAlarmasFirebase(tmin, tmax, hmin, hmax, co2min, co2max)) {
        alarmaTempMin = tmin;
        alarmaTempMax = tmax;
        alarmaHumMin = hmin;
        alarmaHumMax = hmax;
        alarmaCO2Min = co2min;
        alarmaCO2Max = co2max;
    }
    if (firebase.leerUltimoEstadoFirebase(estado)) {
        if (estado == "ESTADO_MODO_FUNCIONAMIENTO") {
            estadoActualApp = ESTADO_MODO_FUNCIONAMIENTO;
        } else {
            estadoActualApp = ESTADO_MENU_PRINCIPAL;
        }
    }  
    // Subida inicial de alarmas y estado si es la primera vez
    firebase.guardarAlarmasFirebase(
        alarmaTempMin, alarmaTempMax,
        alarmaHumMin, alarmaHumMax,
        alarmaCO2Min, alarmaCO2Max
    );


 

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
  
  Wire.begin(I2C_SDA, I2C_SCL);
  lcd.init();
  delay(1000);
  lcd.backlight(); // Asegurar que la luz de fondo esté encendida
  lcd.clear();
  delay(100); // Espera para estabilizar el LCD y relés
}

// ---------------- LOOP PRINCIPAL ----------------
void loop() {

static unsigned long lastEncoderCheckTime = 0;
  if (millis() - lastEncoderCheckTime > RETARDO_ANTI_REBOTE_ENCODER) {
    leerEncoder(); // Llama siempre, o solo si flag==true
    lastEncoderCheckTime = millis();
  }

  noInterrupts();
  int deltaEncoderActual = valorEncoder;
  valorEncoder = 0;
  bool switchPulsadoActual = swFuePresionadoISR;
  swFuePresionadoISR = false;
  interrupts();
  // Si hubo movimiento de encoder, fuerza refresco inmediato
  if (deltaEncoderActual != 0) {
    necesitaRefrescarLCD = true;
  }
  EstadoApp proximoEstado = estadoActualApp;
  // Manejo de estados de la aplicación
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
          case 1: proximoEstado = ESTADO_EDITAR_ALARMAS; break;
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
          firebase.guardarSetpointsFirebase(setpointTemperatura, setpointHumedad, setpointCO2);
          proximoEstado = ESTADO_MENU_PRINCIPAL;
          firebase.guardarUltimoEstadoFirebase("ESTADO_MENU_PRINCIPAL");

              break;
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
            firebase.guardarAlarmasFirebase(
                alarmaTempMin, alarmaTempMax,
                alarmaHumMin, alarmaHumMax,
                alarmaCO2Min, alarmaCO2Max
            );
            proximoEstado = ESTADO_MENU_PRINCIPAL;
            firebase.guardarUltimoEstadoFirebase("ESTADO_MENU_PRINCIPAL"); // o el estado que corresponda
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
                firebase.guardarUltimoEstadoFirebase("ESTADO_MODO_FUNCIONAMIENTO");
            }
            necesitaRefrescarLCD = true;
        }
        break;

    case ESTADO_MODO_FUNCIONAMIENTO:
        // En este modo, el encoder no hace nada, solo el switch
        // Las actualizaciones de sensores y Firebase se manejan dentro de manejarModoFuncionamiento
        // para un control más fino. No necesitamos 'necesitaRefrescarLCD = true;' aquí
        // a menos que sea para una transición de estado, lo cual ya se maneja arriba.
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


  }

  // Lógica de transición de estado y refresco de LCD (((BANDERAS)))
  if (proximoEstado != estadoActualApp || necesitaRefrescarLCD) {
      if (proximoEstado != estadoActualApp) {
          // Lógica para reiniciar índices al cambiar de estado principal
          if (proximoEstado == ESTADO_MENU_PRINCIPAL) {
              indiceMenuPrincipal = 0;
              desplazamientoScroll = 0;
              firebase.guardarUltimoEstadoFirebase("ESTADO_MENU_PRINCIPAL"); // Guardar el estado de Menú Principal en Firebase
          } else if (proximoEstado == ESTADO_EDITAR_SETPOINTS) {
              pantallaEditSetMostrada = false;
              indiceSubMenuSetpoints = 0;
              desplazamientoScroll = 0;
          } else if (proximoEstado == ESTADO_EDITAR_ALARMAS) {
              indiceEditarAlarmas = 0;
              desplazamientoScroll = 0;
              pantallaMostradaAlarma = false;
          } else if (proximoEstado == ESTADO_CONFIRMACION_MODO_FUNCIONAMIENTO) {
              indiceConfirmacion = 0; // Siempre iniciar en "SI" para confirmaciones
              desplazamientoScroll = 0;
              mostrarAvisoAlarma = false;
              alarmasSilenciadas = false;
              tiempoSilenciadoAlarmas = 0;
              tiempoUltimoAvisoAlarma = 0;
              mensajeMostrado = false;
          } else if (proximoEstado == ESTADO_MODO_FUNCIONAMIENTO) {
              initialMessageDisplayed = false; // ETIQUETA: Reiniciar bandera de mensaje inicial
              lastDisplayedTemp = -999.0; // Forzar la primera visualización de datos de sensores
              lastDisplayedHum = -999.0;
              lastDisplayedCO2 = -999.0;
              // Resetear estado del ventilador al entrar en modo funcionamiento
              estadoVentiladorActual = VENTILADOR_INACTIVO;
              digitalWrite(RELAY1, HIGH); // Asegurarse de que el ventilador esté apagado
              digitalWrite(RELAY2, HIGH); // Asegurarse de que el calefactor esté apagado
              firebase.guardarUltimoEstadoFirebase("ESTADO_MODO_FUNCIONAMIENTO"); // o el estado que corresponda
          }  else if (proximoEstado == ESTADO_EDITAR_TEMP){
          pantallaEditTempMostrada = false; // Reiniciar bandera de pantalla de edición de temperatura
          } else if (proximoEstado == ESTADO_EDITAR_HUM){
          pantallaEditHumMostrada = false; // Reiniciar bandera de pantalla de edición de humedad
          } else if (proximoEstado == ESTADO_EDITAR_CO2){
          pantallaEditCO2Mostrada = false; // Reiniciar bandera de pantalla de edición de CO2
          } else if (proximoEstado == ESTADO_EDITAR_ALARMAS){
          pantallaMostradaAlarma  = false; // Reiniciar bandera de pantalla de edición de alarmas
          } else if (proximoEstado == ESTADO_EDITAR_ALARMA_TEMP_MIN){
          pantallaMostradaATMin = false; // Reiniciar bandera de pantalla de alarma de temperatura mínima
          } else if (proximoEstado == ESTADO_EDITAR_ALARMA_TEMP_MAX){
          pantallaMostradaATMax = false; // Reiniciar bandera de pantalla de alarma de temperatura máxima
          } else if (proximoEstado == ESTADO_EDITAR_ALARMA_HUM_MIN){
          pantallaMostradaAHMin = false; // Reiniciar bandera de pantalla de alarma de humedad mínima
          } else if (proximoEstado == ESTADO_EDITAR_ALARMA_HUM_MAX){
          pantallaMostradaAHMax = false; // Reiniciar bandera de pantalla de alarma de humedad máxima
          } else if (proximoEstado == ESTADO_EDITAR_ALARMA_CO2_MIN){
          pantallaMostradaACO2Min = false; // Reiniciar bandera de pantalla de alarma de CO2 mínima
          } else if (proximoEstado == ESTADO_EDITAR_ALARMA_CO2_MAX){
          pantallaMostradaACO2Max = false; // Reiniciar bandera de pantalla de alarma de CO2 máxima
          }

          // Lógica para mantener índices al volver de una edición a un submenú
          if (estadoActualApp == ESTADO_EDITAR_TEMP && proximoEstado == ESTADO_EDITAR_SETPOINTS) indiceSubMenuSetpoints = 0;
          if (estadoActualApp == ESTADO_EDITAR_HUM && proximoEstado == ESTADO_EDITAR_SETPOINTS) indiceSubMenuSetpoints = 1;
          if (estadoActualApp == ESTADO_EDITAR_CO2 && proximoEstado == ESTADO_EDITAR_SETPOINTS) indiceSubMenuSetpoints = 2;

          if ( (estadoActualApp == ESTADO_EDITAR_ALARMA_TEMP_MIN || estadoActualApp == ESTADO_EDITAR_ALARMA_TEMP_MAX ||
                estadoActualApp == ESTADO_EDITAR_ALARMA_HUM_MIN || estadoActualApp == ESTADO_EDITAR_ALARMA_HUM_MAX ||
                estadoActualApp == ESTADO_EDITAR_ALARMA_CO2_MIN || estadoActualApp == ESTADO_EDITAR_ALARMA_CO2_MAX) &&
                proximoEstado == ESTADO_EDITAR_ALARMAS) {
                    // Mantiene el índice en indiceEditarAlarmas donde estaba
                }

          // Ajustes de índice al volver a MENU_PRINCIPAL desde confirmación o modo funcionamiento
          if ((estadoActualApp == ESTADO_CONFIRMACION_MODO_FUNCIONAMIENTO && proximoEstado == ESTADO_MENU_PRINCIPAL)) {
              indiceMenuPrincipal = 2; // Vuelve a la opción "Modo Funcionamiento"
          }
      }

      estadoActualApp = proximoEstado;

      switch (estadoActualApp) {
        case ESTADO_MENU_PRINCIPAL: manejarMenuPrincipal(0, false); break;
        case ESTADO_EDITAR_SETPOINTS: manejarEditarSetpoints(0, false); break;
        case ESTADO_EDITAR_TEMP: manejarEditarTemperatura(0, false); break;
        case ESTADO_EDITAR_HUM: manejarEditarHumedad(0, false); break;
        case ESTADO_EDITAR_CO2: manejarEditarCO2(0, false); break;
        case ESTADO_EDITAR_ALARMAS: manejarEditarAlarmas(0, false); break;
        case ESTADO_CONFIRMACION_MODO_FUNCIONAMIENTO: manejarConfirmacionModoFuncionamiento(0, false); break;
        case ESTADO_MODO_FUNCIONAMIENTO: manejarModoFuncionamiento(0, false); break;
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

  delay(2);
}

// ---------------- FUNCIONES DE INTERRUPCIÓN (ISR) ----------------
void IRAM_ATTR leerEncoderISR() {
  bool clk = digitalRead(CLOCK_ENCODER);
  bool dt = digitalRead(DT_ENCODER);
  if (clk != estadoClkAnterior) {
    if (dt != clk) {
      valorEncoder++;
    } else {
      valorEncoder--;
    }
    estadoClkAnterior = clk;
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
void leerEncoder() {
  bool clk = digitalRead(CLOCK_ENCODER);
  bool dt = digitalRead(DT_ENCODER);
  if (clk != estadoClkAnterior) {
    if (dt != clk) {
      valorEncoder++;
    } else {
      valorEncoder--;
    }
    estadoClkAnterior = clk;
  }
}

// ---------------- FUNCIONES DE MANEJO DE ESTADO (Solo dibujan) ----------------

void manejarMenuPrincipal(int deltaEncoder, bool pulsadoSwitch) {
  if (indiceMenuPrincipal < desplazamientoScroll) {
      desplazamientoScroll = indiceMenuPrincipal;
  } else if (indiceMenuPrincipal >= desplazamientoScroll + OPCIONES_VISIBLES_PANTALLA) {
      desplazamientoScroll = indiceMenuPrincipal - OPCIONES_VISIBLES_PANTALLA + 1;
  }
  static int lastIndiceMenuPrincipal = -1;
  static int lastDesplazamientoScroll = -1;
 
// === 1. Procesar input del Encoder ===
  if (deltaEncoder != 0) {
    indiceMenuPrincipal += deltaEncoder;
    // Asegura de que el índice esté dentro de los límites
    if (indiceMenuPrincipal < 0) indiceMenuPrincipal = TOTAL_OPCIONES_MENU_PRINCIPAL - 1;
    if (indiceMenuPrincipal >= TOTAL_OPCIONES_MENU_PRINCIPAL) indiceMenuPrincipal = 0;
    necesitaRefrescarLCD = true; // Forzamos refresco si hay cambio de índice
  }
  // === 2. Lógica de Scroll ===
  // Solo aplicar scroll si el número de opciones excede las líneas visibles
  if (TOTAL_OPCIONES_MENU_PRINCIPAL > OPCIONES_VISIBLES_PANTALLA) {
      if (indiceMenuPrincipal < desplazamientoScroll) {
          desplazamientoScroll = indiceMenuPrincipal;
      } else if (indiceMenuPrincipal >= desplazamientoScroll + OPCIONES_VISIBLES_PANTALLA) {
          desplazamientoScroll = indiceMenuPrincipal - OPCIONES_VISIBLES_PANTALLA + 1;
      }
  }
  // === 3. Lógica de Refresco del LCD solo si hay cambios ===
  if (necesitaRefrescarLCD || lastIndiceMenuPrincipal != indiceMenuPrincipal || lastDesplazamientoScroll != desplazamientoScroll) {
    for (int i = 0; i < OPCIONES_VISIBLES_PANTALLA; i++) {
      int indiceOpcion = desplazamientoScroll + i;
      lcd.setCursor(0, i); // Mover el cursor a la línea i

      if (indiceOpcion < TOTAL_OPCIONES_MENU_PRINCIPAL) {
        String textoOpcion = opcionesMenuPrincipal[indiceOpcion];
        String lineaMostrada = (indiceOpcion == indiceMenuPrincipal ? ">" : " ") + textoOpcion;

        // Rellenar con espacios para borrar el contenido anterior de la línea
        int len = lineaMostrada.length();
        for(int k=0; k < (LCD_COLUMNS - len); k++) {
            lineaMostrada += " ";
        }
        lcd.print(lineaMostrada);
      } else {
        // Limpiar líneas vacías si el menú es más corto que las OPCIONES_VISIBLES_PANTALLA
        lcd.print("                    "); // Rellenar con espacios
      }
    }
    // Actualizar los estados estáticos para la próxima comparación
    lastIndiceMenuPrincipal = indiceMenuPrincipal;
    lastDesplazamientoScroll = desplazamientoScroll;
    necesitaRefrescarLCD = false; // El display ha sido refrescado
  }
 
}

void manejarEditarSetpoints(int deltaEncoder, bool pulsadoSwitch) {
    static int lastIndice = -1;
   

    // Forzar refresco completo al entrar por primera vez
    if (!pantallaEditSetMostrada) {
        lcd.clear();
        for (int i = 0; i < TOTAL_OPCIONES_SUBMENU_SETPOINTS; i++) {
            lcd.setCursor(0, i);
            String linea = (i == indiceSubMenuSetpoints ? ">" : " ");
            switch (i) {
                case 0: linea += "Temp: " + String(setpointTemperatura, 1) + " C        "; break;
                case 1: linea += "Hum:  " + String(setpointHumedad, 0) + " %        "; break;
                case 2: linea += "CO2:  " + String(setpointCO2) + " ppm      "; break;
                case 3: linea += "Guardar y salir     "; break;
            }
            // Rellenar con espacios para limpiar la línea
            while (linea.length() < LCD_COLUMNS) linea += " ";
            lcd.print(linea);
        }
        lastIndice = indiceSubMenuSetpoints;
        pantallaEditSetMostrada = true;
        necesitaRefrescarLCD = false;
        return;
    }

    // Procesar input del Encoder
    if (deltaEncoder != 0) {
        indiceSubMenuSetpoints += deltaEncoder;
        if (indiceSubMenuSetpoints < 0) indiceSubMenuSetpoints = TOTAL_OPCIONES_SUBMENU_SETPOINTS - 1;
        if (indiceSubMenuSetpoints >= TOTAL_OPCIONES_SUBMENU_SETPOINTS) indiceSubMenuSetpoints = 0;
        necesitaRefrescarLCD = true;
    }

    // Solo actualizar las líneas necesarias
    if (necesitaRefrescarLCD || lastIndice != indiceSubMenuSetpoints) {
        // Actualiza solo la línea anterior y la nueva
        if (lastIndice != -1 && lastIndice != indiceSubMenuSetpoints) {
            lcd.setCursor(0, lastIndice);
            lcd.print(" "); // Borra el ">"
            switch (lastIndice) {
                case 0: lcd.print("Temp: " + String(setpointTemperatura, 1) + " C"); break;
                case 1: lcd.print("Hum:  " + String(setpointHumedad, 0) + " %"); break;
                case 2: lcd.print("CO2:  " + String(setpointCO2) + " ppm"); break;
                case 3: lcd.print("Guardar y salir"); break;
            }
        }
        // Dibuja la línea seleccionada con ">"
        lcd.setCursor(0, indiceSubMenuSetpoints);
        lcd.print(">");
        switch (indiceSubMenuSetpoints) {
            case 0: lcd.print("Temp: " + String(setpointTemperatura, 1) + " C        "); break;
            case 1: lcd.print("Hum:  " + String(setpointHumedad, 0) + " %        "); break;
            case 2: lcd.print("CO2:  " + String(setpointCO2) + " ppm      "); break;
            case 3: lcd.print("Guardar y salir     "); break;
        }
        lastIndice = indiceSubMenuSetpoints;
        necesitaRefrescarLCD = false;
        // --- GUARDADO SOLO AL PULSAR SWITCH EN "Guardar y salir" ---
        if (pulsadoSwitch && indiceSubMenuSetpoints == 3) {
          firebase.guardarSetpointsFirebase(setpointTemperatura, setpointHumedad, setpointCO2);
          firebase.guardarUltimoEstadoFirebase("ESTADO_MENU_PRINCIPAL");
          // Cambia de estado aquí si lo necesitas, por ejemplo:
          // estadoActualApp = ESTADO_MENU_PRINCIPAL;
          necesitaRefrescarLCD = true;
        }
    }
    

    // Si cambias de estado, recuerda resetear pantallaMostrada = false;
}



void manejarEditarAlarmas(int deltaEncoder, bool pulsadoSwitch) {
   static int lastIndiceAlarma = -1;

    // Lógica de scroll
    if (indiceEditarAlarmas < desplazamientoScroll) {
        desplazamientoScroll = indiceEditarAlarmas;
    } else if (indiceEditarAlarmas >= desplazamientoScroll + OPCIONES_VISIBLES_PANTALLA) {
        desplazamientoScroll = indiceEditarAlarmas - OPCIONES_VISIBLES_PANTALLA + 1;
    }

    // Refresco completo al entrar por primera vez
    if (!pantallaMostradaAlarma) {
        lcd.clear();
        for (int i = 0; i < OPCIONES_VISIBLES_PANTALLA; i++) {
            int indiceOpcion = desplazamientoScroll + i;
            String linea;
            switch (indiceOpcion) {
                case 0: linea = "T Min: " + String(alarmaTempMin, 1) + " C"; break;
                case 1: linea = "T Max: " + String(alarmaTempMax, 1) + " C"; break;
                case 2: linea = "H Min: " + String(alarmaHumMin, 0) + " % "; break;
                case 3: linea = "H Max: " + String(alarmaHumMax, 0) + " % "; break;
                case 4: linea = "CO2 Min: " + String(alarmaCO2Min) + " ppm "; break;
                case 5: linea = "CO2 Max: " + String(alarmaCO2Max) + " ppm "; break;
                case 6: linea = "Guardar y Volver "; break;
                default: linea = ""; break;
            }
            if (indiceOpcion == indiceEditarAlarmas) {
                linea = "> " + linea;
            } else {
                linea = "  " + linea;
            }
            while (linea.length() < LCD_COLUMNS) linea += " ";
            lcd.setCursor(0, i);
            lcd.print(linea);
        }
        lastIndiceAlarma = indiceEditarAlarmas;
        pantallaMostradaAlarma = true;
        return;
    }

    // Procesar input del Encoder
    if (deltaEncoder != 0) {
        indiceEditarAlarmas += deltaEncoder;
        if (indiceEditarAlarmas < 0) indiceEditarAlarmas = TOTAL_OPCIONES_EDITAR_ALARMAS - 1;
        if (indiceEditarAlarmas >= TOTAL_OPCIONES_EDITAR_ALARMAS) indiceEditarAlarmas = 0;
    }

    // Solo actualizar las líneas necesarias
    if (lastIndiceAlarma != indiceEditarAlarmas || desplazamientoScroll != lastDesplazamientoScrollAlarma) {
        // Redibuja toda la ventana visible si el scroll cambió
        lcd.clear();
        for (int i = 0; i < OPCIONES_VISIBLES_PANTALLA; i++) {
            int indiceOpcion = desplazamientoScroll + i;
            String linea;
            switch (indiceOpcion) {
                case 0: linea = "T Min: " + String(alarmaTempMin, 1) + " C"; break;
                case 1: linea = "T Max: " + String(alarmaTempMax, 1) + " C"; break;
                case 2: linea = "H Min: " + String(alarmaHumMin, 0) + " % "; break;
                case 3: linea = "H Max: " + String(alarmaHumMax, 0) + " % "; break;
                case 4: linea = "CO2 Min: " + String(alarmaCO2Min) + " ppm "; break;
                case 5: linea = "CO2 Max: " + String(alarmaCO2Max) + " ppm "; break;
                case 6: linea = "Guardar y Volver "; break;
                default: linea = ""; break;
            }
            if (indiceOpcion == indiceEditarAlarmas) {
                linea = "> " + linea;
            } else {
                linea = "  " + linea;
            }
            while (linea.length() < LCD_COLUMNS) linea += " ";
            lcd.setCursor(0, i);
            lcd.print(linea);
        }
        // Aquí sí: solo si el usuario pulsa el switch estando en "Guardar y Volver"
        if (pulsadoSwitch && indiceEditarAlarmas == 6) {
            firebase.guardarAlarmasFirebase(alarmaTempMin, alarmaTempMax, alarmaHumMin, alarmaHumMax, alarmaCO2Min, alarmaCO2Max);
            firebase.guardarUltimoEstadoFirebase("ESTADO_MENU_PRINCIPAL");
            estadoActualApp = ESTADO_MENU_PRINCIPAL;
            necesitaRefrescarLCD = true;
        }
        lastIndiceAlarma = indiceEditarAlarmas;
        lastDesplazamientoScrollAlarma = desplazamientoScroll;
    }
}

void manejarConfirmacionModoFuncionamiento(int deltaEncoder, bool pulsadoSwitch) {
    lcd.clear();
    lcd.home();
    lcd.print("Cambiar a M. Fun?");
    lcd.setCursor(0, 1);
    lcd.print("                    ");

    String opcionesConfirmacion[] = {"SI", "NO"};

    for (int i = 0; i < TOTAL_OPCIONES_CONFIRMACION; i++) {
        String linea = (i == indiceConfirmacion ? "> " : "  ") + opcionesConfirmacion[i];
        lcd.setCursor(0, 2 + i);
        lcd.print(linea);
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
  if (abs(currentTemp - lastTempFirebase) > 1.0 ||
    abs(currentHum - lastHumFirebase) > 2.0 ||
    abs(currentCO2 - lastCO2Firebase) > 50) {

    firebase.sendData(currentTemp, currentHum, currentCO2);

    lastTempFirebase = currentTemp;
    lastHumFirebase = currentHum;
    lastCO2Firebase = currentCO2;
}
}

// ETIQUETA: Implementacion de manejarModoFuncionamiento
void manejarModoFuncionamiento(int deltaEncoder, bool pulsadoSwitch) {
static bool mensajeMostrado = false;
static int indiceConfirmacionLocal = 0; // 0 = SI, 1 = NO
static int lastIndiceConfirmacion = -1;
static bool pantallaConfirmacionMostrada = false;
static bool primerIngreso = true; // Para marcar el primer ingreso al modo funcionamiento
static bool avisoMostradoEnLCD = false;
static String ultimoAvisoMostrado = "";

unsigned long currentMillis = millis();

 // Inicializar tiempo de inicio al entrar por primera vez
    if (primerIngreso) {
        tiempoInicioModoFuncionamiento = currentMillis;
        tiempoUltimoAvisoAlarma = currentMillis;
        mostrarAvisoAlarma = false;
        alarmasSilenciadas = false;
        tiempoSilenciadoAlarmas = 0;
        primerIngreso = false;
    }
  // Mostrar mensaje inicial por 3 segundos
    if (!initialMessageDisplayed) {
        lcd.clear();
        lcd.home();
        lcd.print("Inicializando");
        lcd.setCursor(0, 1);
        lcd.print("sistema...");
        modoFuncionamientoStartTime = currentMillis;
        initialMessageDisplayed = true;
    }

    // Esperar 3 segundos antes de mostrar datos y permitir interacción
    if (currentMillis - modoFuncionamientoStartTime < 3000) {
        return;
    }

    // Si estamos esperando confirmación de salida
    if (esperandoConfirmacion) {
      //solo refrescar si el LCD cambia la opvion o es la primera vez
      if (!pantallaConfirmacionMostrada||lastIndiceConfirmacion != indiceConfirmacionLocal) {
          lcd.clear();
          lcd.home();
          lcd.print("Salir de M. Funcion?");
          lcd.setCursor(0, 2);
          lcd.print((indiceConfirmacionLocal == 0 ? "> SI" : "  SI"));
          lcd.setCursor(0, 3);
          lcd.print((indiceConfirmacionLocal == 1 ? "> NO" : "  NO"));
          pantallaConfirmacionMostrada = true;
          lastIndiceConfirmacion = indiceConfirmacionLocal;
        }
        // Manejar input del encoder para cambiar opción
        if (deltaEncoder != 0) {
            indiceConfirmacionLocal += deltaEncoder;
            if (indiceConfirmacionLocal < 0) indiceConfirmacionLocal = 1;
            if (indiceConfirmacionLocal > 1) indiceConfirmacionLocal = 0;
            pantallaConfirmacionMostrada = false; // Forzar refresco en el próximo ciclo
        }

        // Si se pulsa el switch, actuar según la opción
        if (pulsadoSwitch) {
            if (indiceConfirmacionLocal == 0) {
                // Confirmó salir: apagar actuadores y cambiar de estado
                digitalWrite(RELAY1, HIGH); // Apagar ventilador
                digitalWrite(RELAY2, HIGH); // Apagar calefactor
                digitalWrite(RELAY3, LOW);  // Apagar humidificador
                estadoActualApp = ESTADO_MENU_PRINCIPAL;
                firebase.guardarUltimoEstadoFirebase("ESTADO_MENU_PRINCIPAL"); // o el estado que corresponda
                necesitaRefrescarLCD = true;
            }
            // Si elige NO, solo sale de la confirmación
            esperandoConfirmacion = false;
            indiceConfirmacionLocal = 0;
            pantallaConfirmacionMostrada = false; // Reset para la próxima vez
            lastIndiceConfirmacion = -1;
            mensajeMostrado = false;
            // Forzar refresco de todas las líneas de sensores
            lastDisplayedTemp = -999.0;
            lastDisplayedHum = -999.0;
            lastDisplayedCO2 = -999.0;
            return;
        }
        return; // No ejecutar el resto de la función mientras se confirma
    }

    // Leer sensores
    leerSensores();

    // Al pulsar el switch, pedir confirmación (pero NO apagar actuadores aquí)
    if (pulsadoSwitch) {
         esperandoConfirmacion = true;
        indiceConfirmacionLocal = 0;
        pantallaConfirmacionMostrada = false; // Forzar refresco al entrar
        lastIndiceConfirmacion = -1;
        return;
    }

    // --- Control del Calefactor (Relé 2 - D0) ---
    if (currentTemp <= setpointTemperatura - HISTERESIS_TEMP_ENCENDER_CALEFACTOR) {
        digitalWrite(RELAY2, LOW); // Enciende el calefactor
    } else if (currentTemp >= setpointTemperatura + HISTERESIS_TEMP_APAGAR_CALEFACTOR) {
        digitalWrite(RELAY2, HIGH); // Apaga el calefactor
    }

    // --- Control del Humidificador (Relé 3 - D1) ---
    if (currentHum <= setpointHumedad - HISTERESIS_HUM_ENCENDER_HUMIDIFICADOR) {
        digitalWrite(RELAY3, HIGH); // Enciende el humidificador
    } else if (currentHum >= setpointHumedad + HISTERESIS_HUM_APAGAR_HUMIDIFICADOR) {
        digitalWrite(RELAY3, LOW); // Apaga el humidificador
    }

    // --- Control del Ventilador (Relé 1 - D8) ---
    // Lógica para la ventilación programada de CO2
    if (!ventilacionProgramadaActiva &&
        (currentMillis - ultimoTiempoVentilacionProgramada >= INTERVALO_VENTILACION_PROGRAMADA) &&
        (estadoVentiladorActual == VENTILADOR_INACTIVO) &&
        !estaEnExcesoTemperatura() && !estaEnExcesoHumedad() && !estaEnExcesoCO2() )
    {
        ventilacionProgramadaActiva = true;
        digitalWrite(RELAY1, LOW); // Enciende el ventilador para pulso programado
        temporizadorActivoVentilador = currentMillis;
        Serial.println("Ventilación programada de CO2 iniciada.");
    }

    if (ventilacionProgramadaActiva) {
        if (currentMillis - temporizadorActivoVentilador >= DURACION_VENTILACION_PROGRAMADA) {
            digitalWrite(RELAY1, HIGH); // Apaga el ventilador
            ventilacionProgramadaActiva = false;
            ultimoTiempoVentilacionProgramada = currentMillis;
            Serial.println("Ventilación programada de CO2 finalizada.");
        }
    } else {
        switch (estadoVentiladorActual) {
            case VENTILADOR_INACTIVO:
                if (estaEnExcesoTemperatura()) {
                    razonActivacionVentilador = TEMPERATURA_ALTA;
                    estadoVentiladorActual = VENTILADOR_ENCENDIDO_3S;
                    temporizadorActivoVentilador = currentMillis;
                    digitalWrite(RELAY1, LOW);
                   //Serial.println("Ventilador ON (Temp alta)");
                } else if (estaEnExcesoHumedad()) {
                    razonActivacionVentilador = HUMEDAD_ALTA;
                    estadoVentiladorActual = VENTILADOR_ENCENDIDO_3S;
                    temporizadorActivoVentilador = currentMillis;
                    digitalWrite(RELAY1, LOW);
                    Serial.println("Ventilador ON (Humedad alta)");
                } else if (estaEnExcesoCO2()) {
                    razonActivacionVentilador = CO2_ALTO;
                    estadoVentiladorActual = VENTILADOR_ENCENDIDO_3S;
                    temporizadorActivoVentilador = currentMillis;
                    digitalWrite(RELAY1, LOW);
                   Serial.println("Ventilador ON (CO2 alto)");
                }
                break;

            case VENTILADOR_ENCENDIDO_3S:
                if (currentMillis - temporizadorActivoVentilador >= 3000) {
                    digitalWrite(RELAY1, HIGH);
                    estadoVentiladorActual = VENTILADOR_APAGADO_5S;
                    temporizadorActivoVentilador = currentMillis;
                    Serial.println("Ventilador OFF (después de 3s)");
                }
                break;

            case VENTILADOR_APAGADO_5S:
                if (currentMillis - temporizadorActivoVentilador >= 5000) {
                    estadoVentiladorActual = VENTILADOR_ENCENDIDO_5S;
                    temporizadorActivoVentilador = currentMillis;
                    digitalWrite(RELAY1, LOW);
                    Serial.println("Ventilador ON (pulso de 5s)");
                }
                break;

            case VENTILADOR_ENCENDIDO_5S:
                if (currentMillis - temporizadorActivoVentilador >= 10000) {
                    digitalWrite(RELAY1, HIGH);
                    estadoVentiladorActual = VENTILADOR_ESPERA_ESTABILIZACION;
                    temporizadorActivoVentilador = currentMillis;
                    Serial.println("Ventilador OFF (después de 10s)");
                }
                break;

            case VENTILADOR_ESPERA_ESTABILIZACION:
                if (currentMillis - temporizadorActivoVentilador >= 15000) {
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
                        case NINGUNA:
                            condicionesMejoraron = true;
                            break;
                    }

                    if (condicionesMejoraron) {
                        estadoVentiladorActual = VENTILADOR_INACTIVO;
                        razonActivacionVentilador = NINGUNA;
                        Serial.println("Ventilador IDLE (condiciones mejoraron)");
                    } else {
                        estadoVentiladorActual = VENTILADOR_ENCENDIDO_5S;
                        temporizadorActivoVentilador = currentMillis;
                        digitalWrite(RELAY1, LOW);
                        Serial.println("Ventilador ON (reinicio ciclo, condiciones no mejoraron)");
                    }
                }
                break;
        }
    }
    // --- Alternancia de pantalla de alarmas/sensores ---
    static unsigned long tiempoAlternarPantalla = 0;
    static bool mostrandoAlarmas = false;
    String avisos = "";
    obtenerAlarmasActivas(avisos);

    if (currentMillis - tiempoInicioModoFuncionamiento >= 180000 && avisos.length() > 0 && !alarmasSilenciadas) {
        if (currentMillis - tiempoAlternarPantalla >= 3000) {
            mostrandoAlarmas = !mostrandoAlarmas;
            tiempoAlternarPantalla = currentMillis;
            avisoMostradoEnLCD = false; // Forzar refresco solo al alternar
        } 

        if (mostrandoAlarmas) {
        // Solo refrescar si el aviso cambió o no se mostró aún
          if (!avisoMostradoEnLCD || ultimoAvisoMostrado != avisos) {
              lcd.clear();
              int linea = 0;
              size_t inicio = 0;
              while (linea < LCD_ROWS && inicio < avisos.length()) {
                  int fin = avisos.indexOf('\n', inicio);
                  if (fin == -1) fin = avisos.length();
                  String avisoLinea = avisos.substring(inicio, fin);
                  while (avisoLinea.length() < LCD_COLUMNS) avisoLinea += " ";
                  lcd.setCursor(0, linea);
                  lcd.print(avisoLinea);
                  inicio = fin + 1;
                  linea++;
              }
          ultimoAvisoMostrado = avisos;
          avisoMostradoEnLCD = true;
          }
          avisoMostradoEnLCD = true;
          ultimoAvisoMostrado = avisos;
          return; // No mostrar sensores mientras se muestra el aviso
        }
    } else {
          avisoMostradoEnLCD = false; // Forzar refresco cuando vuelva a mostrar aviso
    }



    // --------------------- Actualización selectiva del LCD ------------------------//
    // Línea 0: Temperatura
    if (abs(currentTemp - lastDisplayedTemp) > TEMP_DISPLAY_THRESHOLD || lastDisplayedTemp == -999.0) {
        lastDisplayedTemp = currentTemp;
        lcd.setCursor(0, 0);
        String linea = "T: " + String(lastDisplayedTemp, 1) + " C";

        while (linea.length() < LCD_COLUMNS) linea += " ";
        lcd.print(linea);
    }
    // Línea 1: Humedad
    if (abs(currentHum - lastDisplayedHum) > HUM_DISPLAY_THRESHOLD || lastDisplayedHum == -999.0) {
        lastDisplayedHum = currentHum;
        lcd.setCursor(0, 1);
        String linea = "H: " + String(lastDisplayedHum, 0) + " %";
        while (linea.length() < LCD_COLUMNS) linea += " ";
        lcd.print(linea);
    }
    // Línea 2: CO2
    if (abs(currentCO2 - lastDisplayedCO2) > CO2_DISPLAY_THRESHOLD || lastDisplayedCO2 == -999.0) {
        lastDisplayedCO2 = currentCO2;
        lcd.setCursor(0, 2);
        String linea = "CO2: " + String((int)lastDisplayedCO2) + " ppm";
        while (linea.length() < LCD_COLUMNS) linea += " ";
        lcd.print(linea);
    }
    // Línea 3: Mensaje fijo

    if (!mensajeMostrado) {
        lcd.setCursor(0, 3);
        String linea = "Pulsa para ir a Menu";
        while (linea.length() < LCD_COLUMNS) linea += " ";
        lcd.print(linea);
        mensajeMostrado = true;
    }

    // --------------------- SISTEMA DE AVISOS DE ALARMAS ---------------------------------------//
    static String ultimasAlarmasActivas = ""; // Guarda el último conjunto de alarmas activas

    if (currentMillis - tiempoInicioModoFuncionamiento >= 180000) {
      String avisos = "";
      obtenerAlarmasActivas(avisos);

      // Detectar si hay un cambio en las alarmas activas (nueva alarma o se desactivó alguna)
      static String alarmasPrevias = "";
      bool cambioAlarmas = (avisos != alarmasPrevias);
      alarmasPrevias = avisos;

    // Si hay un cambio en las alarmas activas, desilenciar automáticamente
    if (cambioAlarmas && alarmasSilenciadas) {
        alarmasSilenciadas = false;
        mostrarAvisoAlarma = false;
        necesitaRefrescarLCD = true;
    }

    // Si el usuario gira el encoder
    if (deltaEncoder != 0 && avisos.length() > 0) {
        if (!alarmasSilenciadas) {
            // Silenciar alarmas
            alarmasSilenciadas = true;
            tiempoSilenciadoAlarmas = currentMillis;
            mostrarAvisoAlarma = false;
            necesitaRefrescarLCD = true;
        } else {
            // Si ya están silenciadas, desilenciar
            alarmasSilenciadas = false;
            mostrarAvisoAlarma = false;
            necesitaRefrescarLCD = true;
        }
    }
//////////////////////////////////////

    
    // Si las alarmas están silenciadas y hay alarmas activas, mostrar mensaje especial
    String avisosTemp = "";
    obtenerAlarmasActivas(avisosTemp);
    if (alarmasSilenciadas && avisosTemp.length() > 0) {
      lcd.setCursor(0, 0);
      String linea = "T: " + String(lastDisplayedTemp, 1) + " C - H: " + String(lastDisplayedHum, 0) + " %";
      while (linea.length() < LCD_COLUMNS) linea += " ";
      lcd.print(linea);
      lcd.setCursor(0, 1);
      linea = "CO2: " + String((int)lastDisplayedCO2) + " ppm";
      while (linea.length() < LCD_COLUMNS) linea += " ";
      lcd.print(linea);
      lcd.setCursor(0, 2);
      linea = "Alarmas silenciadas";
      while (linea.length() < LCD_COLUMNS) linea += " ";
      lcd.print(linea);
      lcd.setCursor(0, 3);
      linea = "Pulsa para ir a menu";
      while (linea.length() < LCD_COLUMNS) linea += " ";
      lcd.print(linea);
      return;
    }

    // Si hay alarmas activas y no están silenciadas, mostrar la opción de silenciar
    if (avisosTemp.length() > 0 && !alarmasSilenciadas && currentMillis - tiempoInicioModoFuncionamiento >= 180000) {
      lcd.setCursor(0, 0);
      String linea = "T: " + String(lastDisplayedTemp, 1) + " C - H: " + String(lastDisplayedHum, 0) + " %";
      while (linea.length() < LCD_COLUMNS) linea += " ";
      lcd.print(linea);
      lcd.setCursor(0, 1);
      linea = "CO2: " + String((int)lastDisplayedCO2) + " ppm";
      while (linea.length() < LCD_COLUMNS) linea += " ";
      lcd.print(linea);
      lcd.setCursor(0, 2);
      linea = "Girar para silenciar";
      while (linea.length() < LCD_COLUMNS) linea += " ";
      lcd.print(linea);
      lcd.setCursor(0, 3);
      linea = "Pulsa para ir a menu";
      while (linea.length() < LCD_COLUMNS) linea += " ";
      lcd.print(linea);
      return;
    }
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

// *** Nueva función para guardar el estado de la aplicación en EEPROM ***
void guardarEstadoAppEEPROM() {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.put(EEPROM_APP_STATE_ADDR, (uint8_t)(estadoActualApp == ESTADO_MODO_FUNCIONAMIENTO ? 1 : 0));
    EEPROM.commit();
    EEPROM.end();
    Serial.printf("Estado de app guardado en EEPROM: %d\n", lastAppStateFlag);
}

// *** Nueva función para cargar el estado de la aplicación desde EEPROM ***
void cargarEstadoAppEEPROM() {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.get(EEPROM_APP_STATE_ADDR, lastAppStateFlag);
    EEPROM.end();
    // Validar si el valor es consistente (0 o 1)
    if (lastAppStateFlag != 0 && lastAppStateFlag != 1) {
        lastAppStateFlag = 0; // Por defecto, si es inconsistente, ir al menú
    }
    Serial.printf("Estado de app cargado de EEPROM: %d\n", lastAppStateFlag);
}


void manejarEditarTemperatura(int deltaEncoder, bool pulsadoSwitch) {
  static bool lastValor = -999;
  // Forzar refresco completo al entrar por primera vez
  if (!pantallaEditTempMostrada) {
  lcd.clear();
  lcd.home();
  String titulo = "Temperatura";
  while (titulo.length() < LCD_COLUMNS) titulo += " ";
  lcd.print(titulo);
  lcd.setCursor(0, 1);
  lcd.print(String(setpointTemperatura, 1) + " C              ");
  lcd.setCursor(0, 2);
  lcd.print("Girar para ajustar  ");
  lcd.setCursor(0, 3);
  lcd.print("Pulsar para volver  ");
  lastValor = setpointTemperatura; // Guardar el valor inicial
  pantallaEditTempMostrada = true; // Marcar que la pantalla ya se mostró
  return; // Salir para evitar refresco innecesario
  }
  if (setpointTemperatura != lastValor) {
    lcd.setCursor(0, 1);
    lcd.print(String(setpointTemperatura, 1) + " C              ");
    lastValor = setpointTemperatura; // Actualizar el último valor mostrado
  }
}

void manejarEditarHumedad(int deltaEncoder, bool pulsadoSwitch) {
  static bool lastValor = -999;
  // Forzar refresco completo al entrar por primera vez
  if (!pantallaEditHumMostrada) {
  lcd.clear();
  lcd.home();
  String titulo = "Humedad";
  while (titulo.length() < LCD_COLUMNS) titulo += " ";
  lcd.print(titulo);
  lcd.setCursor(0, 1);
  lcd.print(String(setpointHumedad, 0) + " %              ");
  lcd.setCursor(0, 2);
  lcd.print("Girar para ajustar  ");
  lcd.setCursor(0, 3);
  lcd.print("Pulsar para volver  ");
  lastValor = setpointHumedad; // Guardar el valor inicial
  pantallaEditHumMostrada = true; // Marcar que la pantalla ya se mostró
  return; // Salir para evitar refresco innecesario
  }
  if (setpointHumedad != lastValor) {
    lcd.setCursor(0, 1);
    lcd.print(String(setpointHumedad, 0) + " %        ");
    lastValor = setpointHumedad; // Actualizar el último valor mostrado
  }
}

void manejarEditarCO2(int deltaEncoder, bool pulsadoSwitch) {
  static bool lastValor = -999;
 
  // Forzar refresco completo al entrar por primera vez
  if (!pantallaEditCO2Mostrada) {
  lcd.clear();
  lcd.home();
  String titulo = "CO2";
  while (titulo.length() < LCD_COLUMNS) titulo += " ";
  lcd.print(titulo);
  lcd.setCursor(0, 1);
  lcd.print(String(setpointCO2) + " ppm            ");
  lcd.setCursor(0, 2);
  lcd.print("Girar para ajustar  ");
  lcd.setCursor(0, 3);
  lcd.print("Pulsar para volver  ");
  lastValor = setpointCO2; // Guardar el valor inicial
  pantallaEditCO2Mostrada = true; // Marcar que la pantalla ya se mostró
  return; // Salir para evitar refresco innecesario
  }
  if (setpointCO2 != lastValor) {
    lcd.setCursor(0, 1);
    lcd.print(String(setpointCO2) + " ppm       ");
    lastValor = setpointCO2; // Actualizar el último valor mostrado
  }
}

void manejarEditarAlarmaTempMin(int deltaEncoder, bool pulsadoSwitch) {
  static bool lastValor = -999;
  if (!pantallaMostradaATMin){
  lcd.clear();
  lcd.home();
  lcd.print("Alarma Temp Min     ");
  lcd.setCursor(0, 1);
  lcd.print(String(alarmaTempMin, 1) + " C        ");
  lcd.setCursor(0, 2);
  lcd.print("Girar para ajustar  ");
  lcd.setCursor(0, 3);
  lcd.print("Pulsar para volver  ");
  lastValor = alarmaTempMin; // Guardar el valor inicial
  pantallaMostradaATMin = true; // Marcar que la pantalla ya se mostró
  return; // Salir para evitar refresco innecesario
  }
  if (alarmaTempMin != lastValor) {
    lcd.setCursor(0, 1);
    lcd.print(String(alarmaTempMin, 1) + " C        ");
    lastValor = alarmaTempMin; // Actualizar el último valor mostrado
  }
  
}

void manejarEditarAlarmaTempMax(int deltaEncoder, bool pulsadoSwitch) {
  static bool lastValor = -999;
  if (!pantallaMostradaATMax) {
    lcd.clear();
    lcd.home();
    lcd.print("Alarma Temp Max     ");
    lcd.setCursor(0, 1);
    lcd.print(String(alarmaTempMax, 1) + " C        ");
    lcd.setCursor(0, 2);
    lcd.print("Girar para ajustar  ");
    lcd.setCursor(0, 3);
    lcd.print("Pulsar para volver  ");
    lastValor = alarmaTempMax; // Guardar el valor inicial
    pantallaMostradaATMax = true; // Marcar que la pantalla ya se mostró
    return;
  }
  if (alarmaTempMax != lastValor) {
    lcd.setCursor(0, 1);
    lcd.print(String(alarmaTempMax, 1) + " C        ");
    lastValor = alarmaTempMax; // Actualizar el último valor mostrado
  }
}

void manejarEditarAlarmaHumMin(int deltaEncoder, bool pulsadoSwitch) {
  static bool lastValor = -999;
  if (!pantallaMostradaAHMin) {
  lcd.clear();
  lcd.home();
  lcd.print("Alarma Hum Min      ");
  lcd.setCursor(0, 1);
  lcd.print(String(alarmaHumMin, 0) + " %        ");
  lcd.setCursor(0, 2);
  lcd.print("Girar para ajustar  ");
  lcd.setCursor(0, 3);
  lcd.print("Pulsar para volver  ");
  lastValor = alarmaHumMin; // Guardar el valor inicial
  pantallaMostradaAHMin = true; // Marcar que la pantalla ya se mostró
  return; // Salir para evitar refresco innecesario
  }
  if (alarmaHumMin != lastValor) {
    lcd.setCursor(0, 1);
    lcd.print(String(alarmaHumMin, 0) + " %        ");
    lastValor = alarmaHumMin; // Actualizar el último valor mostrado
  }
}

void manejarEditarAlarmaHumMax(int deltaEncoder, bool pulsadoSwitch) {
  static bool lastValor = -999;
  if (!pantallaMostradaAHMax) {
  lcd.clear();
  lcd.home();
  lcd.print("Alarma Hum Max      ");
  lcd.setCursor(0, 1);
  lcd.print(String(alarmaHumMax, 0) + " %        ");
  lcd.setCursor(0, 2);
  lcd.print("Girar para ajustar  ");
  lcd.setCursor(0, 3);
  lcd.print("Pulsar para volver  ");
  lastValor = alarmaHumMax; // Guardar el valor inicial
  pantallaMostradaAHMax = true; // Marcar que la pantalla ya se mostró
  return; // Salir para evitar refresco innecesario
  }
  if (alarmaHumMax != lastValor) {
    lcd.setCursor(0, 1);
    lcd.print(String(alarmaHumMax, 0) + " %        ");
    lastValor = alarmaHumMax; // Actualizar el último valor mostrado
  }
}

void manejarEditarAlarmaCO2Min(int deltaEncoder, bool pulsadoSwitch) {
  static bool lastValor = -999;
  if (!pantallaMostradaACO2Min) {
  lcd.clear();
  lcd.home();
  lcd.print("Alarma CO2 Min      ");
  lcd.setCursor(0, 1);
  lcd.print(String(alarmaCO2Min) + " ppm       ");
  lcd.setCursor(0, 2);
  lcd.print("Girar para ajustar  ");
  lcd.setCursor(0, 3);
  lcd.print("Pulsar para volver  ");
  lastValor = alarmaCO2Min; // Guardar el valor inicial
  pantallaMostradaACO2Min = true; // Marcar que la pantalla ya se mostró
  return; // Salir para evitar refresco innecesario
  }
  if (alarmaCO2Min != lastValor) {
    lcd.setCursor(0, 1);
    lcd.print(String(alarmaCO2Min) + " ppm       ");
    lastValor = alarmaCO2Min; // Actualizar el último valor mostrado
  }
}

void manejarEditarAlarmaCO2Max(int deltaEncoder, bool pulsadoSwitch) {
  static bool lastValor = -999;
  if (!pantallaMostradaACO2Max) {
  lcd.clear();
  lcd.home();
  lcd.print("Alarma CO2 Max      ");
  lcd.setCursor(0, 1);
  lcd.print(String(alarmaCO2Max) + " ppm       ");
  lcd.setCursor(0, 2);
  lcd.print("Girar para ajustar  ");
  lcd.setCursor(0, 3);
  lcd.print("Pulsar para volver  ");
  lastValor = alarmaCO2Max; // Guardar el valor inicial
  pantallaMostradaACO2Max = true; // Marcar que la pantalla ya se mostró
  return; // Salir para evitar refresco innecesario
  } 
  if (alarmaCO2Max != lastValor) {
    lcd.setCursor(0, 1);
    lcd.print(String(alarmaCO2Max) + " ppm       ");
    lastValor = alarmaCO2Max; // Actualizar el último valor mostrado
  }
}

void obtenerAlarmasActivas(String &avisos) {
    avisos = "";
    if (currentTemp > alarmaTempMax) avisos += "ALERTA: TEMP ALTA!\n";
    if (currentTemp < alarmaTempMin) avisos += "ALERTA: TEMP BAJA!\n";
    if (currentHum > alarmaHumMax) avisos += "ALERTA: HUM ALTA!\n";
    if (currentHum < alarmaHumMin) avisos += "ALERTA: HUM BAJA!\n";
    if (currentCO2 > alarmaCO2Max) avisos += "ALERTA: CO2 ALTO!\n";
    if (currentCO2 < alarmaCO2Min) avisos += "ALERTA: CO2 BAJO!\n";
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