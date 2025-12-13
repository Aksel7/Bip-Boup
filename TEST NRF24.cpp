/*
   DIAGNOSTIC NRF24
   But : Vérifier si le module est bien branché.
*/
#include <SPI.h>
#include <RF24.h>
#include <printf.h> // Nécessaire pour printDetails

#define PIN_CE  7
#define PIN_CSN 8

RF24 radio(PIN_CE, PIN_CSN);

void setup() {
  pinMode(10, OUTPUT);
  Serial.begin(9600);
  printf_begin(); // Initialise la fonction d'impression détaillée
  
  Serial.println("--- TEST DE CONNEXION RADIO ---");
  
  if (radio.begin()) {
    Serial.println("1. Radio.begin() : SUCCES !");
  } else {
    Serial.println("1. Radio.begin() : ECHEC (Verifiez le cablage !)");
  }

  if (radio.isChipConnected()) {
    Serial.println("2. Puce detectee : OUI");
  } else {
    Serial.println("2. Puce detectee : NON (Fil MISO/MOSI/SCK ou Alimentation HS)");
  }

  // Affiche les détails techniques internes
  radio.printDetails(); 
}

void loop() {
  // Rien à faire ici
}