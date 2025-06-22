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
// --- NUEVAS VARIABLES GLOBALES PARA ALARMAS CRITICAS ---
unsigned long tiempoInicioAlarmaTempMin;
unsigned long tiempoInicioAlarmaTempMax;
unsigned long tiempoInicioAlarmaHumMin;
unsigned long tiempoInicioAlarmaHumMax;
unsigned long tiempoInicioAlarmaCO2Min;
unsigned long tiempoInicioAlarmaCO2Max;

bool notificacionCriticaTempMinActiva = false;
bool notificacionCriticaTempMaxActiva = false;
bool notificacionCriticaHumMinActiva = false;
bool notificacionCriticaHumMaxActiva = false;
bool notificacionCriticaCO2MinActiva = false;
bool notificacionCriticaCO2MaxActiva = false;
bool alarmaDescartada = false;
bool estaEnExcesoTemperatura();
bool estaEnDefectoTemperatura();
bool estaEnExcesoHumedad();
bool estaEnDefectoHumedad();
bool estaEnExcesoCO2();
bool estaEnDefectoCO2();
bool hayAlarmasCriticas() {
  return estaEnExcesoTemperatura() || estaEnDefectoTemperatura() ||
         estaEnExcesoHumedad() || estaEnDefectoHumedad() ||
         estaEnExcesoCO2() || estaEnDefectoCO2();
};


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
  ESTADO_EDITAR_TEMP = 10, // Explicitly assign 10 here
  ESTADO_EDITAR_HUM,
  ESTADO_EDITAR_CO2,
  ESTADO_EDITAR_ALARMA_TEMP_MIN,
  ESTADO_EDITAR_ALARMA_TEMP_MAX,
  ESTADO_EDITAR_ALARMA_HUM_MIN,
  ESTADO_EDITAR_ALARMA_HUM_MAX,
  ESTADO_EDITAR_ALARMA_CO2_MIN,
  ESTADO_EDITAR_ALARMA_CO2_MAX,
  ESTADO_ALARMA_CRITICA_AVISO // Este se asignará automáticamente al siguiente valor único (19 en este caso).
};
EstadoApp estadoActualApp = ESTADO_MENU_PRINCIPAL;
EstadoApp lastValidState = ESTADO_MENU_PRINCIPAL; // Inicializa con un estado por defect
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

unsigned long ultimaActualizacionSensores = 0;
const unsigned long INTERVALO_LECTURA_SENSORES = 1000; // Leer sensores cada 1 segundo
unsigned long lastSensorReadTime = 0; // Declaración de lastSensorReadTime

// Variable para el estado guardado en EEPROM
// 0: Modo Menu, 1: Modo Funcionamiento
uint8_t lastAppStateFlag = 0;
const int EEPROM_APP_STATE_ADDR = EEPROM_SIZE - sizeof(uint8_t); // Último byte de la EEPROM

// Banderas de estado de alarma (activas cuando la condición de alarma se cumple)
bool banderaAlarmaTempMinActiva = false;
bool banderaAlarmaTempMaxActiva = false;
bool banderaAlarmaHumMinActiva = false;
bool banderaAlarmaHumMaxActiva = false;
bool banderaAlarmaCO2MinActiva = false;
bool banderaAlarmaCO2MaxActiva = false;
bool banderaBajoCO2 = false; // Bandera específica para CO2 bajo (para la ventilación)

// Estados para el ventilador por ventilación programada
enum EstadoVentilacionProgramada {
  VENTILACION_PROGRAMADA_INACTIVA,
  VENTILACION_PROGRAMADA_ACTIVA,
  VENTILACION_PROGRAMADA_PAUSA
};
EstadoVentilacionProgramada estadoVentilacionProgramada = VENTILACION_PROGRAMADA_INACTIVA;
unsigned long tiempoInicioVentilacionProgramada = 0;




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
void guardarEstadoAppEEPROM();
void cargarEstadoAppEEPROM();

// Funciones auxiliares para la prioridad de ventilación
bool estaEnExcesoTemperatura();
bool estaEnExcesoHumedad();
bool estaEnExcesoCO2();
bool estaEnDefectoCO2();

void manejarVentilador(int);
void evaluarAlarmasCriticas();
void manejarAlarmaCriticaAviso(int deltaEncoder, bool pulsadoSwitch); // Prototipo añadido

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

  

  // Cargar setpoints y alarmas al iniciar
  cargarSetpointsEEPROM();
  cargarEstadoAppEEPROM();

  // ETIQUETA: Inicializacion de sensores y Firebase de 'prueba encoder'
  pinMode(DHTPIN, INPUT_PULLUP); // Fuerza modo correcto para GPIO14
  delay(100);
  dhtSensor.iniciar();
  delay(2000); // Espera crítica para DHT11
  gasSensor.begin();
  firebase.begin(WIFI_SSID, WIFI_PASSWORD,
                FIREBASE_API_KEY, FIREBASE_URL,
                FIREBASE_EMAIL, FIREBASE_PASSWORD);


  Wire.begin(4, 5); // Inicializa la comunicación I2C. Para ESP8266, los pines SDA=4, SCL=5 son comunes.
  lcd.init();      // Inicializa el controlador I2C interno de la librería
  lcd.begin(LCD_COLUMNS, LCD_ROWS); // Inicializa el display con sus dimensiones
  lcd.backlight(); // Enciende la luz de fondo del display
  lcd.clear();     // Limpia el contenido del display
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

// ---------------- FUNCIONES DE MANEJO DE ESTADOS Y MENÚ ----------------
void manejarMenuPrincipal(int deltaEncoder, bool pulsadoSwitch) {
  if (necesitaRefrescarLCD) {
    lcd.clear();
    desplazamientoScroll = 0; // Resetear desplazamiento al entrar al menú principal
    necesitaRefrescarLCD = false;
  }

  if (deltaEncoder != 0) {
    indiceMenuPrincipal += deltaEncoder;
    if (indiceMenuPrincipal < 0) indiceMenuPrincipal = TOTAL_OPCIONES_MENU_PRINCIPAL - 1;
    if (indiceMenuPrincipal >= TOTAL_OPCIONES_MENU_PRINCIPAL) indiceMenuPrincipal = 0;
    necesitaRefrescarLCD = true; // Forzar refresco si hay cambio
  }

  // Lógica de scroll
  if (indiceMenuPrincipal >= desplazamientoScroll + OPCIONES_VISIBLES_PANTALLA) {
    desplazamientoScroll = indiceMenuPrincipal - OPCIONES_VISIBLES_PANTALLA + 1;
  } else if (indiceMenuPrincipal < desplazamientoScroll) {
    desplazamientoScroll = indiceMenuPrincipal;
  }

  if (necesitaRefrescarLCD) {
    lcd.clear();
    for (int i = 0; i < OPCIONES_VISIBLES_PANTALLA; i++) {
      int indiceActual = desplazamientoScroll + i;
      if (indiceActual < TOTAL_OPCIONES_MENU_PRINCIPAL) {
        lcd.setCursor(0, i);
        if (indiceActual == indiceMenuPrincipal) {
          lcd.print(">");
        } else {
          lcd.print(" ");
        }
        lcd.print(opcionesMenuPrincipal[indiceActual]);
      }
    }
  }
}

