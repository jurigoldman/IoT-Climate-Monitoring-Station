#include <Wire.h>
#include <Adafruit_BME280.h>
#include <SparkFun_SCD4x_Arduino_Library.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ThingSpeak.h>

// WiFi и ThingSpeak настройки
const char* ssid = "Kolledz"; // Ваша WiFi сеть
const char* password = "";    // Пароль (оставьте пустым, если сеть открытая)
unsigned long myChannelNumber = 2956854; // Ваш Channel ID
const char* myWriteAPIKey = "25A2ZVPHPF290UK5"; // Ваш Write API Key

// Объект WiFiClient для ThingSpeak
WiFiClient client;

// Определение пинов для светодиодов
#define LED1_PIN 2   // D2 для 7 зелёных светодиодов
#define LED2_PIN 4   // D4 для 3 мигающих светодиодов

// Определение для OLED-дисплея
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define NUM_LEDS_D2 7   // 7 зелёных светодиодов на D2
#define NUM_LEDS_D4 3   // 3 мигающих светодиода на D4

// Создание объектов NeoPixel
Adafruit_NeoPixel strip1 = Adafruit_NeoPixel(NUM_LEDS_D2, LED1_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip2 = Adafruit_NeoPixel(NUM_LEDS_D4, LED2_PIN, NEO_GRB + NEO_KHZ800);

// Цвета для светодиодов
uint32_t greenColor = strip1.Color(0, 255, 0); // Зелёный для D2
uint32_t whiteColor = strip2.Color(255, 255, 255); // Белый для D4
uint32_t offColor = strip2.Color(0, 0, 0); // Выключено для D4

// Создание объектов для датчиков
Adafruit_BME280 bme;
SCD4x scd4x;

// NTP-клиент
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 10800, 60000); // UTC+3 для EEST

// Глобальные переменные для хранения последних валидных данных
float last_temp_bme = -1, last_hum_bme = -1, last_pres_bme = -1;
uint16_t last_co2 = -1;
float last_temp_scd = -1, last_hum_scd = -1;

// Буфер для усреднения CO2
#define CO2_BUFFER_SIZE 5
uint16_t co2_buffer[CO2_BUFFER_SIZE];
int co2_buffer_index = 0;
bool co2_buffer_full = false;

// Переменные для отслеживания времени
unsigned long lastLogTime = 0;
unsigned long lastBlinkTime = 0;
unsigned long lastSendTime = 0;
bool blueLedState = true;
bool outOfRange = false;
const unsigned long logInterval = 60000; // 1 минута
const unsigned long blinkInterval = 1000; // 1 секунда
const unsigned long sendInterval = 300000; // 5 минут (300,000 миллисекунд)

 // Нормальные диапазоны показателей
const float TEMP_MIN = 18.0;
const float TEMP_MAX = 26.0;
const float HUM_MIN = 40.0;
const float HUM_MAX = 60.0;
const float PRES_MIN = 990.0;
const float PRES_MAX = 1010.0;
const float CO2_MIN = 400;
const float CO2_MAX = 1000;

// Высота над уровнем моря (в метрах)
const float ALTITUDE = 100.0;

// Флаг для состояния OLED
bool oledInitialized = false;

// Функция для корректировки давления
float adjustPressure(float measuredPressure) {
  if (measuredPressure <= 0) return measuredPressure;
  return measuredPressure * exp(ALTITUDE / 8430.0);
}

// Функция проверки диапазонов
bool checkRanges(float temp_bme, float hum_bme, float pres_bme, uint16_t co2, float temp_scd) {
  bool inRange = true;
  if (temp_bme < TEMP_MIN || temp_bme > TEMP_MAX) inRange = false;
  if (hum_bme < HUM_MIN || hum_bme > HUM_MAX) inRange = false;
  if (pres_bme < PRES_MIN || pres_bme > PRES_MAX) inRange = false;
  if (co2 < CO2_MIN || co2 > CO2_MAX) inRange = false;
  if (temp_scd < TEMP_MIN || temp_scd > TEMP_MAX) inRange = false;
  return inRange;
}

// Функция для усреднения CO2
uint16_t averageCO2() {
  uint32_t sum = 0;
  int count = co2_buffer_full ? CO2_BUFFER_SIZE : co2_buffer_index;
  if (count == 0) return 0;
  for (int i = 0; i < count; i++) {
    sum += co2_buffer[i];
  }
  return sum / count;
}

