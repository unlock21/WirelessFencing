#include <WiFi.h>
#include <WiFiUdp.h>
#include <GTimer.h>
#include <FlashMemory.h>
#include <SoftwareSerial.h>

#define PACKET_SIZE 20
#if defined(BOARD_RTL8195A)
SoftwareSerial mySerial(0, 1); // RX, TX
#elif defined(BOARD_RTL8710)
SoftwareSerial mySerial(17, 5); // RX, TX
#else
SoftwareSerial mySerial(0, 1); // RX, TX
#endif

String SSID = "";
String KEY = "";
char ID = '1';
int port = 2390;
int status = WL_IDLE_STATUS;
char hit[5];
char buffer[PACKET_SIZE];
WiFiUDP Udp;
boolean gameRunning = false;
IPAddress base(192,168,1,1);
unsigned int timer = 0;
char serialReceive;
char weapon = 'P';
boolean checkUDP = false;
boolean scoring = false;

void hitGame();
void startGame();
void endGame();
void readFlash();
void writeFlash(String NEW_SSID, String NEW_KEY);
void clearFlash();
void timer_tick(uint32_t data);
void check_tick(uint32_t data);
void hit_handler(uint32_t id, uint32_t event);

void setup() {
  bool startupContinue = false;
  bool noprompt = false; // CHANGE TO FALSE FOR WAITING
  
  Serial.begin(38400);
  while (!Serial) {}
  Serial.println("STARTUP ROUTINE INITIATED!!");

  mySerial.begin(38400);
  
  if (WiFi.status() == WL_NO_SHIELD) {}

  Serial.println("WAITING FOR APP INPUT OR BUTTON PRESS!! (U)");
  
  pinMode(2, INPUT); // GA5 / Connect Button
  while (!startupContinue) {
    if (mySerial.available() || noprompt) {
      serialReceive = (char)mySerial.read();
      if (serialReceive == 'U') { // SETUP
        while (mySerial.available()) { // TRASH EXTRA INPUT
          mySerial.read();
        }
        Serial.println("FLASH WRITE ROUTINE INITIATED!!");
        readFlash();
        Serial.print("CURRENT SSID: ");
        Serial.println(SSID);
        Serial.print("CURRENT KEY: ");
        Serial.println(KEY);
        SSID = "";
        KEY = "";
        while (!mySerial.available()) {} // WAIT FOR SERIAL
        while (mySerial.available()) {
          serialReceive = mySerial.read();
          if (serialReceive > 31 && serialReceive < 127) {
            SSID += serialReceive;
          }
        }
        while (!mySerial.available()) {} // WAIT FOR SERIAL
        while (mySerial.available()) {
          serialReceive = mySerial.read();
          if (serialReceive > 31 && serialReceive < 127) {
            KEY += serialReceive;
          }
        }
        Serial.println("NEW SSID AND KEY ACCEPTED!!");
        clearFlash();
        writeFlash(SSID, KEY);
        SSID = "";
        KEY = "";
        readFlash();
        Serial.print("NEW SSID: ");
        Serial.println(SSID);
        Serial.print("NEW KEY: ");
        Serial.println(KEY);
        startupContinue = true;
      }
    }
    if (digitalRead(2) == HIGH) {
      readFlash();
      Serial.print("CURRENT SSID: ");
      Serial.println(SSID);
      Serial.print("CURRENT KEY: ");
      Serial.println(KEY);
      startupContinue = true;
    }
  }
  Serial.println("FLASH ROUTINE COMPLETE!!");

  char ssid_tmp[SSID.length()+1], key_tmp[KEY.length()+1];
  SSID.toCharArray(ssid_tmp, SSID.length()+1);
  KEY.toCharArray(key_tmp, KEY.length()+1);
  Serial.print("RUNNING SSID: ");
  Serial.println(ssid_tmp);
  Serial.print("RUNNING KEY: ");
  Serial.println(key_tmp);

  while (status != WL_CONNECTED) {
    status = WiFi.begin(ssid_tmp, key_tmp);
  }
  
  Udp.begin(port);

  pinMode(10, OUTPUT); // Buzzer/Blue
  pinMode(11, OUTPUT); // Green
  pinMode(13, OUTPUT); // Red
  digitalWrite(13, HIGH); // Red Connection LED
  Serial.println("STARTUP ROUTINE COMPLETE!!");

  do {
    while (!Udp.parsePacket()) {
      if (WiFi.status() != WL_CONNECTED) {
        digitalWrite(13, LOW);
        endGame();
        WiFi.disconnect();
        char ssid_tmp[SSID.length()+1], key_tmp[KEY.length()+1];
        SSID.toCharArray(ssid_tmp, SSID.length()+1);
        KEY.toCharArray(key_tmp, KEY.length()+1);
        status = WL_IDLE_STATUS;
        while (status != WL_CONNECTED) {
          status = WiFi.begin(ssid_tmp, key_tmp);
        }
        digitalWrite(13, HIGH);
      }
    }
    Udp.read(buffer, PACKET_SIZE);
  } while (buffer[0] == 'E');
  Serial.print("START TYPE: ");
  Serial.println(buffer);
  if (buffer[0] == 'P') { // Epee
    weapon = 'P';
  } else if (buffer[0] == 'F') { // Foil
    weapon = 'F';
  }
  Serial.print("WEAPON: ");
  Serial.println(weapon);
  pinMode(12, INPUT);
  startGame();
  memset(buffer, 0, sizeof buffer); // Clear Buffer
  GTimer.begin(1, 1000000, check_tick);
}