void manejarEditarSetpoints(int deltaEncoder, bool pulsadoSwitch) {
  if (necesitaRefrescarLCD) {
    lcd.clear();
    lcd.home();
    lcd.print("Editar Setpoints");
    necesitaRefrescarLCD = false;
  }

  if (deltaEncoder != 0) {
    indiceSubMenuSetpoints += deltaEncoder;
    if (indiceSubMenuSetpoints < 0) indiceSubMenuSetpoints = TOTAL_OPCIONES_SUBMENU_SETPOINTS - 1;
    if (indiceSubMenuSetpoints >= TOTAL_OPCIONES_SUBMENU_SETPOINTS) indiceSubMenuSetpoints = 0;
    necesitaRefrescarLCD = true;
  }

  if (necesitaRefrescarLCD) {
    lcd.setCursor(0, 1);
    lcd.print("                "); // Limpiar línea
    lcd.setCursor(0, 2);
    lcd.print("                "); // Limpiar línea
    lcd.setCursor(0, 3);
    lcd.print("                "); // Limpiar línea

    lcd.setCursor(0, 1);
    switch (indiceSubMenuSetpoints) {
      case 0:
        lcd.print("> Temp: " + String(setpointTemperatura, 1) + " C");
        break;
      case 1:
        lcd.print("> Hum: " + String(setpointHumedad, 0) + " %");
        break;
      case 2:
        lcd.print("> CO2: " + String(setpointCO2) + " ppm");
        break;
      case 3:
        lcd.print("> Volver");
        break;
    }
    necesitaRefrescarLCD = false;
  }
}

void manejarSubMenuAlarmas(int deltaEncoder, bool pulsadoSwitch) {
  if (necesitaRefrescarLCD) {
    lcd.clear();
    lcd.home();
    lcd.print("SubMenu Alarmas");
    necesitaRefrescarLCD = false;
  }

  if (deltaEncoder != 0) {
    indiceSubMenuAlarmas += deltaEncoder;
    if (indiceSubMenuAlarmas < 0) indiceSubMenuAlarmas = TOTAL_OPCIONES_SUBMENU_ALARMAS - 1;
    if (indiceSubMenuAlarmas >= TOTAL_OPCIONES_SUBMENU_ALARMAS) indiceSubMenuAlarmas = 0;
    necesitaRefrescarLCD = true;
  }

  if (necesitaRefrescarLCD) {
    lcd.setCursor(0, 1);
    lcd.print("                "); // Limpiar línea
    lcd.setCursor(0, 2);
    lcd.print("                "); // Limpiar línea
    lcd.setCursor(0, 3);
    lcd.print("                "); // Limpiar línea

    lcd.setCursor(0, 1);
    switch (indiceSubMenuAlarmas) {
      case 0:
        lcd.print("> Mostrar Alarmas");
        break;
      case 1:
        lcd.print("> Editar Alarmas");
        break;
      case 2:
        lcd.print("> Volver");
        break;
    }
    necesitaRefrescarLCD = false;
  }
}


void manejarConfirmacionModoFuncionamiento(int deltaEncoder, bool pulsadoSwitch) {
  if (necesitaRefrescarLCD) {
    lcd.clear();
    lcd.home();
    lcd.print("Confirmar Modo?");
    necesitaRefrescarLCD = false;
  }

  if (deltaEncoder != 0) {
    indiceConfirmacion += deltaEncoder;
    if (indiceConfirmacion < 0) indiceConfirmacion = TOTAL_OPCIONES_CONFIRMACION - 1;
    if (indiceConfirmacion >= TOTAL_OPCIONES_CONFIRMACION) indiceConfirmacion = 0;
    necesitaRefrescarLCD = true;
  }

  if (necesitaRefrescarLCD) {
    lcd.setCursor(0, 2);
    lcd.print("                "); // Limpiar línea
    lcd.setCursor(0, 1);
    if (indiceConfirmacion == 0) { // SI
      lcd.print("> SI   NO");
    } else { // NO
      lcd.print("  SI > NO");
    }
    necesitaRefrescarLCD = false;
  }
}

void manejarModoFuncionamiento(int deltaEncoder, bool pulsadoSwitch) {
  if (necesitaRefrescarLCD) {
    if (!initialMessageDisplayed) {
      lcd.clear();
      lcd.home();
      lcd.print("Modo Funcionamiento");
      lcd.setCursor(0,1);
      lcd.print("Iniciado");
      modoFuncionamientoStartTime = millis();
      initialMessageDisplayed = true;
    } else if (millis() - modoFuncionamientoStartTime > 2000) { // Después de 2 segundos, mostrar datos
      lcd.clear();
      // Actualizar pantalla solo si los valores han cambiado significativamente
      if (abs(currentTemp - lastDisplayedTemp) > TEMP_DISPLAY_THRESHOLD || necesitaRefrescarLCD) {
        lcd.home();
        lcd.print("Temp: " + String(currentTemp, 1) + " C");
        lastDisplayedTemp = currentTemp;
      }
      if (abs(currentHum - lastDisplayedHum) > HUM_DISPLAY_THRESHOLD || necesitaRefrescarLCD) {
        lcd.setCursor(0,1);
        lcd.print("Hum:  " + String(currentHum, 0) + " %");
        lastDisplayedHum = currentHum;
      }
      if (abs(currentCO2 - lastDisplayedCO2) > CO2_DISPLAY_THRESHOLD || necesitaRefrescarLCD) {
        lcd.setCursor(0,2);
        lcd.print("CO2:  " + String(currentCO2) + " ppm");
        lastDisplayedCO2 = currentCO2;
      }
      lcd.setCursor(0,3);
      lcd.print("SW para Menu");
      necesitaRefrescarLCD = false;
    }
  }

  if (pulsadoSwitch) {
    // Si se presiona el switch en modo funcionamiento, ir a la confirmación
    estadoActualApp = ESTADO_CONFIRMACION_VOLVER_MENU;
    indiceConfirmacion = 0; // Reiniciar índice a SI
    necesitaRefrescarLCD = true;
    initialMessageDisplayed = false; // Resetear para la próxima vez
  }
}

void manejarConfirmacionVolverMenu(int deltaEncoder, bool pulsadoSwitch) {
  if (necesitaRefrescarLCD) {
    lcd.clear();
    lcd.home();
    lcd.print("Salir Modo Func?");
    necesitaRefrescarLCD = false;
  }

  if (deltaEncoder != 0) {
    indiceConfirmacion += deltaEncoder;
    if (indiceConfirmacion < 0) indiceConfirmacion = TOTAL_OPCIONES_CONFIRMACION - 1;
    if (indiceConfirmacion >= TOTAL_OPCIONES_CONFIRMACION) indiceConfirmacion = 0;
    necesitaRefrescarLCD = true;
  }

  if (necesitaRefrescarLCD) {
    lcd.setCursor(0, 2);
    lcd.print("                "); // Limpiar línea
    lcd.setCursor(0, 1);
    if (indiceConfirmacion == 0) { // SI
      lcd.print("> SI   NO");
    } else { // NO
      lcd.print("  SI > NO");
    }
    necesitaRefrescarLCD = false;
  }
}

void manejarEditarTemperatura(int deltaEncoder, bool pulsadoSwitch) {
  if (necesitaRefrescarLCD) {
    lcd.clear();
    necesitaRefrescarLCD = false;
  }
  lcd.setCursor(0, 0);
  lcd.print("Set Temp");
  lcd.setCursor(0, 1);
  lcd.print(String(setpointTemperatura, 1) + " C       "); // Limpiar el resto de la línea
  lcd.setCursor(0, 2);
  lcd.print("Girar para ajustar");
  lcd.setCursor(0, 3);
  lcd.print("SW para guardar");

  if (deltaEncoder != 0) {
    setpointTemperatura += deltaEncoder * TEMP_PASO;
    if (setpointTemperatura < TEMP_MIN) setpointTemperatura = TEMP_MIN;
    if (setpointTemperatura > TEMP_MAX) setpointTemperatura = TEMP_MAX;
    necesitaRefrescarLCD = true; // Para actualizar valor en pantalla
  }
}

