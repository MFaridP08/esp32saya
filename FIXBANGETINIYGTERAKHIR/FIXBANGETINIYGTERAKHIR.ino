#include <DHT.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_Sensor.h>

// Konfigurasi pin DHT22
#define DHT_PIN 4
#define DHT_TYPE DHT22
#define OPERATING_VOLTAGE   3.3 // или 5.0

constexpr uint8_t PIN_AOUT = 25;
constexpr uint8_t PIN_LED = 27;


// Inisialisasi objek DHT
DHT dht(DHT_PIN, DHT_TYPE);

// Membaca nilai analog dari sensor MQ-135
int sensorValue = analogRead(36);

// Konfigurasi pin relay kipas
const int relayPin = 5;
int sensorADC;
float sensorVoltage;
float sensorDustDensity;

// Inisialisasi objek server web
AsyncWebServer server(80);

// Variabel suhu, kelembapan, dan kecepatan kipas
float voltage = sensorValue * (5.0 / 1023.0); // Mengonversi nilai sensor ke tegangan (0-5V)
float temperature = 0.0;
float humidity = 0.0;
float methaneConcentration = (voltage - 0.4) * 50.0;
float co2Concentration = 500 * voltage;
float coConcentration = 9.956 * pow((3.3 / voltage - 1), -1.286);
float zeroSensorDustDensity = 0.6;
int fanSpeed = 0;

// Fungsi untuk mengatur kecepatan kipas
void setFanSpeed(int speed) {
  int pwmValue = map(speed, 0, 100, 0, 255);
  analogWrite(relayPin, pwmValue);
}
// Fungsi untuk mengirim data suhu, kelembapan, dan kecepatan kipas dalam format JSON
String getSensorData() {
  String json = "{";
  json += "\"temperature\": " + String(temperature, 1) + ",";
  json += "\"humidity\": " + String(humidity, 1) + ",";
  json += "\"co2Concentration\": "+ String(co2Concentration,1) + ",";
  json += "\"coConcentration\": "+ String(coConcentration,1) + ",";
  json += "\"methaneConcentration\" : "+ String(methaneConcentration,1) + ",";
  json += "\"sensorDustDensity\":"+String(sensorDustDensity,1)+",";
  json += "\"sensorVoltage\":"+String(sensorVoltage,1)+",";
  json += "\"fanSpeed\": " + String(fanSpeed);
  json += "}";
  return json;
}

void setup() {
    //konfigurasui sensormq-135
pinMode (36, INPUT);
  // Inisialisasi Serial Monitor
  Serial.begin(115200);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  // Inisialisasi DHT
  dht.begin();
  
    // Konfigurasi pin relay kipas
  pinMode(relayPin, OUTPUT);
  setFanSpeed(0); // Kipas mati awal

  // Konfigurasi WiFi AP
  WiFi.softAP("ESP32-AP CNC ROOM"); // Nama SSID dan password WiFi AP

  // Tampilkan alamat IP WiFi AP pada Serial Monitor
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Mengonfigurasi rute server web
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String htmlContent = "<html><head><meta http-equiv='refresh' content='3'></head><body>";
    htmlContent += "<h1>Sensor DHT22</h1>";
    htmlContent += "<p>Suhu: " + String(temperature) + " &deg;C</p>";
    htmlContent += "<p>Kelembaban: " + String(humidity) + " %</p>";
    htmlContent += "<p>Karbon Dioksida: " + String(co2Concentration,1) + " &deg;C</p>";
    htmlContent += "<p>Karbon Monodioksida: " + String(coConcentration,1) + " %</p>";
    htmlContent += "<p>Methana: " + String(methaneConcentration,1) + " %</p>";
    htmlContent += "<p>Dusk Density: " + String(sensorDustDensity,1) + " ug/m3</p>";
    htmlContent += "<p><input type='range' id='speedSlider' min='0' max='100' value='" + String(fanSpeed) + "' onchange='setFanSpeed(this.value)'>";
    htmlContent += "<script>function setFanSpeed(speed) { var xhr = new XMLHttpRequest(); xhr.open('GET', '/setFanSpeed?speed=' + speed, true); xhr.send(); }</script>";
    htmlContent += "<script>setInterval(getSensorData, 2000); function getSensorData() { var xhr = new XMLHttpRequest(); xhr.onreadystatechange = function() { if (this.readyState == 4 && this.status == 200) { var data = JSON.parse(this.responseText); document.getElementById('temperature').innerHTML = data.temperature; document.getElementById('humidity').innerHTML = data.humidity; document.getElementById('fanSpeed').innerHTML = data.fanSpeed; document.getElementById('speedSlider').value = data.fanSpeed; } }; xhr.open('GET', '/getSensorData', true); xhr.send(); }</script>";
    htmlContent += "</body></html>";
    request->send(200, "text/html", htmlContent);
  });
  server.on("/setFanSpeed", HTTP_GET, [](AsyncWebServerRequest *request){
    String speedParam = request->getParam("speed")->value();
    int newSpeed = speedParam.toInt();
    if (newSpeed >= 0 && newSpeed <= 100) {
      fanSpeed = newSpeed;
      setFanSpeed(fanSpeed);
    }
    request->send(200, "text/plain", "OK");
  });
   server.on("/getSensorData", HTTP_GET, [](AsyncWebServerRequest *request){
    String sensorData = getSensorData();
    request->send(200, "application/json", sensorData);
  });
  // Konfigurasi server web
  server.begin();
}

void loop() {
    for (int i = 0; i < 10 ; i++) {
    digitalWrite(PIN_LED, HIGH);
    delayMicroseconds(280);
    sensorADC += analogRead(PIN_AOUT);
    digitalWrite(PIN_LED, LOW);
    delay(10);
  }
  sensorADC = sensorADC / 10;
  sensorVoltage = (OPERATING_VOLTAGE / 1024.0) * sensorADC * 11;
  if (sensorVoltage < zeroSensorDustDensity) {
    sensorDustDensity = 0;
  } else {
    sensorDustDensity = 0.17 * sensorVoltage - 0.1;
  }
  
  // Baca suhu dan kelembaban dari DHT22
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
// Mengatur kecepatan kipas secara otomatis berdasarkan suhu
  if (temperature <= 28) {
    fanSpeed = 50;
  } else if (temperature >= 29 && temperature <= 33) {
    fanSpeed = 75;
  } else if (temperature > 34) {
    fanSpeed = 100;
  }
 //Baca Kadar Gas CO, CO2, dan Metana
  if (co2Concentration >= 400 && co2Concentration <=600){
    fanSpeed = 75;
  } else if (co2Concentration >= 600 && co2Concentration <= 1000){
    fanSpeed = 100;
  }else if (co2Concentration >1000){
    fanSpeed = 100;
  }
  if (coConcentration >= 0 && co2Concentration <=5){
    fanSpeed = 75;
  } else if (coConcentration >= 5 && coConcentration <= 9){
        fanSpeed = 100;
  }
  if (methaneConcentration >= 1 && co2Concentration <=3){
    fanSpeed = 75;
  } else if (methaneConcentration >10){
    fanSpeed = 100;
  }
  // Tampilkan suhu dan kelembaban di Serial Monitor
  Serial.print("Suhu: ");
  Serial.print(temperature);
  Serial.print(" °C\tKelembaban: ");
  Serial.print(humidity);
  Serial.println(" %");

  // Delay sejenak
  delay(3000);
}
