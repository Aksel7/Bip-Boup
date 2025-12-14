/*
 * PROJET FINAL : VERSION 6 (LISIBLE & CORRIGEE)
 * ---------------------------------------------
 * - Validation EF6 (Son), EF7 (LED Couleur), EF8 (Arrêt), EF9 (Autonome)
 * - Priorités : URGENT / NORMAL / BASSE
 * - Code déplié pour une lecture facile
 */

#include <SPI.h>
#include <RF24.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================
// --- 1. CONFIGURATION DU MATERIEL ---
// ============================================================

// --- ECRAN OLED ---
#define LARGEUR_ECRAN 128
#define HAUTEUR_ECRAN 64
#define OLED_ADDR 0x3C 
Adafruit_SSD1306 display(LARGEUR_ECRAN, HAUTEUR_ECRAN, &Wire, -1);

// --- RADIO NRF24L01 ---
RF24 radio(7, 8); // Broches CE, CSN
const byte adresse[6] = "PIPE1"; 

// --- BOUTONS & BUZZER ---
const int pinCLK = 3;         // Encodeur A
const int pinDT  = 4;         // Encodeur B
const int pinSW  = 2;         // Bouton Encodeur
const int pinBoutonAux = A6;  // Bouton Rouge (Retour/Envoi)
const int pinBuzzer = 10;     // Buzzer

// --- LED RGB (Validation EF7) ---
// Branchement : R->D5, V->D6, B->D9
const int pinLedRouge = 5;
const int pinLedVerte = 6;
const int pinLedBleue = 9;

// Réglage sensibilité encodeur (plus c'est haut, plus c'est lent)
const int SENSIBILITE_ENCODEUR = 2; 

// ============================================================
// --- 2. VARIABLES GLOBALES ---
// ============================================================

// --- Navigation ---
enum GlobalState { MENU_PRINCIPAL, MODE_LIRE, MODE_ECRIRE, MODE_PARAMETRES };
GlobalState etatGlobal = MENU_PRINCIPAL; 
int indexMenu = 0; 

// --- Encodeur ---
volatile int encoderDelta = 0; 

// --- Drapeaux Boutons ---
bool flagClickCourt = false;   
bool flagClickLong = false;    
bool flagAux = false;          

// --- Réception ---
char dernierMessageRecu[33] = ""; 
bool nouveauMessageRecu = false;  

// --- Paramètres ---
enum ModeParam { P_CANAL, P_PSEUDO, P_BUZZER };
ModeParam modeParam = P_CANAL;

int canal = 0;                  
char pseudo[11] = "NanoUser";   
int modeBuzzer = 0; // 0=Court, 1=Long, 2=Double

