#include <SPI.h>
#include <RF24.h>
#include <Arduino.h>

// NRF24L01 pins
#define CE_PIN 7
#define CSN_PIN 8
RF24 radio(CE_PIN, CSN_PIN);

// Buzzer
#define BUZZER_PIN 10

// LED RGB
#define RED_PIN 5
#define GREEN_PIN 6
#define BLUE_PIN 9

// Bouton d'arrêt
#define BUTTON_PIN 2

// Variables
struct Message {
  char text[100];
  int prio; // 1=Urgent, 2=Normal, 3=Basse
};
Message msg;

void setup() {
  Serial.begin(9600);

  // Initialisation NRF24
  radio.begin();
  delay(200);
  radio.openReadingPipe(1, 0xF0F0F0F0E1LL);
  radio.startListening();

  // Initialisation pins
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void loop() {
  // Vérifier la réception d'un message
  if (radio.available()) {
    radio.read(&msg, sizeof(msg));
    Serial.println(msg.text);

    // Allumer LED RGB selon la priorité
    setLED(msg.prio);

    // Activer buzzer
    // digitalWrite(BUZZER_PIN, HIGH); // 
    PROBLEME ICI , IL FAUT CREER UNE FONCTION ALERTE QUI VA FAIRE SONNER LE BUZZER ET CLIGNOTER LES LEDS
  }

  // Vérifier si bouton appuyé pour arrêter les alertes
  if (digitalRead(BUTTON_PIN) == LOW) {
    stopAlerts();
  }
}

// Fonction pour régler LED selon priorité
void setLED(int prio) {
  switch(prio) {
    case 1: // Urgent
      analogWrite(RED_PIN, 255);
      analogWrite(GREEN_PIN, 0);
      analogWrite(BLUE_PIN, 0);
      break;
    case 2: // Normal
      analogWrite(RED_PIN, 255);
      analogWrite(GREEN_PIN, 255);
      analogWrite(BLUE_PIN, 0);
      break;
    case 3: // Basse
      analogWrite(RED_PIN, 0);
      analogWrite(GREEN_PIN, 0);
      analogWrite(BLUE_PIN, 255);
      break;
  }
}

// Fonction pour arrêter les alertes
void stopAlerts() {
  digitalWrite(BUZZER_PIN, LOW);
  analogWrite(RED_PIN, 0);
  analogWrite(GREEN_PIN, 0);
  analogWrite(BLUE_PIN, 0);
}