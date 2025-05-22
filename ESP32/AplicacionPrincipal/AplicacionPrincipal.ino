#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "AsyncJson.h"
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <NoDelay.h>
#include "time.h"
#include <Bounce2.h>
#include <LittleFS.h>
#include "DHT.h"

// Conexion a internet
const char *SSID = "INFINITUM89B1";
const char *PWD = "FE7AKvWFFz";

const unsigned int BAUD_RATE = 115200;

noDelay pausa(0);
noDelay pausaLecturas(1500);
noDelay pausaReloj(1000);
Bounce debouncer = Bounce();
AsyncWebServer servidor(80);
int contador;
// Numero de valores de salida del ADC
const unsigned ADC_VALORES = 4096;

// Declaración de pines
const unsigned int PIN_LED_LAMPARA = 27;
const unsigned int PIN_LED_ILUMINACION = 26;
const unsigned int PIN_LED_VERDE = 2;
const unsigned int PIN_LED_AMARILLO = 4;
const unsigned int PIN_LED_ROJO = 16;
const unsigned int PIN_BOTON = 17;
const unsigned int PIN_TOUCH = 32;
const unsigned int PIN_BOMBA = 21;
const unsigned int PIN_POT = 33;
const unsigned int PIN_FOT = 34;
const unsigned int PIN_DHT = 22;

// Configuración para comunicación con api de telegram
#define BOTtoken "token"
#define CHAT_ID "id"
#define HOST_TCP "192.168.1.71"  
#define PORT_TCP 5000
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

// Configuración NTP
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -25200;
const int daylightOffset_sec = 0;

struct tm timeinfo;

// Configuración DHT
#define DHTPIN PIN_DHT
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Parametros de la señal PWM
const int FRECUENCIA = 5000;
const int RESOLUCION = 8;
const unsigned int CT_MIN = 0;
const unsigned int CT_MAX = 255;

// Variables para lecturas de parametros y limites
float temperatura;
float humedad;
int iluminacion;
int intensidadLuz;
int cicloTrabajo = 0;
int muestraTouch;
const int touchTreshold = 12;
float temperaturaMinima = 17;
float temperaturaMaxima = 29;
float humedadMinima = 50;
float humedadMaxima = 80;
float iluminacionMinima = 200;
float iluminacionMaxima = 1100;
bool yaNotificoEscape = false;    // Para evitar que mande el mensaje de Telegram muchas veces
int conteoEscape = 0;             // Para contar toques válidos
const int maxIntentosEscape = 3;  // Cuántas veces debe detectar toque para activarse

// Definicion de métodos
// Setup inicial
void setupRutas();
void iniciarConexionWifi();
void inicializaLittleFS();
void enviarDatosPorTCP();
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
// Leecturas
void actualizaIntensidadLuz();
void realizarLecturas();
String obtenFecha();
noDelay pausaEnvioTCP(10000);
void setup() {
  Serial.begin(BAUD_RATE);
  iniciarConexionWifi();
  setupRutas();
  inicializaLittleFS();
  dht.begin();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  ledcAttachChannel(PIN_LED_LAMPARA, FRECUENCIA, RESOLUCION, 0);
  ledcAttachChannel(PIN_LED_ILUMINACION, FRECUENCIA, RESOLUCION, 1);

  pinMode(PIN_LED_VERDE, OUTPUT);
  pinMode(PIN_LED_AMARILLO, OUTPUT);
  pinMode(PIN_LED_ROJO, OUTPUT);
  pinMode(PIN_BOMBA, OUTPUT);
  debouncer.attach(PIN_BOTON);
  debouncer.interval(15);

  estadoOptimo();
  bot.sendMessage(CHAT_ID, "Monitoreo iniciado", "");
}

