#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>

// Définition de l'écran OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


// Fonction d'affichage
void displayMessage(const char* msg) 
{
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("=== Message Recu ===");
  display.println(msg);
  display.display();
}

// Pins et NRF24
RF24 radio(7, 8);  // CE, CSN

// Variables pour réception
char receivedMessage[128]; // EF11
char senderPseudo[10];     // EF10
bool newMessage = false;

// Setup
void setup() 
{
  Serial.begin(9600);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  
  radio.begin();
  radio.setChannel(100);
  radio.openReadingPipe(1, 0xF0F0F0F0E1LL);
  radio.startListening();
  
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  displayMessage("En attente de messages..."); // OK maintenant
}

void loop() 
{
  if (radio.available()) {
    radio.read(&senderPseudo, sizeof(senderPseudo));
    radio.read(&receivedMessage, sizeof(receivedMessage));
    newMessage = true;
  }

  if (newMessage) {
    char buffer[150];
    snprintf(buffer, sizeof(buffer), "De: %s\n%s", senderPseudo, receivedMessage);
    displayMessage(buffer);
    Serial.println(buffer);
    newMessage = false;
  }
}

