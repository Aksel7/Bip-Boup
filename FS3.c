#include <SPI.h>
#include <RF24.h>
#include <EEPROM.h>

// ---------------- CONFIGURATION ----------------
#define CE_PIN 7
#define CSN_PIN 8

#define BUZZER_PIN 10
#define STOP_BTN_PIN 2

// LED RGB (PWM)
#define LED_R 5
#define LED_G 6
#define LED_B 9

// Adresse radio (DOIT être la même que FS2)
const byte adresse[6] = "PIPE1";

// Adresse EEPROM (la même que FS5)
const int addrCanal = 0;

// ---------------- RADIO ----------------
RF24 radio(CE_PIN, CSN_PIN);

// ---------------- STRUCTURE MESSAGE ----------------
struct Message {
  char pseudo[20];
  char texte[100];
  int priorite; // 1 = faible, 2 = moyenne, 3 = haute
};

Message msg;
bool message_recu = false;

// ---------------- LED SELON PRIORITÉ ----------------
void setLED(int prio) {
  switch (prio) {
    case 1: // Vert
      analogWrite(LED_R, 0);
      analogWrite(LED_G, 255);
      analogWrite(LED_B, 0);
      break;
    case 2: // Jaune
      analogWrite(LED_R, 255);
      analogWrite(LED_G, 255);
      analogWrite(LED_B, 0);
      break;
    case 3: // Rouge
      analogWrite(LED_R, 255);
      analogWrite(LED_G, 0);
      analogWrite(LED_B, 0);
      break;
    default: // Éteint
      analogWrite(LED_R, 0);
      analogWrite(LED_G, 0);
      analogWrite(LED_B, 0);
  }
}

// ---------------- SETUP ----------------
void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(STOP_BTN_PIN, INPUT_PULLUP);

  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);

  // Lecture du canal choisi dans FS5
  int canal = EEPROM.read(addrCanal);
  if (canal < 0 || canal > 125) canal = 0;

  radio.begin();
  radio.setChannel(canal);               // 🔥 CANAL FS5
  radio.openReadingPipe(1, adresse);     // même adresse que l’émetteur
  radio.startListening();
}

// ---------------- LOOP ----------------
void loop() {

  // Réception message
  if (radio.available()) {
    radio.read(&msg, sizeof(msg));
    message_recu = true;
  }

  // Traitement message
  if (message_recu) {
    setLED(msg.priorite);
    digitalWrite(BUZZER_PIN, HIGH);
    message_recu = false;
  }

  // Arrêt alerte par bouton
  if (digitalRead(STOP_BTN_PIN) == LOW) {
    digitalWrite(BUZZER_PIN, LOW);
    setLED(0);
    delay(300); // anti-rebond simple
  }
}
