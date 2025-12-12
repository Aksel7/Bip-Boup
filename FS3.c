#include <RF24.h>
#include <stdio.h>

#define CE_PIN 7
#define CSN_PIN 8
#define BUZZER_PIN 10
#define STOP_BTN_PIN 2   // bouton pour arrêter les alertes

RF24 radio(CE_PIN, CSN_PIN);

// Structure du message
struct Message {
  char pseudo[20];
  char text[100];
  int prio; // 1 = faible, 2 = moyenne, 3 = haute
};
Message msg;

int message_recu = 0; //  pour savoir qu'un message est arrivé

// --- Fonction pour allumer la LED selon la priorité ---
void setLED(int prio) {
  switch(prio) {
    case 1: // faible → vert
      analogWrite(5, 0);   // rouge
      analogWrite(6, 255); // vert
      analogWrite(9, 0);   // bleu
      break;
    case 2: // moyenne → jaune
      analogWrite(5, 255); // rouge
      analogWrite(6, 255); // vert
      analogWrite(9, 0);   // bleu
      break;
    case 3: // haute → rouge
      analogWrite(5, 255); // rouge
      analogWrite(6, 0);   // vert
      analogWrite(9, 0);   // bleu
      break;
    default: // éteinte
      analogWrite(5, 0);
      analogWrite(6, 0);
      analogWrite(9, 0);
      break;
  }
}

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(STOP_BTN_PIN, INPUT_PULLUP); // bouton avec résistance pull-up
  radio.begin();
  radio.openReadingPipe(1, 0xF0F0F0F0E1LL);
  radio.startListening();
}

void loop() {
  // --- Détection du message ---
  if (radio.available()) {
    radio.read(&msg, sizeof(msg));
    message_recu = 1;
  }

  // --- Traitement du message ---
  if (message_recu == 1) {
    setLED(msg.prio);           // allume la LED selon priorité
    digitalWrite(BUZZER_PIN, HIGH); // allume le buzzer
    message_recu = 0;               // réinitialise 
  }

  // --- Arrêt des alertes si l'utilisateur appuie sur le bouton ---
  if (digitalRead(STOP_BTN_PIN) == LOW) { // bouton pressé
    digitalWrite(BUZZER_PIN, LOW);        // éteint le buzzer
    setLED(0);                             // éteint la LED
  }
}