void manejarEditarHumedad(int deltaEncoder, bool pulsadoSwitch) {
  if (necesitaRefrescarLCD) {
    lcd.clear();
    necesitaRefrescarLCD = false;
  }
  lcd.setCursor(0, 0);
  lcd.print("Set Humedad");
  lcd.setCursor(0, 1);
  lcd.print(String(setpointHumedad, 0) + " %       "); // Limpiar el resto de la línea
  lcd.setCursor(0, 2);
  lcd.print("Girar para ajustar");
  lcd.setCursor(0, 3);
  lcd.print("SW para guardar");

  if (deltaEncoder != 0) {
    setpointHumedad += deltaEncoder * HUM_PASO;
    if (setpointHumedad < HUM_MIN) setpointHumedad = HUM_MIN;
    if (setpointHumedad > HUM_MAX) setpointHumedad = HUM_MAX;
    necesitaRefrescarLCD = true;
  }
}

void manejarEditarCO2(int deltaEncoder, bool pulsadoSwitch) {
  if (necesitaRefrescarLCD) {
    lcd.clear();
    necesitaRefrescarLCD = false;
  }
  lcd.setCursor(0, 0);
  lcd.print("Set CO2");
  lcd.setCursor(0, 1);
  lcd.print(String(setpointCO2) + " ppm       "); // Limpiar el resto de la línea
  lcd.setCursor(0, 2);
  lcd.print("Girar para ajustar");
  lcd.setCursor(0, 3);
  lcd.print("SW para guardar");

  if (deltaEncoder != 0) {
    setpointCO2 += deltaEncoder * CO2_PASO;
    if (setpointCO2 < CO2_MIN) setpointCO2 = CO2_MIN;
    if (setpointCO2 > CO2_MAX) setpointCO2 = CO2_MAX;
    necesitaRefrescarLCD = true;
  }
}

void manejarEditarAlarmas(int deltaEncoder, bool pulsadoSwitch) {
  if (necesitaRefrescarLCD) {
    lcd.clear();
    lcd.home();
    lcd.print("Editar Alarmas");
    necesitaRefrescarLCD = false;
  }

  if (deltaEncoder != 0) {
    indiceEditarAlarmas += deltaEncoder;
    if (indiceEditarAlarmas < 0) indiceEditarAlarmas = TOTAL_OPCIONES_EDITAR_ALARMAS - 1;
    if (indiceEditarAlarmas >= TOTAL_OPCIONES_EDITAR_ALARMAS) indiceEditarAlarmas = 0;
    necesitaRefrescarLCD = true;
  }

  if (necesitaRefrescarLCD) {
    lcd.setCursor(0, 1);
    lcd.print("                "); // Limpiar línea
    lcd.setCursor(0, 2);
    lcd.print("                "); // Limpiar línea
    lcd.setCursor(0, 3);
    lcd.print("                "); // Limpiar línea

    lcd.setCursor(0, 1);
    switch (indiceEditarAlarmas) {
      case 0: lcd.print("> Temp Min: " + String(alarmaTempMin, 1)); break;
      case 1: lcd.print("> Temp Max: " + String(alarmaTempMax, 1)); break;
      case 2: lcd.print("> Hum Min: " + String(alarmaHumMin, 0)); break;
      case 3: lcd.print("> Hum Max: " + String(alarmaHumMax, 0)); break;
      case 4: lcd.print("> CO2 Min: " + String(alarmaCO2Min)); break;
      case 5: lcd.print("> CO2 Max: " + String(alarmaCO2Max)); break;
      case 6: lcd.print("> Guardar y Volver"); break;
    }
    necesitaRefrescarLCD = false;
  }
}

void manejarEditarAlarmaTempMin(int deltaEncoder, bool pulsadoSwitch) {
  if (necesitaRefrescarLCD) {
    lcd.clear();
    necesitaRefrescarLCD = false;
  }
  lcd.setCursor(0, 0);
  lcd.print("Alarma Temp Min");
  lcd.setCursor(0, 1);
  lcd.print(String(alarmaTempMin, 1) + " C       ");
  lcd.setCursor(0, 2);
  lcd.print("Girar para ajustar");
  lcd.setCursor(0, 3);
  lcd.print("SW para volver");

  if (deltaEncoder != 0) {
    alarmaTempMin += deltaEncoder * ALARMA_TEMP_PASO;
    if (alarmaTempMin < ALARMA_TEMP_MIN_RANGO) alarmaTempMin = ALARMA_TEMP_MIN_RANGO;
    if (alarmaTempMin > ALARMA_TEMP_MAX_RANGO) alarmaTempMin = ALARMA_TEMP_MAX_RANGO;
    necesitaRefrescarLCD = true;
  }
}

void manejarEditarAlarmaTempMax(int deltaEncoder, bool pulsadoSwitch) {
  if (necesitaRefrescarLCD) {
    lcd.clear();
    necesitaRefrescarLCD = false;
  }
  lcd.setCursor(0, 0);
  lcd.print("Alarma Temp Max");
  lcd.setCursor(0, 1);
  lcd.print(String(alarmaTempMax, 1) + " C       ");
  lcd.setCursor(0, 2);
  lcd.print("Girar para ajustar");
  lcd.setCursor(0, 3);
  lcd.print("SW para volver");

  if (deltaEncoder != 0) {
    alarmaTempMax += deltaEncoder * ALARMA_TEMP_PASO;
    if (alarmaTempMax < ALARMA_TEMP_MIN_RANGO) alarmaTempMax = ALARMA_TEMP_MIN_RANGO;
    if (alarmaTempMax > ALARMA_TEMP_MAX_RANGO) alarmaTempMax = ALARMA_TEMP_MAX_RANGO;
    necesitaRefrescarLCD = true;
  }
}

void manejarEditarAlarmaHumMin(int deltaEncoder, bool pulsadoSwitch) {
  if (necesitaRefrescarLCD) {
    lcd.clear();
    necesitaRefrescarLCD = false;
  }
  lcd.setCursor(0, 0);
  lcd.print("Alarma Hum Min");
  lcd.setCursor(0, 1);
  lcd.print(String(alarmaHumMin, 0) + " %       ");
  lcd.setCursor(0, 2);
  lcd.print("Girar para ajustar");
  lcd.setCursor(0, 3);
  lcd.print("SW para volver");

  if (deltaEncoder != 0) {
    alarmaHumMin += deltaEncoder * ALARMA_HUM_PASO;
    if (alarmaHumMin < ALARMA_HUM_MIN_RANGO) alarmaHumMin = ALARMA_HUM_MIN_RANGO;
    if (alarmaHumMin > ALARMA_HUM_MAX_RANGO) alarmaHumMin = ALARMA_HUM_MAX_RANGO;
    necesitaRefrescarLCD = true;
  }
}

