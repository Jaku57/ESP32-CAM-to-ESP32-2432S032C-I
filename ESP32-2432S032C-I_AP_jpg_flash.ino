// Dieser Sketch für das ESP32-2432S032C oder JC2432W328N-Board empfängt jpg-Bilder (320x240),
// die eine ESP32-CAM per WLAN (Access Point) auf Anfrage sendet!
// Durch Touch auf Display wird die Flash-LED der ESP32-CAM ein- und ausgeschaltet!
// Dafür müssen in der library "TFT-eSPI" in der User_Setup.h Änderungen vorgenommen werden:
// #define ST7789_DRIVER                      Treiber Display
// #define TFT_RGB_ORDER TFT_BGR              für korrekte Farbe auskommentieren!
// #define TFT_INVERSION_ON                   hell und dunkel invertiert
// Für JC2432W328N: #define TFT_INVERSION_OFF
// #define TFT_BL 27                          Backlight
// Werkzeuge, Board: "ESP32 Dev Module"
// Werkzeuge, CPU Frequency: "240 MHz (WiFi/BT)"!   wegen Framerate

#include <WiFi.h>
#include <HTTPClient.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <JPEGDecoder.h>
#include <TAMC_GT911.h>  // Bibliothek für Touch-Funktion

#define TOUCH_SDA 33
#define TOUCH_SCL 32
#define TOUCH_INT 21
#define TOUCH_RST 25
#define TOUCH_WIDTH 320
#define TOUCH_HEIGHT 240

// WLAN-Konfiguration
const char* ssid = "ESP32-CAM";
const char* password = "23571113";  // Die ersten 6 Primzahlen

// IP-Adresse der ESP32-CAM für 320x240 Auflösung:
const char* cameraIP = "192.168.4.1";  // ca. 4-5 frames/s

const int BLUE = 17;
const int GREEN = 16;
const int RED = 4;  // 17=blau, 16=grün, 4=rot

TFT_eSPI tft = TFT_eSPI();
TAMC_GT911 tp = TAMC_GT911(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, TOUCH_WIDTH, TOUCH_HEIGHT);

bool State = true;            // true Flash aus, false Flash ein
bool lastTouchState = false;  // Zum Entprellen des Touch-Signals

void setup() {
  Serial.begin(115200);
  pinMode(BLUE, OUTPUT);  // GPIO-Pin BLUE als Output
  pinMode(GREEN, OUTPUT);
  pinMode(RED, OUTPUT);

  digitalWrite(BLUE, HIGH);  // aktiv-low
  digitalWrite(GREEN, HIGH);
  digitalWrite(RED, LOW);

  // Display initialisieren
  tft.begin();
  tft.setRotation(1);  // Möglicherweise anpassen
  tft.fillScreen(TFT_BLACK);
  /*########################################################################*/
  tft.setSwapBytes(true);  //    jpg-Format-RGB(888) nach RGB(565)
  /*########################################################################*/
  // WLAN verbinden
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  digitalWrite(BLUE, HIGH);  // aktiv-low
  digitalWrite(GREEN, LOW);
  digitalWrite(RED, HIGH);

  // Touch-Controller initialisieren
  tp.begin();
}

void loop() {
  // Berührung auf Touchscreen prüfen:
  tp.read();
  if (tp.isTouched && !lastTouchState) {
    sendFlashCommand(State);
    State = !State;  // State toggeln
  }
  // Letzten Touch-Status speichern
  lastTouchState = tp.isTouched;

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String("http://") + cameraIP + "/cam-lo.jpg";
    http.begin(url);

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      int len = http.getSize();
      uint8_t* buffer = (uint8_t*)malloc(len);
      if (buffer) {
        WiFiClient* stream = http.getStreamPtr();
        int bytesRead = stream->readBytes(buffer, len);

        if (JpegDec.decodeArray(buffer, len)) {
          renderJPEG(0, 0);
        } else {
          Serial.println("JPEG decode failed");
        }

        free(buffer);
      } else {
        Serial.println("Memory allocation failed");
      }
    } else {
      Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  }
}

void renderJPEG(int xpos, int ypos) {
  uint16_t* pImg;
  uint16_t mcu_w = JpegDec.MCUWidth;
  uint16_t mcu_h = JpegDec.MCUHeight;
  uint32_t max_x = JpegDec.width;
  uint32_t max_y = JpegDec.height;

  while (JpegDec.read()) {
    pImg = JpegDec.pImage;
    int mcu_x = JpegDec.MCUx * mcu_w + xpos;
    int mcu_y = JpegDec.MCUy * mcu_h + ypos;

    if (mcu_x + mcu_w <= max_x && mcu_y + mcu_h <= max_y) {
      tft.pushImage(mcu_x, mcu_y, mcu_w, mcu_h, pImg);
    } else {
      uint32_t win_w = std::min(static_cast<uint32_t>(mcu_w), max_x - mcu_x);
      uint32_t win_h = std::min(static_cast<uint32_t>(mcu_h), max_y - mcu_y);
      tft.pushImage(mcu_x, mcu_y, win_w, win_h, pImg);
    }
  }
}

// Funktion zur Steuerung des Flash auf der ESP32-CAM
void sendFlashCommand(bool state) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String("http://") + cameraIP + "/control?var=flash&val=" + (state ? "1" : "0");
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      Serial.println("Flash command sent successfully");
    } else {
      Serial.printf("Flash command failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  }
}
