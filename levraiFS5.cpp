/// FS5 : Régler les paramètres


#include <Arduino.h>
#include <EEPROM.h>
#include <SPI.h>
#include <RF24.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Encoder.h>


// Définition des pins et périphériques
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


#define BUZZER_PIN 10
#define ENCODER_PIN_A 3
#define ENCODER_PIN_B 4
#define BUTTON_PIN 2

RF24 radio(7, 8);           // CE, CSN du NRF24
Encoder encoder(ENCODER_PIN_A, ENCODER_PIN_B);

// Adresses EEPROM pour EF12 à EF15
#define EEPROM_CHANNEL 0     // EF12: canal radio
#define EEPROM_PSEUDO 1      // EF13: pseudo (10 caractères max)
#define EEPROM_ALERT 11      // EF14: sonorité alerte

// Paramètres utilisateur (valeurs par défaut)
uint8_t channel = 100;       // EF12
char pseudo[10] = "User";    // EF13
uint8_t alertTone = 0;       // EF14

long lastEncoderPos = -999;  // Pour détecter changement encodeur
int menuIndex = 0;           // 0=canal, 1=pseudo, 2=alerte
int pseudoCharIndex = 0;     // Pour modifier caractère par caractère

// EF15: Charger les paramètres depuis EEPROM
void loadParameters() 
{
  channel = EEPROM.read(EEPROM_CHANNEL);
  for(int i=0; i<10; i++) pseudo[i] = EEPROM.read(EEPROM_PSEUDO+i);
  alertTone = EEPROM.read(EEPROM_ALERT);
}

// EF15: Sauvegarder les paramètres dans EEPROM
void saveParameters() 
{
  EEPROM.write(EEPROM_CHANNEL, channel);
  for(int i=0; i<10; i++) EEPROM.write(EEPROM_PSEUDO+i, pseudo[i]);
  EEPROM.write(EEPROM_ALERT, alertTone);
}

// EF12 à EF14: Affichage menu OLED
void displayMenu() 
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("=== Menu BiPeur ===");
  
  display.setCursor(0,10);
  display.print("> "); 
  display.print((menuIndex==0)?"* ":"  "); // Indicateur curseur
  display.print("1.Channel: "); display.println(channel); // EF12
  
  display.setCursor(0,20);
  display.print("> "); 
  display.print((menuIndex==1)?"* ":"  ");
  display.print("2.Pseudo: "); display.println(pseudo); // EF13
  
  display.setCursor(0,30);
  display.print("> "); 
  display.print((menuIndex==2)?"* ":"  ");
  display.print("3.Alert Tone: "); display.println(alertTone); // EF14

  display.display();
}

// EF14: Jouer l’alerte sonore
void playAlert(uint8_t toneId) 
{
  switch(toneId) {
    case 0: tone(BUZZER_PIN, 1000, 200); break; // Son 1
    case 1: tone(BUZZER_PIN, 1500, 200); break; // Son 2
    case 2: tone(BUZZER_PIN, 2000, 200); break; // Son 3
  }
}

// EF12: Configuration NRF24 avec canal sélectionné
void setupRadio() 
{
  radio.begin();
  radio.setChannel(channel); // canal EF12
  radio.openWritingPipe(0xF0F0F0F0E1LL); // adresse fixe pour test
}

// Initialisation
void setup() 
{
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // OLED
  loadParameters();                           // EF15: charger EEPROM
  setupRadio();                               // EF12: config canal
  displayMenu();                              
}

// Boucle principale
void loop() 
{
  long newPos = encoder.read()/4; // Ajustement sensibilité

  // Navigation menu ou modification valeur
  if(newPos != lastEncoderPos){
    lastEncoderPos = newPos;

    if(menuIndex == 0){
      // EF12: changer canal
      channel = constrain(newPos, 0, 125);
      setupRadio(); 
    } else if(menuIndex == 1){
      // EF13: changer caractère du pseudo
      pseudo[pseudoCharIndex] = constrain(newPos+65, 32, 122); // ASCII 32-122
    } else if(menuIndex == 2){
      // EF14: changer son alerte
      alertTone = constrain(newPos, 0, 2);
    }
    displayMenu();
  }

  // Bouton pour sauvegarder / passer au menu suivant
  if(digitalRead(BUTTON_PIN) == LOW){
    saveParameters();       // EF15: sauvegarder dans EEPROM
    playAlert(alertTone);   // EF14: tester son
    menuIndex = (menuIndex+1)%3; // passer à l’option suivante
    if(menuIndex==1) pseudoCharIndex = 0; // recommencer au 1er caractère
    encoder.write(0);       // reset encodeur
    lastEncoderPos = -999;  // pour éviter répétition
    displayMenu();
    delay(300);             // anti-rebond
  }
}
