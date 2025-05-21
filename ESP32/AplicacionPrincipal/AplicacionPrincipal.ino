#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <NoDelay.h>
#include "time.h"
#include <Bounce2.h>

// Conexion a internet
const char *SSID = "ssid";
const char *PWD = "pass";

const unsigned int BAUD_RATE = 115200;

noDelay pausa(0);
noDelay pausaLecturas(100);
noDelay pausaReloj(1000);
Bounce debouncer = Bounce();
AsyncWebServer servidor(80);
int contador;
// Numero de valores de salida del ADC. 2**12 = 4096
const unsigned ADC_VALORES = 4096;

// Declaración de pines
const unsigned int PIN_LED_LAMPARA = 14;
const unsigned int PIN_LED_ILUMINACION = 2;
const unsigned int PIN_LED_VERDE = 2;
const unsigned int PIN_LED_AMARILLO = 2;
const unsigned int PIN_LED_ROJO = 2;
const unsigned int PIN_LED_A = 2;
const unsigned int PIN_BOTON = 2;
const unsigned int PIN_TOUCH = 4;
const unsigned int PIN_BOMBA = 4;
const unsigned int PIN_POT = 4;
const unsigned int PIN_FOT = 34;

// Configuración para comunicación con api de telegram
#define BOTtoken "token"
#define CHAT_ID "id"
WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

// Estados en los que puede estar el terrario
typedef enum {
  CONDICIONES_ADECUADAS,
  HUMEDAD_BAJA,
  HUMEDAD_ALTA,
  TEMPERATURA_INADECUADA,
  ILUMINACION_ALTA,
  ILUMINACION_BAJA,
  MASCOTA_ESCAPADA
} Estado;

Estado estado;

// Objeto para manejar documentos JSON
StaticJsonDocument<250> jsonDocument;
char buffer[250];

// URL de un servidor NTP (Network Time Protocol)
const char *ntpServer = "pool.ntp.org";
// Offset en segundos de la zona horaria local con respecto a GMT
const long gmtOffset_sec = -25200;  // -7*3600
// Offset en segundos del tiempo, en el caso de horario de verano
const int daylightOffset_sec = 0;
// Estructura con la informacion de la fecha/hora actual
struct tm timeinfo;

// Parametros de la señal PWM
const int FRECUENCIA = 5000;
const int RESOLUCION = 8;
// Valor minimo del ciclo de trabajo
const unsigned int CT_MIN = 0;
// Valor maximo del ciclo de trabajo. 2**RESOLUCION - 1
const unsigned int CT_MAX = 255;

// Variables para lecturas de parametros y limites
float temperatura;
float humedad;
float iluminacion;
int intensidadLuz;
int cicloTrabajo = 0;
int muestraTouch;
const int touchTreshold = 30;
float temperaturaMinima;
float temperaturaMaxima;
float humedadMinima;
float humedadMaxima;
float iluminacionMinima;
float iluminacionMaxima;

// Definicion de métodos
// Setup inicial
void setupRutas();
void iniciarConexionWifi();
// Manejo de documentos json
void crearJson(char *tag, float value, char *unit);
void anhadirObjetoJson(char *tag, float value, char *unit);
// Cambios de estado
void estadoOptimo();
void alertarHumedadAlta();
void alertarHumedadBaja();
void alertarTemperatura();
void alertarIluminacionAlta();
void alertarIluminacionBaja();
void alertarEscape();
void leerDatos();
// Manejo de peticiones
void actualizaIntensidadLuz();
void realizarLecturas();

void setup() {
  Serial.begin(BAUD_RATE);
  estadoOptimo();
  iniciarConexionWifi();

  // iniciarPines

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  ledcAttach(PIN_LED_LAMPARA, FRECUENCIA, RESOLUCION);
  debouncer.attach(PIN_BOTON);
  debouncer.interval(5);

  bot.sendMessage(CHAT_ID, "Monitoreo iniciado", "");
}

