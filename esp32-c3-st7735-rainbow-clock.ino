// ===============================================================
// Проект: Радужные часы на ESP32-C3 и дисплее ST7735
// Авторы: Пользователь и DeepSeek
// Версия: 1.0
// Дата: 2026
// Описание: Часы с крупными радужными цифрами на черном фоне,
//           синхронизация времени по NTP, плавное переливание цветов
// ===============================================================

// Подключаем необходимые библиотеки
#include <WiFi.h>              // Для работы с Wi-Fi
#include <NTPClient.h>          // Для получения времени по NTP
#include <WiFiUdp.h>            // Для UDP соединения (нужен NTPClient)
#include <Adafruit_GFX.h>       // Графическая библиотека Adafruit
#include <Adafruit_ST7735.h>    // Библиотека для дисплея ST7735
#include <SPI.h>                // Для SPI интерфейса
#include <Fonts/FreeSansBold12pt7b.h>  // Жирный шрифт (опционально)

// ========== РАЗРЕШЕНИЕ ДИСПЛЕЯ ==========
// Важно! Из-за особенностей ориентации (rotation 3) 
// ширина и высота меняются местами
#define TFT_WIDTH  160  // Фактическая ширина в данной ориентации
#define TFT_HEIGHT 80   // Фактическая высота в данной ориентации

// ========== СМЕЩЕНИЕ ЭКРАНА ==========
// Компенсирует сдвиг изображения при rotation 3
// Подобрано экспериментально для полного заполнения экрана
#define Y_OFFSET 25

// ========== НАСТРОЙКИ Wi-Fi ==========
// Введите данные вашей Wi-Fi сети
const char* ssid = "YOUR WIFI NAME";
const char* password = "YOUR WIFI PASS";

// ========== НАСТРОЙКИ ДИСПЛЕЯ ==========
// Пины подключения согласно схеме выше
#define TFT_CS    7   // Chip Select
#define TFT_DC    2   // Data/Command
#define TFT_RST   3   // Reset
#define TFT_BL    8   // Backlight (подсветка)

// Создаем объект дисплея
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// ========== НАСТРОЙКИ ВРЕМЕНИ ==========
WiFiUDP ntpUDP;  // Для NTP
const long utcOffsetInSeconds = 14400;  // Смещение для Ульяновска (UTC+4)
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

// ========== ПЕРЕМЕННЫЕ ==========
String currentTime = "--:--";        // Текущее время (ЧЧ:ММ)
String currentDate = "01.01.1970";    // Текущая дата (ДД.ММ.ГГГГ)
int hueOffset = 0;                    // Смещение для анимации цветов

