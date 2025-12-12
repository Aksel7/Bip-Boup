// =========================================
// PROJET BIPEUR ECE – VERSION FINALE
// Assemblage FS1, FS2, FS3, FS4, FS5
// =========================================

#include <SPI.h>
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <RF24.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>

// -----------------------------------------
// Paramètres de l'écran OLED
// -----------------------------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// -----------------------------------------
// NRF24
// -----------------------------------------
#define CE_PIN 7
#define CSN_PIN 8
RF24 radio(CE_PIN, CSN_PIN);
int canal = 0; // Canal par défaut

// -----------------------------------------
// Structure du message
// -----------------------------------------
struct Message {
  char pseudo[20];
  char text[100];
  int prio; // 1 = faible, 2 = moyenne, 3 = haute
};
Message msg;         // Message reçu
Message monMessage;   // Message à envoyer
int message_recu = 0;

// -----------------------------------------
// FS1 – Saisie message
// -----------------------------------------
const int brocheEncodeur_A = 3; 
const int brocheEncodeur_B = 4; 
const int brocheBouton_Encodeur = 2; 
const int brocheBouton_Valider = A6; 
const int brocheLED_Validation = 5;  

const char* listeCaracteres = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .,!?@\x82\x85\x87\x88\x97<";
const int tailleListeCaracteres = 43; 

volatile int indexCaractereSelectionne = 0; 
int dernierIndexCaractereAffiche = -1;
volatile int compteurBrutEncodeur = 0; 
const int SENSIBILITE = 2; 
int positionCurseurMessage = 0;

long dernierTempsDebounceEnc = 0;
int etatPrecedentBoutonEnc = HIGH;
int etatPrecedentBoutonVal = HIGH;
unsigned long tempsDebutAppuiVal = 0;
bool appuiLongTraite = false; 
const long delaiDebounce = 50;
const long DUREE_APPUI_LONG = 1000; 

enum EtatSysteme { SAISIE_TEXTE, CHOIX_PRIORITE, MESSAGE_PRET };
EtatSysteme etatActuel = SAISIE_TEXTE;

// -----------------------------------------
// FS3 – Alerte message
// -----------------------------------------
#define BUZZER_PIN 10
#define STOP_BTN_PIN 2

void setLED(int prio) {
  switch(prio) {
    case 1: // faible → vert
      analogWrite(5, 0);   
      analogWrite(6, 255); 
      analogWrite(9, 0);   
      break;
    case 2: // moyenne → jaune
      analogWrite(5, 255); 
      analogWrite(6, 255); 
      analogWrite(9, 0);   
      break;
    case 3: // haute → rouge
      analogWrite(5, 255); 
      analogWrite(6, 0);   
      analogWrite(9, 0);   
      break;
    default: // éteinte
      analogWrite(5, 0);
      analogWrite(6, 0);
      analogWrite(9, 0);
      break;
  }
}

// -----------------------------------------
// FS5 – Paramètres (canal, pseudo, buzzer)
// -----------------------------------------
const int pinCLK = 3;
const int pinDT  = 4;
const int pinSW  = 2; 

volatile int dernierCLK = 0;
volatile bool changement = false;

enum Mode { CANAL, PSEUDO, BUZZER };
Mode modeActuel = CANAL; 

