#include <U8g2lib.h>
#include <AccelStepper.h>
#include <Wire.h>
#include <LCD_I2C.h>
#include <HCSR04.h>

// --- Constantes ---
#define TRIGGER_PIN 9
#define ECHO_PIN 10
#define IN_1 30
#define IN_2 31
#define IN_3 32
#define IN_4 33
#define RED_PIN 5
#define BLUE_PIN 6
#define BUZZER_PIN 7
#define CLK_PIN 26
#define DIN_PIN 25
#define CS_PIN 27  // Chip Select

const int stepsPerRevolution = 2048;
const int angleMin = 10;
const int angleMax = 170;
const float angleTolerance = 1.0;
const int DISTANCE_ALARME = 15;
const int DISTANCE_MAX = 400;  // Distance max par défaut si rien détecté

// --- Objets globaux ---
LCD_I2C lcd(0x27, 16, 2);
HCSR04 hc(TRIGGER_PIN, ECHO_PIN);
AccelStepper myStepper(AccelStepper::HALF4WIRE, IN_1, IN_3, IN_2, IN_4);
U8G2_MAX7219_8X8_F_4W_SW_SPI u8g2(U8G2_R0, CLK_PIN, DIN_PIN, CS_PIN, U8X8_PIN_NONE, U8X8_PIN_NONE);

// --- Variables globales ---
int distance = 0;
float angle = 0;
bool alarmActive = false;
unsigned long lastDetectionTime = 0;
unsigned long currentTime = 0;
int distAlarme = DISTANCE_ALARME;
int limiteInf = 30;
int limiteSup = 60;


enum AppState { STOP, RUNNING };
AppState appState = RUNNING;

//----symbole-----

const uint8_t bitmap_check[8] = {
  0b00000000,
  0b00000001,
  0b00000010,
  0b00000100,
  0b10001000,
  0b01010000,
  0b00100000,
  0b00000000
};

const uint8_t bitmap_cross[8] = {
  0b10000001,
  0b01000010,
  0b00100100,
  0b00011000,
  0b00011000,
  0b00100100,
  0b01000010,
  0b10000001
};

const uint8_t bitmap_cercle[8] = {
  0b00111100,
  0b01000010,
  0b10100001,
  0b10010001,
  0b10001001,
  0b10000101,
  0b01000010,
  0b00111100
};

enum Symbol { SYMBOL_CHECK, SYMBOL_CROSS, SYMBOL_CERCLE };

#pragma region Fonctions Utilitaires
int angleToSteps(float angle) {
  return (angle * stepsPerRevolution) / 360.0;
}

float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

float calculerAngleDepuisDistance(int d) {
  if (d <= limiteInf) return angleMax;
  if (d >= limiteSup) return angleMin;
  return mapFloat(d, limiteInf, limiteSup, angleMax, angleMin);
}

#pragma endregion

#pragma region Tâches

void measureAndControlTask(unsigned long ct) {
  static unsigned long lastTime = 0;
  const unsigned long rate = 50;
  if (ct - lastTime < rate) return;
  lastTime = ct;

  int d = hc.dist();
  if (d == 0 || d > DISTANCE_MAX) {
    distance = DISTANCE_MAX;
  } else {
    distance = d;
  }

  angle = calculerAngleDepuisDistance(distance);
  myStepper.moveTo(angleToSteps(angle));

  if (distance <= DISTANCE_ALARME) {
    lastDetectionTime = ct;
    alarmActive = true;
  } else {
    if (ct - lastDetectionTime > 3000) {
      alarmActive = false;
    }
  }
}

void alarmTask(unsigned long ct) {
  static unsigned long lastToggleTime = 0;
  static bool ledState = false;
  const unsigned long blinkRate = 250;

  if (alarmActive) {
    if (ct - lastToggleTime >= blinkRate) {
      lastToggleTime = ct;
      ledState = !ledState;
      digitalWrite(RED_PIN, ledState ? HIGH : LOW);
      digitalWrite(BLUE_PIN, ledState ? LOW : HIGH);

      tone(BUZZER_PIN, ledState ? 1500 : 440); // Sirène de police
    }
  } else {
    digitalWrite(RED_PIN, LOW);
    digitalWrite(BLUE_PIN, LOW);
    noTone(BUZZER_PIN);
  }
}

void displayTask(unsigned long ct) {
  static unsigned long lastTime = 0;
  const unsigned long rate = 100;

  if (ct - lastTime < rate) return;
  lastTime = ct;

  

  lcd.setCursor(0, 1);
  if (alarmActive) {
    lcd.print("ALARME!         ");
      if (distance == DISTANCE_MAX) {
    lcd.print("Hors portée     ");
   } else {
    int secondsLeft = max(0, 3 - (int)((ct - lastDetectionTime) / 1000));
    lcd.print("Repos dans ");
    lcd.print(secondsLeft);
    lcd.print("s");
   }
  }else{
        lcd.setCursor(0, 0);
        lcd.print("Distance:     ");
         lcd.setCursor(10, 0);
        lcd.print(distance);
        lcd.print("cm ");
        lcd.setCursor(0, 1);
        lcd.print("Angle:   ");
        
        lcd.print(angle);
  }
}

