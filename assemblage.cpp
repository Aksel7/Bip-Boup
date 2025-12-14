/*
 * PROJET FUSIONNE : Terminal de Messagerie NRF24 + Saisie OLED
 * Matériel : Arduino Nano, NRF24L01, OLED SSD1306, Encodeur Rotatif
 */

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <RF24.h>
#include <EEPROM.h>

// ============================================================
// --- CONFIGURATION MATERIEL ---
// ============================================================

// --- Ecran OLED ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Radio NRF24L01 ---
#define PIN_CE  7
#define PIN_CSN 8
RF24 radio(PIN_CE, PIN_CSN);
const byte adresse[6] = "PIPE1"; 

// --- Configuration Canal Radio ---
int canal = 0;           
const int addrCanal = 0; // Adresse mémoire EEPROM

// --- Encodeur & Boutons ---
const int brocheEncodeur_A = 3; 
const int brocheEncodeur_B = 4; 
const int brocheBouton_Encodeur = 2; // Selection lettre
const int brocheBouton_Valider = A6; // Validation finale / Backspace long
const int brocheLED_Validation = 5;  // LED Feedback

// ============================================================
// --- VARIABLES GLOBALES ---
// ============================================================

// --- Pseudo ---
String pseudo = "NanoUser"; 

// --- Gestion du Texte ---
const char* listeCaracteres = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .,!?'@\x82\x85\x87\x88\x97<";
const int tailleListeCaracteres = 44; 

struct Message {
  char texte[101]; 
  int priorite;    
};
Message monMessage; 
int positionCurseurMessage = 0;

// --- Variables Interface (UI) ---
volatile int indexCaractereSelectionne = 0; 
int dernierIndexCaractereAffiche = -1;
volatile int compteurBrutEncodeur = 0; 
const int SENSIBILITE = 2; 

// --- Gestion des Rebond (Debounce) ---
long dernierTempsDebounceEnc = 0;
int etatPrecedentBoutonEnc = HIGH;
int etatPrecedentBoutonVal = HIGH;
unsigned long tempsDebutAppuiVal = 0;
bool appuiLongTraite = false; 
const long delaiDebounce = 50;
const long DUREE_APPUI_LONG = 1000; 

// --- Machine d'état ---
enum EtatSysteme {
  SAISIE_TEXTE,
  CHOIX_PRIORITE,
  ENVOI_EN_COURS,
  ATTENTE_RESET
};
EtatSysteme etatActuel = SAISIE_TEXTE;

// ============================================================
// --- PROTOTYPES ---
// ============================================================
void gererRotationEncodeur();
void gererLimitesIndex(int max);
void gererLimitesPriorite();
void gererAffichageSaisie();
void gererAffichagePriorite();
void envoyerMessageRadio();
void envoyer_paquet_fragment(char type, String texte_a_envoyer);
void clignoterLED();
void resetSysteme();
void gererBoutonSelectionLettre(); 
void gererBoutonValidation();       

// ============================================================
// --- SETUP ---
// ============================================================
void setup() {
  Serial.begin(9600);
  pinMode(10, OUTPUT);
  // Init Ecran
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    for(;;);
  }
  display.cp437(true); 
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Init Pins
  pinMode(brocheEncodeur_A, INPUT_PULLUP);
  pinMode(brocheEncodeur_B, INPUT_PULLUP);
  pinMode(brocheBouton_Encodeur, INPUT_PULLUP);
  pinMode(brocheLED_Validation, OUTPUT);
  digitalWrite(brocheLED_Validation, LOW);
  
  // Interruptions Encodeur
  attachInterrupt(digitalPinToInterrupt(brocheEncodeur_A), gererRotationEncodeur, CHANGE);

  // Init Radio
  radio.begin();
  
  // Lecture Canal EEPROM
  byte lectureEEPROM = EEPROM.read(addrCanal);
  if (lectureEEPROM > 125) canal = 0; 
  else canal = lectureEEPROM;
  
  radio.setChannel(canal); 
  radio.openWritingPipe(adresse); 
  radio.setPALevel(RF24_PA_MIN); 
  radio.stopListening(); 

  // Vérification Radio
 /*  if (!radio.isChipConnected()) {
    display.setCursor(0,0);
    display.println(F("ERR: Radio HS"));
    display.display();
    delay(2000);
  } */

  // Init Variables
  resetSysteme();
  
  // Ecran d'accueil rapide
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(F("SYSTEME PRET"));
  display.print(F("Canal: ")); display.println(canal);
  display.display();
  delay(1000);
  display.clearDisplay();
}

