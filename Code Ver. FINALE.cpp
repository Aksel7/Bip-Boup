// Code Version FINALE !!

#include <SPI.h>
#include <RF24.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define LARGEUR_ECRAN 128
#define HAUTEUR_ECRAN 64
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(LARGEUR_ECRAN, HAUTEUR_ECRAN, &Wire, -1);

RF24 radio(7, 8);
const byte adresse[6] = "PIPE1";

const int pinCLK = 3;
const int pinDT  = 4;
const int pinSW  = 2;
const int pinBoutonAux = A6;
const int pinBuzzer = 10;

const int pinLedRouge = 5;
const int pinLedVerte = 6;
const int pinLedBleue = 9;

const int SENSIBILITE_ENCODEUR = 2;

enum GlobalState { 
  MENU_PRINCIPAL, 
  MODE_LIRE, 
  MODE_ECRIRE, 
  MODE_PARAMETRES 
};

GlobalState etatGlobal = MENU_PRINCIPAL; 
int indexMenu = 0;

volatile int encoderDelta = 0;

bool flagClickCourt = false;
bool flagClickLong = false;
bool flagAux = false;
bool flagAuxLong = false;

String messageRecuComplet = "";
String pseudoRecu = "";
int prioriteRecue = 2;
bool messageEstPretALire = false;

enum ModeParam { 
  P_CANAL, 
  P_PSEUDO, 
  P_BUZZER 
};
ModeParam modeParam = P_CANAL;

int canal = 0;
char pseudo[11] = "Aksooul";
int modeBuzzer = 0;

char lettreActuelle = 'A';
int indexLettre = 0;

String messageSaisi = "";
int prioriteEnvoi = 2;

int indexCaractereSaisie = 0;
const char* listeCaracteresSaisie = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .,!?'@";

enum EtatEcriture { 
  E_SAISIE, 
  E_PRIORITE, 
  E_ENVOI 
};
EtatEcriture etatEcriture = E_SAISIE;

const int addrCanal = 0;
const int addrBuzzer = 10;
const int addrPseudo = 20;

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

void allumerLedPriorite(int priorite) {
  digitalWrite(pinLedRouge, LOW);
  digitalWrite(pinLedVerte, LOW);
  digitalWrite(pinLedBleue, LOW);

  if (priorite == 1) {
    digitalWrite(pinLedRouge, HIGH);
  } 
  else if (priorite == 2) {
    digitalWrite(pinLedVerte, HIGH);
  } 
  else if (priorite == 3) {
    digitalWrite(pinLedBleue, HIGH);
  }
}

