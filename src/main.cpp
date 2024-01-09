#include <WiFiS3.h>
#include <arduinoJson.h>
#include <NewPing.h>
#include <ArduinoHttpClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_LEDBackpack.h>

int trigPin = 7;
int echoPin = 8;
NewPing sonar(trigPin, echoPin, 100);

Adafruit_AlphaNum4 alpha4 = Adafruit_AlphaNum4();

char ssid[] = "Netlab-OIL";
char pass[] = "DesignChallenge";
char* server = "145.220.74.222";
char* sensorDataPath = "/api/sensor/data";
char* valveStatePath = "/api/Valve";
char* authHeader = "Basic QXF1YUJyYWluOkFxdWFCcmFpbiMyMDIzIQ==";
int port = 80;

int valve1 = 12;
int valve2 = 13;

unsigned long lastPostTime = 0;
unsigned long lastGetTime = 0;
unsigned long GetStateSwitch = 0;
bool previousValve1State;
bool previousValve2State;

WiFiClient wifi;
HttpClient httpClient = HttpClient(wifi, server, port);

void connectToWiFi() {
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void postSensorData(int watertonId, int sensorId, String sensorType, int sensorValue) {
  if(WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost. Attempting to reconnect...");
    connectToWiFi();
  }

  DynamicJsonDocument doc(1024);
  doc["watertonId"] = watertonId;
  doc["sensorID"] = sensorId;
  doc["waarde"] = sensorValue;
  doc["type"] = sensorType;

  String jsonData;
  serializeJson(doc, jsonData);

  httpClient.beginRequest();
  httpClient.post(sensorDataPath);
  httpClient.sendHeader("Authorization", authHeader);
  httpClient.sendHeader("Content-Type", "application/json");
  httpClient.sendHeader("Content-Length", jsonData.length());
  httpClient.beginBody();
  httpClient.print(jsonData);
  httpClient.endRequest();
 
  int statusCode = httpClient.responseStatusCode();

  if (statusCode == 200) {
    String response = httpClient.responseBody();
    Serial.print("Post Data Status code: ");
    Serial.println(statusCode);
    //Serial.print("Response: ");
    //Serial.println(response);
  } else {
    Serial.print("HTTP POST failed, error code: ");
    Serial.println(statusCode);
  }

  delay(5);
  httpClient.stop();
}

void getValveState(int watertonId, int valveId, int valvePin, bool& previousValveState) {
  if(WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost. Attempting to reconnect...");
    connectToWiFi();
  }

  httpClient.beginRequest();
  httpClient.get(String(valveStatePath) + "/" + watertonId + "/" + valveId);
  httpClient.sendHeader("Authorization", authHeader);
  httpClient.endRequest();

  int httpResponseCode = httpClient.responseStatusCode();

  if(httpResponseCode == 200) {
    String response = httpClient.responseBody();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, response);
    bool isOpen = doc["open"]; // Get the state of the valve
    if (isOpen != previousValveState) {
      Serial.print("Valve " + String(valveId) + " state changed: ");
      Serial.println(isOpen ? "open" : "closed");
      digitalWrite(valvePin, isOpen ? HIGH : LOW);
      previousValveState = isOpen;
    }
  } else {
    Serial.print("HTTP GET failed, error code: ");
    Serial.println(httpResponseCode);
  }

  delay(5);
  httpClient.stop();
  //Serial.println("Checked valve: " + valvePin);
}

void setup() {
  Serial.begin(9600);

  alpha4.begin(0x70);

  delay(10);
  connectToWiFi();
  pinMode(valve1, OUTPUT);
  pinMode(valve2, OUTPUT);
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastPostTime >= 10000) {
    int cm = sonar.ping_cm();
    lastPostTime = currentMillis;

    alpha4.clear();
    
    //create a percentage. max is 40 and min is 0
    //when cm is 40, the percentage is 0%
    //when cm is 0, the percentage is 100%
    int perint = map(cm, 0, 45, 100, 0);

    postSensorData(1, 1, "waterpercentage", perint);

    String perc = String(perint);

    for (int i = 0; i < perc.length(); i++) {
      alpha4.writeDigitAscii(i, perc[i]);
    }
    alpha4.writeDisplay();
  }

  if (currentMillis - lastGetTime >= 1000) {
    lastGetTime = currentMillis;

    if (GetStateSwitch == 0) {
      getValveState(1, 1, valve1, previousValve1State);
      GetStateSwitch = 1;
    } else if (GetStateSwitch == 1) {
      getValveState(1, 2, valve2, previousValve2State);
      GetStateSwitch = 0;
    }
  }
}