void manejarEditarAlarmaHumMax(int deltaEncoder, bool pulsadoSwitch) {
  if (necesitaRefrescarLCD) {
    lcd.clear();
    necesitaRefrescarLCD = false;
  }
  lcd.setCursor(0, 0);
  lcd.print("Alarma Hum Max");
  lcd.setCursor(0, 1);
  lcd.print(String(alarmaHumMax, 0) + " %       ");
  lcd.setCursor(0, 2);
  lcd.print("Girar para ajustar");
  lcd.setCursor(0, 3);
  lcd.print("SW para volver");

  if (deltaEncoder != 0) {
    alarmaHumMax += deltaEncoder * ALARMA_HUM_PASO;
    if (alarmaHumMax < ALARMA_HUM_MIN_RANGO) alarmaHumMax = ALARMA_HUM_MIN_RANGO;
    if (alarmaHumMax > ALARMA_HUM_MAX_RANGO) alarmaHumMax = ALARMA_HUM_MAX_RANGO;
    necesitaRefrescarLCD = true;
  }
}

void manejarEditarAlarmaCO2Min(int deltaEncoder, bool pulsadoSwitch) {
  if (necesitaRefrescarLCD) {
    lcd.clear();
    necesitaRefrescarLCD = false;
  }
  lcd.setCursor(0, 0);
  lcd.print("Alarma CO2 Min");
  lcd.setCursor(0, 1);
  lcd.print(String(alarmaCO2Min) + " ppm      ");
  lcd.setCursor(0, 2);
  lcd.print("Girar para ajustar");
  lcd.setCursor(0, 3);
  lcd.print("SW para volver");

  if (deltaEncoder != 0) {
    alarmaCO2Min += deltaEncoder * ALARMA_CO2_PASO;
    if (alarmaCO2Min < ALARMA_CO2_MIN_RANGO) alarmaCO2Min = ALARMA_CO2_MIN_RANGO;
    if (alarmaCO2Min > ALARMA_CO2_MAX_RANGO) alarmaCO2Min = ALARMA_CO2_MAX_RANGO;
    necesitaRefrescarLCD = true;
  }
}

void manejarEditarAlarmaCO2Max(int deltaEncoder, bool pulsadoSwitch) {
  if (necesitaRefrescarLCD) {
    lcd.clear();
    necesitaRefrescarLCD = false;
  }
  lcd.setCursor(0, 0);
  lcd.print("Alarma CO2 Max");
  lcd.setCursor(0, 1);
  lcd.print(String(alarmaCO2Max) + " ppm      ");
  lcd.setCursor(0, 2);
  lcd.print("Girar para ajustar");
  lcd.setCursor(0, 3);
  lcd.print("SW para volver");

  if (deltaEncoder != 0) {
    alarmaCO2Max += deltaEncoder * ALARMA_CO2_PASO;
    if (alarmaCO2Max < ALARMA_CO2_MIN_RANGO) alarmaCO2Max = ALARMA_CO2_MIN_RANGO;
    if (alarmaCO2Max > ALARMA_CO2_MAX_RANGO) alarmaCO2Max = ALARMA_CO2_MAX_RANGO;
    necesitaRefrescarLCD = true;
  }
}

// ---------------- FUNCIONES AUXILIARES ----------------
void IRAM_ATTR leerEncoderISR() {
  unsigned long currentMillis = millis();
  if (currentMillis - tiempoUltimaLecturaCLK > RETARDO_ANTI_REBOTE_ENCODER) {
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
    tiempoUltimaLecturaCLK = currentMillis;
  }
}

void IRAM_ATTR leerSwitchISR() {
  unsigned long currentMillis = millis();
  if (currentMillis - tiempoUltimaPresionSW > RETARDO_ANTI_REBOTE_SW) {
    swFuePresionadoISR = true;
    tiempoUltimaPresionSW = currentMillis;
  }
}

void leerSensores() {
  currentTemp = dhtSensor.getTemperatura();
  currentHum = dhtSensor.getHumedad();
  currentCO2 = gasSensor.leerCO2();

  Serial.print("Temp: "); Serial.print(currentTemp); Serial.print(" C, Hum: "); Serial.print(currentHum); Serial.print(" %, CO2: "); Serial.print(currentCO2); Serial.println(" ppm");

  // Enviar datos a Firebase solo si estamos en modo funcionamiento
  if (estadoActualApp == ESTADO_MODO_FUNCIONAMIENTO) {
    firebase.sendData(currentTemp, currentHum, currentCO2);
  }
}

void guardarSetpointsEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(0, setpointTemperatura);
  EEPROM.put(sizeof(float), setpointHumedad);
  EEPROM.put(2 * sizeof(float), setpointCO2);
  // Guardar alarmas
  EEPROM.put(3 * sizeof(float), alarmaTempMin);
  EEPROM.put(4 * sizeof(float), alarmaTempMax);
  EEPROM.put(5 * sizeof(float), alarmaHumMin);
  EEPROM.put(6 * sizeof(float), alarmaHumMax);
  EEPROM.put(7 * sizeof(float), alarmaCO2Min);
  EEPROM.put(7 * sizeof(float) + sizeof(int), alarmaCO2Max);
  EEPROM.commit();
  EEPROM.end();
  Serial.println("Setpoints y Alarmas guardados en EEPROM.");
}

void cargarSetpointsEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, setpointTemperatura);
  EEPROM.get(sizeof(float), setpointHumedad);
  EEPROM.get(2 * sizeof(float), setpointCO2);
  // Cargar alarmas
  EEPROM.get(3 * sizeof(float), alarmaTempMin);
  EEPROM.get(4 * sizeof(float), alarmaTempMax);
  EEPROM.get(5 * sizeof(float), alarmaHumMin);
  EEPROM.get(6 * sizeof(float), alarmaHumMax);
  EEPROM.get(6 * sizeof(float) + sizeof(int), alarmaCO2Min); // Corregido: sizeof(float) + sizeof(int)
  EEPROM.get(6 * sizeof(float) + 2 * sizeof(int), alarmaCO2Max); // Corregido: sizeof(float) + 2 * sizeof(int)

  EEPROM.end();
  Serial.println("Setpoints y Alarmas cargados de EEPROM.");

  // Validar rangos después de cargar de EEPROM
  if (setpointTemperatura < TEMP_MIN || setpointTemperatura > TEMP_MAX) {
    setpointTemperatura = 25.0;
  }
  if (setpointHumedad < HUM_MIN || setpointHumedad > HUM_MAX) {
    setpointHumedad = 70.0;
  }
  if (setpointCO2 < CO2_MIN || setpointCO2 > CO2_MAX) {
    setpointCO2 = 6000;
  }
  if (alarmaTempMin < ALARMA_TEMP_MIN_RANGO || alarmaTempMin > ALARMA_TEMP_MAX_RANGO) {
    alarmaTempMin = 18.0;
  }
  if (alarmaTempMax < ALARMA_TEMP_MIN_RANGO || alarmaTempMax > ALARMA_TEMP_MAX_RANGO) {
    alarmaTempMax = 30.0;
  }
  if (alarmaHumMin < ALARMA_HUM_MIN_RANGO || alarmaHumMin > ALARMA_HUM_MAX_RANGO) {
    alarmaHumMin = 50.0;
  }
  if (alarmaHumMax < ALARMA_HUM_MIN_RANGO || alarmaHumMax > ALARMA_HUM_MAX_RANGO) {
    alarmaHumMax = 90.0;
  }
  if (alarmaCO2Min < ALARMA_CO2_MIN_RANGO || alarmaCO2Min > ALARMA_CO2_MAX_RANGO) {
    alarmaCO2Min = 600;
  }
  if (alarmaCO2Max < ALARMA_CO2_MIN_RANGO || alarmaCO2Max > ALARMA_CO2_MAX_RANGO) {
    alarmaCO2Max = 1200;
  }
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
  // Asegúrate de que esta lógica esté completa y sea correcta
  return currentTemp > (setpointTemperatura + UMBRAL_TEMP_VENTILADOR);
}

