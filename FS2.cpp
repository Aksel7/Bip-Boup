/*
   PROJET : Emetteur NRF24 - Version Débutant
   BUT    : Envoyer un Pseudo et un Message long morceau par morceau.
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
// On utilise "String" car c'est plus facile pour un débutant
String mon_pseudo = "Passion_Electro";
String mon_message = "Ceci est un test facile. Je decoupe ce message en petits morceaux avec une boucle for simple !";

void setup() {
  pinMode(10,OUTPUT);
  // 1. Démarrage Ecran
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Parfois 0x3D
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  
  // 2. Démarrage Radio
  radio.begin();
  radio.openWritingPipe(adresse);
  radio.setPALevel(RF24_PA_MIN);
  radio.stopListening(); // On est l'émetteur

  // Petit message d'accueil
  display.setCursor(0, 0);
  display.println("Systeme Pret !");
  display.display();
  delay(2000);
}

// --- FONCTION MAGIQUE SIMPLIFIEE ---
// Cette fonction prend un texte et une lettre type ('P' ou 'M')
// et s'occupe de tout découper et envoyer.
void envoyer_le_texte(char type, String texte_a_envoyer) {
  
  int longueur_totale = texte_a_envoyer.length();
  int curseur = 0; // Où on en est dans le texte (lettre 0, 10, 20...)

  // Tant qu'on n'a pas tout envoyé...
  while (curseur < longueur_totale) {
    
    // 1. On prépare un "paquet" vide de 32 cases
    char paquet[32]; 
    
    // 2. La case 0 sert à dire ce que c'est ('P' ou 'M')
    paquet[0] = type;

    // 3. On remplit les 31 cases restantes avec une boucle simple
    // On va de 1 à 31 (car 0 est déjà pris)
    for (int i = 1; i < 32; i++) {
      
      if (curseur < longueur_totale) {
        // On prend la lettre du texte et on la met dans le paquet
        paquet[i] = texte_a_envoyer[curseur];
        curseur++; // On passe à la lettre suivante du texte global
      } else {
        // Si le texte est fini, on met un 0 (vide)
        paquet[i] = 0; 
      }
    }

    // 4. On envoie le paquet par la radio
    radio.write(&paquet, sizeof(paquet));

    // 5. On affiche sur l'écran ce qu'on fait
    display.clearDisplay();
    display.setCursor(0, 0);
    
    if (type == 'P') {
      display.println("Envoi du PSEUDO...");
      display.println(mon_pseudo);
    } else {
      display.println("Envoi du MESSAGE...");
      // Affiche : "Reste : 50 caracteres"
      display.print("Reste a envoyer: ");
      display.println(longueur_totale - curseur);
    }
    display.display();

    // Petite pause pour laisser le temps de voir
    delay(100);
  }
}

void loop() {
  // Etape 1 : Envoyer le Pseudo
  envoyer_le_texte('P', mon_pseudo);
  
  delay(1000); // Pause

  // Etape 2 : Envoyer le Message
  envoyer_le_texte('M', mon_message);

  // Fin
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Transmission FINIE");
  display.println("Attente...");
  display.display();
  
  delay(5000); // On attend 5 secondes avant de recommencer
}