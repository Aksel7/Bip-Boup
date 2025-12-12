#include <Wire.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);


// Drapeau pour indiquer qu'un message doit être affiché
int message_recu = 0;

// --- Initialisation OLED à mettre dans setup() ---
void setupOLED() {
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Adresse I2C
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
}

// --- Fonction pour afficher le message ---
void afficherMessage(Message msg) {
  display.clearDisplay();       // Efface l'écran
  display.setCursor(0, 0);      // Début de l'affichage
  display.print("De : ");
  display.println(msg.pseudo);  // Affiche le pseudo de l'expéditeur
  display.println("----------------");
  display.println(msg.text);    // Affiche le texte du message
  display.display();            // Met à jour l'écran
}

void traiterMessageRecu() {
  if (message_recu == 1) {
    afficherMessage(msg);       // Affiche le message reçu
    message_recu = 0;           // Réinitialise le drapeau
  }
}
