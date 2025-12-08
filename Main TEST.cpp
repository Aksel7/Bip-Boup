#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>

// Pinout (adjust to your wiring)
const uint8_t PIN_ENC_A = 2; // must be interrupt pin
const uint8_t PIN_ENC_B = 3;
const uint8_t PIN_ENC_BTN = 4; // encoder push
const uint8_t PIN_BACKSPACE = 5; // backspace button

// NRF24 pins
const uint8_t PIN_CE = 9;
const uint8_t PIN_CSN = 10;

RF24 radio(PIN_CE, PIN_CSN);
const uint64_t PIPE_ADDRESS = 0xF0F0F0F0E1LL;

// Character set including common French characters (UTF-8 literal strings)
const char* charset[] = {
  " ","a","b","c","d","e","f","g","h","i","j","k","l","m","n","o","p","q","r","s","t","u","v","w","x","y","z",
  "A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z",
  "0","1","2","3","4","5","6","7","8","9",
  ",",";",":","!","?",".","-","_","/","\\","@","#","$","%","&","*","( ",")",
  "é","è","ê","ë","à","â","ä","ù","û","ü","î","ï","ô","ö","ç","œ","æ","É","À","Ç"
};
const size_t CHARSET_LEN = sizeof(charset) / sizeof(charset[0]);

volatile int enc_pos = 0; // index into charset
volatile bool enc_moved = false;

String message = "";
uint8_t priority = 0; // 0..7

// Debounce / state
unsigned long lastBtnMillis = 0;
const unsigned long DEBOUNCE_MS = 50;

// Encoder ISR
void IRAM_ATTR handleEncoder() {
  bool b = digitalRead(PIN_ENC_B);
  if (b) enc_pos++;
  else enc_pos--;
  if (enc_pos < 0) enc_pos = CHARSET_LEN - 1;
  if (enc_pos >= (int)CHARSET_LEN) enc_pos = 0;
  enc_moved = true;
}

void sendMessageViaNRF(const String &msg, uint8_t prio) {
  // Fragment message in 28-byte chunks (reserve 4 bytes header)
  const char* data = msg.c_str();
  size_t len = msg.length();
  uint8_t chunkSize = 28;
  uint8_t total = (len + chunkSize - 1) / chunkSize;

  for (uint8_t seq = 0; seq < total; ++seq) {
    uint8_t packet[32];
    packet[0] = 0x01; // type: data
    packet[1] = seq;
    packet[2] = total;
    packet[3] = prio;
    size_t offset = seq * chunkSize;
    size_t thisLen = min((size_t)chunkSize, len - offset);
    memset(packet + 4, 0, chunkSize);
    memcpy(packet + 4, data + offset, thisLen);
    radio.write(packet, sizeof(packet));
    delay(10);
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { }
  Serial.println("Encoder -> message -> NRF24 demo");

  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_BTN, INPUT_PULLUP);
  pinMode(PIN_BACKSPACE, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), handleEncoder, CHANGE);

  // init radio
  if (!radio.begin()) {
    Serial.println("RF24 init failed");
  } else {
    radio.openWritingPipe(PIPE_ADDRESS);
    radio.setRetries(5, 15);
    radio.setPALevel(RF24_PA_LOW);
    radio.setDataRate(RF24_1MBPS);
    radio.enableDynamicPayloads();
    radio.setAutoAck(true);
    Serial.println("RF24 ready");
  }

  Serial.println("Usage:");
  Serial.println(" - Rotate encoder to select character");
  Serial.println(" - Press encoder (short) to append char");
  Serial.println(" - Press backspace button to delete last char");
  Serial.println(" - Hold encoder button (>1500ms) to SEND message (requires >=100 chars)");
  Serial.println(" - Hold backspace button (>1500ms) to enter priority-adjust mode (rotate to change, short press to confirm)");
}

bool inPriorityMode = false;
unsigned long btnDownStart = 0;

void loop() {
  // show current selection / message periodically
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 300) {
    lastPrint = millis();
    noInterrupts();
    int sel = enc_pos;
    bool moved = enc_moved;
    enc_moved = false;
    interrupts();

    Serial.print("Selected: ");
    Serial.print(charset[sel]);
    Serial.print("  | Message length: ");
    Serial.print(message.length());
    Serial.print("  | Priority: ");
    Serial.print(priority);
    if (inPriorityMode) Serial.print(" (PRIO MODE)");
    Serial.println();
    Serial.println("Message: ");
    Serial.println(message);
  }

  // encoder button handling
  int btnState = digitalRead(PIN_ENC_BTN);
  if (btnState == LOW) {
    if (btnDownStart == 0) btnDownStart = millis();
    unsigned long held = millis() - btnDownStart;
    if (held > 1500 && (!inPriorityMode)) {
      // long press -> send
      if (message.length() >= 100) {
        Serial.println("Sending message via NRF24...");
        sendMessageViaNRF(message, priority);
        Serial.println("Sent.");
        message = "";
      } else {
        Serial.println("Message too short (>=100 chars required)");
      }
      // wait until release
      while (digitalRead(PIN_ENC_BTN) == LOW) delay(10);
      btnDownStart = 0;
    }
  } else {
    if (btnDownStart != 0) {
      unsigned long held = millis() - btnDownStart;
      if (held < 1500) {
        // short press -> append selected char
        noInterrupts();
        int sel = enc_pos;
        interrupts();
        message += String(charset[sel]);
        Serial.print("Appended: "); Serial.println(charset[sel]);
      }
      btnDownStart = 0;
    }
  }

  // backspace button handling (short: delete last, long: priority mode)
  static unsigned long backDownStart = 0;
  int backState = digitalRead(PIN_BACKSPACE);
  if (backState == LOW) {
    if (backDownStart == 0) backDownStart = millis();
    unsigned long held = millis() - backDownStart;
    if (held > 1500 && !inPriorityMode) {
      inPriorityMode = true;
      Serial.println("Entering priority adjust mode. Rotate to change, press encoder to confirm.");
      // wait until release
      while (digitalRead(PIN_BACKSPACE) == LOW) delay(10);
      backDownStart = 0;
    }
  } else {
    if (backDownStart != 0) {
      unsigned long held = millis() - backDownStart;
      if (held < 1500 && !inPriorityMode) {
        // short press -> backspace
        if (message.length() > 0) {
          // remove last UTF-8 character safely
          int newLen = message.length();
          while (newLen > 0 && (message.charAt(newLen - 1) & 0xC0) == 0x80) {
            newLen--; // skip continuation bytes
          }
          if (newLen > 0) newLen--; // remove leading byte
          message = message.substring(0, newLen);
          Serial.println("Deleted last char");
        }
      }
      backDownStart = 0;
    }
  }

  // priority mode: rotate to change priority, encoder press to confirm
  if (inPriorityMode) {
    static int lastSel = -1;
    noInterrupts();
    int sel = enc_pos;
    interrupts();
    // map encoder position to small range for priority
    uint8_t newPrio = sel % 8; // 0..7
    if (newPrio != priority) {
      priority = newPrio;
      Serial.print("Priority set to "); Serial.println(priority);
      delay(150);
    }
    // confirm with encoder short press
    if (digitalRead(PIN_ENC_BTN) == LOW) {
      unsigned long t = millis();
      while (digitalRead(PIN_ENC_BTN) == LOW) {
        if (millis() - t > 500) break;
        delay(5);
      }
      inPriorityMode = false;
      Serial.print("Priority confirmed: "); Serial.println(priority);
      delay(200);
    }
  }

  delay(20);
}