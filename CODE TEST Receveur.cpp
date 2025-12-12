/*
    PROJET : Récepteur NRF24 - Version OLED
    BUT    : Recevoir Pseudo et Message et les afficher proprement sur l'écran.
*/

#include <SPI.h>
#include <RF24.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>

// --- CONFIGURATION ---
// Radio
#define PIN_CE  7
#define PIN_CSN 8
RF24 radio(PIN_CE, PIN_CSN);
const byte adresse[6] = "PIPE1";

// Ecran OLED
Adafruit_SSD1306 display(128, 64, &Wire, -1);

void setup() {
  // 1. Démarrage Ecran
  // Si l'écran reste noir, essaie 0x3D au lieu de 0x3C
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); 
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  
  // Message de bienvenue
  display.setCursor(0, 0);
  display.println("Recepteur Pret...");
  display.println("En attente radio");
  display.display();

  // 2. Démarrage Radio
  radio.begin();
  radio.openReadingPipe(0, adresse);
  radio.setPALevel(RF24_PA_MIN);
  radio.startListening(); // Mode écoute activé
}

void loop() {
  if (radio.available()) {
    
    // 1. On récupère le paquet
    char paquet[32] = "";
    radio.read(&paquet, sizeof(paquet));
    
    char type = paquet[0]; // 'P' ou 'M'

    // 2. GESTION DE L'AFFICHAGE

    // CAS 1 : C'est le PSEUDO (Début de transmission)
    if (type == 'P') {
      display.clearDisplay(); // On efface tout pour le nouveau message
      display.setCursor(0, 0);
      
      display.print("DE: "); // "De la part de :"
      afficher_paquet_sur_oled(paquet); // Affiche le nom
      
      display.println();        // Saut de ligne
      display.println("-----"); // Petite barre de séparation
      // Le curseur est maintenant prêt en dessous pour le message
    }
    
    // CAS 2 : C'est le MESSAGE (Suite du texte)
    else if (type == 'M') {
      // On continue d'écrire là où le curseur s'est arrêté
      afficher_paquet_sur_oled(paquet);
    }

    // 3. On met à jour l'écran pour que le texte apparaisse
    display.display();
  }
}

// --- FONCTION D'AIDE ---
// Affiche les caractères de la case 1 à 31 sur l'OLED
void afficher_paquet_sur_oled(char p[]) {
  for (int i = 1; i < 32; i++) {
    // Si la case n'est pas vide (0), on l'affiche
    if (p[i] != 0) {
      display.print(p[i]);
    }
  }
}