#include <SPI.h>
#include <RF24.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//Configuration de l'écran OLED 
#define LARGEUR_ECRAN 128
#define HAUTEUR_ECRAN 64
#define OLED_ADDR 0x3C // Adresse I2C de l'écran, peut être 0x3D selon le modèle
Adafruit_SSD1306 ecran(LARGEUR_ECRAN, HAUTEUR_ECRAN, &Wire, -1);



// Configuration du module NRF24 
RF24 radio(7, 8); // Pins CE et CSN du module NRF24


//  Encodeur rotatif et bouton
const int pinCLK = 3; // Signal CLK de l'encodeur
const int pinDT  = 4; // Signal DT de l'encodeur
const int pinSW  = 2; // Bouton intégré de l'encodeur
const int buzzer = 10; // Buzzer pour signal sonore



volatile int dernierCLK = 0;  // Variable pour détecter le changement du signal CLK
volatile bool changement = false; // Indique qu'une valeur a été modifiée



// Modes de configuration 
enum Mode { CANAL, PSEUDO, BUZZER };
Mode modeActuel = CANAL; // Mode par défaut au démarrage



//Paramètres configurables 
int canal = 0;           // Canal de communication NRF24 (0 à 125)
String pseudo = "";      // Nom d'utilisateur
const int longueurMaxPseudo = 10; // Nombre max de caractères du pseudo
char lettreActuelle = 'A'; // Lettre sélectionnée actuellement
int indexLettre = 0;      // Position dans le tableau de lettres autorisées
const char lettresAutorisees[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 .,!?;:@#&+-*/@]}";

int modeBuzzer = 0;       // Sélection du mode sonore
const int nbModesBuzzer = 3; // Nombre total de sons possibles




// Adresses EEPROM pour sauvegarde 
const int addrCanal = 0;  // Adresse pour le canal
const int addrBuzzer = 1; // Adresse pour le mode buzzer
const int addrPseudo = 2; // Adresse de début pour le pseudo





// Fonction pour lire l'encodeur 
void lireEncodeur() {
  int clk = digitalRead(pinCLK); // On lit l'état actuel du signal CLK
  // On déclenche seulement quand le signal change de LOW à HIGH (front montant)
  if(clk != dernierCLK && clk==HIGH){
    if(modeActuel==CANAL){ // Modification du canal
      if(digitalRead(pinDT)==HIGH) canal--; // Rotation sens inverse
      else canal++;                          // Rotation sens direct
      // Limitation des valeurs
      if(canal<0) canal=0;
      if(canal>125) canal=125;
    } 




    else if(modeActuel==PSEUDO){ // Modification de la lettre du pseudo
      if(digitalRead(pinDT)==HIGH) indexLettre--;
      else indexLettre++;
      // le Gestion du débordement pour revenir au début ou à la fin
      if(indexLettre>=sizeof(lettresAutorisees)-1) indexLettre=0;
      if(indexLettre<0) indexLettre=sizeof(lettresAutorisees)-2;
      lettreActuelle = lettresAutorisees[indexLettre];
    } 
    else if(modeActuel==BUZZER){ // Modification du mode buzzer
      if(digitalRead(pinDT)==HIGH) modeBuzzer--;
      else modeBuzzer++;
      // Gestion du débordement
      if(modeBuzzer>=nbModesBuzzer) modeBuzzer=0;
      if(modeBuzzer<0) modeBuzzer=nbModesBuzzer-1;
    }
    changement=true; // ici on Indique à la boucle principale qu'il faut mettre à jour l'affichage
  }
  dernierCLK=clk; // On mémorise l'état pour le prochain changement
}



// Sauvegarde des paramètres dans l'EEPROM 
void sauvegarderParams(){
  EEPROM.update(addrCanal, canal);         // Sauvegarde du canal
  EEPROM.update(addrBuzzer, modeBuzzer);   // Sauvegarde du mode buzzer
  // Sauvegarde du pseudo caractère par caractère
  for(int i=0;i<longueurMaxPseudo;i++){
    char c = (i<pseudo.length()) ? pseudo[i] : 0; // 0 = fin de chaîne
    EEPROM.update(addrPseudo+i, c);
  }
}




//  Chargement des paramètres depuis l'EEPROM 
void chargerParams(){
  canal = EEPROM.read(addrCanal);
  modeBuzzer = EEPROM.read(addrBuzzer);
  pseudo="";
  for(int i=0;i<longueurMaxPseudo;i++){
    char c = EEPROM.read(addrPseudo+i);
    if(c==0) break; // On arrête si on atteint la fin du pseudo
    pseudo+=c;
  }
}



// Affichage des informations sur l'écran OLED 
void afficherInfo(){
  ecran.clearDisplay();
  ecran.setTextSize(1);
  ecran.setTextColor(SSD1306_WHITE);
  ecran.setCursor(0,0);
  ecran.print("Mode: ");
  if(modeActuel==CANAL) ecran.println("CANAL");
  else if(modeActuel==PSEUDO) ecran.println("PSEUDO");
  else ecran.println("BUZZER");



  if(modeActuel==CANAL){
    ecran.setCursor(0,15);
    ecran.print("Canal: "); ecran.println(canal);
    ecran.setCursor(0,30);
    ecran.println("Tournez l'encodeur"); // Indique l'action à l'utilisateur
    ecran.setCursor(0,45);
    ecran.println("Appui = valider"); // Bouton pour valider
  } 


  else if(modeActuel==PSEUDO){
    ecran.setCursor(0,15);
    ecran.print("Pseudo: "); ecran.println(pseudo);
    ecran.setCursor(0,30);
    ecran.print("Lettre: "); ecran.println(lettreActuelle);
    ecran.setCursor(0,45);
    ecran.println("Tournez pour changer"); // On change la lettre avec l'encodeur
    ecran.setCursor(0,55);
    ecran.println("Appui = ajouter");    // Ajoute la lettre au pseudo
  } 



  else if(modeActuel==BUZZER){
    ecran.setCursor(0,15);
    ecran.print("Sonorite: ");
    if(modeBuzzer==0) ecran.println("Court");
    else if(modeBuzzer==1) ecran.println("Long");
    else ecran.println("Double");
    ecran.setCursor(0,30);
    ecran.println("Tournez pour changer"); // Changer le type de son
    ecran.setCursor(0,45);
    ecran.println("Appui = valider"); // Valider le choix du buzzer
  }



  ecran.display(); // Met à jour l'écran
}

//  Configuration initiale 
void setup(){
  pinMode(pinCLK, INPUT_PULLUP); // Pull-up interne pour éviter les flottements
  pinMode(pinDT, INPUT_PULLUP);
  pinMode(pinSW, INPUT_PULLUP);
  pinMode(buzzer, OUTPUT);

  dernierCLK = digitalRead(pinCLK);
  attachInterrupt(digitalPinToInterrupt(pinCLK), lireEncodeur, CHANGE); 
  // Déclenche la fonction lireEncodeur à chaque changement du signal CLK

  if(!ecran.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)){
    while(1); // Bloque si l'écran n'est pas détecté
  }



  ecran.clearDisplay();
  ecran.setCursor(0,0);
  ecran.println("Veuillez choisir canal | Pseudo | Son");
  ecran.display();

  // Initialisation des valeurs
  canal = 0;
  pseudo = "";
  modeBuzzer = 0;
  indexLettre = 0;
  lettreActuelle = lettresAutorisees[0];
  sauvegarderParams(); // On sauvegarde les valeurs par défaut

  radio.begin();
  radio.setChannel(canal); // On configure le canal initial
}




