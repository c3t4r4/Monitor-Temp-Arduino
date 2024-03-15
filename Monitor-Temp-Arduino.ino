#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <UrlEncode.h>
#include <time.h>
#include "ThingSpeak.h"
#include "secrets.h"
#include "DHT.h"
#include "certs.h"
#include "HTTPClient.h"
#include <ArduinoJson.h>
#include <iostream>

#define DHTTYPE DHT22 // DHT 22  (AM2302), AM2321
#define LED_BUILTIN 2 // turn on led port board Metanol

// ----------------- API Tringspeak -----------------------
unsigned long myChannelNumber = SECRET_CH_ID;
const char *myWriteAPIKey = SECRET_WRITE_APIKEY;

// ----------------- Wifi -----------------------

const char *ssid = SECRET_SSID;     // Enter SSID here
const char *password = SECRET_PASS; // Enter Password here

// ----------------- API CallMeBot.com -----------------------
String wpPhone[10] = {WP_PHONE, WP_PHONE2, WP_PHONE3, WP_PHONE4};
String wpAPI[10] = {WP_KEY, WP_KEY2, WP_KEY3, WP_KEY4};
String messageWP = "";
//--------------element-------------//
const String token_el = ACCESS_TOKEN;
const String room_el = ROOM_ID;

const char fingerprint[] PROGMEM = "7F:08:BF:52:2A:FA:E9:32:36:DD:0D:D4:2F:FE:D5:7B:A8:04:35:55";
const String host = "https://api.callmebot.com";
const int httpPort = 443;
const String Whats = "Msg Padrão";

WiFiClient client;
WiFiClientSecure clientssl;
// DHT Sensor
uint8_t DHTPin = D1;
// Initialize DHT sensor.
DHT dht(DHTPin, DHTTYPE);
float Temperature;
float Humidity;
long Rssi;
int UpdateTimes = 0;
bool TempNormal = true;

void setup()
{
  // initialize digital pin LED_BUILTIN as an output.
  // pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  delay(100);

  pinMode(DHTPin, INPUT);
  dht.begin();
  FazConexaoWiFi();
  SetTime();
  clientssl.setInsecure(); // the magic line, use with caution
  ThingSpeak.begin(client);
  //sendMessage("Hello from BABUINO-ESP8266!- CALLMEBOT\n");
  sendMessageElement("Hello from BABUINO-ESP8266!-  INICIALIZANDO.");
}

void loop()
{
  // Connect or reconnect to WiFi
  if (WiFi.status() != WL_CONNECTED)
  {
    FazConexaoWiFi();
  }
  SetTime();
    String Dt = SetTime();
  // monitoring led
  // digitalWrite(LED_BUILTIN, HIGH); // turn the LED on (HIGH is the voltage level)
  // Serial.println("Ligado");
  // SetTime();
  // delay(1500);                    // wait for a second
  // digitalWrite(LED_BUILTIN, LOW); // turn the LED off by making the voltage LOW
  // Serial.println("Desligado!-------");
  // delay(1500);

  // Measure Temperature
  Temperature = dht.readTemperature();

  // Serial.print("Temperatura: ");
  // Serial.println((int)Temperature);

  messageWP = "Temperatura: " + String((int)Temperature) + "\n";

  // Measure Humidity
  Humidity = dht.readHumidity();

  messageWP = messageWP + "Umidade: " + String((int)Humidity) + "\n";

  // Serial.print("Umidade: ");
  // Serial.println((int)Humidity);

  // Measure Signal Strength (RSSI) of Wi-Fi connection
  Rssi = WiFi.RSSI();

  messageWP = messageWP + "Sinal Wifi: " + String((int)Rssi) + "\n";

  // Serial.print("Sinal Wifi: ");
  // Serial.println((int)(Rssi / 2) * -1);

  UpdateTimes = UpdateTimes + 1;

  sendData();

 sendMessageElement( Dt + "_" + String((int)Temperature) + "_ " + String((int)Humidity) +"_ " + String((int)Rssi));
    delay(600000);


  if (Temperature > 25)
  {
    TempNormal = false;
    String Dt = SetTime();
    //sendMessage("DATA: " + Dt + " - VERIFICAR TEMPERATURA DA SALA DO SERVIDOR : TEMPERATURA: " + String((int)Temperature) + "°C - ");
    //sendMessage("DATA: " + Dt + " - Umidade: " + String((int)Humidity) + "");
     //sendMessageElement("DATA: _" + Dt + "_ - Umidade: _" + String((int)Humidity) +"_");
     //sendMessageElement("DATA: _" + Dt + "_ - VERIFICAR TEMPERATURA DA SALA DO SERVIDOR : TEMPERATURA: _" + String((int)Temperature) + "_°C - ");
      sendMessageElement( Dt + "_" + String((int)Temperature) + "_ " + String((int)Humidity) +"_ " + String((int)Rssi));
    delay(300000);
  }

  // Wait 30 seconds to update the channel again
  delay(30000);
}

