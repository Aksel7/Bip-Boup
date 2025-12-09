#include <SPI.h>
#include <RF24.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C // ou 0x3D selon ton écran
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

//NRF24
RF24 radio(7, 8); // CE, CSN

//Encodeur et bouton
const int pinCLK = 3;
const int pinDT  = 4;
const int pinSW  = 2; // LE bouton encodeur
const int buzzer = 10;

volatile int lastCLK = 0;
volatile bool changed = false;

//Mode
enum Mode { CANAL, PSEUDO, BUZZER };
Mode modeActuel = CANAL;

//paramètres
int canal = 0;
String pseudo = "";
const int maxPseudo = 10;
char currentLetter = 'A';
int charIndex = 0;
const char allowedChars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 .,!?;:@#&+-*/";

int buzzerMode = 0;
const int buzzerModes = 3;

//EEPROM
const int addrCanal = 0;
const int addrBuzzer = 1;
const int addrPseudo = 2;

//Fonction Encodeur
void readEncoder() {
  int clk = digitalRead(pinCLK);
  if(clk != lastCLK && clk==HIGH){
    if(modeActuel==CANAL){
      if(digitalRead(pinDT)==HIGH) canal--;
      else canal++;
      if(canal<0) canal=0;
      if(canal>125) canal=125;
    } else if(modeActuel==PSEUDO){
      if(digitalRead(pinDT)==HIGH) charIndex--;
      else charIndex++;
      if(charIndex>=sizeof(allowedChars)-1) charIndex=0;
      if(charIndex<0) charIndex=sizeof(allowedChars)-2;
      currentLetter = allowedChars[charIndex];
    } else if(modeActuel==BUZZER){
      if(digitalRead(pinDT)==HIGH) buzzerMode--;
      else buzzerMode++;
      if(buzzerMode>=buzzerModes) buzzerMode=0;
      if(buzzerMode<0) buzzerMode=buzzerModes-1;
    }
    changed=true;
  }
  lastCLK=clk;
}

//EEPROM
void saveParams(){
  EEPROM.update(addrCanal, canal);
  EEPROM.update(addrBuzzer, buzzerMode);
  for(int i=0;i<maxPseudo;i++){
    char c = (i<pseudo.length()) ? pseudo[i] : 0;
    EEPROM.update(addrPseudo+i, c);
  }
}

void loadParams(){
  canal = EEPROM.read(addrCanal);
  buzzerMode = EEPROM.read(addrBuzzer);
  pseudo="";
  for(int i=0;i<maxPseudo;i++){
    char c = EEPROM.read(addrPseudo+i);
    if(c==0) break;
    pseudo+=c;
  }
}

//Affichage OLED
void displayInfo(){
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.print("Mode: ");
  if(modeActuel==CANAL) display.println("CANAL");
  else if(modeActuel==PSEUDO) display.println("PSEUDO");
  else display.println("BUZZER");

  if(modeActuel==CANAL){
    display.setCursor(0,15);
    display.print("Canal: "); display.println(canal);
    display.setCursor(0,30);
    display.println("Tournez encodeur");
    display.setCursor(0,45);
    display.println("Appui = valider");
  } else if(modeActuel==PSEUDO){
    display.setCursor(0,15);
    display.print("Pseudo: "); display.println(pseudo);
    display.setCursor(0,30);
    display.print("Lettre: "); display.println(currentLetter);
    display.setCursor(0,45);
    display.println("Tournez pour changer");
    display.setCursor(0,55);
    display.println("Appui = ajouter");
  } else if(modeActuel==BUZZER){
    display.setCursor(0,15);
    display.print("Sonorite: ");
    if(buzzerMode==0) display.println("Court");
    else if(buzzerMode==1) display.println("Long");
    else display.println("Double");
    display.setCursor(0,30);
    display.println("Tournez pour changer");
    display.setCursor(0,45);
    display.println("Appui = valider");
  }
  display.display();
}

//Setup
void setup(){
  pinMode(pinCLK, INPUT_PULLUP);
  pinMode(pinDT, INPUT_PULLUP);
  pinMode(pinSW, INPUT_PULLUP);
  pinMode(buzzer, OUTPUT);

  lastCLK = digitalRead(pinCLK);
  attachInterrupt(digitalPinToInterrupt(pinCLK), readEncoder, CHANGE);

  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)){
    while(1);
  }

  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Veuillez choisir le canal | Pseudo | Son");
  display.display();

  // Initialiser tout à zéro au démarrage
  canal = 0;
  pseudo = "";
  buzzerMode = 0;
  charIndex = 0;
  currentLetter = allowedChars[0];
  saveParams();

  radio.begin();
  radio.setChannel(canal);
}

//Loop
void loop(){
  if(changed){
    displayInfo();
    changed=false;
  }

  int swState = digitalRead(pinSW);
  static unsigned long pressStart=0;

  if(swState==LOW){
    if(pressStart==0) pressStart=millis();
  } else {
    if(pressStart!=0){
      unsigned long duration = millis()-pressStart;
      if(duration>=800){ // appui long = changer mode
        if(modeActuel==CANAL) modeActuel=PSEUDO;
        else if(modeActuel==PSEUDO) modeActuel=BUZZER;
        else modeActuel=CANAL;
        displayInfo();
      } else { // appui court = valider
        if(modeActuel==CANAL){
          radio.setChannel(canal);
          saveParams();
        } else if(modeActuel==PSEUDO){
          if(pseudo.length()<maxPseudo){
            pseudo+=currentLetter;
            saveParams();
          }
        } else if(modeActuel==BUZZER){
          if(buzzerMode==0) tone(buzzer,1000,100);
          else if(buzzerMode==1) tone(buzzer,1000,500);
          else { tone(buzzer,1000,100); delay(150); tone(buzzer,1000,100);}
          saveParams();
        }
      }
      pressStart=0;
    }
  }
}

