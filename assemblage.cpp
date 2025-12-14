/*
 * PROJET FINAL : Terminal de Messagerie LoRa/NRF24
 * Fusion : Menu Principal + Saisie (Assemblage) + Paramètres (FS5)
 * Matériel : Arduino Nano, NRF24L01, OLED SSD1306, Encodeur Rotatif, Buzzer
 */

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RF24.h>
#include <EEPROM.h>

// ============================================================
// --- CONFIGURATION MATERIEL (PINS) ---
// ============================================================

// --- Ecran OLED ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- Radio NRF24L01 ---
#define PIN_CE  7
#define PIN_CSN 8
RF24 radio(PIN_CE, PIN_CSN);
const byte adresse[6] = "PIPE1"; 

// --- Encodeur & Boutons ---
const int PIN_ENC_CLK = 3;  // Interruption
const int PIN_ENC_DT  = 4; 
const int PIN_BTN_SW  = 2;  // Bouton Encodeur
const int PIN_BTN_AUX = A6; // Bouton Rouge/Auxiliaire
const int PIN_LED_FB  = 5;  // LED Feedback
const int PIN_BUZZER  = 10; // Buzzer Passif

// ============================================================
// --- VARIABLES GLOBALES & ETATS ---
// ============================================================

// --- Machine d'état Globale ---
enum GlobalState {
  MENU_PRINCIPAL,
  ECRIRE_MESSAGE,
  LIRE_MESSAGE,
  PARAMETRES
};
GlobalState etatGlobal = MENU_PRINCIPAL;

// --- Données Persistantes (EEPROM) ---
// Adresses EEPROM
const int ADDR_CANAL = 0;
const int ADDR_BUZZER = 10;
const int ADDR_PSEUDO = 20;

// Variables configurables
int radioCanal = 0;
int modeBuzzer = 0; // 0=Court, 1=Long, 2=Double
char pseudoGlobal[11] = "User"; // Max 10 chars + null

// --- Variables de Gestion Encodeur ---
volatile int encoderDelta = 0; // Compteur de pas
int lastEncoded = 0;

// --- Variables Menu Principal ---
const char* menuItems[] = {"LIRE MSG", "ECRIRE MSG", "PARAMETRES"};
int menuIndex = 0;

// --- Variables "Ecrire Message" ---
const char* listeCaracteres = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .,!?'@";
const int tailleListeCaracteres = 43;
struct MessageData {
  char texte[101]; 
  int priorite; // 1=Urgent, 2=Normal, 3=Basse
};
MessageData monMessage; 
int msgCursorPos = 0;
int charSelectIndex = 0;
int etatEcriture = 0; // 0=Saisie, 1=Priorité, 2=Envoi
int dernierIndexAffiche = -1;

// --- Variables "Paramètres" ---
int paramSubState = 0; // 0=Canal, 1=Pseudo, 2=Son
int paramCharIndex = 0;
const char paramLettres[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

// ============================================================
// --- PROTOTYPES ---
// ============================================================
void isrEncoder();
void bip(int type); // 0=Valid, 1=Erreur, 2=Menu
void sauvegarderParams();
void chargerParams();
void afficherMenuPrincipal();
void logiqueMenuPrincipal();
void logiqueEcrireMessage();
void logiqueParametres();
void logiqueLireMessage();
void envoyerMessageRadio();
void envoyer_paquet_fragment(char type, String texte_a_envoyer);
bool checkBoutonAuxLong(); // Retour True si appui long (Retour Menu)

// ============================================================
// --- SETUP ---
// ============================================================
void setup() {
  Serial.begin(9600);
  
  // Init Pins
  pinMode(PIN_ENC_CLK, INPUT_PULLUP);
  pinMode(PIN_ENC_DT, INPUT_PULLUP);
  pinMode(PIN_BTN_SW, INPUT_PULLUP);
  pinMode(PIN_LED_FB, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  
  // Init Ecran
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) { 
    for(;;); // Bloque si erreur
  }
  display.cp437(true); 
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();

  // Chargement Paramètres
  chargerParams();

  // Init Radio
  radio.begin();
  radio.setChannel(radioCanal);
  radio.openWritingPipe(adresse); 
  radio.setPALevel(RF24_PA_MIN);
  radio.stopListening();

  // Interruptions
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_CLK), isrEncoder, CHANGE);

  // Splash Screen
  display.setTextSize(1);
  display.setCursor(20, 20); display.println(F("SYSTEME PRET"));
  display.setCursor(20, 35); display.print(F("Pseudo: ")); display.println(pseudoGlobal);
  display.display();
  delay(1500);
  bip(0);
}