// Функция для сканирования I2C-устройств
void scanI2C() {
  Serial.println("Scanning I2C bus...");
  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  Serial.println("I2C scan complete.");
}

// Функция для сканирования WiFi-сетей
void scanWiFiNetworks() {
  Serial.println("Starting WiFi scan...");
  WiFi.mode(WIFI_STA);
  Serial.println("WiFi mode set to STA.");
  WiFi.disconnect();
  Serial.println("WiFi disconnected for scanning.");
  delay(100);
  
  Serial.println("Calling WiFi.scanNetworks()...");
  int n = WiFi.scanNetworks();
  Serial.println("WiFi.scanNetworks() completed.");
  
  if (n == 0) {
    Serial.println("No networks found. Possible WiFi module issue or no networks in range.");
  } else {
    Serial.print(n);
    Serial.println(" networks found:");
    for (int i = 0; i < n; ++i) {
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (RSSI: ");
      Serial.print(WiFi.RSSI(i));
      Serial.print(" dBm)");
      if (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) {
        Serial.print(" [OPEN]");
      } else {
        Serial.print(" [ENCRYPTED]");
      }
      Serial.println();
      delay(10);
    }
  }
  Serial.println("WiFi scan complete.");
}

// Функция для расшифровки статуса WiFi
String getWiFiStatusString(wl_status_t status) {
  switch (status) {
    case WL_IDLE_STATUS: return "WL_IDLE_STATUS (WiFi idle, waiting for connection)";
    case WL_NO_SSID_AVAIL: return "WL_NO_SSID_AVAIL (SSID not available)";
    case WL_SCAN_COMPLETED: return "WL_SCAN_COMPLETED (Scan completed)";
    case WL_CONNECTED: return "WL_CONNECTED (Connected to WiFi)";
    case WL_CONNECT_FAILED: return "WL_CONNECT_FAILED (Connection failed)";
    case WL_CONNECTION_LOST: return "WL_CONNECTION_LOST (Connection lost)";
    case WL_DISCONNECTED: return "WL_DISCONNECTED (Disconnected from WiFi)";
    default: return "Unknown status: " + String(status);
  }
}

// Функция для подключения к WiFi
void connectToWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Connecting to WiFi (SSID: ");
    Serial.print(ssid);
    Serial.println(")...");
    
    WiFi.mode(WIFI_STA);
    Serial.println("WiFi mode set to STA for connection.");
    if (password == "") {
      Serial.println("Attempting to connect to an open network...");
      WiFi.begin(ssid);
    } else {
      Serial.println("Attempting to connect to a password-protected network...");
      WiFi.begin(ssid, password);
    }

    int attempts = 0;
    const int maxAttempts = 40;
    while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
      delay(500);
      Serial.print(".");
      attempts++;
      if (attempts % 5 == 0) {
        Serial.print("\nAttempt ");
        Serial.print(attempts);
        Serial.print(": Status: ");
        Serial.println(getWiFiStatusString(WiFi.status()));
      }
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnected!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      Serial.print("RSSI: ");
      Serial.print(WiFi.RSSI());
      Serial.println(" dBm");
    } else {
      Serial.println("\nFailed to connect!");
      Serial.print("Final WiFi Status: ");
      Serial.println(getWiFiStatusString(WiFi.status()));
      if (WiFi.status() == WL_NO_SSID_AVAIL) {
        Serial.println("Error: The specified SSID (" + String(ssid) + ") is not available. Please check the SSID and ensure the network is in range.");
      } else if (WiFi.status() == WL_CONNECT_FAILED) {
        Serial.println("Error: Connection failed. Please check if the network requires a password or authentication.");
      }
    }
  } else {
    Serial.println("WiFi already connected!");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Serial port initialized...");
  while (!Serial) {
    delay(100);
  }
  Serial.println("Serial started successfully.");
  delay(1000);

  // Тестирование WiFi модуля
  Serial.println("Testing WiFi module...");
  WiFi.mode(WIFI_STA);
  Serial.println("WiFi mode set to STA for initial test.");

  // Сканирование WiFi-сетей
  Serial.println("Initiating WiFi scan...");
  scanWiFiNetworks();
  Serial.println("WiFi scan completed (or failed).");

  // Подключение к WiFi
  Serial.println("Starting WiFi connection...");
  connectToWiFi();
  Serial.println("WiFi connection attempt finished.");

  // Инициализация NTP
  Serial.println("Initializing NTP client...");
  if (WiFi.status() == WL_CONNECTED) {
    timeClient.begin();
    timeClient.update();
    Serial.println("NTP time updated.");
  } else {
    Serial.println("NTP time not updated (WiFi not connected).");
  }

  // Инициализация ThingSpeak
  Serial.println("Initializing ThingSpeak...");
  if (WiFi.status() == WL_CONNECTED) {
    ThingSpeak.begin(client);
    Serial.println("ThingSpeak initialized.");
  } else {
    Serial.println("ThingSpeak not initialized (WiFi not connected).");
  }

  // Инициализация светодиодов
  Serial.println("Initializing LEDs...");
  strip1.begin();
  strip2.begin();
  strip1.setBrightness(5);
  strip2.setBrightness(10);
  delay(100);
  for (int i = 0; i < NUM_LEDS_D2; i++) {
    strip1.setPixelColor(i, greenColor);
  }
  strip1.show();
  for (int i = 0; i < NUM_LEDS_D4; i++) {
    strip2.setPixelColor(i, whiteColor);
  }
  strip2.show();
  Serial.println("LEDs on D2 set to constant green (brightness 5), LEDs on D4 set to white with blinking on out-of-range...");

  // Инициализация I2C
  Serial.println("Initializing I2C...");
  Wire.begin(21, 22);
  Wire.setClock(50000);
  Serial.println("I2C initialized.");

  // Сканирование I2C-шины
  Serial.println("Starting I2C scan...");
  scanI2C();
  Serial.println("I2C scan completed.");

  // Инициализация OLED-дисплея
  Serial.println("Initializing OLED display...");
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 failed at address 0x3C. Trying 0x3D...");
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
      Serial.println("SSD1306 allocation failed! Continuing without OLED...");
      oledInitialized = false;
    } else {
      oledInitialized = true;
      Serial.println("OLED initialized successfully at 0x3D.");
    }
  } else {
    oledInitialized = true;
    Serial.println("OLED initialized successfully at 0x3C.");
  }

  if (oledInitialized) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Starting...");
    display.display();
    Serial.println("OLED set to 'Starting'...");
  }

  // Инициализация BME280
  Serial.println("Initializing BME280...");
  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }
  Serial.println("BME280 initialized successfully.");

  // Инициализация SCD41
  Serial.println("Initializing SCD41...");
  if (!scd4x.begin(Wire, true)) {
    Serial.println("Could not communicate with SCD41, check wiring!");
    while (1);
  }
  delay(5000);

  // Включение автоматической калибровки SCD41
  Serial.println("Enabling Automatic Self-Calibration for SCD41...");
  if (scd4x.setAutomaticSelfCalibrationEnabled(true)) {
    Serial.println("ASC enabled successfully.");
  } else {
    Serial.println("Failed to enable ASC.");
  }
  Serial.println("SCD41 initialized successfully.");

  // Инициализация буфера CO2
  Serial.println("Initializing CO2 buffer...");
  for (int i = 0; i < CO2_BUFFER_SIZE; i++) {
    co2_buffer[i] = 0;
  }
  Serial.println("CO2 buffer initialized.");

}