bool estaEnDefectoTemperatura() {
  // Asegúrate de que esta lógica esté completa y sea correcta
  return currentTemp < (setpointTemperatura - UMBRAL_TEMP_VENTILADOR);
}

bool estaEnExcesoHumedad() {
  // Asegúrate de que esta lógica esté completa y sea correcta
  return currentHum > (setpointHumedad + UMBRAL_HUM_VENTILADOR);
}

bool estaEnDefectoHumedad() {
  // Asegúrate de que esta lógica esté completa y sea correcta
  return currentHum < (setpointHumedad - UMBRAL_HUM_VENTILADOR);
}

bool estaEnExcesoCO2() {
  // Asegúrate de que esta lógica esté completa y sea correcta
  return currentCO2 > (setpointCO2 + UMBRAL_CO2_VENTILADOR);
}

bool estaEnDefectoCO2() {
  // Asegúrate de que esta lógica esté completa y sea correcta
  return currentCO2 < (setpointCO2 - UMBRAL_CO2_VENTILADOR);
}

// ---------------- FUNCIONES DE CONTROL DE RELÉS Y SENSORES ----------------
void manejarVentilador(unsigned long currentMillis) { //
    bool necesitaVentilarPorSensores = estaEnExcesoTemperatura() || estaEnExcesoHumedad() || estaEnExcesoCO2();
    bool necesitaVentilarPorCO2Bajo = estaEnDefectoCO2();

    // Actualizar razonActivacionVentilador si cambia la condición de exceso
    if (necesitaVentilarPorSensores) {
        if (estaEnExcesoTemperatura()) razonActivacionVentilador = TEMPERATURA_ALTA;
        else if (estaEnExcesoHumedad()) razonActivacionVentilador = HUMEDAD_ALTA;
        else if (estaEnExcesoCO2()) razonActivacionVentilador = CO2_ALTO;
    } else {
        // Solo resetear si la razón no es por CO2 bajo
        if (razonActivacionVentilador != NINGUNA && !necesitaVentilarPorCO2Bajo) {
             razonActivacionVentilador = NINGUNA;
        }
    }

    // Lógica para ventilación programada de CO2
    if (!ventilacionProgramadaActiva && (currentMillis - ultimoTiempoVentilacionProgramada >= INTERVALO_VENTILACION_PROGRAMADA)) {
        ventilacionProgramadaActiva = true;
        tiempoInicioVentilacionProgramada = currentMillis;
        Serial.println("Ventilacion programada de CO2 iniciada.");
        estadoVentilacionProgramada = VENTILACION_PROGRAMADA_ACTIVA;
    }

    // Gestionar el estado del ventilador
    switch (estadoVentiladorActual) {
        case VENTILADOR_INACTIVO:
            if (necesitaVentilarPorSensores || ventilacionProgramadaActiva) {
                digitalWrite(RELAY1, LOW); // Enciende ventilador
                estadoVentiladorActual = VENTILADOR_ENCENDIDO_3S;
                temporizadorActivoVentilador = currentMillis;
                Serial.print("Ventilador ON por: ");
                if (necesitaVentilarPorSensores) {
                    Serial.print(razonActivacionVentilador == TEMPERATURA_ALTA ? "Temp Alta" : (razonActivacionVentilador == HUMEDAD_ALTA ? "Hum Alta" : "CO2 Alto"));
                } else if (ventilacionProgramadaActiva) {
                    Serial.print("Ventilacion Programada");
                }
                Serial.println();
            } else if (necesitaVentilarPorCO2Bajo) {
                digitalWrite(RELAY1, HIGH); // Apaga ventilador
            }
            break;

        case VENTILADOR_ENCENDIDO_3S:
            if (currentMillis - temporizadorActivoVentilador >= 3000) {
                digitalWrite(RELAY1, HIGH); // Apaga ventilador
                estadoVentiladorActual = VENTILADOR_APAGADO_5S;
                temporizadorActivoVentilador = currentMillis;
                Serial.println("Ventilador OFF (3s completados)");
            }
            break;

        case VENTILADOR_APAGADO_5S:
            if (currentMillis - temporizadorActivoVentilador >= 5000) {
                if (necesitaVentilarPorSensores) { // Si la condición persiste, vuelve a encender
                    digitalWrite(RELAY1, LOW); // Enciende ventilador
                    estadoVentiladorActual = VENTILADOR_ENCENDIDO_5S;
                    temporizadorActivoVentilador = currentMillis;
                    Serial.print("Ventilador ON de nuevo por "); Serial.println(razonActivacionVentilador == TEMPERATURA_ALTA ? "Temp Alta" : (razonActivacionVentilador == HUMEDAD_ALTA ? "Hum Alta" : "CO2 Alto"));
                } else if (ventilacionProgramadaActiva) { // Si la ventilación programada sigue activa
                    digitalWrite(RELAY1, LOW); // Enciende ventilador
                    estadoVentiladorActual = VENTILADOR_ENCENDIDO_5S;
                    temporizadorActivoVentilador = currentMillis;
                    Serial.println("Ventilador ON de nuevo por Ventilacion Programada");
                } else { // Si la condición ya no está activa
                    estadoVentiladorActual = VENTILADOR_INACTIVO;
                    razonActivacionVentilador = NINGUNA;
                    Serial.println("Ventilador en INACTIVO (condicion ya no activa)");
                }
            }
            break;

        case VENTILADOR_ENCENDIDO_5S:
            if (currentMillis - temporizadorActivoVentilador >= 5000) {
                digitalWrite(RELAY1, HIGH); // Apaga ventilador
                estadoVentiladorActual = VENTILADOR_ESPERA_ESTABILIZACION;
                temporizadorActivoVentilador = currentMillis;
                Serial.println("Ventilador OFF (5s completados)");
            }
            break;

        case VENTILADOR_ESPERA_ESTABILIZACION:
            if (currentMillis - temporizadorActivoVentilador >= 15000) { // Esperar 15 segundos
                if (ventilacionProgramadaActiva) {
                    if (currentMillis - tiempoInicioVentilacionProgramada >= DURACION_VENTILACION_PROGRAMADA) {
                        ventilacionProgramadaActiva = false;
                        ultimoTiempoVentilacionProgramada = currentMillis; // Reinicia el temporizador para la próxima ventilación programada
                        Serial.println("Ventilacion programada de CO2 finalizada.");
                        estadoVentilacionProgramada = VENTILACION_PROGRAMADA_INACTIVA;
                        estadoVentiladorActual = VENTILADOR_INACTIVO; // Vuelve al estado inactivo
                    } else {
                        // Si la ventilación programada aún no termina, vuelve a encender el ventilador
                        digitalWrite(RELAY1, LOW); // Enciende ventilador
                        estadoVentiladorActual = VENTILADOR_ENCENDIDO_3S;
                        temporizadorActivoVentilador = currentMillis;
                        Serial.println("Ventilador ON de nuevo por Ventilacion Programada.");
                    }
                } else { // Si no es ventilación programada, es por sensores
                    if (necesitaVentilarPorSensores) { // Si la condición persiste, vuelve a encender el ventilador
                        digitalWrite(RELAY1, LOW); // Enciende ventilador
                        estadoVentiladorActual = VENTILADOR_ENCENDIDO_3S;
                        temporizadorActivoVentilador = millis();
                        Serial.print("Ventilador ON de nuevo por "); Serial.println(razonActivacionVentilador == TEMPERATURA_ALTA ? "Temp Alta" : (razonActivacionVentilador == HUMEDAD_ALTA ? "Hum Alta" : "CO2 Alto"));
                    } else { // Si la condición ya no está activa (por si hubo un cambio justo antes de este estado)
                        estadoVentiladorActual = VENTILADOR_INACTIVO;
                        razonActivacionVentilador = NINGUNA;
                        Serial.println("Ventilador en INACTIVO (condicion ya no activa)");
                    }
                }
            }
            break;
    }
}

