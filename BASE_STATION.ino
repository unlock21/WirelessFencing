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
char channel[] = "11";
int port = 2390;
int status = WL_IDLE_STATUS;
char buffer[PACKET_SIZE];
char endBuffer[] = "E";
boolean gameRunning = false;
WiFiUDP Udp;
IPAddress rec1(192,168,1,2);
IPAddress rec2(192,168,1,3);
unsigned int timer;
unsigned int lockout_timer = 0;
unsigned int check_timer = 0;
char serialReceive;
int diff = 0;
char lasthit = ' ';
char hit_one_timer_buffer[3];
char hit_two_timer_buffer[3];
int hit_one_timer = 0;
int hit_two_timer = 0;
unsigned int lockout = 0; // 50 | 300
unsigned int wait = 0;
unsigned int abs_timer = 600;
char weaponType = 'P';
boolean checkUDP = false;
unsigned int checkUDP_wait = 3000; // 3ms

void sendReceiver(char num, char command = 'P', boolean verify = true, int tryCount = 0);
void checkReceiver(char num, char command, int tryCount);
void startGame(char weapon);
void endGame();
void endGame(char winner);
void readFlash();
void writeFlash(String NEW_SSID, String NEW_KEY);
void clearFlash();
void timer_tick(uint32_t data);
void lockout_tick(uint32_t data);
void check_tick(uint32_t data);