void loop() {
  debouncer.update();
  int valor = debouncer.read();

  if (pausaLecturas.update()) {
    realizarLecturas();
  }

  if (pausaReloj.update()) {
    if (!getLocalTime(&timeinfo)) {
      Serial.println("No se pudo obtener la fecha/hora");
    }
  }

   if (pausaEnvioTCP.update()) {
    enviarDatosPorTCP();
  }

  ledcWrite(PIN_LED_LAMPARA, cicloTrabajo);
  ledcWrite(PIN_LED_ILUMINACION, intensidadLuz);
  switch (estado) {
    case CONDICIONES_ADECUADAS:
      if (humedad < humedadMinima) alertarHumedadBaja();
      if (humedad > humedadMaxima) alertarHumedadAlta();
      if (iluminacion < iluminacionMinima) alertarIluminacionBaja();
      if (iluminacion > iluminacionMaxima) alertarIluminacionAlta();
      if (temperatura < temperaturaMinima || temperatura > temperaturaMaxima) alertarTemperatura();
      if (muestraTouch < touchTreshold) alertarEscape();
      break;

    case HUMEDAD_BAJA:
      Serial.print("Humedad: ");
      Serial.println(humedad);
      Serial.print("Contador de riego: ");
      Serial.println(contador);

      if (contador == 10) {
        digitalWrite(PIN_BOMBA, LOW);
        estadoOptimo();
      }
      contador++;
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
  // Declaración de rutas para páginas
  servidor.serveStatic("/", LittleFS, "/");
  servidor.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html");
  });

  // Declaración de API REST
  /*
   * Get datos - obtiene todos los datos de que lee el esp32, así como los minimos y máximos actuales
   * con un JSON en el siguiente formato:
   * {
   *  "temperatura": {
   *      "minima": 45,
   *      "maxima": 90,
   *      "actual": 50
   *  },
   *  "iluminacion": {
   *      "minima": 45,
   *      "maxima": 90,
   *      "actual": 50
   *  },
   *  "humedad": {
   *      "minima": 45,
   *      "maxima": 90,
   *      "actual": 50
   *  },
   *  "fecha": ""
   * }
   */
  servidor.on("/datos", [](AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    JsonDocument doc;
    JsonDocument tempJson;
    JsonDocument ilumJson;
    JsonDocument humJson;
    // Recopila datos de la temperatura
    tempJson["minima"] = temperaturaMinima;
    tempJson["maxima"] = temperaturaMaxima;
    tempJson["actual"] = temperatura;
    // Recopila datos de la iluminación
    ilumJson["minima"] = iluminacionMinima;
    ilumJson["maxima"] = iluminacionMaxima;
    ilumJson["actual"] = iluminacion;
    // Recopila datos de la humedad
    humJson["minima"] = humedadMinima;
    humJson["maxima"] = humedadMaxima;
    humJson["actual"] = humedad;
    // Arma documento a enviar
    doc["temperatura"] = tempJson;
    doc["iluminacion"] = ilumJson;
    doc["humedad"] = humJson;
    doc["fecha"] = obtenFecha();

    serializeJson(doc, *response);

    request->send(response);
  });

  servidor.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "URL no encontrada");
  });

  /*
   * Los siguientes son manejadores para los post que establcen los valores con los que se comparan las lecturas responde a peticiones con jsons del siguiente formato
   * {
   *   "minima": 45,
   *   "maxima": 90
   * }
   */
AsyncCallbackJsonWebHandler *handlerEstablecerTempMinMax = new AsyncCallbackJsonWebHandler("/establecer/temperatura", [](AsyncWebServerRequest *request, JsonVariant &json) {
  Serial.println("JSON recibido:");
  serializeJson(json, Serial); 
  Serial.println();

  JsonObject jsonObj = json.as<JsonObject>();

  if (jsonObj.containsKey("minima") && jsonObj.containsKey("maxima")) {
    temperaturaMinima = jsonObj["minima"];
    temperaturaMaxima = jsonObj["maxima"];
    Serial.println("Valores actualizados");
  } else {
    Serial.println("JSON inválido. No contiene 'minima' o 'maxima'");
  }

  request->send(200, "application/json", "{\"status\":\"ok\"}");
});

AsyncCallbackJsonWebHandler *handlerEstablecerHumMinMax = new AsyncCallbackJsonWebHandler("/establecer/humedad", [](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject jsonObj = json.as<JsonObject>();
    humedadMinima = jsonObj["minima"];
    humedadMaxima = jsonObj["maxima"];
    Serial.println("POST temperatura recibido:");
    Serial.println(humedadMinima);
    Serial.println(humedadMaxima);
    request->send(200, "application/json", "{\"status\":\"ok\"}");
  });

AsyncCallbackJsonWebHandler *handlerEstablecerIluminacionMinMax = new AsyncCallbackJsonWebHandler("/establecer/iluminacion", [](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject jsonObj = json.as<JsonObject>();
    iluminacionMinima = jsonObj["minima"];
    iluminacionMaxima = jsonObj["maxima"];
    Serial.println("POST temperatura recibido:");
    Serial.println(iluminacionMinima);
    Serial.println(iluminacionMaxima);
    request->send(200, "application/json", "{\"status\":\"ok\"}");
  });
  servidor.addHandler(handlerEstablecerTempMinMax);
  servidor.addHandler(handlerEstablecerHumMinMax);
  servidor.addHandler(handlerEstablecerIluminacionMinMax);

  servidor.begin();
}

void inicializaLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("Ocurrió un error al montar LittleFS");
  } else {
    Serial.println("Se monto LittleFS con exito");
  }
}