void serialTask(unsigned long ct) {
  static unsigned long lastTime = 0;
  const unsigned long rate = 100;

  if (ct - lastTime < rate) return;
  lastTime = ct;

  // Serial.print("etd:2411160,dist:");
  // Serial.print(distance);
  // Serial.print(",deg:");
  // Serial.println(angle, 1);
}

#pragma endregion

#pragma region États
void runningState(unsigned long ct) {
  static bool firstTime = true;
  if (firstTime) {
    firstTime = false;
    return;
  }

  measureAndControlTask(ct);
  alarmTask(ct);
  displayTask(ct);
  serialTask(ct);
}
#pragma endregion

void stateManager(unsigned long ct) {
  switch (appState) {
    case STOP:
      break;
    case RUNNING:
      runningState(ct);
      break;
  }
}

#pragma commande

void afficherCheck() {
  u8g2.clearBuffer();
  u8g2.drawXBM(0, 0, 8, 8, bitmap_check);
  u8g2.sendBuffer();
}

void afficherCross() {
  u8g2.clearBuffer();
  u8g2.drawXBM(0, 0, 8, 8, bitmap_cross);
  u8g2.sendBuffer();
}

void afficherCercle() {
  u8g2.clearBuffer();
  u8g2.drawXBM(0, 0, 8, 8, bitmap_cercle);
  u8g2.sendBuffer();
}

unsigned long tempsAffichage = 0;
bool afficherSymbol = false;
int symbole = 0;

void gererCommande(String cmd) {

  if (cmd == "gDist") {
    Serial.println("PC: gDist");
    Serial.print("Arduino:  ");
    Serial.println(distance);
    afficheSymboleMillis(1);
  } else if (cmd.startsWith("cfg;alm;")) {
    String valStr = cmd.substring(cmd.lastIndexOf(';') + 1);
    distAlarme = valStr.toInt();
    if (distAlarme >= 0 && distAlarme <= 100) {
      Serial.print("PC:cfg;alm;");
      Serial.println(distance);
      Serial.println("Configure la distance de détection de l’alarme à " + valStr);
      afficheSymboleMillis(1);
    } else {
      Serial.println("Erreur de valeur");
      afficheSymboleMillis(1);
    }
  } else if (cmd.startsWith("cfg;lim_inf;")) {
    String valStr = cmd.substring(cmd.lastIndexOf(';') + 1);
    int val = valStr.toInt();
    if (val >= 0 && val <= 100) {
      if (val < limiteSup) {
        limiteInf = val;
        Serial.println("Limite inférieure configurée à " + valStr);
        afficheSymboleMillis(1);
      } else {
        Serial.println("Erreur - Limite inférieure plus grande que limite supérieure");
        afficheSymboleMillis(2);
      }
    } else {
      Serial.println("Valeur invalide");
      afficheSymboleMillis(2);
    }
  } else if (cmd.startsWith("cfg;lim_sup;")) {
    String valStr = cmd.substring(cmd.lastIndexOf(';') + 1);
    int val = valStr.toInt();
    if (val >= 0 && val <= 100) {
      if (val > limiteInf) {
        limiteSup = val;
        Serial.println("Limite supérieure configurée à " + valStr);
        afficheSymboleMillis(1);
      } else {
        Serial.println("Erreur - Limite supérieure plus petite que limite inférieure");
        afficheSymboleMillis(2);
      }
    } else {
      Serial.println("Valeur invalide");
      afficheSymboleMillis(2);
    }
  } else {
    Serial.println("Commande inconnue");
    afficheSymboleMillis(3);
  }
}

void afficheSymboleMillis(int s){
  symbole = s;
  tempsAffichage = millis();
  afficherSymbol = true;
}

void afficherSymbole(){
  if(afficherSymbol){
    switch (symbole){
      case 1:
      afficherCheck();
      break;
      case 2:
      afficherCercle();
      break;
      case 3: 
      afficherCross();
      break;
    }
    if(millis() - tempsAffichage >= 3000){
      afficherSymbol = false;
      u8g2.clearBuffer();
      u8g2.sendBuffer();
    }
  }
}


#pragma endregion

#pragma region setup-loop
void setup() {
  Serial.begin(115200);
  lcd.begin();
  lcd.backlight();
  u8g2.begin();
  u8g2.setContrast(5); // luminosité
  pinMode(RED_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(RED_PIN, LOW);
  digitalWrite(BLUE_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  // Splash screen
  unsigned long startTime = millis() + 2000;
  while (millis() < startTime) {
    lcd.setCursor(0, 0);
    lcd.print("2411160");
    lcd.setCursor(0, 1);
    lcd.print("Labo 6A");
  }
  lcd.clear();

  myStepper.setMaxSpeed(500);
  myStepper.setAcceleration(200);
  myStepper.setSpeed(200);
  myStepper.setCurrentPosition(0);
}

void loop() {
  currentTime = millis();
 if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    gererCommande(cmd);
  }
  afficherSymbole();
  myStepper.run(); // Pour AccelStepper
  stateManager(currentTime);
}
#pragma endregion