void loop() {
  if (checkUDP || !gameRunning) {
    if (Udp.available()) {
      Serial.println("GOT PACKET!");
      Udp.parsePacket();
      Udp.read(buffer, PACKET_SIZE);
      Serial.println(buffer);
      if (buffer[0] == 'P' || buffer[0] == 'F') {
        startGame();
      } else if (buffer[0] == 'E') {
        endGame();
      }
      memset(buffer, 0, sizeof buffer); // Clear Buffer
    }
    if (WiFi.status() != WL_CONNECTED) {
      digitalWrite(13, LOW);
      endGame();
      WiFi.disconnect();
      char ssid_tmp[SSID.length()+1], key_tmp[KEY.length()+1];
      SSID.toCharArray(ssid_tmp, SSID.length()+1);
      KEY.toCharArray(key_tmp, KEY.length()+1);
      status = WL_IDLE_STATUS;
      while (status != WL_CONNECTED) {
        status = WiFi.begin(ssid_tmp, key_tmp);
      }
      digitalWrite(13, HIGH);
    }
    checkUDP = false;
  }

  if ((digitalRead(12) == HIGH) && gameRunning && !scoring) {
    scoring = true;
    delay(2);
    if (digitalRead(12) == HIGH) {
      hitGame();  
    }
    scoring = false;
  }
}

void hitGame() {
  gameRunning = false;
  String send;
  digitalWrite(10, HIGH); // Blue
  digitalWrite(11, LOW); // Green
  Udp.beginPacket(base, port);
  send = String("H") + String(ID) + timer;
  send.toCharArray(hit, 6);
  Udp.write(hit);
  Udp.endPacket();
  Serial.print("TIMER: ");
  Serial.println(timer);
  memset(hit, 0, sizeof buffer);
  digitalWrite(10, LOW);
}

void startGame() {
  String send;
  char sendit[2];
  Udp.beginPacket(base, port);
  send = weapon + String(ID);
  send.toCharArray(sendit, 3);
  Udp.write(sendit);
  Udp.endPacket();
  digitalWrite(11, HIGH); // Green
  gameRunning = true;
  Serial.println("STARTING GAME!!");
  timer = 0;
  GTimer.begin(0, 1000, timer_tick);
}

void endGame() {
  digitalWrite(11, LOW); // Green
  GTimer.stop(0);
  gameRunning = false;
  Serial.println("ENDING GAME!");
  Serial.print("END TIMER: ");
  Serial.println(timer);
  Serial.println("End Complete!!");
}

void readFlash() {
  unsigned int i = 0;
  // READ CURRENT SSID + KEY
  FlashMemory.read();
  while (FlashMemory.buf[i] != '\0') {
    SSID += FlashMemory.buf[i++];
  }
  i++;
  while (FlashMemory.buf[i] != '\0') {
    KEY += FlashMemory.buf[i++];
  }
  SSID = String(SSID);
  KEY = String(KEY);
  Serial.println("READ ROUTINE COMPLETE!!");
}

void writeFlash(String NEW_SSID, String NEW_KEY) {
  // WRITE NEW SSID + KEY TO BUFFER
  unsigned int i = 0;
  FlashMemory.read();
  for (unsigned int j = 0; j < (NEW_SSID.length()+1); j++) {
    FlashMemory.buf[i++] = NEW_SSID.c_str()[j];
  }
  for (unsigned int j = 0; j < (NEW_KEY.length()+1); j++) {
    FlashMemory.buf[i++] = NEW_KEY.c_str()[j];
  }
  FlashMemory.update();
  Serial.println("WRITE ROUTINE COMPLETE!!");
}

void clearFlash() {
  // CLEAR SSID + KEY BUFFER
  unsigned int i = 0;
  FlashMemory.read();
  while (FlashMemory.buf[i] != '\0') {
    FlashMemory.buf[i++] = 0x00;
  }
  FlashMemory.buf[i++] = 0x00;
  while (FlashMemory.buf[i] != '\0') {
    FlashMemory.buf[i++] = 0x00;
  }
  FlashMemory.buf[i++] = 0x00;
  FlashMemory.update();
  Serial.println("CLEAR ROUTINE COMPLETE!!");
}

void timer_tick(uint32_t data) {
  timer++;
  if (timer > 599) {
    timer= 0;
  }
}

void check_tick(uint32_t data) {
  checkUDP = true;
}

void hit_handler(uint32_t id, uint32_t event) {
  //hitGame();
}
