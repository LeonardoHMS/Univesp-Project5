#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* WIFI_SSID = "ID";
const char* WIFI_PASSWORD = "PASSWORD";
const String SERVER_BASE = "http://192.168.100.8:5000";

const int SOIL_PIN = 34;
const int PUMP_RELAY_PIN = 25;

const float SOIL_DRY_RAW = 4095.0f;
const float SOIL_WET_RAW = 1800.0f;
const int TARGET_MOISTURE = 60;
const int PUMP_RUN_TIME_MS = 18000;
const int STATUS_INTERVAL_MS = 5000;

bool pumpRunning = false;
bool reservoirEmpty = false;
unsigned long pumpStartedAt = 0;
unsigned long lastStatusUpdate = 0;

float readSoilMoisturePercent()
{
  int raw = analogRead(SOIL_PIN);

  if (raw >= SOIL_DRY_RAW) {
    return 0.0f;
  }

  if (raw <= SOIL_WET_RAW) {
    return 100.0f;
  }

  float mapped = (SOIL_DRY_RAW - raw) / (SOIL_DRY_RAW - SOIL_WET_RAW);
  float pct = constrain(mapped * 100.0f, 0.0f, 100.0f);
  return pct;
}

void setPumpState(bool enabled)
{
  pumpRunning = enabled;
  digitalWrite(PUMP_RELAY_PIN, enabled ? LOW : HIGH);
}

bool httpPostJson(const String& endpoint, const String& payload)
{
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  HTTPClient http;
  String url = SERVER_BASE + endpoint;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(payload);
  bool ok = (httpCode >= 200 && httpCode < 300);
  http.end();
  return ok;
}

void sendStatusToServer(float moisturePercent)
{
  StaticJsonDocument<256> doc;
  doc["moisture"] = (int)round(moisturePercent);
  doc["pump_running"] = pumpRunning;
  doc["reservoir_empty"] = reservoirEmpty;
  doc["alert_message"] = reservoirEmpty ? "Reservatório vazio. Encha o reservatório para retomar a irrigação." : "Sistema funcionando normalmente.";

  String payload;
  serializeJson(doc, payload);
  httpPostJson("/api/esp32/update", payload);
}

bool requestReservoirStatus()
{
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  HTTPClient http;
  String url = SERVER_BASE + "/api/esp32/status";
  http.begin(url);

  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  String response = http.getString();
  http.end();

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, response);
  if (error) {
    return false;
  }

  if (doc["reservoir_empty"].is<bool>()) {
    reservoirEmpty = doc["reservoir_empty"].as<bool>();
  }

  if (doc["target_moisture"].is<int>()) {
    // target moisture is not used here directly, but available for future logic
  }

  return true;
}

void connectWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("Conectando ao WiFi...");

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("WiFi conectado: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("Falha ao conectar ao WiFi. O sistema continuará tentando.");
  }
}

void setup()
{
  Serial.begin(115200);

  pinMode(SOIL_PIN, INPUT);
  pinMode(PUMP_RELAY_PIN, OUTPUT);
  digitalWrite(PUMP_RELAY_PIN, HIGH);

  connectWiFi();
}

void loop()
{
  float moisture = readSoilMoisturePercent();

  if (millis() - lastStatusUpdate >= STATUS_INTERVAL_MS) {
    requestReservoirStatus();
    sendStatusToServer(moisture);
    lastStatusUpdate = millis();
  }

  if (reservoirEmpty) {
    if (pumpRunning) {
      setPumpState(false);
    }
    delay(1000);
    return;
  }

  if (!pumpRunning && moisture < TARGET_MOISTURE) {
    Serial.println("Umidade baixa. Ligando a bomba...");
    setPumpState(true);
    pumpStartedAt = millis();
  }

  if (pumpRunning) {
    if (millis() - pumpStartedAt >= PUMP_RUN_TIME_MS) {
      setPumpState(false);
      Serial.println("Tempo maximo da bomba atingido.");

      if (moisture < TARGET_MOISTURE) {
        Serial.println("Umidade ainda baixa. Reservatório vazio ou bomba sem sucção.");
        reservoirEmpty = true;
        sendStatusToServer(moisture);
      }
    }
  }

  if (pumpRunning && moisture >= TARGET_MOISTURE) {
    Serial.println("Umidade atingida. Desligando bomba.");
    setPumpState(false);
  }

  delay(1000);
}
