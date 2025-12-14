/*
    PROJET : Emetteur NRF24 - Version FINALE (Avec Confirmation + Canal EEPROM)
    BUT    : Envoyer un Pseudo et un Message long sur un canal spécifique.
*/

#include <SPI.h>
#include <RF24.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h> // <--- AJOUT : Nécessaire pour sauvegarder le canal

// --- CONFIGURATION ---
// Radio
#define PIN_CE  7
#define PIN_CSN 8
RF24 radio(PIN_CE, PIN_CSN);

const byte adresse[6] = "PIPE1"; // On garde PIPE1 comme convenu

// <--- AJOUT : Variables pour le canal
int canal = 0;           
const int addrCanal = 0; // Adresse mémoire dans l'Arduino

// Ecran OLED
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// --- LES DONNEES A ENVOYER ---
String mon_pseudo = "Jo";
String mon_message = "Ceci est un test avec confirmation. Si tu vois OK, c'est que le recepteur a bien recu !";

void setup() {
  pinMode(10,OUTPUT); 
  
  // 1. Démarrage Ecran
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    for(;;); 
  }
  
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  
  // 2. Démarrage Radio
  radio.begin();

  // <--- AJOUT : Configuration du CANAL depuis la mémoire
  byte lectureEEPROM = EEPROM.read(addrCanal);
  if (lectureEEPROM > 125) { 
    canal = 0; // Sécurité si mémoire vide
  } else {
    canal = lectureEEPROM;
  }
  
  radio.setChannel(canal); // <--- C'est ici que la fréquence est réglée !
  // ---------------------------------------------------------
  
  // Vérification si le module radio est bien branché
  if (!radio.isChipConnected()) {
    display.setCursor(0,0);
    display.println("Erreur Radio !");
    display.println("Verifiez cablage."); 
    display.display();
    while(1);
  }

  radio.openWritingPipe(adresse); 
  radio.setPALevel(RF24_PA_MIN); 
  radio.stopListening(); // Mode Emetteur

  // Petit message d'accueil AVEC le canal
  display.setCursor(0, 0);
  display.println("Emetteur Pret !");
  
  // <--- AJOUT : Affichage du canal pour info
  display.print("Canal Radio: ");
  display.println(canal);
  display.display();
  
  delay(2000);
}

// --- FONCTION D'ENVOI INTELLIGENTE ---
void envoyer_le_texte(char type, String texte_a_envoyer) {
  
  int longueur_totale = texte_a_envoyer.length();
  int curseur = 0; 

  // Tant qu'on n'a pas tout envoyé...
  while (curseur < longueur_totale) {
    
    // 1. Préparation du paquet
    char paquet[32]; 
    paquet[0] = type; // 'P' ou 'M'

    // Remplissage des 31 autres cases
    for (int i = 1; i < 32; i++) {
      if (curseur < longueur_totale) {
        paquet[i] = texte_a_envoyer[curseur];
        curseur++; 
      } else {
        paquet[i] = 0; // Nettoyage de la fin
      }
    }

    // 2. ENVOI AVEC VERIFICATION
    // radio.write renvoie 'true' si le récepteur a dit "J'ai reçu !"
    bool accuse_reception = radio.write(&paquet, sizeof(paquet));

    // 3. Affichage du statut
    display.clearDisplay();
    display.setCursor(0, 0);
    
    // <--- AJOUT : Rappel du canal en haut de l'écran
    display.print("[CH:"); 
    display.print(canal);
    display.println("]");

    if (type == 'P') {
      display.println(">> Envoi PSEUDO");
      display.println(mon_pseudo);
    } else {
      display.println(">> Envoi MESSAGE");
      display.print("Reste: ");
      display.println(longueur_totale - curseur);
    }
    
    display.println("-----");
    display.print("Statut: ");
    
    if (accuse_reception) {
       display.println("OK (Recu)"); // Succès
    } else {
       display.println("X (Perdu)");  // Échec
    }

    display.display();

    // Petite pause visuelle
    delay(150);
  }
}

void loop() {
  // Etape 1 : Envoyer le Pseudo
  envoyer_le_texte('P', mon_pseudo);
  
  delay(1000); // Pause entre pseudo et message

  // Etape 2 : Envoyer le Message
  envoyer_le_texte('M', mon_message);

  // Fin de la séquence
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Message termine.");
  display.print("Canal: ");
  display.println(canal);
  display.println("Pause 5 sec...");
  display.display();
  
  delay(5000); 
}