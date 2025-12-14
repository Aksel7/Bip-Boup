#include <SPI.h>
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>

// --- Paramètres de l'écran ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Paramètres Matériel ---
const int brocheEncodeur_A = 3; 
const int brocheEncodeur_B = 4; 
const int brocheBouton_Encodeur = 2; // Bouton 1 (Ajout lettre)
const int brocheBouton_Valider = A6; // Bouton 2 (Validation / Effacement Long)
const int brocheLED_Validation = 5;  // LED

// --- Liste des caractères ---
const char* listeCaracteres = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .,!?'@\x82\x85\x87\x88\x97<";
const int tailleListeCaracteres = 44; 

// --- Variables Globales ---
volatile int indexCaractereSelectionne = 0; 
int dernierIndexCaractereAffiche = -1;

// Sensibilité Encodeur
volatile int compteurBrutEncodeur = 0; 
const int SENSIBILITE = 2; 

struct Message {
  char texte[101]; 
  int priorite;    
};
Message monMessage; 
int positionCurseurMessage = 0;

// Variables Gestion Boutons
long dernierTempsDebounceEnc = 0;
int etatPrecedentBoutonEnc = HIGH;
int etatPrecedentBoutonVal = HIGH;
unsigned long tempsDebutAppuiVal = 0;
bool appuiLongTraite = false; 

const long delaiDebounce = 50;
const long DUREE_APPUI_LONG = 1000; 

enum EtatSysteme {
  SAISIE_TEXTE,
  CHOIX_PRIORITE,
  MESSAGE_PRET
};
EtatSysteme etatActuel = SAISIE_TEXTE;

// --- Interruption (ISR) ---
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

// --- Fonctions (Déclarations) ---
void gererLimitesIndex(int max);
void gererLimitesPriorite();
void gererAffichageSaisie();
void gererAffichagePriorite();
void afficherEcranValidation(); // <-- Celle qu'on modifie
void clignoterLED();
void resetSysteme();
void gererBoutonSelectionLettre(); 
void gererBoutonValidation();      

// --- Setup ---
void setup() {
  Serial.begin(9600);
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    for(;;);
  }
  display.cp437(true); 

  pinMode(brocheEncodeur_A, INPUT_PULLUP);
  pinMode(brocheEncodeur_B, INPUT_PULLUP);
  pinMode(brocheBouton_Encodeur, INPUT_PULLUP);
  pinMode(brocheLED_Validation, OUTPUT);
  digitalWrite(brocheLED_Validation, LOW);
  
  attachInterrupt(digitalPinToInterrupt(brocheEncodeur_A), gererRotationEncodeur, CHANGE);

  memset(monMessage.texte, 0, sizeof(monMessage.texte));
  monMessage.priorite = 2; 

  display.clearDisplay();
  display.display();
}

// --- Loop ---
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
      
    case MESSAGE_PRET:
      // 1. Afficher le RÉSUMÉ DU MESSAGE
      afficherEcranValidation(); 
      
      // 2. Clignoter
      clignoterLED();            
      
      // 3. Pause lecture
      delay(2500);               
      
      // 4. Reset
      resetSysteme();            
      break;
  }

  if (etatActuel != MESSAGE_PRET) {
    gererBoutonSelectionLettre(); 
    gererBoutonValidation();   
  }
  
  delay(10); 
}

// --- Nouvelle Logique : Affichage complet du message ---
void afficherEcranValidation() {
  display.clearDisplay();
  
  // Titre en haut
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(F(">> ENVOI VALIDE <<"));

  // Affichage de la priorité
  display.setCursor(0, 12);
  display.print(F("Prio: "));
  if (monMessage.priorite == 1) display.print(F("URGENT !!!"));
  else if (monMessage.priorite == 2) display.print(F("Normal"));
  else display.print(F("Basse"));

  // Ligne de séparation
  display.drawLine(0, 22, 128, 22, SSD1306_WHITE);

  // Le Message complet (Adafruit gère le retour à la ligne auto)
  display.setCursor(0, 26);
  display.println(monMessage.texte);

  display.display();
}

void clignoterLED() {
  digitalWrite(brocheLED_Validation, LOW);
  delay(100);
  for(int i=0; i<3; i++) {
    digitalWrite(brocheLED_Validation, HIGH); 
    delay(200); 
    digitalWrite(brocheLED_Validation, LOW);  
    delay(200); 
  }
}