void setup() {
  bool startupContinue = false;
  bool noprompt = false; // CHANGE TO FALSE FOR WAITING
  
  Serial.begin(38400);
  while (!Serial) {}
  Serial.println("STARTUP ROUTINE INITIATED!!");

  mySerial.begin(38400);

  if (WiFi.status() == WL_NO_SHIELD) {}

  Serial.println("WAITING FOR APP INPUT!! (U OR S)"); // Setup / Fence
  while (!startupContinue) {
    if (mySerial.available() || noprompt) {
      serialReceive = (char)mySerial.read();
      if (serialReceive == 'U') { // SETUP
        while (mySerial.available()) { // TRASH EXTRA INPUT
          mySerial.read();
        }
        Serial.println("FLASH ROUTINE INITIATED!!");
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
      } else if (serialReceive == 'P' || serialReceive == 'F' || noprompt) { // FENCE // Epee / Foil
        readFlash();
        Serial.print("CURRENT SSID: ");
        Serial.println(SSID);
        Serial.print("CURRENT KEY: ");
        Serial.println(KEY);
        if (serialReceive == 'F') {
          lockout = 300;
          weaponType = 'F';
        } else if (serialReceive == 'P') {
          lockout = 50;
          weaponType = 'P';
        }
        wait = lockout+25;
        startupContinue = true;
      }
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
    status = WiFi.apbegin(ssid_tmp, key_tmp, channel);
  }
  
  Udp.begin(port);

  GTimer.begin(2, checkUDP_wait, checkUDP_tick);
    
  pinMode(10, OUTPUT); // Blue
  pinMode(11, OUTPUT); // Green
  pinMode(13, OUTPUT); // Red
  digitalWrite(13, HIGH); // Red Connection LED

  Serial.println("STARTUP ROUTINE COMPLETE!!");
}

void loop() { //  2-5ms
  if (lockout_timer >= wait) {
    GTimer.stop(1);
    Serial.print("KILLING GAME: ");
    Serial.println(lockout_timer);
    lockout_timer = 0;
    endGame(lasthit);
  }
  if (checkUDP) {
    checkUDP = false;
    if (Udp.parsePacket()) {
      Udp.read(buffer, PACKET_SIZE);
      
      Serial.println();
      Serial.print("BUFFER: ");
      Serial.println(buffer);
      Serial.print("TIMER: ");
      Serial.println(timer);
      
      if (buffer[0] == 'H' && gameRunning) {
        if (lockout_timer == 0) {
          GTimer.begin(1, 1000, lockout_tick);
          digitalWrite(10, HIGH);
          lasthit = buffer[1];
          if (buffer[1] == '1') {
            Serial.println("TEMP WINNER 1!!");
            mySerial.println("T1");
          } else if (buffer[1] == '2') {
            Serial.println("TEMP WINNER 2!!");
            mySerial.println("T2");
          }
          memcpy(hit_one_timer_buffer, buffer+10, 4);
          hit_one_timer = atoi(hit_one_timer_buffer) + lockout;
        } else if (buffer[1] != lasthit) {
          memcpy(hit_two_timer_buffer, buffer+10, 4);
          hit_two_timer = atoi(hit_two_timer_buffer);
          if (hit_one_timer-lockout > (abs_timer-wait) && hit_two_timer < (abs_timer-wait)) { // A2>H3-I3 && B2<H3-I3
            hit_two_timer += abs_timer; //B2+H3
          }
          if (hit_one_timer >= hit_two_timer) { // Hit
            GTimer.stop(1);
            lockout_timer = 0;
            endGame(buffer[1]);
          } else { // No Hit
            GTimer.stop(1);
            lockout_timer = 0;
            endGame(lasthit);
          }
        }
      } else if (buffer[0] == 'S' && !gameRunning) { // Alt Start Method
        startGame(weaponType);
      } else {
        //Serial.println("THROW AWAY!!");
      }
      memset(buffer, 0, sizeof buffer);
    }
  
    if (mySerial.available()) {
      serialReceive = (char)mySerial.read();
      Serial.print("FROM APP: ");
      Serial.println(serialReceive);
      if (serialReceive == 'S') { // Epee / Foil
        Serial.println("STARTING");
        startGame(weaponType);
      } else if (serialReceive == 'E') { // End Game
        Serial.println("FORCE END");
        digitalWrite(11, LOW);
        endGame();
      } 
      while(mySerial.available()) {
        mySerial.read();
        // Eat Extra Bits
      }
    }
  }
}

void sendReceiver(char num, char command, boolean verify, int tryCount) {
  if (tryCount > 4) {
    Serial.print("GIVING UP, CAN'T CONNECT TO REC");
    Serial.print(num);
    Serial.println("!!");
    gameRunning = false;
    return;
  }
  if (num == '1') {
    Udp.beginPacket(rec1, port);
  } else {
    Udp.beginPacket(rec2, port);
  }
  Udp.write(command);
  Udp.endPacket();
  if (verify) {
    //checkReceiver(num, command, tryCount);
  }
}

void checkReceiver(char num, char command, int tryCount) {
  Serial.print("Waiting for return back from ");
  Serial.println(num);
  
  lockout_timer = 0;
  GTimer.begin(2, 1000, check_tick);
  while (!Udp.parsePacket() && !(check_timer > 500)) {}
  GTimer.stop(2);
  check_timer = 0;
  
  Udp.read(buffer, PACKET_SIZE);
  if (buffer[0] == command && buffer[1] == num) {
    return;
  } else {
    sendReceiver(num, command, true, ++tryCount);
  }
}

//void startGame() {
//  digitalWrite(11, HIGH);
//  gameRunning = true;
//  timer = 0;
//  int start_timer_1 = 0, start_timer_2 = 0;
//  GTimer.begin(0, 1000, timer_tick);
//  sendReceiver('1', 'S');
//  Serial.println();
//  Serial.print("Started R1 At: ");
//  start_timer_1 = timer;
//  Serial.println(timer);
//  Serial.println();
//  sendReceiver('2', 'S');
//  Serial.println();
//  Serial.print("Started R2 At: ");
//  start_timer_2 = timer;
//  Serial.println(timer);
//  Serial.println();
//  Serial.println("READY!");
//  diff = start_timer_2 - start_timer_1;
//}

void startGame(char weapon) {
  digitalWrite(11, HIGH);
  gameRunning = true;
  timer = 0;
  int start_timer_1 = 0, start_timer_2 = 0;
  GTimer.begin(0, 1000, timer_tick);
  sendReceiver('1', weapon);
  if (gameRunning == true) {
    Serial.println();
    Serial.print("Started R1 At: ");
    start_timer_1 = timer;
    Serial.println(timer);
    Serial.println();
    sendReceiver('2', weapon);
    if (gameRunning  == true) {
      Serial.println();
      Serial.print("Started R2 At: ");
      start_timer_2 = timer;
      Serial.println(timer);
      Serial.println();
      Serial.println("READY!");
      diff = start_timer_2 - start_timer_1;
    }
  }
}

void endGame() {
  gameRunning = false;
  sendReceiver('1', 'E', false);
  Serial.println();
  Serial.print("Ended R1 At: ");
  Serial.println(timer);
  Serial.println();
  sendReceiver('2', 'E', false);
  Serial.println();
  Serial.print("Ended R2 At: ");
  Serial.println(timer);
  Serial.println();
}

void endGame(char winner) {
  Serial.println("GAME OVER!");
  endGame();
  digitalWrite(10, LOW);
  digitalWrite(11, LOW);
  if (winner == '1') {
    Serial.println("WINNER 1!");
    mySerial.println("T1");
  } else if (winner == '2') {
    Serial.println("WINNER 2!");
    mySerial.println("T2");
  } else {
    Serial.println("NO WINNER!");
    mySerial.println("T3");
  }
  Serial.print("END TIMER: ");
  Serial.println(timer);
  Serial.println("GAME COMPLETE!");
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
}

void checkUDP_tick(uint32_t data) {
  checkUDP = true;
}

void lockout_tick(uint32_t data) {
  lockout_timer++;
  if (lockout_timer >= wait) {
    gameRunning = false;
  }
}
