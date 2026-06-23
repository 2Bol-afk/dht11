#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <Firebase_ESP_Client.h>
#include <time.h>
#include <DHT.h>
#include <ESP_Mail_Client.h>   // ESP Mail Client by Mobizt
#include "secrets.h"           // credentials — not committed (see secrets.example.h)

// ================= PIN CONFIG =================
#define DHTPIN       16
#define DHTTYPE      DHT11

#define LED1         17
#define LED2         18
#define LED3         19
#define LED4         21
#define LED5         22
#define MOTOR_PIN    27

// Firebase + Gmail SMTP credentials are defined in secrets.h (gitignored).
// See secrets.example.h for the list of values to set.

// ================= TIME =================
#define GMT_OFFSET_SEC       (8 * 3600)
#define DAYLIGHT_OFFSET_SEC  0

// ================= OBJECTS =================
DHT dht(DHTPIN, DHTTYPE);
WiFiManager wm;

FirebaseData fbdo;
FirebaseData fbdoThresh;   // separate stream for threshold data
FirebaseAuth auth;
FirebaseConfig config;

SMTPSession smtp;

// ================= TIMERS =================
unsigned long lastSend       = 0;
unsigned long lastAlertSent  = 0;
const unsigned long SEND_INTERVAL = 10000;  // 10 sec

// ================= THRESHOLD CACHE =================
// Cached from Firebase so we don't fetch every 10 s
struct Thresholds {
  float  tempMin    =  0.0;
  float  tempMax    = 50.0;
  float  humMin     =  0.0;
  float  humMax     = 100.0;
  String alertEmail = "";
  unsigned long cooldownMs = 600000UL;  // 10 min default
  bool   loaded = false;
} thresholds;

unsigned long lastThreshFetch = 0;
const unsigned long THRESH_FETCH_INTERVAL = 30000;  // re-fetch every 30 s

// =====================================================

void initWiFi() {
  WiFi.mode(WIFI_STA);
  wm.setConnectTimeout(20);
  if (!wm.autoConnect("ESP32-Setup", "12345678")) {
    Serial.println("WiFi Failed. Restarting...");
    delay(1000);
    ESP.restart();
  }
  Serial.println("WiFi Connected: " + WiFi.localIP().toString());
}

void initTime() {
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, "pool.ntp.org", "time.nist.gov");
  Serial.print("Syncing time");
  while (time(nullptr) < 1609459200) { Serial.print("."); delay(500); }
  Serial.println("\nTime Synced!");
}

void initFirebase() {
  config.api_key    = WEB_API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email   = USER_EMAIL;
  auth.user.password = USER_PASS;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  Serial.println("Firebase Ready");
}

// =====================================================
// ================= FETCH THRESHOLDS ==================
// =====================================================

void fetchThresholds() {
  if (Firebase.RTDB.getJSON(&fbdoThresh, "Thresholds")) {
    FirebaseJson &json = fbdoThresh.jsonObject();
    FirebaseJsonData result;

    if (json.get(result, "temperature/min"))  thresholds.tempMin    = result.floatValue;
    if (json.get(result, "temperature/max"))  thresholds.tempMax    = result.floatValue;
    if (json.get(result, "humidity/min"))     thresholds.humMin     = result.floatValue;
    if (json.get(result, "humidity/max"))     thresholds.humMax     = result.floatValue;
    if (json.get(result, "alertEmail"))       thresholds.alertEmail = result.stringValue;
    if (json.get(result, "cooldownMin")) {
      thresholds.cooldownMs = (unsigned long)(result.intValue) * 60000UL;
    }

    thresholds.loaded = true;
    Serial.printf("Thresholds: T[%.1f-%.1f] H[%.1f-%.1f] -> %s (cooldown %lu min)\n",
      thresholds.tempMin, thresholds.tempMax,
      thresholds.humMin,  thresholds.humMax,
      thresholds.alertEmail.c_str(),
      thresholds.cooldownMs / 60000UL);
  } else {
    Serial.println("Thresholds fetch failed: " + fbdoThresh.errorReason());
  }
}

// =====================================================
// ================= SMTP ALERT ========================
// =====================================================

void smtpCallback(SMTP_Status status) {
  Serial.println(status.info());
}

void sendAlertEmail(float temp, float hum, const String &reason) {
  if (thresholds.alertEmail.isEmpty()) {
    Serial.println("No alert email configured.");
    return;
  }

  // Build readable timestamp
  time_t now;
  struct tm ti;
  time(&now);
  localtime_r(&now, &ti);
  char ts[32];
  sprintf(ts, "%04d-%02d-%02d %02d:%02d:%02d",
    ti.tm_year+1900, ti.tm_mon+1, ti.tm_mday,
    ti.tm_hour, ti.tm_min, ti.tm_sec);

  // SMTP session config
  ESP_Mail_Session session;
  session.server.host_name = "smtp.gmail.com";
  session.server.port      = 465;
  session.login.email      = SMTP_SENDER_EMAIL;
  session.login.password   = SMTP_APP_PASSWORD;
  session.login.user_domain = "";

  SMTP_Message message;
  message.sender.name  = SMTP_SENDER_NAME;
  message.sender.email = SMTP_SENDER_EMAIL;
  message.subject      = "⚠️ DHT11 Alert: " + reason;
  message.addRecipient("Recipient", thresholds.alertEmail.c_str());

  String body =
    "DHT11 Threshold Alert\n"
    "=====================\n\n"
    "Time    : " + String(ts) + "\n"
    "Reason  : " + reason + "\n\n"
    "Current Readings:\n"
    "  Temperature : " + String(temp, 1) + " °C\n"
    "  Humidity    : " + String(hum,  1) + " %\n\n"
    "Configured Thresholds:\n"
    "  Temperature : " + String(thresholds.tempMin,1) + " – " + String(thresholds.tempMax,1) + " °C\n"
    "  Humidity    : " + String(thresholds.humMin,1)  + " – " + String(thresholds.humMax,1)  + " %\n\n"
    "---\nSent automatically by your ESP32 IoT device.";

  message.text.content = body.c_str();
  message.text.charSet = "utf-8";
  message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

  smtp.debug(0);
  smtp.callback(smtpCallback);

  if (!smtp.connect(&session)) {
    Serial.println("SMTP connect failed: " + smtp.errorReason());
    return;
  }

  if (!MailClient.sendMail(&smtp, &message)) {
    Serial.println("Email send failed: " + smtp.errorReason());
  } else {
    Serial.println("✅ Alert email sent → " + thresholds.alertEmail);
    lastAlertSent = millis();
  }

  smtp.closeSession();
}

