/*
 * PROJET FINAL : LOGIQUE CORRIGEE (V3)
 * * NAVIGATION :
 * - Rotation Encodeur : Changer lettre / menu
 * - Bouton Encodeur (D2) COURT : Ajouter lettre / Entrer menu
 * - Bouton Encodeur (D2) LONG  : Effacer lettre (Backspace)
 * - Bouton AUX (A6) : VALIDER tout le message / Passer à la suite
 */

#include <SPI.h>
#include <RF24.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================
// --- 1. CONFIGURATION MATERIEL ---
// ============================================================

// Ecran OLED
#define LARGEUR_ECRAN 128
#define HAUTEUR_ECRAN 64
#define OLED_ADDR 0x3C 
Adafruit_SSD1306 display(LARGEUR_ECRAN, HAUTEUR_ECRAN, &Wire, -1);

// Radio NRF24
RF24 radio(7, 8); // CE, CSN
const byte adresse[6] = "PIPE1";

// Pins
const int pinCLK = 3; 
const int pinDT  = 4; 
const int pinSW  = 2;         // Bouton Saisie / Effacer (Court/Long)
const int pinBoutonAux = A6;  // Bouton Validation Finale (Next)
const int pinBuzzer = 10; 

// ============================================================
// --- 2. VARIABLES GLOBALES & ETATS ---
// ============================================================

enum GlobalState { MENU_PRINCIPAL, MODE_LIRE, MODE_ECRIRE, MODE_PARAMETRES };
GlobalState etatGlobal = MENU_PRINCIPAL;
int indexMenu = 0; 

// --- Variables Encodeur ---
volatile int encoderDelta = 0; 

// --- Flags Boutons (Gérés par lireBoutons) ---
bool flagClickCourt = false;   // D2 Relaché rapidement
bool flagClickLong = false;    // D2 Maintenu
bool flagAux = false;          // A6 Appuyé

