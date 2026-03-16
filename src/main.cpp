#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include "ThingSpeak.h"
#include "secrets.h"
#include "DHT.h"
#include "certs.h"
#include <ArduinoJson.h>

#define DHTTYPE DHT22
#define LED_BUILTIN 2

// ================= ALERT CONTROL =================

#define ALERT_TEMP 25
#define ALERT_INTERVAL 300000   // 5 minutos

unsigned long lastAlertSent = 0;

// ================= ThingSpeak =================

unsigned long myChannelNumber = SECRET_CH_ID;
const char *myWriteAPIKey = SECRET_WRITE_APIKEY;

// ================= WiFi =================

const char *ssid = SECRET_SSID;
const char *password = SECRET_PASS;

// ================= DHT =================

uint8_t DHTPin = D1;
DHT dht(DHTPin, DHTTYPE);

float Temperature;
float Humidity;
long Rssi;

WiFiClient client;
WiFiClientSecure clientssl;

// ================= timers =================

unsigned long lastHeartbeat = 0;
unsigned long lastReading = 0;
unsigned long lastViewUpdate = 0;

// ================= funções =================

void FazConexaoWiFi(void);
String SetTime(void);
void sendData(void);
void sendMessageElement(String message_el);
String montarPayload();
String urlEncode(const String &msg);

// =================================================
// SETUP
// =================================================

void setup()
{
  Serial.begin(115200);
  delay(200);

  Serial.println("\n====================================");
  Serial.println("INTELLIBOT ESP8266 BOOT");
  Serial.println("====================================");

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(DHTPin, INPUT);

  WiFi.mode(WIFI_STA);

  dht.begin();

  Serial.println("\nInicializando WiFi...");
  FazConexaoWiFi();

  Serial.println("\nSincronizando horário NTP...");
  SetTime();

  clientssl.setInsecure();

  ThingSpeak.begin(client);

  Serial.println("\nLeitura inicial sensor");

  Temperature = dht.readTemperature();
  Humidity = dht.readHumidity();
  Rssi = WiFi.RSSI();

  Serial.print("Temperatura: ");
  Serial.println(Temperature);

  Serial.print("Umidade: ");
  Serial.println(Humidity);

  Serial.print("RSSI: ");
  Serial.println(Rssi);

  Serial.println("\nEnviando payload inicial");

  sendMessageElement(montarPayload());
}

// =================================================
// LOOP
// =================================================

void loop()
{
  unsigned long now = millis();

  // heartbeat LED

  if (now - lastHeartbeat >= 2000)
  {
    lastHeartbeat = now;

    digitalWrite(LED_BUILTIN, LOW);
    delay(40);
    digitalWrite(LED_BUILTIN, HIGH);
  }

  // leitura sensor

  if (now - lastReading >= 5000)
  {
    lastReading = now;

    Serial.println("\n===== LEITURA SENSOR =====");

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h))
    {
      Temperature = t;
      Humidity = h;

      Serial.print("Temperatura: ");
      Serial.println(Temperature);

      Serial.print("Umidade: ");
      Serial.println(Humidity);
    }
    else
    {
      Serial.println("ERRO leitura DHT");
    }

    Rssi = WiFi.RSSI();

    Serial.print("RSSI WiFi: ");
    Serial.println(Rssi);
  }

  // envio ThingSpeak

  if (now - lastViewUpdate >= 600000)
  {
    lastViewUpdate = now;

    Serial.println("\n===== ENVIO THINGSPEAK =====");

    if (WiFi.status() == WL_CONNECTED)
    {
      sendData();
    }
    else
    {
      Serial.println("WiFi desconectado. Cancelando envio.");
    }
  }

  // ================= ALERTA TEMPERATURA =================

  if (Temperature > ALERT_TEMP)
  {
    if (millis() - lastAlertSent > ALERT_INTERVAL)
    {
      Serial.println("\n===== ALERTA TEMPERATURA =====");

      sendMessageElement(montarPayload());

      lastAlertSent = millis();
    }
  }
}

