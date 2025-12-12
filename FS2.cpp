#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <RF24.h>

#define pinCE   7             
#define pinCSN  8            
#define tunnel  "PIPE1"       // nom channel choisie par USER
RF24 radio(pinCE, pinCSN);    // Instanciation du NRF24L01

const byte adresse[6] = tunnel;               // Mise au format "byte array" du nom du tunnel
const char message[100];     // Message à transmettre à l'autre NRF24 (32 caractères maxi, avec cette librairie)

void setup() {
  radio.begin();                      // Initialisation du module NRF24
  radio.openWritingPipe(adresse);     // Ouverture du tunnel en ÉCRITURE, avec le "nom" qu'on lui a donné
  radio.setPALevel(RF24 _PA_MIN);      // Sélection d'un niveau "MINIMAL" pour communiquer (pas besoin d'une forte puissance, pour nos essais)
  radio.stopListening();              // Arrêt de l'écoute du NRF24 (signifiant qu'on va émettre, et non recevoir, ici)
}

void loop() {
  radio.write(&message, sizeof(message));     // Envoi de notre message
  delay(1000);                                // … toutes les secondes !
}


/* 

void envoyerMessage() {
  strncpy(messageEnvoye.texte, messageEnCoursDeComposition, sizeof(messageEnvoye.texte) - 1);
  messageEnvoye.texte[sizeof(messageEnvoye.texte) - 1] = '\0'; // S'assurer que la chaîne est terminée
  messageEnvoye.priorite = prioriteMessage; // Utilise la priorité par défaut pour l'instant

  radio.write(&messageEnvoye, sizeof(messageEnvoye));

  // Réinitialiser pour un nouveau message
  memset(messageEnCoursDeComposition, 0, sizeof(messageEnCoursDeComposition));
  positionCurseurMessage = 0;
  indexCaractereSelectionne = 0;
  dernierIndexCaractereAffiche = 0;

  display.clearDisplay();
  display.setCursor(0,0);
  display.println(F("Message envoye!"));
  display.display();
  delay(1500);
  afficherSaisieMessageOLED(); // Revient à l'affichage de la saisie
}


*/