// ============================================================
// --- LOOP ---
// ============================================================
void loop() {
  // Sécurité Globale : Appui long sur A6 ramène au menu
  if (checkBoutonAuxLong()) {
    etatGlobal = MENU_PRINCIPAL;
    bip(2);
    delay(500); // Anti-rebond après reset
  }

  switch (etatGlobal) {
    case MENU_PRINCIPAL:
      logiqueMenuPrincipal();
      break;
    
    case ECRIRE_MESSAGE:
      logiqueEcrireMessage();
      break;

    case LIRE_MESSAGE:
      logiqueLireMessage();
      break;

    case PARAMETRES:
      logiqueParametres();
      break;
  }
}

// ============================================================
// --- FONCTIONS LOGIQUES PAR ETAT ---
// ============================================================

// --- 1. MENU PRINCIPAL ---
void logiqueMenuPrincipal() {
  // Gestion Navigation
  if (encoderDelta != 0) {
    if (encoderDelta > 0) menuIndex++;
    else menuIndex--;
    encoderDelta = 0; // Reset delta
    
    if (menuIndex > 2) menuIndex = 0;
    if (menuIndex < 0) menuIndex = 2;
    bip(2);
  }

  // Affichage
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0); display.println(F("--- MENU PRINCIPAL ---"));
  
  for(int i=0; i<3; i++) {
    if (i == menuIndex) {
       display.fillRect(0, 20 + (i*15), 128, 14, SSD1306_WHITE);
       display.setTextColor(SSD1306_BLACK);
    } else {
       display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(5, 23 + (i*15));
    display.println(menuItems[i]);
  }
  display.setTextColor(SSD1306_WHITE);
  display.display();

  // Validation
  if (digitalRead(PIN_BTN_SW) == LOW) {
    delay(50); // Debounce
    while(digitalRead(PIN_BTN_SW) == LOW); // Attente relachement
    
    bip(0);
    if (menuIndex == 0) etatGlobal = LIRE_MESSAGE;
    if (menuIndex == 1) {
      etatGlobal = ECRIRE_MESSAGE;
      // Reset variables écriture
      msgCursorPos = 0;
      memset(monMessage.texte, 0, sizeof(monMessage.texte));
      etatEcriture = 0; 
      monMessage.priorite = 2;
      dernierIndexAffiche = -1;
    }
    if (menuIndex == 2) {
      etatGlobal = PARAMETRES;
      paramSubState = 0; // Commencer par Canal
    }
  }
}