// =================================================
// MONTA PAYLOAD
// =================================================

String montarPayload()
{
  String data = SetTime();
  data.replace("\n","");

  String payload =
      data + "_" +
      String(Temperature) + "_" +
      String(Humidity) + "_" +
      String(Rssi);

  Serial.println("\nPayload montado:");
  Serial.println(payload);

  return payload;
}

// =================================================
// THINGSPEAK
// =================================================

void sendData(void)
{
  ThingSpeak.setField(1, Temperature);
  ThingSpeak.setField(2, Humidity);
  ThingSpeak.setField(3, Rssi);

  int httpCode = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);

  Serial.print("HTTP ThingSpeak: ");
  Serial.println(httpCode);

  if (httpCode == 200)
  {
    Serial.println("ThingSpeak atualizado com sucesso");
  }
  else
  {
    Serial.println("Erro envio ThingSpeak");
  }
}

// =================================================
// WIFI
// =================================================

void FazConexaoWiFi(void)
{
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("\nTentando rede principal:");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    int tentativas = 0;

    while (WiFi.status() != WL_CONNECTED && tentativas < 40)
    {
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      delay(500);
      Serial.print(".");
      tentativas++;
    }

    if (WiFi.status() == WL_CONNECTED)
      break;

    Serial.println("\nFalha. Tentando rede fallback");

    WiFi.begin(SECRET_SSID_FALLBACK, SECRET_PASS_FALLBACK);

    tentativas = 0;

    while (WiFi.status() != WL_CONNECTED && tentativas < 40)
    {
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      delay(500);
      Serial.print("-");
      tentativas++;
    }

    if (WiFi.status() == WL_CONNECTED)
      break;

    Serial.println("\nNenhuma rede conectou. Tentando novamente em 5s");
    delay(5000);
  }

  Serial.println("\n===== WIFI CONECTADO =====");

  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  Serial.print("RSSI: ");
  Serial.println(WiFi.RSSI());
}

// =================================================
// NTP
// =================================================

String SetTime(void)
{
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  time_t now = time(nullptr);

  while (now < 8 * 3600 * 2)
  {
    delay(500);
    now = time(nullptr);
  }

  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);

  timeinfo.tm_hour -= 3;

  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));

  return String(asctime(&timeinfo));
}

// =================================================
// ENVIO API
// =================================================

void sendMessageElement(String message_el)
{
  const char* url = "http://179.107.1.50:8080/api/sendBotAlerta.php";

  Serial.println("\n===== ENVIO API =====");

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi desconectado. Cancelando envio.");
    return;
  }

  HTTPClient http;
  WiFiClient client;

  Serial.print("URL: ");
  Serial.println(url);

  http.begin(client, url);

  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String httpRequestData =
      "param1=1421302024-1273914&param2=" + urlEncode(message_el);

  Serial.println("POST DATA:");
  Serial.println(httpRequestData);

  int httpResponseCode = http.POST(httpRequestData);

  Serial.print("HTTP Code: ");
  Serial.println(httpResponseCode);

  if (httpResponseCode > 0)
  {
    String payload = http.getString();

    Serial.println("Resposta servidor:");
    Serial.println(payload);
  }
  else
  {
    Serial.println("Erro HTTP");
  }

  http.end();

  Serial.println("===== FIM ENVIO =====\n");
}

// =================================================
// URL ENCODE
// =================================================

String urlEncode(const String &msg)
{
  String encoded = "";
  char c;

  for (size_t i = 0; i < msg.length(); i++)
  {
    c = msg.charAt(i);

    if (isalnum(c))
    {
      encoded += c;
    }
    else
    {
      char buf[4];
      sprintf(buf, "%%%02X", (unsigned char)c);
      encoded += buf;
    }
  }

  return encoded;
}