void evaluarAlarmasCriticas() { //
    // Temperatura Mínima Crítica
    if (currentTemp < alarmaTempMin - 5.0 && !notificacionCriticaTempMinActiva) {
        tiempoInicioAlarmaTempMin = millis();
        notificacionCriticaTempMinActiva = true;
        Serial.println("ALARMA CRITICA: Temperatura MUY BAJA!");
        estadoActualApp = ESTADO_ALARMA_CRITICA_AVISO;
        necesitaRefrescarLCD = true;
        alarmaDescartada = false;
    } else if (currentTemp >= alarmaTempMin - 5.0 && notificacionCriticaTempMinActiva) {
        notificacionCriticaTempMinActiva = false;
        Serial.println("Alarma Critica Temp Min resuelta.");
    }

    // Temperatura Máxima Crítica
    if (currentTemp > alarmaTempMax + 5.0 && !notificacionCriticaTempMaxActiva) {
        tiempoInicioAlarmaTempMax = millis();
        notificacionCriticaTempMaxActiva = true;
        Serial.println("ALARMA CRITICA: Temperatura MUY ALTA!");
        estadoActualApp = ESTADO_ALARMA_CRITICA_AVISO;
        necesitaRefrescarLCD = true;
        alarmaDescartada = false;
    } else if (currentTemp <= alarmaTempMax + 5.0 && notificacionCriticaTempMaxActiva) {
        notificacionCriticaTempMaxActiva = false;
        Serial.println("Alarma Critica Temp Max resuelta.");
    }

    // Humedad Mínima Crítica
    if (currentHum < alarmaHumMin - 10.0 && !notificacionCriticaHumMinActiva) {
        tiempoInicioAlarmaHumMin = millis();
        notificacionCriticaHumMinActiva = true;
        Serial.println("ALARMA CRITICA: Humedad MUY BAJA!");
        estadoActualApp = ESTADO_ALARMA_CRITICA_AVISO;
        necesitaRefrescarLCD = true;
        alarmaDescartada = false;
    } else if (currentHum >= alarmaHumMin - 10.0 && notificacionCriticaHumMinActiva) {
        notificacionCriticaHumMinActiva = false;
        Serial.println("Alarma Critica Hum Min resuelta.");
    }

    // Humedad Máxima Crítica
    if (currentHum > alarmaHumMax + 10.0 && !notificacionCriticaHumMaxActiva) {
        tiempoInicioAlarmaHumMax = millis();
        notificacionCriticaHumMaxActiva = true;
        Serial.println("ALARMA CRITICA: Humedad MUY ALTA!");
        estadoActualApp = ESTADO_ALARMA_CRITICA_AVISO;
        necesitaRefrescarLCD = true;
        alarmaDescartada = false;
    } else if (currentHum <= alarmaHumMax + 10.0 && notificacionCriticaHumMaxActiva) {
        notificacionCriticaHumMaxActiva = false;
        Serial.println("Alarma Critica Hum Max resuelta.");
    }

    // CO2 Mínimo Crítico
    if (currentCO2 < alarmaCO2Min - 200 && !notificacionCriticaCO2MinActiva) {
        tiempoInicioAlarmaCO2Min = millis();
        notificacionCriticaCO2MinActiva = true;
        Serial.println("ALARMA CRITICA: CO2 MUY BAJO!");
        estadoActualApp = ESTADO_ALARMA_CRITICA_AVISO;
        necesitaRefrescarLCD = true;
        alarmaDescartada = false;
    } else if (currentCO2 >= alarmaCO2Min - 200 && notificacionCriticaCO2MinActiva) {
        notificacionCriticaCO2MinActiva = false;
        Serial.println("Alarma Critica CO2 Min resuelta.");
    }

    // CO2 Máximo Crítico
    if (currentCO2 > alarmaCO2Max + 200 && !notificacionCriticaCO2MaxActiva) {
        tiempoInicioAlarmaCO2Max = millis();
        notificacionCriticaCO2MaxActiva = true;
        Serial.println("ALARMA CRITICA: CO2 MUY ALTO!");
        estadoActualApp = ESTADO_ALARMA_CRITICA_AVISO;
        necesitaRefrescarLCD = true;
        alarmaDescartada = false;
    } else if (currentCO2 <= alarmaCO2Max + 200 && notificacionCriticaCO2MaxActiva) {
        notificacionCriticaCO2MaxActiva = false;
        Serial.println("Alarma Critica CO2 Max resuelta.");
    }

    // Si no hay ninguna alarma crítica activa, y el estado actual es AVISO, vuelve al modo de funcionamiento
    if (!notificacionCriticaTempMinActiva && !notificacionCriticaTempMaxActiva &&
        !notificacionCriticaHumMinActiva && !notificacionCriticaHumMaxActiva &&
        !notificacionCriticaCO2MinActiva && !notificacionCriticaCO2MaxActiva &&
        estadoActualApp == ESTADO_ALARMA_CRITICA_AVISO) {
        
        if (alarmaDescartada){ //Solo vuelve al modo funcionamiento si el usuario descarto la alarma
            estadoActualApp = ESTADO_MODO_FUNCIONAMIENTO;
            necesitaRefrescarLCD = true; // Forzar refresco para mostrar el modo funcionamiento
        }
    }
}

void manejarMostrarAlarmas(int deltaEncoder, bool pulsadoSwitch) {
  if (necesitaRefrescarLCD) {
    lcd.clear();
    lcd.home();
    lcd.print("Mostrando Alarmas"); // Título
    // Aquí podrías añadir lógica para mostrar qué alarmas están activas
    // Por ejemplo, iterar sobre las banderas de alarma y mostrarlas.
    // lcd.print("Temp Alta: " + String(banderaAlarmaTempMaxActiva), 0, 1);
    lcd.setCursor(0,3);
    lcd.print("Presione SW para volver");
    necesitaRefrescarLCD = false;
  }

  // Si el switch fue presionado, vuelve al submenu de alarmas
  if (pulsadoSwitch) {
    estadoActualApp = ESTADO_ALARMAS_SUBMENU;
    necesitaRefrescarLCD = true;
  }
}

