/*
    PROJET : Récepteur NRF24 - Version OLED Confirmée
    BUT    : Recevoir et accuser réception automatiquement.
*/

#include <RF24.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>

// --- CONFIGURATION ---
#define PIN_CE  7
#define PIN_CSN 8
RF24 radio(PIN_CE, PIN_CSN);
const byte adresse[6] = "PIPE1";

Adafruit_SSD1306 display(128, 64, &Wire, -1);

void afficher_paquet_sur_oled(char p[]) {
  for (int i = 1; i < 32; i++) {
    if (p[i] != 0) display.print(p[i]);
  }
}

void setup() {
  // Ecran
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { for(;;); }
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  
  display.setCursor(0, 0);
  display.println("Recepteur Actif.");
  display.display();

  // Radio
  radio.begin();
  if (!radio.isChipConnected()) {
    display.println("Erreur Radio !");
    display.display();
    while (1); 
  }
  
  radio.openReadingPipe(0, adresse);
  radio.setPALevel(RF24_PA_MIN);
  radio.startListening(); 
}

void loop() {
  if (radio.available()) {
    
    char paquet[32] = "";
    radio.read(&paquet, sizeof(paquet));
    // Note : Au moment où .read() est fait, le module a DEJA envoyé 
    // l'accusé de réception à l'émetteur automatiquement.
    
    char type = paquet[0]; 

    if (type == 'P') {
      display.clearDisplay();
      display.setCursor(0, 0);
      display.print("DE: "); 
      afficher_paquet_sur_oled(paquet); 
      display.println();        
      display.println("-----"); 
    }
    else if (type == 'M') {
      afficher_paquet_sur_oled(paquet);
    }
    
    display.display();
  }
}