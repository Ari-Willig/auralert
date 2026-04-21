#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <math.h>

// --- LED Pin ---
const int ledPin = 2; // Use 2 for ESP32 built-in LED
const int batteryLED = 32;
const int batteryPin = 34;

// --- EEG input (GPIO34 is input-only, good for analog on ESP32) ---
const int eegPin = 35;

// --- WiFi Credentials ---
const char* ssid = "DukeVisitor";
const char* password = "";

// --- Server Details ---
const char* host = "alexa-overtrue-tomiko.ngrok-free.dev";
const int httpsPort = 443;
const char* urlPath = "/predict";

// --- EEG Sample Size (same as arduino_led_working.c: 5 sec at 256 Hz = 1280) ---
const int channels = 1;
const int sequence_length = 1280;
const int sample_size = channels * sequence_length;

// --- Sampling ---
const int SAMPLE_RATE = 256;  // Hz - must match model training
const int WINDOW_SECONDS = 5;

// --- WiFi client ---
WiFiClientSecure client;

// --- LED control variables ---
int ledBlinkTimes = 0;
int ledBlinkCounter = 0;
unsigned long lastBlinkMillis = 0;
const long blinkInterval = 300; // ms
bool ledState = false;

// --- Collect 5 second window of real EEG from GPIO34 ---
void collectEEGWindow(float* sample) {
  const unsigned long sampleIntervalUs = 1000000 / SAMPLE_RATE; // ~3906 us at 256 Hz

  Serial.println("Collecting 5s EEG window from GPIO34...");

  for (int i = 0; i < sample_size; i++) {
    unsigned long t = micros();

    // Read 12-bit ADC (0-4095) from GPIO34
    int raw = analogRead(eegPin);
    sample[i] = (float)raw;

    // Busy-wait until next sample time
    while (micros() - t < sampleIntervalUs) {
      yield();
    }
  }

  // --- Z-score normalize the window (same scale as typical EEG features) ---
  float mean = 0.0f;
  for (int i = 0; i < sample_size; i++) mean += sample[i];
  mean /= (float)sample_size;

  float variance = 0.0f;
  for (int i = 0; i < sample_size; i++) {
    float diff = sample[i] - mean;
    variance += diff * diff;
  }
  variance /= (float)sample_size;
  float std_dev = (float)sqrt((double)variance);

  if (std_dev < 1e-6f) std_dev = 1e-6f;

  for (int i = 0; i < sample_size; i++) {
    sample[i] = (sample[i] - mean) / std_dev;
  }

  Serial.printf("EEG window done. Mean=%.4f Std=%.4f (post-norm)\n", mean, std_dev);
}

// --- Blink LED non-blocking ---
void handleLEDBlink() {
  if (ledBlinkCounter < ledBlinkTimes) {
    unsigned long currentMillis = millis();
    if (currentMillis - lastBlinkMillis >= blinkInterval) {
      lastBlinkMillis = currentMillis;
      ledState = !ledState;
      digitalWrite(ledPin, ledState);

      if (!ledState) { // finished one on/off cycle
        ledBlinkCounter++;
        if (ledBlinkCounter >= ledBlinkTimes) {
          ledState = false;
          digitalWrite(ledPin, LOW);
        }
      }
    }
  }
}

void checkbattery(){
  int raw = analogRead(batteryPin);
  float conversiontoV = 3.3f * ((float)raw / 4095.0f);
  if (conversiontoV < 1.89){
    digitalWrite(batteryLED, HIGH);
  }
  else{
    digitalWrite(batteryLED, LOW);
  }
}

// --- Send EEG sample as binary (same as arduino_led_working.c) ---
bool sendEEGSample(float* sample) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected!");
    return false;
  }

  unsigned long contentLength = sample_size * sizeof(float);

  if (!client.connected()) {
    Serial.println("Connecting to server...");
    if (!client.connect(host, httpsPort)) {
      Serial.println("Connection failed!");
      return false;
    }
  }

  // --- Send headers ---
  client.println("POST " + String(urlPath) + " HTTP/1.1");
  client.println("Host: " + String(host));
  client.println("Content-Type: application/octet-stream");
  client.print("Content-Length: ");
  client.println(contentLength);
  client.println("Connection: keep-alive");
  client.println();

  // --- Send binary data ---
  client.write((uint8_t*)sample, contentLength);
  client.flush();

  // --- Read response ---
  long timeout = millis();
  while (!client.available()) {
    if (millis() - timeout > 20000) {
      Serial.println("Timeout waiting for server response!");
      client.stop();
      return false;
    }
    delay(1);
  }

  // --- Read status line ---
  String statusLine = client.readStringUntil('\n');
  Serial.println("STATUS: " + statusLine);

  // --- Skip headers ---
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
  }

  // --- Read body ---
  String responseBody = "";
  while (client.available()) {
    responseBody += (char)client.read();
  }
  Serial.println("BODY: " + responseBody);

  // --- Parse prediction ---
  int prediction = -1;
  int idx = responseBody.indexOf("prediction");
  if (idx != -1) {
    int colon = responseBody.indexOf(":", idx);
    int comma = responseBody.indexOf(",", colon);
    if (comma == -1) comma = responseBody.indexOf("}", colon);
    String val = responseBody.substring(colon + 1, comma);
    val.trim();
    prediction = val.toInt();
  }

  Serial.print("Parsed prediction: ");
  Serial.println(prediction);

  // --- Set LED blink based on prediction (same as arduino_led_working.c) ---
  if (prediction == 0) { ledBlinkTimes = 3; }
  else if (prediction == 1) { ledBlinkTimes = 1; }
  else if (prediction == 2) { ledBlinkTimes = 2; }
  ledBlinkCounter = 0; // reset blink cycle

  return true;
}

// --- Setup ---
void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  pinMode(batteryLED, OUTPUT);
  digitalWrite(ledPin, LOW);

  // ESP32 ADC for GPIO34
  analogReadResolution(12);       // 12-bit: 0-4095
  analogSetAttenuation(ADC_11db); // full 0-3.3V range

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  client.setInsecure();
  client.setTimeout(30000);
}

// --- Main loop ---
void loop() {
  static float eeg_sample[sample_size];
  checkbattery();
  collectEEGWindow(eeg_sample);   // 5 second window from GPIO34
  sendEEGSample(eeg_sample);     // same POST as arduino_led_working.c → LED blink

  unsigned long startMillis = millis();
  while (millis() - startMillis < 5000) { // 5s delay between EEG sends
    handleLEDBlink(); // non-blocking LED blink
    delay(1); // small delay to avoid locking
  }
}