// --- Variables PARAMETRES ---
enum ModeParam { P_CANAL, P_PSEUDO, P_BUZZER };
ModeParam modeParam = P_CANAL;
int canal = 0;
char pseudo[11] = "NanoUser"; 
int modeBuzzer = 0; 
char lettreActuelle = 'A';
int indexLettre = 0;
const char lettresAutorisees[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 .,!?;:@#&+-*/";

// --- Variables ECRIRE ---
struct Message {
  char texte[101]; 
  int priorite; 
};
Message monMessage; 
int positionCurseurMessage = 0;
int indexCaractereSaisie = 0;
const char* listeCaracteresSaisie = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .,!?'@"; // @ inutile maintenant mais gardé
enum EtatEcriture { E_SAISIE, E_PRIORITE, E_ENVOI };
EtatEcriture etatEcriture = E_SAISIE;

// Adresses EEPROM
const int addrCanal = 0;
const int addrBuzzer = 10;
const int addrPseudo = 20;

// ============================================================
// --- 3. FONCTIONS UTILITAIRES ---
// ============================================================

// Interruption Encodeur
void isrEncodeur() {
  static unsigned long lastInterruptTime = 0;
  unsigned long interruptTime = millis();
  if (interruptTime - lastInterruptTime > 5) {
    if (digitalRead(pinDT) != digitalRead(pinCLK)) {
      encoderDelta++;
    } else {
      encoderDelta--;
    }
    lastInterruptTime = interruptTime;
  }
}

void bip(int type) {
  if (type == 0) { // Click standard (Ajout)
     tone(pinBuzzer, 2000, 30); 
  } else if (type == 1) { // Validation Finale / Envoi
     if(modeBuzzer >= 1) tone(pinBuzzer, 1000, 100); delay(100); tone(pinBuzzer, 2000, 200);
  } else if (type == 2) { // Effacer (Grave)
     tone(pinBuzzer, 500, 100);
  }
}

void sauvegarderParams() {
  EEPROM.put(addrCanal, canal);
  EEPROM.put(addrBuzzer, modeBuzzer);
  EEPROM.put(addrPseudo, pseudo); 
}

void chargerParams() {
  EEPROM.get(addrCanal, canal);
  EEPROM.get(addrBuzzer, modeBuzzer);
  EEPROM.get(addrPseudo, pseudo);
  if (canal < 0 || canal > 125) canal = 0;
  if (modeBuzzer < 0 || modeBuzzer > 2) modeBuzzer = 0;
  pseudo[10] = '\0'; 
}

// --- GESTION INTELLIGENTE DES BOUTONS ---
void lireBoutons() {
  // Reset des flags d'action immédiate
  flagClickCourt = false;
  flagClickLong = false;
  flagAux = false;

  static unsigned long debutAppuiSW = 0;
  static bool appuiEnCoursSW = false;
  static bool longPressTraite = false;
  int etatSW = digitalRead(pinSW);

  // --- Gestion Bouton Encodeur (D2) : Court vs Long ---
  if (etatSW == LOW) { // Bouton enfoncé
    if (!appuiEnCoursSW) {
      debutAppuiSW = millis();
      appuiEnCoursSW = true;
      longPressTraite = false;
    } else {
      // Bouton maintenu : Check si Long Press atteint (600ms)
      if (!longPressTraite && (millis() - debutAppuiSW > 600)) {
        flagClickLong = true; // Déclenchement Backspace
        longPressTraite = true; // On verrouille pour ne pas répéter trop vite ou confondre
      }
    }
  } else { // Bouton relâché
    if (appuiEnCoursSW) {
      // Si on relâche AVANT le délai long, c'est un click court
      if (!longPressTraite) {
        flagClickCourt = true;
      }
      appuiEnCoursSW = false;
    }
  }

  // --- Gestion Bouton Aux (A6) : Simple Click ---
  static bool relacheAux = true;
  if (analogRead(pinBoutonAux) < 500) { // Appuyé (GND)
    if (relacheAux) {
      flagAux = true;
      relacheAux = false;
      delay(50); // Petit debounce
    }
  } else {
    relacheAux = true;
  }
}

// ============================================================
// --- 4. LOGIQUES DES MODES ---
// ============================================================

// --- MENU PRINCIPAL ---
void gestionMenuPrincipal() {
  if (encoderDelta != 0) {
    if (encoderDelta > 0) indexMenu++; else indexMenu--;
    encoderDelta = 0;
    if (indexMenu > 2) indexMenu = 0;
    if (indexMenu < 0) indexMenu = 2;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(30, 0); display.println(F("MENU RADIO"));
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  
  display.setCursor(10, 20);
  if(indexMenu == 0) display.print(F("> ")); display.println(F("LIRE MSG"));
  
  display.setCursor(10, 35);
  if(indexMenu == 1) display.print(F("> ")); display.println(F("ECRIRE MSG"));
  
  display.setCursor(10, 50);
  if(indexMenu == 2) display.print(F("> ")); display.println(F("PARAMETRES"));

  display.display();

  // Validation par Court ou Aux
  if (flagClickCourt || flagAux) {
    bip(0);
    if (indexMenu == 0) etatGlobal = MODE_LIRE;
    else if (indexMenu == 1) {
        etatEcriture = E_SAISIE;
        memset(monMessage.texte, 0, sizeof(monMessage.texte));
        positionCurseurMessage = 0;
        etatGlobal = MODE_ECRIRE;
    }
    else if (indexMenu == 2) {
        modeParam = P_CANAL;
        etatGlobal = MODE_PARAMETRES;
    }
  }
}

// --- MODE LIRE ---
void gestionLire() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(30, 25);
  display.println(F("HELLO"));
  
  display.setTextSize(1);
  display.setCursor(0, 55);
  display.println(F("Btn Aux -> Retour"));
  display.display();

  // Sortie par bouton Aux
  if (flagAux) {
    bip(2);
    etatGlobal = MENU_PRINCIPAL;
  }
}

// --- MODE PARAMETRES ---
void gestionParametres() {
  
  // 1. CONFIG CANAL
  if (modeParam == P_CANAL) {
    if (encoderDelta != 0) {
      canal += encoderDelta;
      encoderDelta = 0;
      if (canal < 0) canal = 125;
      if (canal > 125) canal = 0;
    }
    
    display.clearDisplay();
    display.setCursor(0, 0); display.println(F("PARAM: CANAL"));
    display.setTextSize(2); display.setCursor(45, 25); display.println(canal);
    display.setTextSize(1); display.setCursor(0, 55); display.println(F("Aux=Suivant"));
    display.display();

    // Aux valide et passe au suivant
    if (flagAux) { 
      bip(1); 
      modeParam = P_PSEUDO; 
      radio.setChannel(canal); 
    }
  }

  // 2. CONFIG PSEUDO
  else if (modeParam == P_PSEUDO) {
    if (encoderDelta != 0) {
      indexLettre += encoderDelta;
      encoderDelta = 0;
      int maxLen = strlen(lettresAutorisees);
      if (indexLettre >= maxLen) indexLettre = 0;
      if (indexLettre < 0) indexLettre = maxLen - 1;
      lettreActuelle = lettresAutorisees[indexLettre];
    }

    display.clearDisplay();
    display.setCursor(0, 0); display.println(F("PARAM: PSEUDO"));
    display.setCursor(0, 20); display.print(F("Nom: ")); display.println(pseudo);
    
    // Selecteur
    display.setCursor(0, 40); display.print(F("Let: [")); 
    display.setTextSize(2); display.print(lettreActuelle); 
    display.setTextSize(1); display.println(F("]"));
    
    display.setCursor(0, 56); display.println(F("Clik=Add Long=Del"));

    display.display();

    // --- LOGIQUE BOUTONS DEMANDEE ---
    
    // Court : Ajouter lettre
    if (flagClickCourt) {
       int len = strlen(pseudo);
       if (len < 10) {
         pseudo[len] = lettreActuelle;
         pseudo[len+1] = '\0';
         bip(0);
       } else bip(2);
    }
    
    // Long : Supprimer lettre (Backspace)
    if (flagClickLong) {
      int len = strlen(pseudo);
      if (len > 0) {
        pseudo[len - 1] = '\0';
        bip(2);
      }
    }

    // Aux : Valider et Suivant
    if (flagAux) {
      bip(1);
      modeParam = P_BUZZER;
    }
  }

  // 3. CONFIG BUZZER
  else if (modeParam == P_BUZZER) {
    if (encoderDelta != 0) {
       if (encoderDelta > 0) modeBuzzer++; else modeBuzzer--;
       encoderDelta = 0;
       if (modeBuzzer > 2) modeBuzzer = 0;
       if (modeBuzzer < 0) modeBuzzer = 2;
    }
    
    display.clearDisplay();
    display.setCursor(0, 0); display.println(F("PARAM: SON"));
    display.setTextSize(2); display.setCursor(20, 25);
    if (modeBuzzer == 0) display.println(F("Court"));
    else if (modeBuzzer == 1) display.println(F("Long"));
    else display.println(F("Double"));
    display.setTextSize(1); display.setCursor(0, 55); display.println(F("Aux=Sauver & Quitter"));
    display.display();

    // Aux valide tout et quitte
    if (flagAux) { 
      bip(1); 
      sauvegarderParams(); 
      etatGlobal = MENU_PRINCIPAL; 
    }
  }
}

// --- MODE ECRITURE ---
void gestionEcriture() {

  // 1. SAISIE TEXTE
  if (etatEcriture == E_SAISIE) {
    if (encoderDelta != 0) {
      indexCaractereSaisie += encoderDelta;
      encoderDelta = 0;
      int maxLen = strlen(listeCaracteresSaisie);
      if (indexCaractereSaisie >= maxLen) indexCaractereSaisie = 0;
      if (indexCaractereSaisie < 0) indexCaractereSaisie = maxLen - 1;
    }

    display.clearDisplay();
    // Affichage lettre courante
    display.setTextSize(2); display.setCursor(58, 5); 
    display.print(listeCaracteresSaisie[indexCaractereSaisie]);
    
    // Affichage message
    display.setTextSize(1); display.setCursor(0, 30); 
    display.print(F(">")); display.println(monMessage.texte);
    
    display.drawLine(0, 48, 128, 48, SSD1306_WHITE);
    display.setCursor(0, 54); display.print(F("Clik:Add  Long:Del"));
    display.display();

    // --- LOGIQUE BOUTONS DEMANDEE ---
    
    // Court : Ajouter caractère
    if (flagClickCourt) {
       if (positionCurseurMessage < 99) {
         monMessage.texte[positionCurseurMessage] = listeCaracteresSaisie[indexCaractereSaisie];
         positionCurseurMessage++;
         monMessage.texte[positionCurseurMessage] = '\0';
         bip(0);
       }
    }
    
    // Long : Supprimer caractère (Backspace)
    if (flagClickLong) {
      if (positionCurseurMessage > 0) {
        positionCurseurMessage--;
        monMessage.texte[positionCurseurMessage] = '\0';
        bip(2);
      }
    }

    // Aux : Valider le message et passer à Priorité
    if (flagAux) {
       if (strlen(monMessage.texte) > 0) { // Si message non vide
         etatEcriture = E_PRIORITE;
         bip(1);
       } else {
         etatGlobal = MENU_PRINCIPAL; // Si vide, on annule/sort
         bip(2);
       }
    }
  }

  // 2. CHOIX PRIORITE
  else if (etatEcriture == E_PRIORITE) {
    if (encoderDelta != 0) {
       monMessage.priorite += (encoderDelta > 0 ? 1 : -1);
       encoderDelta = 0;
       if (monMessage.priorite > 3) monMessage.priorite = 1;
       if (monMessage.priorite < 1) monMessage.priorite = 3;
    }
    
    display.clearDisplay();
    display.setCursor(0,0); display.println(F("PRIORITE ?"));
    display.setTextSize(2); display.setCursor(20, 30);
    if(monMessage.priorite == 1) display.println(F("URGENT"));
    else if(monMessage.priorite == 2) display.println(F("NORMAL"));
    else display.println(F("INFO"));
    
    display.setTextSize(1); display.setCursor(0, 55); display.println(F("Aux = ENVOYER"));
    display.display();

    // Bouton Aux envoie le message
    if (flagAux) {
       etatEcriture = E_ENVOI;
       bip(1);
    }
    // Bouton Encodeur permet d'annuler/retourner modif message si besoin
    if (flagClickLong) {
       etatEcriture = E_SAISIE;
       bip(2);
    }
  }

  // 3. ENVOI RADIO
  else if (etatEcriture == E_ENVOI) {
    display.clearDisplay();
    display.setCursor(10, 20); display.println(F("ENVOI EN COURS..."));
    display.display();
    
    String paquet = String(pseudo) + ": ";
    if (monMessage.priorite == 1) paquet += "[URGENT] ";
    paquet += String(monMessage.texte);

    char buffer[32];
    paquet.toCharArray(buffer, 32);
    
    radio.stopListening();
    bool ok = radio.write(&buffer, sizeof(buffer));
    
    display.clearDisplay();
    display.setCursor(10, 20); 
    if(ok) display.println(F("ENVOI REUSSI!"));
    else display.println(F("ECHEC ENVOI"));
    display.display();
    
    bip(ok ? 1 : 2);
    delay(1500); 
    etatGlobal = MENU_PRINCIPAL; 
  }
}

// ============================================================
// --- SETUP & LOOP ---
// ============================================================

void setup() {
  pinMode(pinCLK, INPUT_PULLUP);
  pinMode(pinDT, INPUT_PULLUP);
  pinMode(pinSW, INPUT_PULLUP);
  pinMode(pinBuzzer, OUTPUT);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    for (;;); 
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(20, 20); display.println(F("SYSTEME PRET"));
  display.display();
  delay(1000);

  chargerParams();
  radio.begin();
  radio.setPALevel(RF24_PA_LOW); 
  radio.openWritingPipe(adresse);
  radio.setChannel(canal);

  attachInterrupt(digitalPinToInterrupt(pinCLK), isrEncodeur, CHANGE);
  monMessage.priorite = 2;
}

void loop() {
  lireBoutons(); // Analyse les appuis courts/longs/aux

  switch (etatGlobal) {
    case MENU_PRINCIPAL:
      gestionMenuPrincipal();
      break;
    case MODE_LIRE:
      gestionLire();
      break;
    case MODE_ECRIRE:
      gestionEcriture();
      break;
    case MODE_PARAMETRES:
      gestionParametres();
      break;
  }
  
  delay(10); // Stabilite
}