/*
   DIAGNOSTIC NRF24
   But : Vérifier si le module est bien branché.
*/
#include <SPI.h>
#include <RF24.h>
#include <printf.h> // Nécessaire pour printDetails
// Correction: Les broches DOIVENT être des broches PWM (avec ~ sur l'Arduino Uno)
// Les broches 5, 6 et 9 SONT des broches PWM, donc elles sont bien choisies!
const int RED_PIN = 5;
const int GREEN_PIN = 6;
const int BLUE_PIN = 9;

void setup() {
  pinMode(10, OUTPUT);
  // Set the pins as outputs
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
}

// CORRIGÉ: Utilise analogWrite pour le contrôle de l'intensité (0-255)
void set_color(int red, int green, int blue) {
  analogWrite(RED_PIN, red);
  analogWrite(GREEN_PIN, green);
  analogWrite(BLUE_PIN, blue);
}

void loop() {
  // Turn on RED (Max intensity: 255)
  set_color(255, 0, 0);
  delay(3000);

  // Turn on GREEN
  set_color(0, 255, 0);
  delay(3000);

  // Turn on BLUE
  set_color(0, 0, 255);
  delay(3000);

  // Turn on YELLOW (Red + Green) - Fonctionne maintenant avec des nuances intermédiaires
  set_color(255, 255, 0);
  delay(1000);

  // ... (le reste de la boucle loop() est correct)
  set_color(255, 0, 255);
  delay(1000);

  set_color(0, 255, 255);
  delay(1000);

  set_color(255, 255, 255);
  delay(1000);

  set_color(0, 0, 0);
  delay(500);
}