// =====================================================
// ================= CHECK THRESHOLDS ==================
// =====================================================

void checkThresholds(float temp, float hum) {
  if (!thresholds.loaded || thresholds.alertEmail.isEmpty()) return;

  // Respect cooldown
  if (millis() - lastAlertSent < thresholds.cooldownMs) {
    Serial.println("Alert cooldown active, skipping.");
    return;
  }

  String reason = "";

  if (temp < thresholds.tempMin)
    reason += "Temperature LOW (" + String(temp,1) + "°C < " + String(thresholds.tempMin,1) + "°C) ";
  else if (temp > thresholds.tempMax)
    reason += "Temperature HIGH (" + String(temp,1) + "°C > " + String(thresholds.tempMax,1) + "°C) ";

  if (hum < thresholds.humMin)
    reason += "Humidity LOW (" + String(hum,1) + "% < " + String(thresholds.humMin,1) + "%) ";
  else if (hum > thresholds.humMax)
    reason += "Humidity HIGH (" + String(hum,1) + "% > " + String(thresholds.humMax,1) + "%) ";

  if (reason.length() > 0) {
    reason.trim();
    Serial.println("⚠️  Threshold breach: " + reason);
    sendAlertEmail(temp, hum, reason);
  }
}

// =====================================================
// ================= CONTROL LISTENER ==================
// =====================================================

void listenControls() {
  String devices[] = {"LED1","LED2","LED3","LED4","LED5","MOTOR"};
  for (String dev : devices) {
    if (Firebase.RTDB.getInt(&fbdo, "Controls/" + dev + "/state")) {
      int state = fbdo.intData();
      if (dev == "LED1") digitalWrite(LED1, state);
      if (dev == "LED2") digitalWrite(LED2, state);
      if (dev == "LED3") digitalWrite(LED3, state);
      if (dev == "LED4") digitalWrite(LED4, state);
      if (dev == "LED5") digitalWrite(LED5, state);
      if (dev == "MOTOR") digitalWrite(MOTOR_PIN, !state);
    }
  }
}

// =====================================================
// ================= SEND SENSOR DATA ==================
// =====================================================

void sendData() {
  float hum  = dht.readHumidity();
  float temp = dht.readTemperature();

  if (isnan(hum) || isnan(temp)) {
    Serial.println("DHT Read Failed");
    return;
  }

  // Write to Firebase
  time_t now;
  struct tm ti;
  time(&now);
  localtime_r(&now, &ti);

  char dateStr[12], h[3], m[3], s[3];
  sprintf(dateStr, "%04d-%02d-%02d", ti.tm_year+1900, ti.tm_mon+1, ti.tm_mday);
  sprintf(h, "%02d", ti.tm_hour);
  sprintf(m, "%02d", ti.tm_min);
  sprintf(s, "%02d", ti.tm_sec);

  String path = "DHT11/" + String(dateStr) + "/" + h + "/" + m + "/" + s;
  Firebase.RTDB.setFloat(&fbdo, path + "/temperature", temp);
  Firebase.RTDB.setFloat(&fbdo, path + "/humidity",    hum);

  Serial.printf("Sent: %.1f°C | %.1f%%\n", temp, hum);

  // Check thresholds after every successful read
  checkThresholds(temp, hum);
}

// =====================================================

void setup() {
  Serial.begin(115200);

  pinMode(LED1,      OUTPUT);
  pinMode(LED2,      OUTPUT);
  pinMode(LED3,      OUTPUT);
  pinMode(LED4,      OUTPUT);
  pinMode(LED5,      OUTPUT);
  pinMode(MOTOR_PIN, OUTPUT);

  dht.begin();
  initWiFi();
  initTime();
  initFirebase();

  // Initial threshold fetch
  delay(2000);  // give Firebase a moment to auth
  fetchThresholds();
}

void loop() {
  if (Firebase.ready()) {
    listenControls();

    // Re-fetch thresholds periodically so dashboard changes take effect
    if (millis() - lastThreshFetch > THRESH_FETCH_INTERVAL) {
      lastThreshFetch = millis();
      fetchThresholds();
    }
  }

  if (millis() - lastSend > SEND_INTERVAL) {
    lastSend = millis();
    sendData();
  }

  delay(200);
}