#include "SSD1306.h" // alias for `#include "SSD1306Wire.h"`
#include <WiFi.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <HTTPServer.hpp>
#include <HTTPRequest.hpp>
#include <HTTPResponse.hpp>

using namespace httpsserver;

// setup for OLED display
#define SDA    4
#define SCL   15
#define RST   16 //RST must be set by software
#define Vext  21
SSD1306  display(0x3c, SDA, SCL, RST);
String millisec;
String pmstr;
String lines[] = {"", "", "", "", "", ""};

int apmode = 1;

// setup for AP mode and web server
// Replace with your network credentials
String ssid     = "SmarterOpen AQ ";
String password = "SmarterOpen";
HTTPServer server = HTTPServer();
IPAddress ip(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);

void handleFile(HTTPRequest * req, HTTPResponse * res);

/**
   Standard Arduino setup function
*/
void setup() {

  // set up serial ports for debugging (serial)
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("Starting"));

  // initialize and set font for OLED display
  pinMode(RST, OUTPUT);
  digitalWrite(Vext, LOW);    // OLED USE Vext as power supply, must turn ON Vext before OLED init
  delay(50);
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);

  displayLine("Initializing");

  if (MDNS.begin("esp32")) {
    Serial.println("MDNS responder started");
  }

  SPIFFS.begin(true, "/data", 10);

  ResourceNode * nodeDefault = new ResourceNode("", "GET", &handleFile);
  server.setDefaultNode(nodeDefault);

  startAPMode();

}

void loop() {

  // don't run the LoRaWAN loop or read settings if in AP mode
  if (!apmode) {


  } else {
    server.loop();
  }

}

/**
   Adds a line to the bottom of the display, shifts other lines up (scrolling display)
*/
void displayLine(String line) {

  display.clear();
  for (int i = 0; i < 5; i++) {
    display.drawString(0, i * 10, lines[i + 1]);
    lines[i] = lines[i + 1];
  }
  display.drawString(0, 50, line);
  lines[5] = line;
  display.display();
}


void startAPMode() {
  displayLine("Starting AP");

  // append 4 digit random # to ssid and password
  int rnd = random(0, 9999);
  ssid += rnd;
  //password += rnd;
  const char* ssid_c = ssid.c_str();
  const char* password_c = password.c_str();

  // init the soft ap, broadcast ssid, only allow a single client
  WiFi.softAP(ssid_c, password.c_str(), 7, 0, 1);

  // we need to wait until SYSTEM_EVENT_AP_START has fired.  Delay is a crude hack...
  delay(500);
  displayLine("Set softAP Config");
  WiFi.softAPConfig(ip, ip, subnet);

  server.start();
}

void endAPMode() {
  server.stop();
  WiFi.softAPdisconnect(true);
}

/**
 * Handles a file request.  If the file is found, it will be served with sensible, 
 * minimal headers.  If not found, 404 page is sent
 */
void handleFile(HTTPRequest * req, HTTPResponse * res) {
  std::string reqString = req->getRequestString();

  static uint8_t buf[512];
  size_t len = 0;

  File file = SPIFFS.open(reqString.c_str());

  // if we can't find the requested file, send the 404 page
  if (!file) {
    file = SPIFFS.open("/404.html");
    res->setStatusCode(404);
  }else {
    res->setStatusCode(200);
  }
  
  len = file.size();
  
  res->setHeader("Content-Length", "" + len);
  res->setHeader("Content-Type", getContentType(reqString.c_str()));
  size_t flen = len;
  size_t i = 0;
  while (len) {
    size_t toRead = len;
    if (toRead > 512) {
      toRead = 512;
    }
    file.read(buf, toRead);
    len -= toRead;
    res->write(buf, toRead);
  }
}

/**
 * Naive content type detection by request extension.  
 */
char* getContentType(const char* reqstr) {
  if(strstr(reqstr, ".html") != NULL) {
    return "text/html";
  }else if (strstr(reqstr, ".css") != NULL) {
    return "text/css";
  }else if (strstr(reqstr, ".js") != NULL) {
    return "application/javascript";
  }else {
    return "application/octet-stream";
  }
}

void handleApiGet() {
  displayLine("Got api request (GET)");
  //server.send(200, "text/plain", "API endpoint expects POST");
}

void handleApiPost() {
  displayLine("Got api request (POST)");
}