void sendData(void)
{
  ThingSpeak.setField(1, Temperature);
  ThingSpeak.setField(2, Humidity);
  ThingSpeak.setField(3, Rssi);
  ThingSpeak.setField(4, UpdateTimes);
  int httpCode = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  if (httpCode == 200)
  {
    Serial.println("Channel write successful.\n");
    sendMessageElement("Channel write successful");
    // sendMessage("Channel write successful");
  }
  else
  {
    Serial.println("Problem writing to channel. HTTP error code " + String(httpCode) + "\n");
    // sendMessage(String(httpCode));
  }
}

void FazConexaoWiFi(void)
{
  Serial.println("Conectando-se à rede WiFi...\n");
  Serial.println();
  delay(1000);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print("(-.-)\n");
  }
  Serial.println("");
  Serial.println("WiFi connectado com sucesso!\n");
  SetTime();
  Serial.println("IP obtido:\n ");
  Serial.println(WiFi.localIP());
  delay(1000);
}

String SetTime(void)
{
  // Set time via NTP, as required for x.509 validation
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2)
  {
    delay(500);
    now = time(nullptr);
  }
  Serial.println("");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  timeinfo.tm_hour = timeinfo.tm_hour - 3;
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
  return (asctime(&timeinfo));

}

void sendMessage(String message)
{
  WiFiClient client;
  HTTPClient http;

  for (int n = 0; n < sizeof(*wpPhone); n++)
  {
    String link = "http://api.callmebot.com/whatsapp.php?phone=" + wpPhone[n] + "&apikey=" + wpAPI[n] + "&text=" + urlEncode(message);
    http.begin(client, link);
    // Specify content-type header
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    // Send HTTP POST request
    int httpResponseCode = http.GET();
    if (httpResponseCode == 200)
    {
      Serial.print("Message sent successfully:" + message);
    }
    else
    {
      Serial.println("Error sending the message");
      Serial.print("HTTP response code: ");
      Serial.println(httpResponseCode);
      String payload = http.getString();
      Serial.println(payload);
    }
  }
  // Free resources
  http.end();
}



void sendMessageElement(String message_el)
{

const char* url = "http://179.107.1.50/api/senBotAlerta.php";
const char* urlSSL = "https://sisint.policiamilitar.sp.gov.br/api/senBotAlerta.php";
   // Variáveis
  WiFiClientSecure client;
  HTTPClient http;
  String postData;
WiFiClient c;

  int httpResponseCode;

 
  // Iniciar a requisição HTTP
  http.begin(c, url);
  // http.begin(client, urlSSL);

  // Definir cabeçalhos
// Especificar cabeçalho da requisição
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
 // Preparar dados da requisição JSON
  //postData = "{\"msgtype\": \"m.text\", \"body\": \"" + message_el + "\"}";
  String httpRequestData = "param1=1421302024-1273914&param2="+message_el;


  // Enviar a requisição POST
  httpResponseCode = http.POST(httpRequestData);

  // Verificar a resposta
  if (httpResponseCode > 0) {
    Serial.print("Código de resposta HTTP: ");
    Serial.println(httpResponseCode);

    if (httpResponseCode == 200) {
      Serial.println("Mensagem enviada com sucesso!");
    } else {
      Serial.println("Erro ao enviar a mensagem:");
      Serial.print("Código de erro HTTP: ");
      Serial.println(httpResponseCode);
    }
  } else {
    Serial.println("Falha ao enviar a requisição.");
  }

  // Liberar recursos
  http.end();
}