void realizarLecturas() {
  muestraTouch = touchRead(PIN_TOUCH);

  // Verificación de escape con filtro
  if (muestraTouch <= touchTreshold) {
    conteoEscape++;
    if (conteoEscape >= maxIntentosEscape && !yaNotificoEscape) {
      alertarEscape();
      yaNotificoEscape = true;
    }
  } else {
    conteoEscape = 0;
  }

  actualizaIntensidadLuz();
  iluminacion = analogRead(PIN_FOT);
  cicloTrabajo = map(iluminacion, 0, ADC_VALORES - 1, CT_MAX, CT_MIN);
  humedad = dht.readHumidity();
  temperatura = dht.readTemperature();

  Serial.println("Iluminacion");
  Serial.println(iluminacion);
  Serial.println("temperatura");
  Serial.println(temperatura);
  Serial.println("humedad");
  Serial.println(humedad);
  Serial.println("touch");
  Serial.println(muestraTouch);
  Serial.println("estado");
  Serial.println(estado);
  Serial.println("intensidad");
  Serial.println(intensidadLuz);
}

String obtenFecha() {
  char sFecha[80];
  // Obtiene la fecha y hora actual
  if (!getLocalTime(&timeinfo)) {
    Serial.println("No se pudo obtener la fecha/hora");
    sFecha[0] = '\0';
  } else {
    strftime(sFecha, 80, "%d/%m/%Y %H:%M:%S", &timeinfo);
  }
  return String(sFecha);
}
void actualizaIntensidadLuz() {
  // Lee y digitaliza el valor del voltaje en el potenciometro
  int muestra = analogRead(PIN_POT);
  Serial.println("potenciometro");
  Serial.println(muestra);
  intensidadLuz = map(muestra, 0, ADC_VALORES - 1, CT_MIN, CT_MAX);
}

void estadoOptimo() {
  estado = CONDICIONES_ADECUADAS;
  yaNotificoEscape = false;
  conteoEscape = 0;
  digitalWrite(PIN_LED_VERDE, HIGH);
  digitalWrite(PIN_LED_AMARILLO, LOW);
  digitalWrite(PIN_LED_ROJO, LOW);
}

void alertarHumedadAlta() {
  estado = HUMEDAD_ALTA;
  digitalWrite(PIN_LED_VERDE, LOW);
  digitalWrite(PIN_LED_AMARILLO, HIGH);
  digitalWrite(PIN_LED_ROJO, LOW);
}

void alertarHumedadBaja() {
  // Inicia temporizador para despliegue de agua
  estado = HUMEDAD_BAJA;
  pausa.setdelay(500);
  pausa.start();
  contador = 0;
  digitalWrite(PIN_BOMBA, HIGH);
  digitalWrite(PIN_LED_VERDE, LOW);
  digitalWrite(PIN_LED_AMARILLO, HIGH);
  digitalWrite(PIN_LED_ROJO, LOW);
}

void alertarTemperatura() {
  estado = TEMPERATURA_INADECUADA;
  digitalWrite(PIN_LED_VERDE, LOW);
  digitalWrite(PIN_LED_AMARILLO, HIGH);
  digitalWrite(PIN_LED_ROJO, LOW);
}

void alertarIluminacionAlta() {
  estado = ILUMINACION_ALTA;
  digitalWrite(PIN_LED_VERDE, LOW);
  digitalWrite(PIN_LED_AMARILLO, HIGH);
  digitalWrite(PIN_LED_ROJO, LOW);
}

void alertarIluminacionBaja() {
  estado = ILUMINACION_BAJA;
  digitalWrite(PIN_LED_VERDE, LOW);
  digitalWrite(PIN_LED_AMARILLO, HIGH);
  digitalWrite(PIN_LED_ROJO, LOW);
}

void alertarEscape() {
  estado = MASCOTA_ESCAPADA;
  digitalWrite(PIN_LED_VERDE, LOW);
  digitalWrite(PIN_LED_AMARILLO, LOW);
  digitalWrite(PIN_LED_ROJO, HIGH);
  bot.sendMessage(CHAT_ID, "Su mascota ha escapado del contenedor!!!", "");
}

void togglePIN(unsigned int pin) {
  digitalWrite(pin, !digitalRead(pin));
}

void enviarDatosPorTCP() {
  WiFiClient client;
  if (!client.connect(HOST_TCP, PORT_TCP)) {
    Serial.println("No se pudo conectar al servidor TCP");
    return;
  }

  StaticJsonDocument<256> doc;

  doc["temperatura"]["minima"] = temperaturaMinima;
  doc["temperatura"]["maxima"] = temperaturaMaxima;
  doc["temperatura"]["actual"] = temperatura;

  doc["humedad"]["minima"] = humedadMinima;
  doc["humedad"]["maxima"] = humedadMaxima;
  doc["humedad"]["actual"] = humedad;

  doc["iluminacion"]["minima"] = iluminacionMinima;
  doc["iluminacion"]["maxima"] = iluminacionMaxima;
  doc["iluminacion"]["actual"] = iluminacion;

  doc["fecha"] = obtenFecha();

  String salida;
  serializeJson(doc, salida);

  client.print(salida);
  Serial.println("JSON enviado al servidor TCP:");
  Serial.println(salida);
  client.stop();
}