void loop() {
  unsigned long currentTime = millis();

  // Чтение данных с датчиков только раз в минуту
  if (currentTime - lastLogTime >= logInterval || lastLogTime == 0) {
    // Чтение данных с BME280
    float temp_bme = bme.readTemperature();
    float hum_bme = bme.readHumidity();
    float pres_bme = bme.readPressure() / 100.0F;
    pres_bme = adjustPressure(pres_bme);

    // Чтение данных с SCD41  Serial.println("Setup complete. Entering loop...");

    uint16_t co2 = -1;
    float temp_scd = -1, hum_scd = -1;
    if (scd4x.readMeasurement()) {
      delay(50);
      co2 = scd4x.getCO2();
      temp_scd = scd4x.getTemperature();
      hum_scd = scd4x.getHumidity();
      if (co2 == 0 || hum_scd < 0 || hum_scd > 100) {
        Serial.println("Invalid SCD41 measurement data!");
        co2 = temp_scd = hum_scd = -1;
      } else {
        co2_buffer[co2_buffer_index] = co2;
        co2_buffer_index = (co2_buffer_index + 1) % CO2_BUFFER_SIZE;
        if (co2_buffer_index == 0) co2_buffer_full = true;
        co2 = averageCO2();
        Serial.println("CO2: " + String(co2) + " ppm | Temp SCD: " + String(temp_scd) + " C | Hum SCD: " + String(hum_scd) + " %");
      }
    } else {
      Serial.println("Error reading SCD41 measurement!");
      co2 = temp_scd = hum_scd = -1;
    }

    // Сохранение последних валидных данных
    if (temp_bme != -1) last_temp_bme = temp_bme;
    if (hum_bme != -1) last_hum_bme = hum_bme;
    if (pres_bme != -1) last_pres_bme = pres_bme;
    if (co2 != -1) last_co2 = co2;
    if (temp_scd != -1) last_temp_scd = temp_scd;
    if (hum_scd != -1) last_hum_scd = hum_scd;

    // Вывод данных в Serial Monitor
    Serial.println("BME280 - Temp: " + String(last_temp_bme) + " C | Hum: " + String(last_hum_bme) + " % | Pres: " + String(last_pres_bme) + " hPa");

    // Проверка диапазонов
    outOfRange = !checkRanges(last_temp_bme, last_hum_bme, last_pres_bme, last_co2, last_temp_scd);
    Serial.println("outOfRange: " + String(outOfRange));

    // Обновление OLED-дисплея
    if (oledInitialized) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 0);
      timeClient.update();
      String timeString = "T: " + timeClient.getFormattedTime();
      display.println(timeString);
      display.println("TBME: " + String(last_temp_bme) + "C");
      display.println("HBME: " + String(last_hum_bme) + "%");
      display.println("PBME: " + String(last_pres_bme) + "hPa");
      display.println("CO2: " + String(last_co2) + "ppm");
      display.println("TSCD: " + String(last_temp_scd) + "C");
      display.println("HSCD: " + String(last_hum_scd) + "%");
      display.display();
      Serial.println("OLED updated with sensor data and time...");
    }

    lastLogTime = currentTime;
  }

  // Управление светодиодами (D4)
  if (lastLogTime != 0) {
    if (outOfRange) {
      if (currentTime - lastBlinkTime >= blinkInterval) {
        blueLedState = !blueLedState;
        for (int i = 0; i < NUM_LEDS_D4; i++) {
          strip2.setPixelColor(i, blueLedState ? whiteColor : offColor);
        }
        strip2.show();
        delay(10);
        Serial.println("Blink: " + String(blueLedState ? "ON" : "OFF") + " (outOfRange: " + String(outOfRange) + ", CO2: " + String(last_co2) + ", Temp SCD: " + String(last_temp_scd) + ", Pres: " + String(last_pres_bme) + ")");
        lastBlinkTime = currentTime;
      }
    } else {
      for (int i = 0; i < NUM_LEDS_D4; i++) {
        strip2.setPixelColor(i, whiteColor);
      }
      strip2.show();
      delay(10);
      blueLedState = true;
    }
  }

  // Отправка данных в ThingSpeak каждые 5 минут
  if (currentTime - lastSendTime >= sendInterval || lastSendTime == 0) {
    if (lastLogTime != 0) {
      connectToWiFi();
      if (WiFi.status() == WL_CONNECTED) {
        ThingSpeak.setField(1, last_temp_bme);
        ThingSpeak.setField(2, last_hum_bme);
        ThingSpeak.setField(3, last_pres_bme);
        ThingSpeak.setField(4, last_co2);
        ThingSpeak.setField(5, last_temp_scd);
        ThingSpeak.setField(6, last_hum_scd);
        int response = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
        if (response == 200) {
          Serial.println("Data sent to ThingSpeak successfully.");
        } else {
          Serial.println("Failed to send data to ThingSpeak. Error: " + String(response));
        }
      } else {
        Serial.println("WiFi not connected, skipping ThingSpeak upload.");
      }
      lastSendTime = currentTime;
    }
  }
}