// ============================================================
// --- LOOP ---
// ============================================================
void loop() {
  switch (etatActuel) {
    case SAISIE_TEXTE:
      gererLimitesIndex(tailleListeCaracteres);
      gererAffichageSaisie();
      break;
      
    case CHOIX_PRIORITE:
      gererLimitesPriorite();
      gererAffichagePriorite();
      break;
      
    case ENVOI_EN_COURS:
      // Envoi du message
      envoyerMessageRadio();
      etatActuel = ATTENTE_RESET;
      break;

    case ATTENTE_RESET:
      // Pause lecture résultat
      delay(2000);
      resetSysteme();
      break;
  }

  // Les boutons ne sont actifs que si on n'est pas en train d'envoyer
  if (etatActuel == SAISIE_TEXTE || etatActuel == CHOIX_PRIORITE) {
    gererBoutonSelectionLettre(); 
    gererBoutonValidation();   
  }
  
  delay(10); 
}

// ============================================================
// --- FONCTIONS LOGIQUE METIER (RADIO) ---
// ============================================================

void envoyerMessageRadio() {
  // Feedback Visuel
  clignoterLED(); 
  
  // Envoi du Pseudo
  envoyer_paquet_fragment('P', pseudo);
  delay(500);

  // Préparation du Message
  String messageFinal = "";
  
  // On ajoute un tag de priorité au début du message
  if (monMessage.priorite == 1) messageFinal += "[URGENT] ";
  // Priorite 2 (Normal) on ne met rien ou [NORMAL]
  else if (monMessage.priorite == 3) messageFinal += "[INFO] ";
  
  messageFinal += String(monMessage.texte);

  // Envoi du Message
  envoyer_paquet_fragment('M', messageFinal);
  
  // Fin
  display.clearDisplay();
  display.setCursor(0,0);
  display.println(F("SEQUENCE TERMINEE"));
  display.display();
}

void envoyer_paquet_fragment(char type, String texte_a_envoyer) {
  int longueur_totale = texte_a_envoyer.length();
  int curseur = 0; 

  while (curseur < longueur_totale) {
    char paquet[32]; 
    paquet[0] = type; // 'P' ou 'M'

    // Remplissage
    for (int i = 1; i < 32; i++) {
      if (curseur < longueur_totale) {
        paquet[i] = texte_a_envoyer[curseur];
        curseur++; 
      } else {
        paquet[i] = 0; 
      }
    }

    // Envoi Radio
    bool accuse_reception = radio.write(&paquet, sizeof(paquet));

    // Affichage Statut en temps réel
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print(F("[CH:")); display.print(canal); display.println(F("] Envoi..."));
    
    if (type == 'P') display.println(F(">> PSEUDO"));
    else display.println(F(">> MESSAGE"));

    display.drawLine(0, 20, 128, 20, SSD1306_WHITE);
    display.setCursor(0, 25);
    display.print(F("Reste: ")); display.println(longueur_totale - curseur);
    
    display.setCursor(0, 45);
    if (accuse_reception) display.println(F("Statut: OK (Recu)"));
    else display.println(F("Statut: .. (Perdu)"));
    
    display.display();
    delay(1000);
  }
}

// ============================================================
// --- FONCTIONS LOGIQUE METIER (INTERFACE) ---
// ============================================================

void gererRotationEncodeur() {
  static unsigned long dernierTempsInterruption = 0;
  unsigned long tempsActuel = millis();

  if (tempsActuel - dernierTempsInterruption > 5) {
    if (digitalRead(brocheEncodeur_A) == digitalRead(brocheEncodeur_B)) {
      compteurBrutEncodeur++;
    } else {
      compteurBrutEncodeur--;
    }

    if (compteurBrutEncodeur >= SENSIBILITE) {
      indexCaractereSelectionne++;
      compteurBrutEncodeur = 0; 
    } 
    else if (compteurBrutEncodeur <= -SENSIBILITE) {
      indexCaractereSelectionne--;
      compteurBrutEncodeur = 0; 
    }
    dernierTempsInterruption = tempsActuel;
  }
}

void clignoterLED() {
  for(int i=0; i<3; i++) {
    digitalWrite(brocheLED_Validation, HIGH); 
    delay(100); 
    digitalWrite(brocheLED_Validation, LOW);  
    delay(100); 
  }
}

void resetSysteme() {
  memset(monMessage.texte, 0, sizeof(monMessage.texte));
  positionCurseurMessage = 0;
  indexCaractereSelectionne = 0;
  dernierIndexCaractereAffiche = -1;
  compteurBrutEncodeur = 0;
  monMessage.priorite = 2;
  etatActuel = SAISIE_TEXTE;
}

void gererLimitesIndex(int max) {
  if (indexCaractereSelectionne >= max) indexCaractereSelectionne = 0;
  if (indexCaractereSelectionne < 0) indexCaractereSelectionne = max - 1;
}

void gererLimitesPriorite() {
  if (indexCaractereSelectionne > 3) indexCaractereSelectionne = 1;
  if (indexCaractereSelectionne < 1) indexCaractereSelectionne = 3;
}