void loop() {
  // Lectura de botón para indicar que se devolvió la mascota
  debouncer.update();
  int valor = debouncer.read();

  if (pausaLecturas.update()) {
    void realizarLecturas();
  }

  if (pausaReloj.update()) {
    if (!getLocalTime(&timeinfo)) {
      Serial.println("No se pudo obtener la fecha/hora");
    }
  }

  // Simulacion de cambio de luz con un potenciometro
  ledcWrite(PIN_LED_LAMPARA, cicloTrabajo);
  ledcWrite(PIN_LED_ILUMINACION, intensidadLuz);

  switch (estado) {
    case CONDICIONES_ADECUADAS:
      if (humedad < humedadMinima) alertarHumedadBaja();
      if (humedad > humedadMaxima) alertarHumedadBaja();
      if (iluminacion < iluminacionMinima) alertarIluminacionBaja();
      if (iluminacion > iluminacionMaxima) alertarIluminacionAlta();
      if (temperatura < temperaturaMinima || temperatura > temperaturaMaxima) alertarTemperatura();
      if (muestraTouch >= touchTreshold) alertarEscape();
      break;
    case HUMEDAD_BAJA:
      // Despues de un periodo de tiempo de riego
      if (contador == 10) {
        estadoOptimo();
      }
      if (pausa.update()) {
        togglePIN(PIN_BOMBA);
        contador++;
      }
      break;
    case HUMEDAD_ALTA:
      if (humedad <= humedadMaxima) estadoOptimo();
      break;
    case TEMPERATURA_INADECUADA:
      if (temperatura >= temperaturaMinima && temperatura <= temperaturaMaxima) {
        estadoOptimo();
      }
      break;
    case ILUMINACION_ALTA:
      if (iluminacion <= iluminacionMaxima) estadoOptimo();
      if (cicloTrabajo >= CT_MIN) cicloTrabajo--;
      break;
    case ILUMINACION_BAJA:
      if (iluminacion >= iluminacionMinima) estadoOptimo();
      if (cicloTrabajo <= CT_MAX) cicloTrabajo++;
      break;
    case MASCOTA_ESCAPADA:
      if (valor == HIGH) estadoOptimo();
      break;
  }
}

// Intenta conectar a wifi e inicia el cliente seguro con certificado para llamar la api de telegram
void iniciarConexionWifi() {
  Serial.print("Conectando a Wifi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PWD);
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print(WiFi.localIP());
}

void setupRutas() {
  //servidor.on("/datos", getData);
  //servidor.on("/mensaje", HTTP_POST, handlePost);

  servidor.begin();
}

// Crea un objeto json a partir de un par llave-valor
void crearJson(char *tag, float value, char *unit) {
  jsonDocument.clear();
  jsonDocument["type"] = tag;
  jsonDocument["value"] = value;
  jsonDocument["unit"] = unit;
  serializeJson(jsonDocument, buffer);
}

// Añade un par llave-valor a un objeto json existente
void anhadirObjetoJson(char *tag, float value, char *unit) {
  JsonObject obj = jsonDocument.createNestedObject();
  obj["type"] = tag;
  obj["value"] = value;
  obj["unit"] = unit;
}

void realizarLecturas() {
  // Lectura de sensor touch que indica que escapó la mascota
  muestraTouch = touchRead(PIN_TOUCH);
  actualizaIntensidadLuz();
  iluminacion = analogRead(PIN_FOT);
  Serial.print("Iluminacion");
  Serial.print(iluminacion);
}
void actualizaIntensidadLuz() {
  // Lee y digitaliza el valor del voltaje en el potenciometro
  int muestra = analogRead(PIN_POT);
  intensidadLuz = map(muestra, 0, ADC_VALORES - 1, CT_MIN, CT_MAX);
}

void estadoOptimo() {
  digitalWrite(PIN_LED_VERDE, HIGH);
  digitalWrite(PIN_LED_AMARILLO, LOW);
  digitalWrite(PIN_LED_ROJO, LOW);
}

void alertarHumedadAlta() {
  digitalWrite(PIN_LED_VERDE, LOW);
  digitalWrite(PIN_LED_AMARILLO, HIGH);
  digitalWrite(PIN_LED_ROJO, LOW);
}

void alertarHumedadBaja() {
  // Inicia temporizador para despliegue de agua
  pausa.setdelay(500);
  pausa.start();
  contador = 0;
  digitalWrite(PIN_LED_VERDE, LOW);
  digitalWrite(PIN_LED_AMARILLO, HIGH);
  digitalWrite(PIN_LED_ROJO, LOW);
}

void alertarTemperatura() {
  digitalWrite(PIN_LED_VERDE, LOW);
  digitalWrite(PIN_LED_AMARILLO, HIGH);
  digitalWrite(PIN_LED_ROJO, LOW);
}

void alertarIluminacionAlta() {
  digitalWrite(PIN_LED_VERDE, LOW);
  digitalWrite(PIN_LED_AMARILLO, HIGH);
  digitalWrite(PIN_LED_ROJO, LOW);
}

void alertarIluminacionBaja() {
  digitalWrite(PIN_LED_VERDE, LOW);
  digitalWrite(PIN_LED_AMARILLO, HIGH);
  digitalWrite(PIN_LED_ROJO, LOW);
}

void alertarEscape() {
  digitalWrite(PIN_LED_VERDE, LOW);
  digitalWrite(PIN_LED_AMARILLO, LOW);
  digitalWrite(PIN_LED_ROJO, HIGH);
  bot.sendMessage(CHAT_ID, "Su mascota ha escapado del contenedor!!!", "");
}

void togglePIN(unsigned int pin) {
  digitalWrite(pin, !digitalRead(pin));
}