char lettreActuelle = 'A';
int indexLettre = 0;
const char lettresAutorisees[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 .,!?;:@#&+-*/";

// --- Ecriture Message ---
struct Message {
  char texte[101]; 
  int priorite; // 1=URGENT, 2=NORMAL, 3=BASSE
};
Message monMessage; 

int positionCurseurMessage = 0; 
int indexCaractereSaisie = 0;   
const char* listeCaracteresSaisie = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .,!?'@";

enum EtatEcriture { E_SAISIE, E_PRIORITE, E_ENVOI };
EtatEcriture etatEcriture = E_SAISIE;

// --- Adresses Mémoire ---
const int addrCanal = 0;
const int addrBuzzer = 10;
const int addrPseudo = 20;

// ============================================================
// --- 3. FONCTIONS OUTILS ---
// ============================================================

// --- Interruption Encodeur ---
void isrEncodeur() {
  static unsigned long lastInterruptTime = 0;
  unsigned long interruptTime = millis();
  
  if (interruptTime - lastInterruptTime > 5) {
    int direction = 0;
    if (digitalRead(pinDT) != digitalRead(pinCLK)) {
      direction = 1;
    } else {
      direction = -1;
    }

    static int compteurBrut = 0;
    compteurBrut += direction;

    if (compteurBrut >= SENSIBILITE_ENCODEUR) {
      encoderDelta++;
      compteurBrut = 0;
    } 
    else if (compteurBrut <= -SENSIBILITE_ENCODEUR) {
      encoderDelta--;
      compteurBrut = 0;
    }
    lastInterruptTime = interruptTime;
  }
}

// --- Gestion des LEDs (EF7 & EF8) ---
void allumerLedPriorite(int priorite) {
  // Etape 1 : On éteint tout
  digitalWrite(pinLedRouge, LOW);
  digitalWrite(pinLedVerte, LOW);
  digitalWrite(pinLedBleue, LOW);

  // Etape 2 : On allume selon la priorité
  if (priorite == 1) { 
    // URGENT -> ROUGE
    digitalWrite(pinLedRouge, HIGH);
  } 
  else if (priorite == 2) { 
    // NORMAL -> VERT
    digitalWrite(pinLedVerte, HIGH);
  } 
  else if (priorite == 3) { 
    // BASSE -> BLEU
    digitalWrite(pinLedBleue, HIGH);
  }
  // Si priorite est 0, tout reste éteint (Arrêt sur commande)
}

// --- Gestion du Buzzer ---
void bip(int type) {
  if (type == 0) { 
    // Click simple
    tone(pinBuzzer, 2000, 30); 
  } 
  else if (type == 1) { 
    // Succès
    if (modeBuzzer >= 1) { 
      tone(pinBuzzer, 1000, 100); 
      delay(100); 
      tone(pinBuzzer, 2000, 200); 
    } else {
      tone(pinBuzzer, 2000, 100);
    }
  } 
  else if (type == 2) { 
    // Erreur / Retour
    tone(pinBuzzer, 500, 100); 
  }
  else if (type == 3) { 
    // RECEPTION MESSAGE (Alerte)
    tone(pinBuzzer, 3000, 50); 
    delay(50); 
    tone(pinBuzzer, 3000, 50); 
  }
}

// --- Sauvegarde & Chargement ---
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

// --- Lecture des Boutons ---
void lireBoutons() {
  flagClickCourt = false;
  flagClickLong = false;
  flagAux = false;

  static unsigned long debutAppuiSW = 0;
  static bool appuiEnCoursSW = false;
  static bool longPressTraite = false;
  int etatSW = digitalRead(pinSW);

  // Gestion Encodeur (Click)
  if (etatSW == LOW) { 
    if (!appuiEnCoursSW) {
      debutAppuiSW = millis();
      appuiEnCoursSW = true;
      longPressTraite = false;
    } else {
      if (!longPressTraite && (millis() - debutAppuiSW > 600)) {
        flagClickLong = true; 
        longPressTraite = true; 
      }
    }
  } else { 
    if (appuiEnCoursSW) {
      if (!longPressTraite) {
        flagClickCourt = true;
      }
      appuiEnCoursSW = false;
    }
  }

  // Gestion Bouton Aux (Rouge)
  static bool relacheAux = true;
  if (analogRead(pinBoutonAux) < 500) { 
    if (relacheAux) {
      flagAux = true;
      relacheAux = false;
      delay(50); 
    }
  } else {
    relacheAux = true;
  }
}

// --- Tâche de fond : Réception Radio ---
void verifierReceptionRadio() {
  // On n'écoute pas si on est en train d'envoyer
  if (etatGlobal == MODE_ECRIRE && etatEcriture == E_ENVOI) return;

  if (radio.available()) {
    char buffer[32] = ""; 
    radio.read(&buffer, sizeof(buffer)); 
    
    strcpy(dernierMessageRecu, buffer);
    nouveauMessageRecu = true;
    
    // --- ANALYSE DE LA PRIORITE POUR LA LED ---
    if (strstr(buffer, "[URGENT]") != NULL) {
      allumerLedPriorite(1); // Rouge
    } 
    else if (strstr(buffer, "[BASSE]") != NULL) {
      allumerLedPriorite(3); // Bleu
    } 
    else {
      allumerLedPriorite(2); // Vert (Normal)
    }
    
    bip(3); // Alerte Sonore
  }
}

// ============================================================
// --- 4. GESTION DES MENUS ---
// ============================================================

void gestionMenuPrincipal() {
  // --- Navigation avec l'encodeur ---
  if (encoderDelta != 0) {
    if (encoderDelta > 0) indexMenu++; else indexMenu--;
    encoderDelta = 0; 
    
    if (indexMenu > 2) indexMenu = 0;
    if (indexMenu < 0) indexMenu = 2;
  }

  // --- Affichage ---
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  display.setCursor(30, 0); 
  display.println(F("MENU RADIO"));
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE); 
  
  display.setCursor(10, 20);
  if(indexMenu == 0) display.print(F("> ")); 
  display.print(F("LIRE MSG"));
  if(nouveauMessageRecu) display.print(F(" [!]")); // Petit indicateur si nouveau message
  
  display.setCursor(10, 35);
  if(indexMenu == 1) display.print(F("> ")); 
  display.println(F("ECRIRE MSG"));
  
  display.setCursor(10, 50);
  if(indexMenu == 2) display.print(F("> ")); 
  display.println(F("PARAMETRES"));

  display.display();

  // --- Validation (Click) ---
  if (flagClickCourt || flagAux) {
    bip(0); // Petit bip de validation touche
    
    if (indexMenu == 0) {
      // --- C'EST ICI QUE L'ALARME S'ARRETE ---
      etatGlobal = MODE_LIRE;
      
      // 1. On dit qu'on a vu le message (enlève le [!] du menu)
      nouveauMessageRecu = false; 
      
      // 2. On éteint les LEDs immédiatement (Validation EF8)
      allumerLedPriorite(0); 

      // 3. On coupe le son immédiatement (au cas où ça sonne encore)
      noTone(pinBuzzer);
      // ---------------------------------------
    }
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

void gestionLire() {
  display.clearDisplay();
  
  if (strlen(dernierMessageRecu) == 0) {
    display.setTextSize(1);
    display.setCursor(20, 30); 
    display.println(F("AUCUN MESSAGE"));
  } 
  else {
    // Découpage "Pseudo : Message"
    String messageComplet = String(dernierMessageRecu);
    int positionDeuxPoints = messageComplet.indexOf(':');
    
    String expediteur = ""; 
    String contenu = "";
    
    if (positionDeuxPoints != -1) {
      expediteur = messageComplet.substring(0, positionDeuxPoints);
      contenu = messageComplet.substring(positionDeuxPoints + 1); 
    } else {
      expediteur = "Inconnu";
      contenu = messageComplet;
    }

    // Affichage
    display.setTextSize(1);
    display.setCursor(0, 0); 
    display.print(F("De : ")); display.println(expediteur);
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
    
    display.setCursor(0, 15);
    display.println(contenu);
  }

  display.drawLine(0, 53, 128, 53, SSD1306_WHITE);
  display.setCursor(0, 56);
  display.println(F("Aux -> Retour Menu"));
  display.display();

  if (flagAux) {
    bip(2);
    etatGlobal = MENU_PRINCIPAL;
  }
}

void gestionParametres() {
  
  // --- MENU CANAL ---
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

    if (flagAux) { 
      bip(1); 
      modeParam = P_PSEUDO; 
      radio.setChannel(canal); 
    }
  }

  // --- MENU PSEUDO ---
  else if (modeParam == P_PSEUDO) {
    if (encoderDelta != 0) {
      indexLettre += encoderDelta;
      encoderDelta = 0;
      int maxLen = strlen(lettresAutorisees);
      
      if (indexLettre < 0) indexLettre = maxLen - 1;
      if (indexLettre >= maxLen) indexLettre = 0;
      
      lettreActuelle = lettresAutorisees[indexLettre];
    }

    display.clearDisplay();
    display.setCursor(0, 0); display.println(F("PARAM: PSEUDO"));
    display.setCursor(0, 20); display.print(F("Nom: ")); display.println(pseudo);
    
    display.setCursor(0, 40); display.print(F("Let: [")); 
    display.setTextSize(2); display.print(lettreActuelle); 
    display.setTextSize(1); display.println(F("]"));
    
    display.setCursor(0, 56); display.println(F("Clik=Add Long=Del"));
    display.display();

    if (flagClickCourt) { 
      int len = strlen(pseudo);
      if (len < 10) { 
        pseudo[len] = lettreActuelle; 
        pseudo[len+1] = '\0'; 
        bip(0); 
      } else {
        bip(2);
      }
    }
    if (flagClickLong) { 
      int len = strlen(pseudo);
      if (len > 0) { 
        pseudo[len - 1] = '\0'; 
        bip(2); 
      }
    }
    if (flagAux) { 
      bip(1); 
      modeParam = P_BUZZER; 
    }
  }

  // --- MENU SON ---
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
    
    display.setTextSize(1); display.setCursor(0, 55); display.println(F("Aux=Quitter"));
    display.display();

    if (flagAux) { 
      bip(1); 
      sauvegarderParams(); 
      etatGlobal = MENU_PRINCIPAL; 
    }
  }
}