// --- Affichage Interface Saisie ---
void gererAffichageSaisie() {
  if (indexCaractereSelectionne != dernierIndexCaractereAffiche) {
    display.clearDisplay();
    int indexPrec = (indexCaractereSelectionne - 1 + tailleListeCaracteres) % tailleListeCaracteres;
    int indexSuiv = (indexCaractereSelectionne + 1) % tailleListeCaracteres;

    // Bandeau Carrousel
    display.setTextSize(1);
    display.setCursor(10, 12); display.print(listeCaracteres[indexPrec]);
    display.setCursor(110, 12); display.print(listeCaracteres[indexSuiv]);

    display.setTextSize(2);
    display.setCursor(58, 8);
    display.print(listeCaracteres[indexCaractereSelectionne]);

    display.setTextSize(1);
    display.setCursor(40, 12); display.print(F(">"));
    display.setCursor(80, 12); display.print(F("<"));

    // Zone de texte
    display.drawLine(0, 30, 128, 30, SSD1306_WHITE);
    display.setCursor(0, 35);
    display.println(monMessage.texte);
    if (positionCurseurMessage < 100) display.print("_"); // Curseur clignotant fixe

    // Compteur caractères
    display.setCursor(0, 0); display.print(positionCurseurMessage);
    display.display();
    dernierIndexCaractereAffiche = indexCaractereSelectionne;
  }
}

// --- Affichage Interface Priorité ---
void gererAffichagePriorite() {
   if (indexCaractereSelectionne != dernierIndexCaractereAffiche) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0,0); display.println(F("CHOIX PRIORITE"));
    
    display.setTextSize(2);
    display.setCursor(10,25);
    
    if (indexCaractereSelectionne == 1) display.print(F("<URGENT>"));
    else if (indexCaractereSelectionne == 2) display.print(F("<NORMAL>"));
    else display.print(F("<BASSE>"));

    display.setTextSize(1);
    display.setCursor(0,55); display.print(F("BtnVal: OK"));
    display.display();
    dernierIndexCaractereAffiche = indexCaractereSelectionne;
   }
}

// --- Gestion Bouton Encodeur (Ajout Lettre) ---
void gererBoutonSelectionLettre() {
  int etat = digitalRead(brocheBouton_Encodeur);
  if (etat != etatPrecedentBoutonEnc && millis() - dernierTempsDebounceEnc > delaiDebounce) {
    if (etat == LOW) { 
      if (etatActuel == SAISIE_TEXTE) {
        char carChoisi = listeCaracteres[indexCaractereSelectionne];
        if (carChoisi == '<') { 
           if (positionCurseurMessage > 0) {
            positionCurseurMessage--;
            monMessage.texte[positionCurseurMessage] = '\0';
            dernierIndexCaractereAffiche = -1; 
          }
        } else {
          if (positionCurseurMessage < 100) { 
            monMessage.texte[positionCurseurMessage] = carChoisi;
            positionCurseurMessage++;
            monMessage.texte[positionCurseurMessage] = '\0';
            dernierIndexCaractereAffiche = -1;
          }
        }
      }
      else if (etatActuel == CHOIX_PRIORITE) {
        monMessage.priorite = indexCaractereSelectionne;
        etatActuel = ENVOI_EN_COURS;
      }
    }
    dernierTempsDebounceEnc = millis();
  }
  etatPrecedentBoutonEnc = etat;
}

// --- Gestion Bouton Validation (Changement Menu / Effacer) ---
void gererBoutonValidation() {
  int lecture = analogRead(brocheBouton_Valider);
  int etatActuelBouton = (lecture < 500) ? LOW : HIGH; 
  unsigned long tempsMaintenant = millis();

  if (etatActuelBouton == LOW && etatPrecedentBoutonVal == HIGH) {
    tempsDebutAppuiVal = tempsMaintenant;
    appuiLongTraite = false; 
  }

  // Gestion Appui Long (Backspace)
  if (etatActuelBouton == LOW && !appuiLongTraite) {
    if (tempsMaintenant - tempsDebutAppuiVal >= DUREE_APPUI_LONG) {
      if (etatActuel == SAISIE_TEXTE && positionCurseurMessage > 0) {
        positionCurseurMessage--;
        monMessage.texte[positionCurseurMessage] = '\0';
        dernierIndexCaractereAffiche = -1; 
        // display.invertDisplay(true); delay(50); display.invertDisplay(false);
      }
      appuiLongTraite = true; 
    }
  }

  // Gestion Relachement
  if (etatActuelBouton == HIGH && etatPrecedentBoutonVal == LOW) {
    if (!appuiLongTraite) {
       if (etatActuel == SAISIE_TEXTE) {
         etatActuel = CHOIX_PRIORITE;
         indexCaractereSelectionne = 2;
         dernierIndexCaractereAffiche = -1;
      }
      else if (etatActuel == CHOIX_PRIORITE) {
         monMessage.priorite = indexCaractereSelectionne;
         etatActuel = ENVOI_EN_COURS;
      }
    }
  }
  etatPrecedentBoutonVal = etatActuelBouton;
}