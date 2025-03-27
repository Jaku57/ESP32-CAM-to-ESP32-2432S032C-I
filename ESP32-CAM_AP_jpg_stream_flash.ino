// Dieser Sketch sendet jpg-Bilder oder einen stream mit einer ESP32-CAM als Access Point.
// Durch Touch-Funktion des ESP32-2432S032C-I kann Flash ein- und ausgeschaltet werden.
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// Werkzeuge, Board: "AI Thinker ESP32-CAM" oder für ältere firmwares "ESP32 Dev Module"
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// Wenn "AI Thinker ESP32-CAM" nötig ist, funktioniert bei ESP32 Dev Module" nur loRes!!!
// Werkzeuge: CPU Frequency: "240 MHz (WiFi/BT)"!   wegen Framerate
// const char* WIFI_SSID = "ESP32-CAM";
// const char* WIFI_PASS = "23571113";
// IP-Adressen werden im seriellen Monitor angezeigt.
// 192.168.4.1/cam-lo.jpg     jpg-Bild mit 320x240 Pixel abrufen 
// 192.168.4.1/cam-hi.jpg     jpg-Bild mit 1280x720 Pixel abrufen
// 192.168.4.1/cam.mjpeg      stream (Video) mit 1280x720 Pixel abrufen
// http://192.168.4.1/control?val=1      // schaltet Flash-LED ein
// http://192.168.4.1/control?val=0      // schaltet Flash-LED aus

#include <WebServer.h>
#include <WiFi.h>
#include <esp32cam.h>   // https://github.com/yoursunny/esp32cam
// Den Ordner "esp32cam" nach ...Dokumente/Arduino/libraries kopieren! 

const char* ssid = "ESP32-CAM";
const char* password = "23571113";

#define FLASH_LED_PIN 4                 // Flash-LED-Pin (GPIO 4 für ESP32-CAM)

WebServer server(80);
static auto loRes = esp32cam::Resolution::find(320, 240);      // QVGA, 4:3
//static auto loRes = esp32cam::Resolution::find(640, 480);      // VGA, 4:3
//static auto hiRes = esp32cam::Resolution::find(768, 576);      // PAL, 4:3
//static auto hiRes = esp32cam::Resolution::find(800, 600);      // SVGA, 4:3
//static auto hiRes = esp32cam::Resolution::find(1024, 768);     // XGA, 4:3
static auto hiRes = esp32cam::Resolution::find(1280, 720);     // HD 720, 16:9
//static auto hiRes = esp32cam::Resolution::find(1600, 1200);    // UXGA, 4:3, ESP32-Cam max

// Funktion zum Flash-Steuern
void handleFlash() {
  if (server.hasArg("val")) {
    int val = server.arg("val").toInt();
    digitalWrite(FLASH_LED_PIN, val ? HIGH : LOW);         // Flash ein/aus
    server.send(200, "text/plain", val ? "Flash ON" : "Flash OFF");
    Serial.println("Flash empfangen!");
  } else {
    server.send(400, "text/plain", "Missing 'val' parameter");
  }
}

void serveJpg() {
  auto frame = esp32cam::capture();
  if (frame == nullptr) {
    Serial.println("CAPTURE FAIL");
    server.send(503, "", "");
    return;
  }
  server.setContentLength(frame->size());
  server.send(200, "image/jpeg");
  WiFiClient client = server.client();
  frame->writeTo(client);
}

void handleJpgLo() {
  if (!esp32cam::Camera.changeResolution(loRes)) {
    Serial.println("SET-LO-RES FAIL");
  }
  server.on("/control", handleFlash);            // Flash-Steuerung       eingefügt!
  serveJpg();
}

void handleJpgHi() {
  if (!esp32cam::Camera.changeResolution(hiRes)) {
    Serial.println("SET-HI-RES FAIL");
  }
  serveJpg();
}

void handleMjpeg() {
  if (!esp32cam::Camera.changeResolution(hiRes)) {
    Serial.println("SET-HI-RES FAIL");
  }
  Serial.println("STREAM BEGIN");
  WiFiClient client = server.client();
  int res = esp32cam::Camera.streamMjpeg(client);
  if (res <= 0) {
    Serial.printf("STREAM ERROR %d\n", res);
    return;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW); // Standardmäßig aus

  {
    using namespace esp32cam;
    Config cfg;
    cfg.setPins(pins::AiThinker);
    cfg.setResolution(hiRes);
    cfg.setBufferCount(2);
    cfg.setJpeg(80);
    bool ok = Camera.begin(cfg);
    Serial.println(ok ? "CAMERA OK" : "CAMERA FAIL");
  }

  // Access Point starten
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP Adresse: ");
  Serial.println(IP);
  server.begin();

  Serial.println();                 // ln=neue Zeile danach!
  Serial.println("192.168.4.1/cam-lo.jpg");
  Serial.println("192.168.4.1/cam-hi.jpg");
  Serial.println("192.168.4.1/cam.mjpeg");
  Serial.println();

  server.on("/cam-lo.jpg", handleJpgLo);
  server.on("/cam-hi.jpg", handleJpgHi);
  server.on("/cam.mjpeg", handleMjpeg);
}

void loop() {
  server.handleClient();
}