void manejarAlarmaCriticaAviso(int deltaEncoder, bool pulsadoSwitch) {
  if (necesitaRefrescarLCD) {
    lcd.clear();
    lcd.home();
    lcd.print("ALARMA CRITICA!!!"); // Título de la alarma
    int lineCount = 1;

    // --- (Tu lógica para mostrar los mensajes de alarma aquí) ---
    // Asegúrate de que los mensajes se muestran solo si las notificaciones están activas
    if (notificacionCriticaTempMinActiva && lineCount < 4) {
      lcd.setCursor(0, lineCount++);
      lcd.print("Temp MINIMA BAJA!");
    }
    if (notificacionCriticaTempMaxActiva && lineCount < 4) {
      lcd.setCursor(0, lineCount++);
      lcd.print("Temp MAXIMA ALTA!");
    }
    if (notificacionCriticaHumMinActiva && lineCount < 4) {
      lcd.setCursor(0, lineCount++);
      lcd.print("Hum MINIMA BAJA!");
    }
    if (notificacionCriticaHumMaxActiva && lineCount < 4) {
      lcd.setCursor(0, lineCount++);
      lcd.print("Hum MAXIMA ALTA!");
    }
    if (notificacionCriticaCO2MinActiva && lineCount < 4) {
      lcd.setCursor(0, lineCount++);
      lcd.print("CO2 MINIMO BAJO!");
    }
    if (notificacionCriticaCO2MaxActiva && lineCount < 4) {
      lcd.setCursor(0, lineCount++);
      lcd.print("CO2 MAXIMO ALTO!");
    }

    if (lineCount < 4) {
        lcd.setCursor(0, lineCount);
        lcd.print("SW para descartar");
    }

    necesitaRefrescarLCD = false;
  }

  // --- PUNTO CLAVE: Lógica de descarte de la alarma al presionar SW ---
  if (pulsadoSwitch) {
    // Resetea todas las banderas de notificación activa
    notificacionCriticaTempMinActiva = false;
    notificacionCriticaTempMaxActiva = false;
    notificacionCriticaHumMinActiva = false;
    notificacionCriticaHumMaxActiva = false;
    notificacionCriticaCO2MinActiva = false;
    notificacionCriticaCO2MaxActiva = false;
    
    alarmaDescartada = true; // MUY IMPORTANTE: marca que la alarma ha sido "vista" y descartada
    necesitaRefrescarLCD = true; // Para asegurar un refresco al salir del estado
    Serial.println("SW presionado en Alarma Critica. Alarmas descartadas."); // Debug

    // Cambia el estado actual directamente aquí al estado válido anterior
    estadoActualApp = lastValidState; // Esto fuerza la salida del estado de alarma crítica
  }
}

