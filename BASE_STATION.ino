#include <WiFi.h>
#include <WiFiUdp.h>
#include <SoftwareSerial.h>

#if defined(BOARD_RTL8710)
SoftwareSerial mySerial(17, 5); // RX, TX1
#else
SoftwareSerial mySerial(0, 1); // RX, TX
#endif

char ssid[] = "STAMP";  // AP SSID
char pass[] = "123456789"; // AP Password
char channel[] = "11"; // AP Channel
int status = WL_IDLE_STATUS; // AP Status
unsigned int localPort = 2390; // UDP Server Port
WiFiUDP UDP; // UDP Server Instance
char packetBuffer[255]; // RX Buffer
char ReplyBuffer[255]; // TX Buffer
bool isGameRunning = false;
bool DEBUG = false;
IPAddress REC1 = (192,168,1,2);
IPAddress REC2 = (192,168,1,3);
char start[] = "GAME START";

void setup() {
  int loopCount = 0;

  
  //Initialize Serial
  Serial.begin(9600); // May Require 38400 BAUD
  while (!Serial) {}

  // Start AP
  while (status != WL_CONNECTED) {
    if (loopCount < 5) {
      Serial.print("STARTING! ATTEMPT: ");
      Serial.println(loopCount);
      status = WiFi.apbegin(ssid, pass, channel);
      delay(10000);
      loopCount++;
    } else {
      Serial.print("FAILED TO START AP IN 5 TRIES! PLEASE RESTART TO TRY AGAIN!");
      while (true) {}
    }
  }

  //AP MODE is Set:
  Serial.print("AP STARTED! ATTEMPT: ");
  Serial.println(loopCount);
  printWifiData();
  printCurrentNet();

  // Start UDP Server
  UDP.begin(localPort);

  // Start Serial Communication
  mySerial.begin(4800);
  mySerial.println("Hello, world?");
}

void loop() {
  // Check for Packet
  int packetSize = UDP.parsePacket();
  if (packetSize) {
    if (DEBUG) {
      Serial.print("Received packet of size ");
      Serial.println(packetSize);
      Serial.print("From ");
      IPAddress remoteIp = UDP.remoteIP();
      Serial.print(remoteIp);
      Serial.print(", port ");
      Serial.println(UDP.remotePort());
    }
    // Read Packet
    int len = UDP.read(packetBuffer, 255);
    if (len > 0) {
      packetBuffer[len] = 0;
    }
    if (DEBUG) {
      Serial.println("Contents:");
      Serial.println(packetBuffer);
    }
    // Replay
    UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
    UDP.write(ReplyBuffer);
    UDP.endPacket();
  }
  if (mySerial.available()) {
    mySerial.write(mySerial.read());
  }
}

void printWifiData() {
  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print your subnet mask:
  IPAddress subnet = WiFi.subnetMask();
  Serial.print("NetMask: ");
  Serial.println(subnet);

  // print your gateway address:
  IPAddress gateway = WiFi.gatewayIP();
  Serial.print("Gateway: ");
  Serial.println(gateway);
  Serial.println();
}

void printCurrentNet() {
  // print the SSID of the AP:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print the encryption type:
  byte encryption = WiFi.encryptionType();
  Serial.print("Encryption Type:");
  Serial.println(encryption, HEX);
  Serial.println();
}

void startGame() {

}

void 