// ========== ФУНКЦИЯ ПРЕОБРАЗОВАНИЯ HSV В RGB565 ==========
// Преобразует цвет из модели HSV (оттенок) в RGB565
// Параметры: h - оттенок (0-359)
// Возвращает: 16-битный цвет в формате RGB565
uint16_t hsvToRgb565(int h) {
  byte r, g, b;
  int region, remainder;
  
  h = h % 360;  // Нормализуем оттенок
  
  // Разбиваем цветовой круг на 6 регионов
  if (h < 120) {
    region = h / 60;
    remainder = h % 60;
    if (region == 0) { r = 255; g = remainder * 4.25; b = 0; }
    if (region == 1) { r = 255 - remainder * 4.25; g = 255; b = 0; }
  } else if (h < 240) {
    h -= 120;
    region = h / 60;
    remainder = h % 60;
    if (region == 0) { r = 0; g = 255; b = remainder * 4.25; }
    if (region == 1) { r = 0; g = 255 - remainder * 4.25; b = 255; }
  } else {
    h -= 240;
    region = h / 60;
    remainder = h % 60;
    if (region == 0) { r = remainder * 4.25; g = 0; b = 255; }
    if (region == 1) { r = 255; g = 0; b = 255 - remainder * 4.25; }
  }
  
  // Конвертируем 8-битные RGB в 16-битный RGB565
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// ========== ФУНКЦИЯ РИСОВАНИЯ РАДУЖНОГО ТЕКСТА ==========
// Рисует текст, где каждый символ имеет свой цвет
// Параметры:
//   text - строка для вывода
//   x, y - координаты начала
//   textSize - размер шрифта (1-5)
void drawRainbowText(String text, int x, int y, int textSize) {
  tft.setTextSize(textSize);
  
  for (int i = 0; i < text.length(); i++) {
    // Каждый символ имеет свой оттенок, смещенный во времени
    int hue = (i * 30 + hueOffset) % 360;
    tft.setTextColor(hsvToRgb565(hue));
    tft.setCursor(x + i * (6 * textSize), y);
    tft.print(text.charAt(i));
  }
}

// ========== ФУНКЦИЯ ОТОБРАЖЕНИЯ ВРЕМЕНИ ==========
// Оптимизирована для уменьшения мерцания
void displayTime() {
  // Статические переменные сохраняют значения между вызовами
  static String lastTime = "";
  static String lastDate = "";
  
  String hourStr = currentTime.substring(0, 2);
  String minuteStr = currentTime.substring(3, 5);
  String fullTime = hourStr + ":" + minuteStr;
  
  // Обновляем смещение цвета для анимации (плавное переливание)
  hueOffset = (hueOffset + 2) % 360;
  
  // ВСЕГДА перерисовываем время (для анимации цветов)
  // Но очищаем только область времени, а не весь экран
  tft.fillRect(0, 10 + Y_OFFSET, TFT_WIDTH, 45, ST77XX_BLACK);
  drawRainbowText(fullTime, 10, 15 + Y_OFFSET, 5);
  
  // ДАТУ перерисовываем ТОЛЬКО если она изменилась
  // Это экономит ресурсы и уменьшает мерцание
  if (currentDate != lastDate) {
    // Очищаем только область даты
    tft.fillRect(0, 55 + Y_OFFSET, TFT_WIDTH, 20, ST77XX_BLACK);
    
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(2);
    tft.setCursor((TFT_WIDTH - (currentDate.length() * 12)) / 2, 60 + Y_OFFSET);
    tft.print(currentDate);
    lastDate = currentDate;
  }
  
  lastTime = fullTime;
}

// ========== SETUP ==========
// Выполняется один раз при запуске
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting Rainbow Clock...");

  // Инициализация SPI с правильными пинами
  SPI.begin(4, -1, 5, 7);  // SCK=4, MISO=-1, MOSI=5, CS=7
  SPI.setFrequency(20000000);  // 20 МГц для стабильной работы

  // Включаем подсветку дисплея
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // Инициализация дисплея
  tft.initR(INITR_BLACKTAB);  // Для ST7735 с черной вкладкой
  tft.setRotation(3);  // Важно! Подобрано для правильной ориентации
  
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(5, 10 + Y_OFFSET);
  tft.print("WiFi...");

  // Подключение к Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 60) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi OK");
    timeClient.begin();  // Запускаем NTP клиент
    
    // Пытаемся получить время
    int ntpAttempts = 0;
    while (!timeClient.update() && ntpAttempts < 10) {
      timeClient.forceUpdate();
      ntpAttempts++;
      delay(1000);
    }
  }
  
  tft.fillScreen(ST77XX_BLACK);
}

// ========== LOOP ==========
// Выполняется бесконечно
void loop() {
  timeClient.update();  // Обновляем время
  
  // Получаем время в формате ЧЧ:ММ
  String fullTime = timeClient.getFormattedTime();
  currentTime = fullTime.substring(0, 5);

  // Получаем дату
  time_t epochTime = timeClient.getEpochTime();
  struct tm *ptm = localtime(&epochTime);
  char dateStr[15];
  sprintf(dateStr, "%02d.%02d.%04d", 
          ptm->tm_mday, 
          ptm->tm_mon + 1, 
          ptm->tm_year + 1900);
  currentDate = String(dateStr);

  displayTime();  // Отображаем на экране
  delay(150);      // Небольшая задержка для плавности
}
