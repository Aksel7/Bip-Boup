/*
    PROJET : Emetteur NRF24 - Version FINALE (Avec Confirmation)
    BUT    : Envoyer un Pseudo et un Message long, et vérifier si ça arrive.
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

// --- LES DONNEES A ENVOYER ---
String mon_pseudo = "Passion_Electro";
String mon_message = "Ceci est un test avec confirmation. Si tu vois OK, c'est que le recepteur a bien recu !";

void setup() {
  pinMode(10,OUTPUT); // Nécessaire pour le SPI sur certains Arduinos
  
  // 1. Démarrage Ecran
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    // Si l'écran ne s'allume pas, on boucle ici
    for(;;); 
  }
  
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  
  // 2. Démarrage Radio
  radio.begin();
  
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

  // Petit message d'accueil
  display.setCursor(0, 0);
  display.println("Emetteur Pret !");
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
    
    if (type == 'P') {
      display.println(">> Envoi PSEUDO");
      display.println(mon_pseudo);
    } else {
      display.println(">> Envoi MESSAGE");
      display.print("Reste: ");
      display.println(longueur_totale - curseur);
    }
    
    display.println("-----");
    display.print("Statut Radio: ");
    
    if (accuse_reception) {
       display.println("OK"); // Succès
    } else {
       display.println("X");  // Échec
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
  display.println("Pause de 5 sec...");
  display.display();
  
  delay(5000); 
}