// --- Reste de la logique (Boutons, etc.) ---
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
        etatActuel = MESSAGE_PRET;
      }
    }
    dernierTempsDebounceEnc = millis();
  }
  etatPrecedentBoutonEnc = etat;
}

void gererBoutonValidation() {
  int lecture = analogRead(brocheBouton_Valider);
  int etatActuelBouton = (lecture < 500) ? LOW : HIGH; 
  unsigned long tempsMaintenant = millis();

  if (etatActuelBouton == LOW && etatPrecedentBoutonVal == HIGH) {
    tempsDebutAppuiVal = tempsMaintenant;
    appuiLongTraite = false; 
  }

  if (etatActuelBouton == LOW && !appuiLongTraite) {
    if (tempsMaintenant - tempsDebutAppuiVal >= DUREE_APPUI_LONG) {
      if (etatActuel == SAISIE_TEXTE && positionCurseurMessage > 0) {
        positionCurseurMessage--;
        monMessage.texte[positionCurseurMessage] = '\0';
        dernierIndexCaractereAffiche = -1; 
        display.invertDisplay(true); delay(50); display.invertDisplay(false);
      }
      appuiLongTraite = true; 
    }
  }

  if (etatActuelBouton == HIGH && etatPrecedentBoutonVal == LOW) {
    if (!appuiLongTraite) {
       if (etatActuel == SAISIE_TEXTE) {
         etatActuel = CHOIX_PRIORITE;
         indexCaractereSelectionne = 2; 
         dernierIndexCaractereAffiche = -1;
      }
      else if (etatActuel == CHOIX_PRIORITE) {
         monMessage.priorite = indexCaractereSelectionne;
         etatActuel = MESSAGE_PRET;
      }
    }
  }
  etatPrecedentBoutonVal = etatActuelBouton;
}

void gererLimitesIndex(int max) {
  if (indexCaractereSelectionne >= max) indexCaractereSelectionne = 0;
  if (indexCaractereSelectionne < 0) indexCaractereSelectionne = max - 1;
}

void gererLimitesPriorite() {
  if (indexCaractereSelectionne > 3) indexCaractereSelectionne = 1;
  if (indexCaractereSelectionne < 1) indexCaractereSelectionne = 3;
}

void gererAffichageSaisie() {
  if (indexCaractereSelectionne != dernierIndexCaractereAffiche) {
    display.clearDisplay();
    int indexPrec = (indexCaractereSelectionne - 1 + tailleListeCaracteres) % tailleListeCaracteres;
    int indexSuiv = (indexCaractereSelectionne + 1) % tailleListeCaracteres;

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 12); display.print(listeCaracteres[indexPrec]);
    display.setCursor(110, 12); display.print(listeCaracteres[indexSuiv]);

    display.setTextSize(2);
    display.setCursor(58, 8);
    display.print(listeCaracteres[indexCaractereSelectionne]);

    display.setTextSize(1);
    display.setCursor(40, 12); display.print(F(">"));
    display.setCursor(80, 12); display.print(F("<"));

    display.setCursor(0, 26); 
    display.print(F("BTN2: Court=OK Long=DEL")); 

    display.drawLine(0, 36, 128, 36, SSD1306_WHITE);
    display.setCursor(0, 40);
    display.println(monMessage.texte);
    if (positionCurseurMessage < 100) display.print("_");

    display.setTextSize(1);
    display.setCursor(0, 0); display.print(positionCurseurMessage);

    display.display();
    dernierIndexCaractereAffiche = indexCaractereSelectionne;
  }
}

void gererAffichagePriorite() {
   if (indexCaractereSelectionne != dernierIndexCaractereAffiche) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0,0);
    display.println(F("CHOIX PRIORITE"));
    display.setTextSize(2);
    display.setCursor(10,25);
    
    if (indexCaractereSelectionne == 1) display.print(F("<URGENT>"));
    else if (indexCaractereSelectionne == 2) display.print(F("<NORMAL>"));
    else display.print(F("<BASSE>"));

    display.setTextSize(1);
    display.setCursor(0,50);
    display.print(F("Btn2 Court pour OK"));
    display.display();
    dernierIndexCaractereAffiche = indexCaractereSelectionne;
   }
}

void resetSysteme() {
  memset(monMessage.texte, 0, sizeof(monMessage.texte));
  positionCurseurMessage = 0;
  etatActuel = SAISIE_TEXTE;
  indexCaractereSelectionne = 0;
  dernierIndexCaractereAffiche = -1;
  compteurBrutEncodeur = 0;
}