// --- 2. ECRIRE MESSAGE (Adapté de assemblage.cpp) ---
void logiqueEcrireMessage() {
  // Navigation Carrousel
  if (etatEcriture == 0 || etatEcriture == 1) { // Saisie ou Priorité
    if (encoderDelta != 0) {
       if (encoderDelta > 0) charSelectIndex++;
       else charSelectIndex--;
       encoderDelta = 0;
       
       int max = (etatEcriture == 0) ? tailleListeCaracteres : 4; // 4 choix en priorité
       if (charSelectIndex >= max) charSelectIndex = 0;
       if (charSelectIndex < 0) charSelectIndex = max - 1;
    }
  }

  // --- Affichage ---
  if (charSelectIndex != dernierIndexAffiche || etatEcriture == 2) {
    display.clearDisplay();
    
    if (etatEcriture == 0) { // SAISIE TEXTE
      // Bandeau sélection
      int idxPrec = (charSelectIndex - 1 + tailleListeCaracteres) % tailleListeCaracteres;
      int idxSuiv = (charSelectIndex + 1) % tailleListeCaracteres;
      
      display.setTextSize(1);
      display.setCursor(10, 12); display.print(listeCaracteres[idxPrec]);
      display.setCursor(110, 12); display.print(listeCaracteres[idxSuiv]);
      
      display.setTextSize(2);
      display.setCursor(58, 8); display.print(listeCaracteres[charSelectIndex]);
      
      display.setTextSize(1);
      display.drawLine(0, 30, 128, 30, SSD1306_WHITE);
      display.setCursor(0, 35); display.print(monMessage.texte);
      display.print(F("_")); // Curseur

      display.setCursor(0,0); display.print(F("Saisie... Btn2=Eff"));
    }
    else if (etatEcriture == 1) { // CHOIX PRIORITE
      display.setTextSize(1);
      display.setCursor(0,0); display.println(F("PRIORITE ?"));
      display.setTextSize(2);
      display.setCursor(10,30);
      
      // Index 1=Urgent, 2=Normal, 3=Basse (On mappe 0..3 vers 1..3 pour simplifier l'UI)
      if (charSelectIndex == 1) display.print(F("<URGENT>"));
      else if (charSelectIndex == 2) display.print(F("<NORMAL>"));
      else display.print(F("<BASSE>"));
    }
    else if (etatEcriture == 2) { // ENVOI
      display.setTextSize(1);
      display.setCursor(30, 30); display.println(F("ENVOI..."));
    }
    display.display();
    dernierIndexAffiche = charSelectIndex;
  }

  // --- Actions Bouton Encodeur (Valider) ---
  if (digitalRead(PIN_BTN_SW) == LOW) {
    delay(100); while(digitalRead(PIN_BTN_SW) == LOW); // Attente relachement
    
    if (etatEcriture == 0) {
      // Ajout caractère
      if (msgCursorPos < 100) {
        monMessage.texte[msgCursorPos] = listeCaracteres[charSelectIndex];
        msgCursorPos++;
        monMessage.texte[msgCursorPos] = '\0';
        dernierIndexAffiche = -1; // Force refresh
      }
    } else if (etatEcriture == 1) {
      // Valider Priorité -> Envoi
      monMessage.priorite = charSelectIndex; // A ajuster selon logique
      if (monMessage.priorite < 1) monMessage.priorite = 2; 
      
      etatEcriture = 2;
      dernierIndexAffiche = -1;
      envoyerMessageRadio();
      delay(2000);
      etatGlobal = MENU_PRINCIPAL; // Retour menu après envoi
    }
  }

  // --- Actions Bouton Auxiliaire (Effacer / Suivant) ---
  if (analogRead(PIN_BTN_AUX) < 500) {
    delay(100); while(analogRead(PIN_BTN_AUX) < 500); // Attente

    if (etatEcriture == 0) {
      if (msgCursorPos > 0) {
        // Backspace
        msgCursorPos--;
        monMessage.texte[msgCursorPos] = '\0';
        dernierIndexAffiche = -1;
        bip(2);
      } else {
         // Si texte vide, on passe à l'étape suivante (Validation du msg vide ou non)
         // Ici on décide que btn Aux vide = Passer à Priorité
         if (strlen(monMessage.texte) > 0) {
            etatEcriture = 1;
            charSelectIndex = 2; // Defaut Normal
            dernierIndexAffiche = -1;
            bip(0);
         }
      }
    }
    // Note: Pour valider la saisie et aller à priorité, 
    // l'utilisateur peut appuyer longuement sur bouton enc 
    // ou on peut ajouter un caractere special "OK" dans la liste.
    // Pour simplifier ici: Si on appuie sur Encodeur sur un ' ' (espace) à la fin ?
    // Ajoutons plutot la logique : Appui long SW = Passer à priorité.
  }
  
  // Raccourci Validation Saisie : Appui Long Encodeur SW
  static unsigned long timerSW = 0;
  if (digitalRead(PIN_BTN_SW) == LOW) {
     if (millis() - timerSW > 1000 && etatEcriture == 0 && strlen(monMessage.texte) > 0) {
        etatEcriture = 1; 
        charSelectIndex = 2;
        dernierIndexAffiche = -1;
        bip(0);
        while(digitalRead(PIN_BTN_SW)==LOW); // Bloque
     }
  } else {
    timerSW = millis();
  }
}

