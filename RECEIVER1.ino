#include <WiFi.h>
#include <WiFiUdp.h>

char ssid[] = "STAMP";  // AP SSID
char pass[] = "123456789"; // AP Password
char channel[] = "11"; // AP Channel
int status = WL_IDLE_STATUS; // AP Status
unsigned int localPort = 2390; // UDP Server Port
char packetBuffer[255]; // RX Buffer
char ReplyBuffer[255]; // TX Buffer
bool isGameRunning = false;
bool DEBUG = false;
IPAddress REC1 = (192,168,1,2);
char start[] = "GAME START";

void hit_handler(uint32_t id, uint32_t event) {
  Serial.println("HIT!");
}

void setup () {


  // Setup Interupt
  // 12 = PIN
  pinMode(12, INPUT_IRQ_RISE);
  digitalSetIrqHandler(12, hit_handler);
}