String pseudo = "";
const int longueurMaxPseudo = 10; 
char lettreActuelle = 'A'; 
int indexLettre = 0; 
int modeBuzzer = 0;
const int nbModesBuzzer = 3; 
const char lettresAutorisees[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 .,!?;:@#&+-*/@]}";

// EEPROM
const int addrCanal = 0;
const int addrBuzzer = 1;
const int addrPseudo = 2;

// -----------------------------------------
// Interruption encodeur FS1
// -----------------------------------------
void gererRotationEncodeur() {
  static unsigned long dernierTempsInterruption = 0;
  unsigned long tempsActuel = millis();
  if (tempsActuel - dernierTempsInterruption > 5) {
    if (digitalRead(brocheEncodeur_A) == digitalRead(brocheEncodeur_B)) compteurBrutEncodeur++;
    else compteurBrutEncodeur--;
    if (compteurBrutEncodeur >= SENSIBILITE) { indexCaractereSelectionne++; compteurBrutEncodeur=0;}
    else if (compteurBrutEncodeur <= -SENSIBILITE) { indexCaractereSelectionne--; compteurBrutEncodeur=0;}
    dernierTempsInterruption = tempsActuel;
  }
}

// -----------------------------------------
// FS2 – Envoi message
// -----------------------------------------
const byte adresse[6] = "PIPE1";  

void envoyerMessage() {
  strncpy(monMessage.text, monMessage.text, sizeof(monMessage.text)-1);
  monMessage.text[sizeof(monMessage.text)-1]='\0';
  monMessage.prio = monMessage.prio;
  radio.write(&monMessage, sizeof(monMessage));
}

// -----------------------------------------
// Setup
// -----------------------------------------
void setup() {
  Serial.begin(9600);

  pinMode(brocheEncodeur_A, INPUT_PULLUP);
  pinMode(brocheEncodeur_B, INPUT_PULLUP);
  pinMode(brocheBouton_Encodeur, INPUT_PULLUP);
  pinMode(brocheLED_Validation, OUTPUT);
  digitalWrite(brocheLED_Validation, LOW);

  attachInterrupt(digitalPinToInterrupt(brocheEncodeur_A), gererRotationEncodeur, CHANGE);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(STOP_BTN_PIN, INPUT_PULLUP);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { for(;;); }
  display.cp437(true);
  display.clearDisplay();
  display.display();

  memset(monMessage.text, 0, sizeof(monMessage.text));
  monMessage.prio = 2;

  // NRF24
  radio.begin();
  radio.openReadingPipe(1, 0xF0F0F0F0E1LL);
  radio.startListening();
  radio.setChannel(canal);

  // FS5 – Chargement EEPROM
  canal = EEPROM.read(addrCanal);
  modeBuzzer = EEPROM.read(addrBuzzer);
  pseudo="";
  for(int i=0;i<longueurMaxPseudo;i++){
    char c = EEPROM.read(addrPseudo+i);
    if(c==0) break;
    pseudo+=c;
  }
}

// -----------------------------------------
// Boucle principale
// -----------------------------------------
void loop() {
  // FS1 – Saisie message
  switch(etatActuel) {
    case SAISIE_TEXTE:
      // Gérer limite et affichage
      if (indexCaractereSelectionne>=tailleListeCaracteres) indexCaractereSelectionne=0;
      if (indexCaractereSelectionne<0) indexCaractereSelectionne=tailleListeCaracteres-1;
      // Affichage message OLED
      display.clearDisplay();
      display.setCursor(0,0);
      display.print(monMessage.text);
      display.display();
      break;
    case CHOIX_PRIORITE:
      if (indexCaractereSelectionne>3) indexCaractereSelectionne=1;
      if (indexCaractereSelectionne<1) indexCaractereSelectionne=3;
      display.clearDisplay();
      display.setCursor(0,0);
      display.print("Priorite: "); display.print(indexCaractereSelectionne);
      display.display();
      break;
    case MESSAGE_PRET:
      envoyerMessage();
      digitalWrite(brocheLED_Validation,HIGH);
      delay(500);
      digitalWrite(brocheLED_Validation,LOW);
      delay(500);
      memset(monMessage.text,0,sizeof(monMessage.text));
      positionCurseurMessage=0;
      etatActuel=SAISIE_TEXTE;
      break;
  }

  // FS3 – Alerte message
  if (radio.available()) {
    radio.read(&msg, sizeof(msg));
    message_recu=1;
  }
  if(message_recu==1){
    setLED(msg.prio);
    digitalWrite(BUZZER_PIN,HIGH);
    display.clearDisplay();
    display.setCursor(0,0);
    display.print("De: "); display.println(msg.pseudo);
    display.print(msg.text);
    display.display();
    message_recu=0;
  }

  if(digitalRead(STOP_BTN_PIN)==LOW){
    digitalWrite(BUZZER_PIN,LOW);
    setLED(0);
  }

  // FS5 – Gestion paramètres
  if(changement){ 
    display.clearDisplay();
    display.setCursor(0,0);
    if(modeActuel==CANAL){ display.print("Canal: "); display.println(canal);}
    else if(modeActuel==PSEUDO){ display.print("Pseudo: "); display.println(pseudo);}
    else { display.print("Buzzer: "); display.println(modeBuzzer);}
    display.display();
    changement=false;
  }

  int etatBouton = digitalRead(pinSW);
  static unsigned long debutAppui=0;
  if(etatBouton==LOW){ if(debutAppui==0) debutAppui=millis();}
  else {
    if(debutAppui!=0){
      unsigned long duree = millis()-debutAppui;
      if(duree>=800){
        if(modeActuel==CANAL) modeActuel=PSEUDO;
        else if(modeActuel==PSEUDO) modeActuel=BUZZER;
        else modeActuel=CANAL;
        changement=true;
      } else {
        if(modeActuel==CANAL){ radio.setChannel(canal); EEPROM.update(addrCanal, canal);}
        else if(modeActuel==PSEUDO){ if(pseudo.length()<longueurMaxPseudo){ pseudo+=lettreActuelle; for(int i=0;i<longueurMaxPseudo;i++){EEPROM.update(addrPseudo+i,(i<pseudo.length())?pseudo[i]:0);}}}
        else { EEPROM.update(addrBuzzer,modeBuzzer);}
      }
      debutAppui=0;
    }
  }

  delay(10);
}