// --- 3. PARAMETRES (Adapté de FS5.cpp) ---
void logiqueParametres() {
  // Navigation
  if (encoderDelta != 0) {
    bool positif = (encoderDelta > 0);
    encoderDelta = 0;

    if (paramSubState == 0) { // Canal
      if (positif) radioCanal++; else radioCanal--;
      if (radioCanal > 125) radioCanal = 0;
      if (radioCanal < 0) radioCanal = 125;
    }
    else if (paramSubState == 1) { // Pseudo
      if (positif) paramCharIndex++; else paramCharIndex--;
      int lenParam = strlen(paramLettres);
      if (paramCharIndex >= lenParam) paramCharIndex = 0;
      if (paramCharIndex < 0) paramCharIndex = lenParam - 1;
    }
    else if (paramSubState == 2) { // Buzzer
      if (positif) modeBuzzer++; else modeBuzzer--;
      if (modeBuzzer > 2) modeBuzzer = 0;
      if (modeBuzzer < 0) modeBuzzer = 2;
    }
  }

  // Affichage
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0); 
  
  if (paramSubState == 0) {
     display.println(F("CONF: CANAL RADIO"));
     display.setTextSize(2); display.setCursor(50, 25);
     display.print(radioCanal);
     display.setTextSize(1); display.setCursor(0, 50);
     display.print(F("Btn: Suivant"));
  }
  else if (paramSubState == 1) {
     display.println(F("CONF: PSEUDO"));
     display.setCursor(0, 15); display.print(F("Actuel: ")); display.println(pseudoGlobal);
     
     display.setCursor(0, 35); display.print(F("Ajout > "));
     display.setTextSize(2); display.write(paramLettres[paramCharIndex]);
     
     display.setTextSize(1); display.setCursor(0, 55);
     display.print(F("Btn:Add  Aux:Del"));
  }
  else if (paramSubState == 2) {
     display.println(F("CONF: SONS"));
     display.setTextSize(2); display.setCursor(20, 25);
     if (modeBuzzer == 0) display.print(F("COURT"));
     if (modeBuzzer == 1) display.print(F("LONG"));
     if (modeBuzzer == 2) display.print(F("DOUBLE"));
     display.setTextSize(1); display.setCursor(0, 55);
     display.print(F("Btn: Save & Exit"));
  }
  display.display();

  // Bouton Encodeur (Validation / Suivant)
  if (digitalRead(PIN_BTN_SW) == LOW) {
    delay(100); while(digitalRead(PIN_BTN_SW) == LOW);
    bip(0);

    if (paramSubState == 0) {
      radio.setChannel(radioCanal);
      paramSubState = 1; 
    }
    else if (paramSubState == 1) {
      // Ajouter lettre au pseudo
      int len = strlen(pseudoGlobal);
      if (len < 10) {
        pseudoGlobal[len] = paramLettres[paramCharIndex];
        pseudoGlobal[len+1] = '\0';
      } else {
        bip(1); // Erreur plein
      }
    }
    else if (paramSubState == 2) {
      // Fin -> Sauvegarde et Retour
      sauvegarderParams();
      display.clearDisplay();
      display.setCursor(20, 25); display.print(F("SAUVEGARDE !"));
      display.display();
      delay(1000);
      etatGlobal = MENU_PRINCIPAL;
    }
  }

  // Bouton Auxiliaire (Effacer Pseudo / Passer Pseudo)
  if (analogRead(PIN_BTN_AUX) < 500) {
    delay(100); while(analogRead(PIN_BTN_AUX) < 500);
    
    if (paramSubState == 1) {
      int len = strlen(pseudoGlobal);
      if (len > 0) {
        pseudoGlobal[len-1] = '\0';
        bip(2);
      } else {
        // Si vide, on valide le pseudo vide (ou on passe)
        paramSubState = 2;
      }
    } else if (paramSubState == 1) {
       // Si on est sur Canal ou Son, Btn Aux pourrait servir de retour ?
       // Pour l'instant on laisse simple.
    }
    
    // Raccourci pour valider Pseudo (Appui Long Aux sur Pseudo)
    // Ici simple click vide
  }
  
  // Raccourci : Appui long SW sur Pseudo = Valider Pseudo
  static unsigned long tPseudo = 0;
  if (digitalRead(PIN_BTN_SW) == LOW && paramSubState == 1) {
      if (millis() - tPseudo > 800) {
         paramSubState = 2;
         bip(0);
         while(digitalRead(PIN_BTN_SW) == LOW);
      }
  } else {
    tPseudo = millis();
  }
}