void bip(int type) {
  if (type == 0) { 
    tone(pinBuzzer, 2000, 30);
  } 
  else if (type == 1) { 
    if (modeBuzzer >= 1) { 
      tone(pinBuzzer, 1000, 100); 
      delay(100); 
      tone(pinBuzzer, 2000, 200); 
    } else {
      tone(pinBuzzer, 2000, 100);
    }
  } 
  else if (type == 2) { 
    tone(pinBuzzer, 500, 100);
  }
  else if (type == 3) { 
    for (int i = 0; i < 3; i++) {
        tone(pinBuzzer, 3000, 50); 
        delay(60);
        tone(pinBuzzer, 2500, 50); 
        delay(60);
    }
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
  
  if (canal < 0 || canal > 125) {
    canal = 0;
  }
  if (modeBuzzer < 0 || modeBuzzer > 2) {
    modeBuzzer = 0;
  }
  pseudo[10] = '\0'; 
}

void lireBoutons() {
  flagClickCourt = false;
  flagClickLong = false;
  flagAux = false;
  flagAuxLong = false;

  static unsigned long debutAppuiSW = 0;
  static bool appuiEnCoursSW = false;
  static bool longPressTraite = false;
  
  int etatSW = digitalRead(pinSW);

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

  static unsigned long debutAppuiAux = 0;
  static bool appuiEnCoursAux = false;
  static bool longPressAuxTraite = false;

  if (analogRead(pinBoutonAux) < 500) {
    if (!appuiEnCoursAux) {
      debutAppuiAux = millis(); 
      appuiEnCoursAux = true; 
      longPressAuxTraite = false;
    } else {
      if (!longPressAuxTraite && (millis() - debutAppuiAux > 800)) {
        flagAuxLong = true; 
        longPressAuxTraite = true; 
      }
    }
  } else {
    if (appuiEnCoursAux) {
      if (!longPressAuxTraite) {
        flagAux = true; 
      }
      appuiEnCoursAux = false;
    }
  }
}

void envoyerPaquet(char type, String donnees) {
    char buffer[32];
    buffer[0] = type;
    
    for (int i = 0; i < 31; i++) {
        if (i < donnees.length()) {
            buffer[i+1] = donnees[i];
        } else {
            buffer[i+1] = 0;
        }
    }
    
    radio.write(&buffer, sizeof(buffer));
    delay(15);
}

void envoyerMessageComplet() {
    radio.stopListening();
    
    String entete = String(prioriteEnvoi) + String(pseudo);
    envoyerPaquet('D', entete);
    
    int len = messageSaisi.length();
    for (int i = 0; i < len; i += 30) {
        String morceau = messageSaisi.substring(i, i + 30);
        envoyerPaquet('T', morceau);
    }
    
    envoyerPaquet('F', "");
    
    radio.startListening();
}

void verifierReceptionRadio() {
  if (etatGlobal == MODE_ECRIRE && etatEcriture == E_ENVOI) {
    return;
  }

  if (radio.available()) {
    char buffer[32] = ""; 
    radio.read(&buffer, sizeof(buffer)); 
    char type = buffer[0];

    if (type == 'D') {
        messageRecuComplet = "";
        messageEstPretALire = false;
        prioriteRecue = String(buffer[1]).toInt();
        pseudoRecu = String(buffer).substring(2);
    }
    else if (type == 'T') {
        messageRecuComplet += String(buffer).substring(1);
    }
    else if (type == 'F') {
        messageEstPretALire = true;
        allumerLedPriorite(prioriteRecue);
        bip(3);
    }
  }
}

void gestionMenuPrincipal() {
  if (encoderDelta != 0) {
    if (encoderDelta > 0) {
      indexMenu++; 
    } else {
      indexMenu--;
    }
    encoderDelta = 0; 
    
    if (indexMenu > 2) indexMenu = 0; 
    if (indexMenu < 0) indexMenu = 2;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(30, 0); 
  display.println(F("MENU RADIO"));
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE); 
  
  display.setCursor(10, 20);
  if (indexMenu == 0) display.print(F("> ")); 
  display.print(F("LIRE MSG"));
  if (messageEstPretALire) display.print(F(" [!]"));
  
  display.setCursor(10, 35);
  if (indexMenu == 1) display.print(F("> ")); 
  display.println(F("ECRIRE MSG"));
  
  display.setCursor(10, 50);
  if (indexMenu == 2) display.print(F("> ")); 
  display.println(F("PARAMETRES"));

  display.display();

  if (flagClickCourt || flagAux) {
    bip(0); 
    
    if (indexMenu == 0) {
      etatGlobal = MODE_LIRE;
      messageEstPretALire = false; 
      allumerLedPriorite(0);
      noTone(pinBuzzer);
    }
    else if (indexMenu == 1) {
        etatEcriture = E_SAISIE;
        messageSaisi = "";
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
  
  if (messageRecuComplet.length() == 0) {
    display.setCursor(20, 30); 
    display.println(F("AUCUN MESSAGE"));
  } 
  else {
    display.setTextSize(1);
    display.setCursor(0, 0); 
    display.print(F("De: ")); 
    display.println(pseudoRecu);
    
    display.setCursor(80, 0);
    if (prioriteRecue == 1) {
      display.print(F("!URG!"));
    }
    
    display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
    
    display.setCursor(0, 15);
    display.println(messageRecuComplet);
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
  if (modeParam == P_CANAL) {
    if (encoderDelta != 0) {
      canal += encoderDelta; 
      encoderDelta = 0;
      
      if (canal < 0) canal = 125; 
      if (canal > 125) canal = 0;
    }
    
    display.clearDisplay();
    display.setCursor(0, 0); 
    display.println(F("PARAM: CANAL"));
    
    display.setTextSize(2); 
    display.setCursor(45, 25); 
    display.println(canal);
    
    display.setTextSize(1); 
    display.setCursor(0, 55); 
    display.println(F("Aux=Suivant"));
    display.display();

    if (flagAux) { 
      bip(1); 
      modeParam = P_PSEUDO; 
      radio.setChannel(canal); 
    }
  }
  else if (modeParam == P_PSEUDO) {
    if (encoderDelta != 0) {
      indexLettre += encoderDelta; 
      encoderDelta = 0;
      
      int maxLen = strlen(listeCaracteresSaisie);
      if (indexLettre < 0) indexLettre = maxLen - 1; 
      if (indexLettre >= maxLen) indexLettre = 0;
      
      lettreActuelle = listeCaracteresSaisie[indexLettre];
    }
    
    display.clearDisplay();
    display.setCursor(0, 0); 
    display.println(F("PARAM: PSEUDO"));
    
    display.setCursor(0, 20); 
    display.print(F("Nom: ")); 
    display.println(pseudo);
    
    display.setCursor(0, 40); 
    display.print(F("Let: [")); 
    
    display.setTextSize(2); 
    display.print(lettreActuelle); 
    
    display.setTextSize(1); 
    display.println(F("]"));
    
    display.setCursor(0, 56); 
    display.println(F("Clik=Add Long=Del"));
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
  else if (modeParam == P_BUZZER) {
    if (encoderDelta != 0) {
       if (encoderDelta > 0) {
         modeBuzzer++; 
       } else {
         modeBuzzer--;
       }
       encoderDelta = 0;
       
       if (modeBuzzer > 2) modeBuzzer = 0; 
       if (modeBuzzer < 0) modeBuzzer = 2;
    }
    
    display.clearDisplay();
    display.setCursor(0, 0); 
    display.println(F("PARAM: SON"));
    
    display.setTextSize(2); 
    display.setCursor(20, 25);
    
    if (modeBuzzer == 0) display.println(F("Court"));
    else if (modeBuzzer == 1) display.println(F("Long"));
    else display.println(F("Double"));
    
    display.setTextSize(1); 
    display.setCursor(0, 55); 
    display.println(F("Aux=Quitter"));
    display.display();

    if (flagAux) { 
      bip(1); 
      sauvegarderParams(); 
      etatGlobal = MENU_PRINCIPAL; 
    }
  }
}

void gestionEcriture() {
  if (etatEcriture == E_SAISIE) {
    if (encoderDelta != 0) {
      indexCaractereSaisie += encoderDelta; 
      encoderDelta = 0;
      
      int maxLen = strlen(listeCaracteresSaisie);
      if (indexCaractereSaisie < 0) indexCaractereSaisie = maxLen - 1;
      if (indexCaractereSaisie >= maxLen) indexCaractereSaisie = 0;
    }

    display.clearDisplay();
    display.setTextSize(2); 
    display.setCursor(58, 5); 
    display.print(listeCaracteresSaisie[indexCaractereSaisie]);
    
    display.setTextSize(1); 
    display.setCursor(0, 30); 
    display.print(F(">")); 
    
    if (messageSaisi.length() > 18) {
      display.println(messageSaisi.substring(messageSaisi.length()-18));
    } else {
      display.println(messageSaisi);
    }

    display.drawLine(0, 48, 128, 48, SSD1306_WHITE);
    display.setCursor(0, 54); 
    display.print(F("Clik:Add  LongRge:OK")); 
    display.display();

    if (flagClickCourt) { 
         if (messageSaisi.length() < 140) { 
           messageSaisi += listeCaracteresSaisie[indexCaractereSaisie]; 
           bip(0); 
         }
    }
    
    if (flagAuxLong) {
       if (messageSaisi.length() > 0) {
         etatEcriture = E_PRIORITE;
         bip(1);
       }
    }

    if (flagAux) { 
       if (messageSaisi.length() > 0) { 
         messageSaisi.remove(messageSaisi.length()-1); 
         bip(2); 
       } else { 
         etatGlobal = MENU_PRINCIPAL; 
         bip(2); 
       }
    }
  }
  
  else if (etatEcriture == E_PRIORITE) {
    if (encoderDelta != 0) {
       if (encoderDelta > 0) {
         prioriteEnvoi++; 
       } else {
         prioriteEnvoi--;
       }
       encoderDelta = 0;
       
       if (prioriteEnvoi > 3) prioriteEnvoi = 1; 
       if (prioriteEnvoi < 1) prioriteEnvoi = 3;
    }
    
    display.clearDisplay();
    display.setCursor(0,0); 
    display.println(F("PRIORITE ?"));
    
    display.setTextSize(2); 
    display.setCursor(20, 30);
    
    if (prioriteEnvoi == 1) display.println(F("URGENT"));
    else if (prioriteEnvoi == 2) display.println(F("NORMAL"));
    else display.println(F("BASSE"));
    
    display.setTextSize(1); 
    display.setCursor(0, 55); 
    display.println(F("Appui Court = ENVOYER"));
    display.display();

    if (flagAux || flagAuxLong) { 
      etatEcriture = E_ENVOI; 
      bip(1); 
    }
  }
  
  else if (etatEcriture == E_ENVOI) {
    display.clearDisplay();
    display.setCursor(10, 20); 
    display.println(F("ENVOI EN COURS..."));
    display.display();
    
    envoyerMessageComplet();
    
    display.clearDisplay();
    display.setCursor(10, 20); 
    display.println(F("MESSAGE ENVOYE!"));
    display.display();
    
    bip(1); 
    delay(1500); 
    etatGlobal = MENU_PRINCIPAL; 
  }
}

void setup() {
  pinMode(pinCLK, INPUT_PULLUP); 
  pinMode(pinDT, INPUT_PULLUP);
  pinMode(pinSW, INPUT_PULLUP); 
  pinMode(pinBuzzer, OUTPUT);
  
  pinMode(pinLedRouge, OUTPUT); 
  pinMode(pinLedVerte, OUTPUT); 
  pinMode(pinLedBleue, OUTPUT);

  digitalWrite(pinLedRouge, HIGH); 
  delay(200); 
  digitalWrite(pinLedRouge, LOW);
  
  digitalWrite(pinLedVerte, HIGH); 
  delay(200); 
  digitalWrite(pinLedVerte, LOW);
  
  digitalWrite(pinLedBleue, HIGH); 
  delay(200); 
  digitalWrite(pinLedBleue, LOW);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) { 
    for (;;); 
  }
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(20, 20); 
  display.println(F("SYSTEME PRET v10"));
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
}

void loop() {
  lireBoutons(); 
  verifierReceptionRadio(); 

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