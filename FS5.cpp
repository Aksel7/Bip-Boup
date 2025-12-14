#include <SPI.h>
#include <RF24.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================
// --- CONFIGURATION MATERIEL ---
// ============================================================

// Ecran OLED
#define LARGEUR_ECRAN 128
#define HAUTEUR_ECRAN 64
#define OLED_ADDR 0x3C 
Adafruit_SSD1306 ecran(LARGEUR_ECRAN, HAUTEUR_ECRAN, &Wire, -1);

// Module NRF24
RF24 radio(7, 8); // Pins CE, CSN

// Encodeur & Buzzer
const int pinCLK = 3; 
const int pinDT  = 4; 
const int pinSW  = 2; 
const int buzzer = 10; 
const int pinBoutonRetour = A6; // Second bouton (sur A6)

// ============================================================
// --- VARIABLES GLOBALES ---
// ============================================================

volatile int dernierCLK = 0;
volatile bool changement = false; 

// Modes du menu
enum Mode { CANAL, PSEUDO, BUZZER };
Mode modeActuel = CANAL; 

// Paramètres
int canal = 0;           
char pseudo[11] = "";     // Tableau de caractères (plus stable que String)
const int longueurMaxPseudo = 10; 

char lettreActuelle = 'A'; 
int indexLettre = 0;      
const char lettresAutorisees[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 .,!?;:@#&+-*/";

int modeBuzzer = 0;       
const int nbModesBuzzer = 3; 

// --- ADRESSES EEPROM (CORRIGEES) ---
// On espace les adresses pour éviter que les variables se marchent dessus
const int addrCanal = 0;        // Int (2 octets) -> occupe adresses 0 et 1
const int addrBuzzer = 10;      // Int -> occupe adresses 10 et 11
const int addrPseudo = 20;      // Char array -> commence à 20

// ============================================================
// --- FONCTIONS LOGIQUES ---
// ============================================================

// --- Gestion de l'Interruption (Encodeur) ---
void lireEncodeur() {
  static unsigned long derniereInterruption = 0;
  unsigned long tempsActuel = millis();

  // Anti-rebond : on ignore si moins de 5ms s'est écoulé
  if (tempsActuel - derniereInterruption > 5) {
    int clk = digitalRead(pinCLK); 
    
    // Détection front montant
    if (clk != dernierCLK && clk == HIGH) { 
      // Lecture du sens (DT par rapport à CLK)
      bool sensHoraire = (digitalRead(pinDT) != HIGH); 

      if (modeActuel == CANAL) { 
        if (sensHoraire) canal++; else canal--;
        // Boucle 0-125
        if (canal < 0) canal = 125;      
        if (canal > 125) canal = 0;      
      } 
      else if (modeActuel == PSEUDO) { 
        if (sensHoraire) indexLettre++; else indexLettre--;
        
        // Cast (int) pour éviter le warning de compilation
        int taille = (int)sizeof(lettresAutorisees) - 1; // -1 pour ignorer le caractère nul de fin
        
        if (indexLettre >= taille) indexLettre = 0;
        if (indexLettre < 0) indexLettre = taille - 1;
        
        lettreActuelle = lettresAutorisees[indexLettre];
      } 
      else if (modeActuel == BUZZER) { 
        if (sensHoraire) modeBuzzer++; else modeBuzzer--;
        if (modeBuzzer >= nbModesBuzzer) modeBuzzer = 0;
        if (modeBuzzer < 0) modeBuzzer = nbModesBuzzer - 1;
      }
      changement = true; 
    }
    dernierCLK = clk;
    derniereInterruption = tempsActuel;
  }
}

// --- Sauvegarde ---
void sauvegarderParams() {
  // EEPROM.put est plus sûr que write, il gère les types int et tableaux auto
  EEPROM.put(addrCanal, canal);
  EEPROM.put(addrBuzzer, modeBuzzer);
  EEPROM.put(addrPseudo, pseudo); 
}

// --- Chargement ---
void chargerParams() {
  EEPROM.get(addrCanal, canal);
  EEPROM.get(addrBuzzer, modeBuzzer);
  EEPROM.get(addrPseudo, pseudo);

  // Sécurité : Si l'EEPROM est vide ou corrompue, on remet à zéro
  if (canal < 0 || canal > 125) canal = 0;
  if (modeBuzzer < 0 || modeBuzzer > 2) modeBuzzer = 0;
  
  // Assure que le pseudo se termine bien par un caractère nul
  pseudo[10] = '\0'; 
}

// --- Affichage ---
void afficherInfo() {
  ecran.clearDisplay();
  ecran.setTextSize(1);
  ecran.setTextColor(SSD1306_WHITE);
  
  // En-tête
  ecran.setCursor(0, 0);
  if (modeActuel == CANAL) ecran.println(F(">> CONFIG CANAL <<"));
  else if (modeActuel == PSEUDO) ecran.println(F(">> CONFIG PSEUDO <<"));
  else ecran.println(F(">> CONFIG SON <<"));
  
  ecran.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  // Contenu selon le mode
  if (modeActuel == CANAL) {
    ecran.setTextSize(2);
    ecran.setCursor(40, 25);
    ecran.println(canal);
    
    ecran.setTextSize(1);
    ecran.setCursor(0, 55);
    ecran.println(F("Click = Valider"));
  } 
  else if (modeActuel == PSEUDO) {
    ecran.setCursor(0, 15);
    ecran.print(F("Actuel: ")); ecran.println(pseudo);
    
    ecran.setCursor(0, 35);
    ecran.print(F("Ajout : [ ")); 
    ecran.setTextSize(2);
    ecran.print(lettreActuelle);
    ecran.setTextSize(1);
    ecran.println(F(" ]"));
    
    ecran.setCursor(0, 55);
    ecran.println(F("Click=Add Long=Exit"));
    ecran.println(F("Click=Add Btn2=Del"));
  } 
  else if (modeActuel == BUZZER) {
    ecran.setTextSize(2);
    ecran.setCursor(20, 25);
    if (modeBuzzer == 0) ecran.println(F("Court"));
    else if (modeBuzzer == 1) ecran.println(F("Long"));
    else ecran.println(F("Double"));
    
    ecran.setTextSize(1);
    ecran.setCursor(0, 55);
    ecran.println(F("Click = Valider"));
  }

  ecran.display();
}

// ============================================================
// --- SETUP & LOOP ---
// ============================================================

void setup() {
  pinMode(pinCLK, INPUT_PULLUP);
  pinMode(pinDT, INPUT_PULLUP);
  pinMode(pinSW, INPUT_PULLUP);
  pinMode(buzzer, OUTPUT);

  // 1. Ecran
  if (!ecran.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    for (;;); // Bloque si erreur écran
  }
  
  // 2. Chargement des paramètres AVANT tout le reste
  chargerParams();
  
  // 3. Radio
  radio.begin();
  radio.setChannel(canal);

  // 4. Initialisation Interruption
  dernierCLK = digitalRead(pinCLK);
  attachInterrupt(digitalPinToInterrupt(pinCLK), lireEncodeur, CHANGE);

  afficherInfo();
}

void loop() {
  // Mise à jour écran si l'encodeur a bougé
  if (changement) {
    afficherInfo();
    changement = false;
  }

  // --- Gestion du Bouton Retour (A6) ---
  static bool boutonRetourRelache = true;
  // Lecture analogique car A6 est purement analogique sur Nano
  if (analogRead(pinBoutonRetour) < 500) { 
    if (boutonRetourRelache && modeActuel == PSEUDO) {
      int len = strlen(pseudo);
      if (len > 0) {
        pseudo[len - 1] = '\0';
        sauvegarderParams();
        tone(buzzer, 500, 100); // Son plus grave pour effacer
        changement = true;
      }
      boutonRetourRelache = false;
    }
  } else {
    boutonRetourRelache = true;
  }

  // Gestion du Bouton (avec anti-rebond logiciel)
  static int dernierEtatBouton = HIGH;
  int etatBouton = digitalRead(pinSW);
  static unsigned long debutAppui = 0;

  // Détection appui
  if (etatBouton == LOW && dernierEtatBouton == HIGH) {
    debutAppui = millis();
  } 
  // Détection relâchement
  else if (etatBouton == HIGH && dernierEtatBouton == LOW) {
    unsigned long duree = millis() - debutAppui;
    
    // On ignore les parasites < 50ms
    if (duree > 50) {
      // --- APPUI LONG (> 800ms) : CHANGER DE MODE ---
      if (duree >= 800) { 
        if (modeActuel == CANAL) modeActuel = PSEUDO;
        else if (modeActuel == PSEUDO) modeActuel = BUZZER;
        else modeActuel = CANAL;
        
        // Son de changement de menu
        tone(buzzer, 2000, 50); delay(100); tone(buzzer, 2500, 50);
      } 
      // --- APPUI COURT : ACTION ---
      else { 
        if (modeActuel == CANAL) {
          radio.setChannel(canal); 
          sauvegarderParams();
          tone(buzzer, 1500, 100); 
        } 
        else if (modeActuel == PSEUDO) {
          int len = strlen(pseudo);
          if (len < longueurMaxPseudo) {
            pseudo[len] = lettreActuelle;
            pseudo[len + 1] = '\0'; // Toujours fermer la chaine de caractères
            sauvegarderParams();
            tone(buzzer, 1500, 50);
          } else {
             tone(buzzer, 200, 300); // Erreur (plein)
          }
        } 
        else if (modeActuel == BUZZER) {
          sauvegarderParams();
          // Test du son choisi
          if (modeBuzzer == 0) tone(buzzer, 1000, 100);
          else if (modeBuzzer == 1) tone(buzzer, 1000, 500);
          else { tone(buzzer, 1000, 100); delay(150); tone(buzzer, 1000, 100); }
        }
      }
      changement = true; // Force rafraichissement écran
    }
  }
  dernierEtatBouton = etatBouton;
}