// --- 4. LIRE MESSAGE (Placeholder) ---
void logiqueLireMessage() {
  display.clearDisplay();
  display.setCursor(0,0);
  display.println(F("--- RECEPTION ---"));
  
  display.drawLine(0, 15, 128, 15, SSD1306_WHITE);
  
  display.setCursor(10, 30);
  display.println(F("(En construction)"));
  
  display.setCursor(0, 55);
  display.println(F("Hold Aux -> Menu"));
  display.display();
  
  // Ici on pourrait mettre radio.startListening() et check radio.available()
}

// ============================================================
// --- FONCTIONS UTILITAIRES ---
// ============================================================

void isrEncoder() {
  int MSB = digitalRead(PIN_ENC_CLK); 
  int LSB = digitalRead(PIN_ENC_DT); 

  int encoded = (MSB << 1) | LSB; 
  int sum  = (lastEncoded << 2) | encoded; 

  if(sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderDelta++;
  if(sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderDelta--;

  lastEncoded = encoded; 
}

bool checkBoutonAuxLong() {
  if (analogRead(PIN_BTN_AUX) < 500) {
    unsigned long start = millis();
    while (analogRead(PIN_BTN_AUX) < 500) {
      if (millis() - start > 1000) return true; // Appui long détecté
    }
  }
  return false;
}

void bip(int type) {
  if (type == 0) { // Valid
    tone(PIN_BUZZER, 2000, 50); 
  } else if (type == 1) { // Erreur
    tone(PIN_BUZZER, 500, 300);
  } else if (type == 2) { // Click/Nav
    if (modeBuzzer > 0) tone(PIN_BUZZER, 1000, 20);
  }
}

// --- EEPROM ---
void sauvegarderParams() {
  EEPROM.put(ADDR_CANAL, radioCanal);
  EEPROM.put(ADDR_BUZZER, modeBuzzer);
  EEPROM.put(ADDR_PSEUDO, pseudoGlobal);
}

void chargerParams() {
  EEPROM.get(ADDR_CANAL, radioCanal);
  EEPROM.get(ADDR_BUZZER, modeBuzzer);
  EEPROM.get(ADDR_PSEUDO, pseudoGlobal);
  
  // Sanitization
  if (radioCanal < 0 || radioCanal > 125) radioCanal = 0;
  if (modeBuzzer < 0 || modeBuzzer > 2) modeBuzzer = 0;
  pseudoGlobal[10] = '\0'; // Sécurité char array
}

// --- RADIO ENVOI ---
void envoyerMessageRadio() {
  digitalWrite(PIN_LED_FB, HIGH);
  
  // Envoi Pseudo
  String sPseudo = String(pseudoGlobal);
  envoyer_paquet_fragment('P', sPseudo);
  delay(100);

  // Préparation Texte
  String messageFinal = "";
  if (monMessage.priorite == 1) messageFinal += "[URGENT] ";
  else if (monMessage.priorite == 3) messageFinal += "[INFO] ";
  messageFinal += String(monMessage.texte);

  // Envoi Texte
  envoyer_paquet_fragment('M', messageFinal);
  
  digitalWrite(PIN_LED_FB, LOW);
}

void envoyer_paquet_fragment(char type, String texte_a_envoyer) {
  int len = texte_a_envoyer.length();
  int cursor = 0;
  
  while (cursor < len) {
    char paquet[32];
    memset(paquet, 0, 32);
    paquet[0] = type;
    
    for (int i = 1; i < 32; i++) {
      if (cursor < len) {
        paquet[i] = texte_a_envoyer[cursor++];
      }
    }
    radio.write(&paquet, sizeof(paquet));
    delay(50); // Petit délai pour laisser le Rx respirer
  }
}