void gestionEcriture() {
  
  // --- ETAPE 1 : SAISIE ---
  if (etatEcriture == E_SAISIE) {
    if (encoderDelta != 0) {
      indexCaractereSaisie += encoderDelta;
      encoderDelta = 0;
      int maxLen = strlen(listeCaracteresSaisie);
      
      if (indexCaractereSaisie < 0) indexCaractereSaisie = maxLen - 1;
      if (indexCaractereSaisie >= maxLen) indexCaractereSaisie = 0;
    }

    display.clearDisplay();
    display.setTextSize(2); display.setCursor(58, 5); 
    display.print(listeCaracteresSaisie[indexCaractereSaisie]);
    
    display.setTextSize(1); display.setCursor(0, 30); 
    display.print(F(">")); display.println(monMessage.texte);
    
    display.drawLine(0, 48, 128, 48, SSD1306_WHITE);
    display.setCursor(0, 54); display.print(F("Clik:Add  Long:Del"));
    display.display();

    if (flagClickCourt) { 
       if (positionCurseurMessage < 99) {
         monMessage.texte[positionCurseurMessage] = listeCaracteresSaisie[indexCaractereSaisie];
         positionCurseurMessage++;
         monMessage.texte[positionCurseurMessage] = '\0';
         bip(0);
       }
    }
    if (flagClickLong) { 
      if (positionCurseurMessage > 0) {
        positionCurseurMessage--;
        monMessage.texte[positionCurseurMessage] = '\0';
        bip(2);
      }
    }
    if (flagAux) { 
       if (strlen(monMessage.texte) > 0) {
         etatEcriture = E_PRIORITE; 
         bip(1);
       } else {
         etatGlobal = MENU_PRINCIPAL; 
         bip(2);
       }
    }
  }

  // --- ETAPE 2 : PRIORITE ---
  else if (etatEcriture == E_PRIORITE) {
    if (encoderDelta != 0) {
       if (encoderDelta > 0) monMessage.priorite++; else monMessage.priorite--;
       encoderDelta = 0;
       
       if (monMessage.priorite > 3) monMessage.priorite = 1;
       if (monMessage.priorite < 1) monMessage.priorite = 3;
    }
    
    display.clearDisplay();
    display.setCursor(0,0); display.println(F("PRIORITE ?"));
    display.setTextSize(2); display.setCursor(20, 30);
    
    if (monMessage.priorite == 1) display.println(F("URGENT"));
    else if (monMessage.priorite == 2) display.println(F("NORMAL"));
    else display.println(F("BASSE")); // <-- MODIFICATION ICI
    
    display.setTextSize(1); display.setCursor(0, 55); display.println(F("Aux = ENVOYER"));
    display.display();

    if (flagAux) { 
      etatEcriture = E_ENVOI; 
      bip(1); 
    }
    if (flagClickLong) { 
      etatEcriture = E_SAISIE; 
      bip(2); 
    }
  }

  // --- ETAPE 3 : ENVOI ---
  else if (etatEcriture == E_ENVOI) {
    display.clearDisplay();
    display.setCursor(10, 20); display.println(F("ENVOI EN COURS..."));
    display.display();
    
    String paquet = String(pseudo) + ": ";
    
    // Ajout du tag de priorité
    if (monMessage.priorite == 1) paquet += "[URGENT] ";
    if (monMessage.priorite == 3) paquet += "[BASSE] "; // <-- MODIFICATION ICI
    
    paquet += String(monMessage.texte);

    char buffer[32]; 
    paquet.toCharArray(buffer, 32);
    
    radio.stopListening(); 
    bool ok = radio.write(&buffer, sizeof(buffer));
    radio.startListening(); 
    
    display.clearDisplay();
    display.setCursor(10, 20); 
    if(ok) display.println(F("ENVOI REUSSI!"));
    else display.println(F("ECHEC ENVOI"));
    display.display();
    
    if(ok) bip(1); else bip(2);
    delay(1500); 
    etatGlobal = MENU_PRINCIPAL; 
  }
}