//  Boucle principale 
void loop(){
  // Si une valeur a changé, on met à jour l'affichage
  if(changement){
    afficherInfo();
    changement=false;
  }

  int etatBouton = digitalRead(pinSW); // Lecture du bouton
  static unsigned long debutAppui=0;   // Temps du début de l'appui

  if(etatBouton==LOW){ // Bouton appuyé
    if(debutAppui==0) debutAppui=millis(); // On enregistre le début de l'appui
  } else { // Bouton relâché
    if(debutAppui!=0){
      unsigned long duree = millis()-debutAppui; // Durée de l'appui
      if(duree>=800){ // Appui long = changer de mode
        if(modeActuel==CANAL) modeActuel=PSEUDO;
        else if(modeActuel==PSEUDO) modeActuel=BUZZER;
        else modeActuel=CANAL;
        afficherInfo();
      } else { // Appui court = valider ou ajouter
        if(modeActuel==CANAL){
          radio.setChannel(canal); // Appliquer le nouveau canal
          sauvegarderParams();     // Sauvegarder dans l'EEPROM
        } 
        else if(modeActuel==PSEUDO){
          if(pseudo.length()<longueurMaxPseudo){
            pseudo+=lettreActuelle; // Ajouter la lettre au pseudo
            sauvegarderParams();
          }
        } 
        else if(modeActuel==BUZZER){
          // Jouer le son correspondant au mode choisi
          if(modeBuzzer==0) tone(buzzer,1000,100);
          else if(modeBuzzer==1) tone(buzzer,1000,500);
          else { tone(buzzer,1000,100); delay(150); tone(buzzer,1000,100);}
          sauvegarderParams();
        }
      }
      debutAppui=0; // Réinitialisation pour le prochain appui
    }
  }
}
