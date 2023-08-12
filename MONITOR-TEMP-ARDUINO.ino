#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <UrlEncode.h>
#include<time.h>
#include "ThingSpeak.h"
#include "secrets.h"
#include "DHT.h"

#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321

// ----------------- API Tringspeak -----------------------
unsigned long myChannelNumber = SECRET_CH_ID;
const char * myWriteAPIKey = SECRET_WRITE_APIKEY;

// ----------------- Wifi -----------------------
const char* ssid = SECRET_SSID; // Enter SSID here
const char* password = SECRET_PASS; //Enter Password here

// ----------------- API CallMeBot.com -----------------------
String wpPhone = WP_PHONE;
String wpAPI = WP_KEY;
String messageWP;
const char fingerprint[] PROGMEM = "7F:08:BF:52:2A:FA:E9:32:36:DD:0D:D4:2F:FE:D5:7B:A8:04:35:55";
const String host = "https://api.callmebot.com";
const int httpPort = 443;

WiFiClient  client;
WiFiClientSecure  clientssl;
 
// DHT Sensor
uint8_t DHTPin = D1;

// Initialize DHT sensor.
DHT dht(DHTPin, DHTTYPE);

float Temperature;
float Humidity;
long Rssi;
int UpdateTimes = 0;

int test = 0;

void setup() {
  Serial.begin(115200);
  delay(100);
    
  pinMode(DHTPin, INPUT);

  dht.begin();

  FazConexaoWiFi();

  SetTime();

  clientssl.setInsecure(); //the magic line, use with caution

  ThingSpeak.begin(client);
}

void loop() {

  // Connect or reconnect to WiFi
  if(WiFi.status() != WL_CONNECTED){
    FazConexaoWiFi();
  }  

  // Measure Temperature
  Temperature = dht.readTemperature();

  Serial.print("Temperatura: ");  Serial.println((int)Temperature);

  messageWP = "Temperatura: " + String((int)Temperature) + "\n";

  // Measure Humidity
  Humidity = dht.readHumidity();

  messageWP = messageWP + "Umidade: " + String((int)Humidity) + "\n";

  Serial.print("Umidade: ");  Serial.println((int)Humidity);

  // Measure Signal Strength (RSSI) of Wi-Fi connection
  Rssi = WiFi.RSSI();

  messageWP = messageWP + "Sinal Wifi: " + String((int)Rssi) + "\n";
  
  Serial.print("Sinal Wifi: ");  Serial.println((int)Rssi);

  UpdateTimes = UpdateTimes + 1;

  sendData();

  if(test == 0){
    test = 1;
    sendMessage("Ligando Monitor Arduino!");
  }

  if(Temperature > 25){
    sendMessage(messageWP);
  }

  // Wait 30 seconds to update the channel again
  delay(30000);
}

void sendData(void) {

  ThingSpeak.setField(1, Temperature);
  ThingSpeak.setField(2, Humidity);
  ThingSpeak.setField(3, Rssi);
  ThingSpeak.setField(4, UpdateTimes);

  int httpCode = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);

  if (httpCode == 200) {
    Serial.println("Channel write successful.");
  }
  else {
    Serial.println("Problem writing to channel. HTTP error code " + String(httpCode));
  }
}

void FazConexaoWiFi(void)
{
    Serial.println("Conectando-se à rede WiFi...");
    Serial.println();  
    delay(1000);
    WiFi.begin(ssid, password);
 
    while (WiFi.status() != WL_CONNECTED) 
    {
        delay(500);
        Serial.print(".");
    }
 
    Serial.println("");
    Serial.println("WiFi connectado com sucesso!");  
    Serial.println("IP obtido: ");
    Serial.println(WiFi.localIP());
 
    delay(1000);
}

void SetTime(void)
{
  // Set time via NTP, as required for x.509 validation
  configTime(-3 * 60 * 60, 0, "a.st1.ntp.br");

  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
}

void sendMessage(String message){
  
  clientssl.setFingerprint(fingerprint);

  if (!clientssl.connect(host, httpPort)) { //works!
    Serial.println("Conexão com API WP Falhou!!!");
    return;
  }

  // Data to send with HTTP POST
  String url = host + "/whatsapp.php?phone=" + wpPhone + "&apikey=" + wpAPI + "&text=" + urlEncode(message);
  HTTPClient http;
  http.begin(clientssl, url);

  Serial.println("Enviando Msg WP: " + wpPhone);
  Serial.println("MSG: " + message);
  Serial.println("URL: " + url);

  // Specify content-type header
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  
  // Send HTTP POST request
  int httpResponseCode = http.GET();
  if (httpResponseCode == 200){
    Serial.print("Message sent successfully");
  }
  else{
    Serial.println("Error sending the message");
    Serial.print("HTTP response code: ");
    Serial.println(httpResponseCode);
    String payload = http.getString();
    Serial.println(payload);
  }

  // Free resources
  http.end();
}
 