// ============================================================
// --- 5. SETUP & LOOP ---
// ============================================================

void setup() {
  pinMode(pinCLK, INPUT_PULLUP);
  pinMode(pinDT, INPUT_PULLUP);
  pinMode(pinSW, INPUT_PULLUP);
  pinMode(pinBuzzer, OUTPUT);

  // Init LEDs
  pinMode(pinLedRouge, OUTPUT);
  pinMode(pinLedVerte, OUTPUT);
  pinMode(pinLedBleue, OUTPUT);

  // Animation LEDs au démarrage
  digitalWrite(pinLedRouge, HIGH); delay(200); digitalWrite(pinLedRouge, LOW);
  digitalWrite(pinLedVerte, HIGH); delay(200); digitalWrite(pinLedVerte, LOW);
  digitalWrite(pinLedBleue, HIGH); delay(200); digitalWrite(pinLedBleue, LOW);

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
  radio.openReadingPipe(1, adresse); 
  radio.setChannel(canal);
  radio.startListening(); 

  attachInterrupt(digitalPinToInterrupt(pinCLK), isrEncodeur, CHANGE);
  monMessage.priorite = 2;
}

void loop() {
  // 1. Lire les boutons
  lireBoutons(); 
  
  // 2. Vérifier si un message arrive
  verifierReceptionRadio(); 

  // 3. Gérer l'affichage selon le mode
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
  
  delay(10); 
} 