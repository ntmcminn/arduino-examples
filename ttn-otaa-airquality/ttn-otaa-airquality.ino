#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include "SSD1306.h" // alias for `#include "SSD1306Wire.h"`
#include <PMS.h>

// pins for OLED
#define SDA    4
#define SCL   15
#define RST   16 //RST must be set by software
#define Vext  21
SSD1306  display(0x3c, SDA, SCL, RST);
String millisec;
String pmstr;
String lines[] = {"", "", "", "", "", ""};

// setup for PMS sensor (Plantower)
HardwareSerial PMSSerial(2);
PMS pms(PMSSerial);
PMS::DATA data;

// This EUI must be in little-endian format, so least-significant-byte
// first. When copying an EUI from ttnctl output, this means to reverse
// the bytes. For TTN issued EUIs the last bytes should be 0xD5, 0xB3,
// 0x70.
static const u1_t PROGMEM APPEUI[8]={ INSERT YOUR ASPPEUI HERE };
void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8);}

// This should also be in little endian format, see above.
static const u1_t PROGMEM DEVEUI[8]={ INSERT YOUR DEVEUI HERE };
void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8);}

// This key should be in big endian format (or, since it is not really a
// number but a block of memory, endianness does not really apply). In
// practice, a key taken from ttnctl can be copied as-is.
// The key shown here is the semtech default key.
static const u1_t PROGMEM APPKEY[16] = { INSERT YOUR APP KEY HERE };
void os_getDevKey (u1_t* buf) {  memcpy_P(buf, APPKEY, 16);}

static uint8_t payload[] = "-1";
static osjob_t sendjob;

// Schedule TX every this many seconds (might become longer due to duty
// cycle limitations).
const unsigned TX_INTERVAL = 60;

int txcomplete = 0;

// Pin mapping
const lmic_pinmap lmic_pins = {
  .nss = 18,
  .rxtx = LMIC_UNUSED_PIN,
  .rst = 14,
  .dio = {26, 33, 32},
};

struct pm
{
  uint16_t pm1_0;
  uint16_t pm2_5;
  uint16_t pm10_0;
};

void onEvent (ev_t ev) {
    Serial.print(os_getTime());
    Serial.print(": ");
    switch(ev) {
        case EV_SCAN_TIMEOUT:
            Serial.println(F("EV_SCAN_TIMEOUT"));
            break;
        case EV_BEACON_FOUND:
            Serial.println(F("EV_BEACON_FOUND"));
            break;
        case EV_BEACON_MISSED:
            Serial.println(F("EV_BEACON_MISSED"));
            break;
        case EV_BEACON_TRACKED:
            Serial.println(F("EV_BEACON_TRACKED"));
            break;
        case EV_JOINING:
            Serial.println(F("EV_JOINING"));
            break;
        case EV_JOINED:
            Serial.println(F("EV_JOINED"));

            // Disable link check validation (automatically enabled
            // during join, but not supported by TTN at this time).
            LMIC_setLinkCheckMode(0);
            //displayLine("Join completed");
            break;
        case EV_RFU1:
            Serial.println(F("EV_RFU1"));
            break;
        case EV_JOIN_FAILED:
            Serial.println(F("EV_JOIN_FAILED"));
            //displayLine("Join failed");
            break;
        case EV_REJOIN_FAILED:
            Serial.println(F("EV_REJOIN_FAILED"));
            break;
            break;
        case EV_TXCOMPLETE:
            Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
            if (LMIC.txrxFlags & TXRX_ACK)
              Serial.println(F("Received ack"));
            if (LMIC.dataLen) {
              Serial.println(F("Received "));
              Serial.println(LMIC.dataLen);
              Serial.println(F(" bytes of payload"));
            }
            // Schedule next transmission
            //os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL), do_send);
            txcomplete = 1;
            break;
        case EV_LOST_TSYNC:
            Serial.println(F("EV_LOST_TSYNC"));
            break;
        case EV_RESET:
            Serial.println(F("EV_RESET"));
            break;
        case EV_RXCOMPLETE:
            // data received in ping slot
            Serial.println(F("EV_RXCOMPLETE"));
            break;
        case EV_LINK_DEAD:
            Serial.println(F("EV_LINK_DEAD"));
            break;
        case EV_LINK_ALIVE:
            Serial.println(F("EV_LINK_ALIVE"));
            break;
         default:
            Serial.println(F("Unknown event"));
            break;
    }
}

void do_send(osjob_t* j){
    // Check if there is not a current TX/RX job running
    if (LMIC.opmode & OP_TXRXPEND) {
        Serial.println(F("OP_TXRXPEND, not sending"));
    } else {
        // Prepare upstream data transmission at the next possible time.
        LMIC_setTxData2(1, payload, sizeof(payload)-1, 0);
        Serial.println(F("Packet queued"));
    }
    // Next TX is scheduled after TX_COMPLETE event.
}

void setup() {
    PMSSerial.begin(9600, SERIAL_8N1, 36, 37);
    Serial.begin(115200);
    delay(1000);
    Serial.println(F("Starting"));

    pinMode(RST,OUTPUT);
    digitalWrite(Vext, LOW);    // OLED USE Vext as power supply, must turn ON Vext before OLED init
    delay(50);
    display.init();
    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);
  
    #ifdef VCC_ENABLE
    // For Pinoccio Scout boards
    pinMode(VCC_ENABLE, OUTPUT);
    digitalWrite(VCC_ENABLE, HIGH);
    delay(1000);
    #endif

    SPI.begin(5, 19, 27);

    displayLine("Initializing");
    
    // LMIC init
    os_init();
    // Reset the MAC state. Session and pending data transfers will be discarded.
    LMIC_reset();

    do_send(&sendjob);
    
}

void loop() {

    os_runloop_once();

    if(txcomplete == 1) {
      millisec = String(millis());
      displayLine(millisec);
      displayLine("Transmission complete");
      Serial.println("tx complete=1, delaying");
      
      delay(60000);

      // insert sensor readings here in the loop, read, then queue up the readings to be sent.
      struct pm pmdata = getPM();
      payload[0]=pmdata.pm2_5 & 0xff;
      payload[1]=(pmdata.pm2_5 >> 8);
      
      txcomplete = 0;
      // Start job (sending automatically starts OTAA too)
      displayLine("Sending job");
      Serial.println("sending job");
      do_send(&sendjob);
    }
}

/**
 * Adds a line to the bottom of the display, shifts other lines up (scrolling display)
 */
void displayLine(String line) {

   display.clear();
   for(int i = 0; i < 5; i++) {
    display.drawString(0, i * 10, lines[i+1]);
    lines[i] = lines[i+1];
   }
   display.drawString(0, 50, line);
   lines[5] = line;
   display.display();
}

/**
 * Reads from Plantower *003 sensor, returns struct with all of the read values.
 */
struct pm getPM() {

  struct pm pm_instance;
  int read = 0;

  // TODO - add a timeout here so that the read can't run forever.
  while(!read) {
    if (pms.read(data)){
        
       pm_instance.pm1_0 = data.PM_AE_UG_1_0;
       pm_instance.pm2_5 = data.PM_AE_UG_2_5;
       pm_instance.pm10_0 = data.PM_AE_UG_10_0;
       read = 1;
    }
  }

  pmstr = String(pm_instance.pm2_5);
  displayLine(pmstr);
  return pm_instance;
}

