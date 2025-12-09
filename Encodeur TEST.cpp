//Test ENCODEUR
#include <SPI.h>
#include <RF24.h>
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>


// Constantes
#define pinArduinoRaccordementSignalSW  2       // La pin D2 de l'Arduino recevra la ligne SW du module KY-040
#define pinArduinoRaccordementSignalCLK 3       // La pin D3 de l'Arduino recevra la ligne CLK du module KY-040
#define pinArduinoRaccordementSignalDT  4       // La pin D4 de l'Arduino recevra la ligne DT du module KY-040

// Définition des pins et périphériques pour l'écran OLED
#define SCREEN_WIDTH 128    // Largeur de l'écran OLED en pixels
#define SCREEN_HEIGHT 64    // Hauteur de l'écran OLED en pixels
#define //Test ENCODEUR
#include <SPI.h>
#include <RF24.h>
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>


// Constantes
#define pinArduinoRaccordementSignalSW  2       // La pin D2 de l'Arduino recevra la ligne SW du module KY-040
#define pinArduinoRaccordementSignalCLK 3       // La pin D3 de l'Arduino recevra la ligne CLK du module KY-040
#define pinArduinoRaccordementSignalDT  4       // La pin D4 de l'Arduino recevra la ligne DT du module KY-040

// Définition des pins et périphériques pour l'écran OLED
#define SCREEN_WIDTH 128    // Largeur de l'écran OLED en pixels
#define SCREEN_HEIGHT 64    // Hauteur de l'écran OLED en pixels
#define OLED_RESET -1       // Broche de réinitialisation OLED (-1 si partagée avec l'Arduino)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Variables
int compteur = 0;                   // Cette variable nous permettra de savoir combien de crans nous avons parcourus sur l'encodeur
                                    // (sachant qu'on comptera dans le sens horaire, et décomptera dans le sens anti-horaire)

int etatPrecedentLigneSW;           // Cette variable nous permettra de stocker le dernier état de la ligne SW lu, afin de le comparer à l'actuel
int etatPrecedentLigneCLK;          // Cette variable nous permettra de stocker le dernier état de la ligne CLK lu, afin de le comparer à l'actuel

// ========================
// Initialisation programme
// ========================
void setup() {

    // Initialisation de l'écran OLED
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Adresse 0x3C pour 128x64
        Serial.println(F("SSD1306 allocation failed"));
        for(;;); // Ne pas continuer si l'initialisation échoue
    }
    display.display();
    delay(2000);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println(F("Test Encodeur KY-040"));
    display.println(F("---------------------"));
    display.display();
    delay(2000);

    // Configuration des pins de notre Arduino Nano en "entrées", car elles recevront les signaux du KY-040
    pinMode(pinArduinoRaccordementSignalSW, INPUT);         // à remplacer par : pinMode(pinArduinoRaccordementSignalSW, INPUT_PULLUP);
                                                            // si jamais votre module KY-040 n'est pas doté de résistance pull-up, au niveau de SW
    pinMode(pinArduinoRaccordementSignalDT, INPUT);
    pinMode(pinArduinoRaccordementSignalCLK, INPUT);

    // Mémorisation des valeurs initiales, au démarrage du programme
    etatPrecedentLigneSW  = digitalRead(pinArduinoRaccordementSignalSW);
    etatPrecedentLigneCLK = digitalRead(pinArduinoRaccordementSignalCLK);

    // Affichage de la valeur initiale du compteur, sur le moniteur série
    Serial.print(F("Valeur initiale du compteur = "));
    Serial.println(compteur);

    // Petite pause pour laisser se stabiliser les signaux, avant d'attaquer la boucle loop
    delay(200);

    // Initialisation de l'écran OLED
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  
    display.clearDisplay();
    display.setTextSize(1);      
    display.setTextColor(SSD1306_WHITE);  
    display.setCursor(0,0);     
    display.print(F("Initialisation..."));
    display.display();
    delay(1000);
    display.clearDisplay();
}


// =================
// Boucle principale
// =================
void loop() {

    // Lecture des signaux du KY-040 arrivant sur l'arduino
    int etatActuelDeLaLigneCLK = digitalRead(pinArduinoRaccordementSignalCLK);
    int etatActuelDeLaLigneSW  = digitalRead(pinArduinoRaccordementSignalSW);
    int etatActuelDeLaLigneDT  = digitalRead(pinArduinoRaccordementSignalDT);

    // *****************************************
    // On regarde si la ligne SW a changé d'état
    // *****************************************
    if(etatActuelDeLaLigneSW != etatPrecedentLigneSW) {

        // Si l'état de SW a changé, alors on mémorise son nouvel état
        etatPrecedentLigneSW = etatActuelDeLaLigneSW;

        // Puis on affiche le nouvel état de SW sur le moniteur série de l'IDE Arduino
        if(etatActuelDeLaLigneSW == LOW)
            Serial.println(F("Bouton SW appuyé"));
        else
            Serial.println(F("Bouton SW relâché"));

        // Petit délai de 10 ms, pour filtrer les éventuels rebonds sur SW
        delay(10);
    }

    // ******************************************
    // On regarde si la ligne CLK a changé d'état
    // ******************************************
    if(etatActuelDeLaLigneCLK != etatPrecedentLigneCLK) {

      // On mémorise cet état, pour éviter les doublons
      etatPrecedentLigneCLK = etatActuelDeLaLigneCLK;

      if(etatActuelDeLaLigneCLK == LOW) {
        
        // On compare le niveau de la ligne CLK avec celui de la ligne DT
        // --------------------------------------------------------------
        // Nota : - si CLK est différent de DT, alors cela veut dire que nous avons tourné l'encodeur dans le sens horaire
        //        - si CLK est égal à DT, alors cela veut dire que nous avons tourné l'encodeur dans le sens anti-horaire

        if(etatActuelDeLaLigneCLK != etatActuelDeLaLigneDT) {
            // CLK différent de DT => cela veut dire que nous comptons dans le sens horaire
            // Alors on incrémente le compteur
            compteur++;

            // Affichage sur le moniteur série
            Serial.print(F("Sens = horaire | Valeur du compteur = "));
            Serial.println(compteur);

            // Affichage sur l'écran OLED
            display.setCursor(0,0);
            display.print(F("Sens = horaire"));
            display.setCursor(0,10);
            display.print(F("Compteur = "));
            display.println(compteur);
            display.display();
        }
        else {
            // CLK est identique à DT => cela veut dire que nous comptons dans le sens antihoraire
            // Alors on décrémente le compteur
            compteur--;

            // Affichage sur le moniteur série
            Serial.print(F("Sens = antihoraire | Valeur du compteur = "));
            Serial.println(compteur);

            // Affichage sur l'écran OLED
            display.setCursor(0,0);
            display.print(F("Sens = antihoraire"));
            display.setCursor(0,10);
            display.print(F("Compteur = "));
            display.println(compteur);
            display.display();
        }

        // Petit délai de 1 ms, pour filtrer les éventuels rebonds sur CLK
        delay(1);
        
      }
    }

    // ********************************
    // Puis on reboucle … à l'infini !
    // ********************************
   
}