// ---------------- LOOP PRINCIPAL ----------------
void loop() {
  // Manejo del encoder para evitar lecturas duplicadas en el mismo ciclo
  static unsigned long lastEncoderCheckTime = 0;
  if (millis() - lastEncoderCheckTime > RETARDO_ANTI_REBOTE_ENCODER) {
    if (digitalRead(CLOCK_ENCODER) != estadoClkAnterior || digitalRead(DT_ENCODER) != estadoDt) {
      // Forzar una nueva lectura ISR para actualizar valorEncoder
      leerEncoderISR();
    }
    lastEncoderCheckTime = millis();
  }
  // Manejo del switch para evitar lecturas duplicadas
  static unsigned long lastSwitchCheckTime = 0;
  if (millis() - lastSwitchCheckTime > RETARDO_ANTI_REBOTE_SW) {
    if (digitalRead(SW_ENCODER) == LOW) { // Si el botón está presionado
      leerSwitchISR(); // Esto pondrá swFuePresionadoISR en true
    }
    lastSwitchCheckTime = millis();
  }
 // noInterrupts();
  int deltaEncoder = valorEncoder; // Renombrado de deltaEncoderActual
  valorEncoder = 0;
  bool pulsadoSwitch = swFuePresionadoISR; // Renombrado de switchPulsadoActual
  if (swFuePresionadoISR) {
    pulsadoSwitch = true;
    swFuePresionadoISR = false; // Reset after reading
  }
  unsigned long currentMillis = millis();
  //interrupts();

  // --- Lectura de sensores (mantener siempre) ---
  if (currentMillis - lastSensorReadTime >= INTERVALO_LECTURA_SENSORES) { // Uso de INTERVALO_LECTURA_SENSORES
    leerSensores();
    lastSensorReadTime = currentMillis; //
    // Esto es importante para que el display de modo de funcionamiento se actualice
    // solo cuando los datos se han leído y enviado.
    if (estadoActualApp == ESTADO_MODO_FUNCIONAMIENTO) {
      necesitaRefrescarLCD = true;
    }
  }

  leerSensores();
  manejarVentilador(currentMillis);
  evaluarAlarmasCriticas();
  

   // --- Lógica para entrar en el estado de alarma crítica ---
  // Se entra a este estado si hay alarmas críticas activas Y NO han sido descartadas por el usuario.
  // La bandera 'alarmaDescartada' permite al usuario temporalmente "silenciar" el aviso visual.
  if (hayAlarmasCriticas()) {
    if (!alarmaDescartada) { // Solo entra si NO está descartada
      if (estadoActualApp != ESTADO_ALARMA_CRITICA_AVISO) {
        lastValidState = estadoActualApp; // Guardar el estado actual antes de ir a la alarma
        estadoActualApp = ESTADO_ALARMA_CRITICA_AVISO;
        necesitaRefrescarLCD = true;
        Serial.println("Entrando en estado de alarma crítica.");
      }
    }
  } else { // Si NO hay alarmas críticas activas
    // Si la bandera estaba en true, significa que el usuario la había descartado.
    // Como ya no hay alarmas, reseteamos la bandera para futuras notificaciones.
    if (alarmaDescartada) {
      alarmaDescartada = false;
      Serial.println("Alarmas críticas despejadas, reseteando alarmaDescartada.");
    }
  }
    EstadoApp proximoEstado = estadoActualApp;  



  switch (estadoActualApp) {
    case ESTADO_MENU_PRINCIPAL:
      if (deltaEncoder != 0) {
        indiceMenuPrincipal += deltaEncoder;
        if (indiceMenuPrincipal < 0) indiceMenuPrincipal = TOTAL_OPCIONES_MENU_PRINCIPAL - 1;
        if (indiceMenuPrincipal >= TOTAL_OPCIONES_MENU_PRINCIPAL) indiceMenuPrincipal = 0;
        necesitaRefrescarLCD = true;
      }
      if (pulsadoSwitch) {
        switch (indiceMenuPrincipal) {
          case 0: proximoEstado = ESTADO_EDITAR_SETPOINTS; break;
          case 1: proximoEstado = ESTADO_ALARMAS_SUBMENU; break;
          case 2: proximoEstado = ESTADO_CONFIRMACION_MODO_FUNCIONAMIENTO; indiceConfirmacion = 0; break;
        }
        necesitaRefrescarLCD = true;
      }
      break;

    case ESTADO_EDITAR_SETPOINTS:
      if (deltaEncoder != 0) {
        indiceSubMenuSetpoints += deltaEncoder;
        if (indiceSubMenuSetpoints < 0) indiceSubMenuSetpoints = TOTAL_OPCIONES_SUBMENU_SETPOINTS - 1;
        if (indiceSubMenuSetpoints >= TOTAL_OPCIONES_SUBMENU_SETPOINTS) indiceSubMenuSetpoints = 0;
        necesitaRefrescarLCD = true;
      }
      if (pulsadoSwitch) {
        switch (indiceSubMenuSetpoints) {
          case 0: proximoEstado = ESTADO_EDITAR_TEMP; break;
          case 1: proximoEstado = ESTADO_EDITAR_HUM; break;
          case 2: proximoEstado = ESTADO_EDITAR_CO2; break;
          case 3: guardarSetpointsEEPROM(); proximoEstado = ESTADO_MENU_PRINCIPAL; break;
        }
        necesitaRefrescarLCD = true;
      }
      break;

    case ESTADO_ALARMAS_SUBMENU:
      if (deltaEncoder != 0) {
        indiceSubMenuAlarmas += deltaEncoder;
        if (indiceSubMenuAlarmas < 0) indiceSubMenuAlarmas = TOTAL_OPCIONES_SUBMENU_ALARMAS - 1;
        if (indiceSubMenuAlarmas >= TOTAL_OPCIONES_SUBMENU_ALARMAS) indiceSubMenuAlarmas = 0;
        necesitaRefrescarLCD = true;
      }
      if (pulsadoSwitch) {
        switch (indiceSubMenuAlarmas) {
          case 0: proximoEstado = ESTADO_MOSTRAR_ALARMAS; break;
          case 1: proximoEstado = ESTADO_EDITAR_ALARMAS; break;
          case 2: proximoEstado = ESTADO_MENU_PRINCIPAL; break;
        }
        necesitaRefrescarLCD = true;
      }
      break;

    case ESTADO_MOSTRAR_ALARMAS:
      manejarMostrarAlarmas(deltaEncoder, pulsadoSwitch);
      if (pulsadoSwitch) { // Lógica para volver si se presiona SW
        proximoEstado = ESTADO_ALARMAS_SUBMENU;
      }
      break;

    case ESTADO_EDITAR_ALARMAS:
      if (deltaEncoder != 0) {
        indiceEditarAlarmas += deltaEncoder;
        if (indiceEditarAlarmas < 0) indiceEditarAlarmas = TOTAL_OPCIONES_EDITAR_ALARMAS - 1;
        if (indiceEditarAlarmas >= TOTAL_OPCIONES_EDITAR_ALARMAS) indiceEditarAlarmas = 0;
        necesitaRefrescarLCD = true;
      }
      if (pulsadoSwitch) {
        switch (indiceEditarAlarmas) {
          case 0: proximoEstado = ESTADO_EDITAR_ALARMA_TEMP_MIN; break;
          case 1: proximoEstado = ESTADO_EDITAR_ALARMA_TEMP_MAX; break;
          case 2: proximoEstado = ESTADO_EDITAR_ALARMA_HUM_MIN; break;
          case 3: proximoEstado = ESTADO_EDITAR_ALARMA_HUM_MAX; break;
          case 4: proximoEstado = ESTADO_EDITAR_ALARMA_CO2_MIN; break;
          case 5: proximoEstado = ESTADO_EDITAR_ALARMA_CO2_MAX; break;
          case 6: guardarSetpointsEEPROM(); proximoEstado = ESTADO_ALARMAS_SUBMENU; break;
        }
        necesitaRefrescarLCD = true;
      }
      break;

    case ESTADO_CONFIRMACION_MODO_FUNCIONAMIENTO:
      manejarConfirmacionModoFuncionamiento(deltaEncoder, pulsadoSwitch);
      if (pulsadoSwitch) {
        if (indiceConfirmacion == 0) { // SI
          proximoEstado = ESTADO_MODO_FUNCIONAMIENTO;
          guardarEstadoAppEEPROM(); // Guardar el estado al entrar en modo funcionamiento
        } else { // NO
          proximoEstado = ESTADO_MENU_PRINCIPAL;
        }
        necesitaRefrescarLCD = true;
      }
      break;

    case ESTADO_MODO_FUNCIONAMIENTO:
      manejarModoFuncionamiento(deltaEncoder, pulsadoSwitch); // El pulsadoSwitch se maneja internamente para la confirmación
      break;

    case ESTADO_CONFIRMACION_VOLVER_MENU:
      manejarConfirmacionVolverMenu(deltaEncoder, pulsadoSwitch);
      if (pulsadoSwitch) {
        if (indiceConfirmacion == 0) { // SI
          proximoEstado = ESTADO_MENU_PRINCIPAL;
          guardarEstadoAppEEPROM(); // Guardar el estado al salir del modo funcionamiento
        } else { // NO
          proximoEstado = ESTADO_MODO_FUNCIONAMIENTO; // Volver a modo funcionamiento
        }
        necesitaRefrescarLCD = true;
        initialMessageDisplayed = false; // Resetear para la próxima vez
      }
      break;

    case ESTADO_EDITAR_TEMP:
      manejarEditarTemperatura(deltaEncoder, pulsadoSwitch);
      if (pulsadoSwitch) {
        guardarSetpointsEEPROM();
        proximoEstado = ESTADO_EDITAR_SETPOINTS;
        necesitaRefrescarLCD = true;
      }
      break;

    case ESTADO_EDITAR_HUM:
      manejarEditarHumedad(deltaEncoder, pulsadoSwitch);
      if (pulsadoSwitch) {
        guardarSetpointsEEPROM();
        proximoEstado = ESTADO_EDITAR_SETPOINTS;
        necesitaRefrescarLCD = true;
      }
      break;

    case ESTADO_EDITAR_CO2:
      manejarEditarCO2(deltaEncoder, pulsadoSwitch);
      if (pulsadoSwitch) {
        guardarSetpointsEEPROM();
        proximoEstado = ESTADO_EDITAR_SETPOINTS;
        necesitaRefrescarLCD = true;
      }
      break;

    case ESTADO_EDITAR_ALARMA_TEMP_MIN:
      manejarEditarAlarmaTempMin(deltaEncoder, pulsadoSwitch);
      if (pulsadoSwitch) {
        guardarSetpointsEEPROM();
        proximoEstado = ESTADO_EDITAR_ALARMAS;
        necesitaRefrescarLCD = true;
      }
      break;

    case ESTADO_EDITAR_ALARMA_TEMP_MAX:
      manejarEditarAlarmaTempMax(deltaEncoder, pulsadoSwitch);
      if (pulsadoSwitch) {
        guardarSetpointsEEPROM();
        proximoEstado = ESTADO_EDITAR_ALARMAS;
        necesitaRefrescarLCD = true;
      }
      break;

    case ESTADO_EDITAR_ALARMA_HUM_MIN:
      manejarEditarAlarmaHumMin(deltaEncoder, pulsadoSwitch);
      if (pulsadoSwitch) {
        guardarSetpointsEEPROM();
        proximoEstado = ESTADO_EDITAR_ALARMAS;
        necesitaRefrescarLCD = true;
      }
      break;

    case ESTADO_EDITAR_ALARMA_HUM_MAX:
      manejarEditarAlarmaHumMax(deltaEncoder, pulsadoSwitch);
      if (pulsadoSwitch) {
        guardarSetpointsEEPROM();
        proximoEstado = ESTADO_EDITAR_ALARMAS;
        necesitaRefrescarLCD = true;
      }
      break;

    case ESTADO_EDITAR_ALARMA_CO2_MIN:
      manejarEditarAlarmaCO2Min(deltaEncoder, pulsadoSwitch);
      if (pulsadoSwitch) {
        guardarSetpointsEEPROM();
        proximoEstado = ESTADO_EDITAR_ALARMAS;
        necesitaRefrescarLCD = true;
      }
      break;

    case ESTADO_EDITAR_ALARMA_CO2_MAX:
      manejarEditarAlarmaCO2Max(deltaEncoder, pulsadoSwitch);
      if (pulsadoSwitch) {
        guardarSetpointsEEPROM();
        proximoEstado = ESTADO_EDITAR_ALARMAS;
        necesitaRefrescarLCD = true;
      }
      break;
    case ESTADO_ALARMA_CRITICA_AVISO:
      manejarAlarmaCriticaAviso(deltaEncoder, pulsadoSwitch);
      if (pulsadoSwitch) {
        alarmaDescartada = true; // El usuario presiona para descartar la notificación visual
        proximoEstado = lastValidState; // Sale de la pantalla de alarma
        necesitaRefrescarLCD = true;
        Serial.println("Alarma crítica descartada y saliendo del aviso.");
      }
      break;
  }

  // Transición de estados
  if (proximoEstado != estadoActualApp) {
    estadoActualApp = proximoEstado;
    necesitaRefrescarLCD = true; // Asegurar que la pantalla se refresque al cambiar de estado
    Serial.print("Cambiando a estado: ");
    Serial.println(estadoActualApp);
  }
}
