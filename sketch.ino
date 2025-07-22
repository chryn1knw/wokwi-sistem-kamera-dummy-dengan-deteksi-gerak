#define BLYNK_TEMPLATE_ID ...
#define BLYNK_TEMPLATE_NAME ...
#define BLYNK_AUTH_TOKEN ...

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <time.h>

// Config
#define PIR_PIN        9       // D2
#define BUZZER_PIN     10      // D3 
#define BAUD_RATE      115200
#define COOLDOWN_MS    3000    // Cooldown 3 detik
#define DEBOUNCE_MS    50     // Debounce 0.05 detik
#define BUZZER_DURATION 10000  // Duration 10 Detik

char auth[] = BLYNK_AUTH_TOKEN;
unsigned long lastDetectionTime = 0;
unsigned long buzzerStartTime = 0;
bool buzzerActive = false;
bool remoteShutoff = false;
bool pirState = false;

void setup() {
  pinMode(PIR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  
  Serial.begin(BAUD_RATE);
  Blynk.begin(auth, "Wokwi-GUEST", "");
  
  // Set waktu Jakarta (GMT+7)
  configTime(13 * 3600, 0, "pool.ntp.org", "time.google.com");
  Serial.println("Menunggu sinkronisasi waktu...");
  
  while (!time(nullptr)) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nSystem Ready!");
}

String getFormattedTime() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  
  char timeStr[20];
  strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
  return String(timeStr);
}

BLYNK_WRITE(V1) {
  if (param.asInt() == 0) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerActive = false;
    remoteShutoff = true;
    Serial.println("Alarm dimatikan secara remote!");
    Serial.print("Waktu shutdown: ");
    Serial.println(getFormattedTime());
  } else {
    remoteShutoff = false;
  }
}

bool isOperatingTime() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  
  // True jika: Weekend (Sabtu/Minggu) ATAU Malam hari (22:00-06:00)
  return (timeinfo.tm_wday == 0 || timeinfo.tm_wday == 6) || 
         (timeinfo.tm_hour >= 22 || timeinfo.tm_hour < 6);
}

void notifyMotionDetected() {
  if (!isOperatingTime()) return;
  
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  
  String timestamp = getFormattedTime();
  String dayType = (timeinfo.tm_wday == 0 || timeinfo.tm_wday == 6) ? "Weekend" : "Malam";
  String message = "Gerakan terdeteksi (" + dayType + "): " + timestamp;
  
  // Aktifkan alarm fisik dan update Switch ke ON
  if (!buzzerActive) {
    digitalWrite(BUZZER_PIN, HIGH);
    buzzerActive = true;
    buzzerStartTime = millis();
    Blynk.virtualWrite(V1, 1);
    
    // Kirim notifikasi
    Blynk.virtualWrite(V0, message);
    Blynk.logEvent("motion_detected", message);
    Serial.println(message);
  }
  
  lastDetectionTime = millis();
}

void loop() {
  Blynk.run();
  
  // Matikan buzzer setelah durasi ATAU jika remote shut-off aktif
  if (buzzerActive && (remoteShutoff || (millis() - buzzerStartTime >= BUZZER_DURATION))) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerActive = false;
    if (remoteShutoff) {
      Blynk.virtualWrite(V1, 0);
      remoteShutoff = false;
    }
  }
  
  // Deteksi gerakan dengan debounce
  bool currentPirState = digitalRead(PIR_PIN);
  if (currentPirState != pirState) {
    pirState = currentPirState;
    if (pirState && millis() - lastDetectionTime >= COOLDOWN_MS) {
      notifyMotionDetected();
    }
    delay(DEBOUNCE_MS);
  }
  
  delay(50);
}
