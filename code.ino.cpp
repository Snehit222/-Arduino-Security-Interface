#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <Servo.h>

// --- Pin Definitions (From Sources) ---
const int TRIG_PIN = A1;
const int ECHO_PIN = A3;
const int BUZZER_PIN = 12;
const int LED_PIN = 11;
const int BTN_VISITOR = 10;
const int SERVO_GARAGE_PIN = A0; // Left Servo
const int SERVO_DOOR_PIN = A2;   // Right Servo

// --- System State Variables ---
String userPIN = "1234";
String adminPIN = "2345";
String masterKey = "1324576890ABCD"; 
String inputBuffer = "";

bool doorOpen = false, garageOpen = false, isLocking = false;
bool alarmArmed = false, alarmTriggered = false, lightsOn = false;
bool statusMode = false, changePassMode = false, masterVerified = false;
int wrongAttemptCount = 0; 

unsigned long lockTimer = 0;
const unsigned long AUTO_LOCK_DELAY = 10000; 

Servo garageServo;
Servo doorServo;
LiquidCrystal_I2C lcd(32, 16, 2); 

const byte ROWS = 4, COLS = 4; 
char keys[ROWS][COLS] = {
  {'1','2','3','A'}, {'4','5','6','B'},
  {'7','8','9','C'}, {'*','0','#','D'}
};
byte rowPins[ROWS] = {9, 8, 7, 6}; 
byte colPins[COLS] = {5, 4, 3, 2}; 
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

void setup() {
  pinMode(TRIG_PIN, OUTPUT); pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT); pinMode(LED_PIN, OUTPUT);
  pinMode(BTN_VISITOR, INPUT_PULLUP);
  garageServo.attach(SERVO_GARAGE_PIN);
  doorServo.attach(SERVO_DOOR_PIN);
  garageServo.write(0); doorServo.write(0);
  lcd.init(); lcd.backlight();
  showMenu();
}

void loop() {
  checkKeypad();
  processVisitor(); 
  checkAutoLock();
  
  if (alarmTriggered) {
    tone(BUZZER_PIN, 2500, 100); 
    digitalWrite(LED_PIN, HIGH);
  }
  delay(100); 
}

void showMenu() {
  lcd.clear();
  lcd.print("1:Dr 2:Lt 3:Alm");
  lcd.setCursor(0,1);
  lcd.print("4:Gr 5:Pass #Clr");
}

void checkKeypad() {
  char key = keypad.getKey();
  if (key) {
    if (key == '#') { 
      inputBuffer = ""; statusMode = false; changePassMode = false; masterVerified = false;
      lcd.clear(); lcd.print("Cleared"); delay(500); showMenu();
      return;
    }
    inputBuffer += key;
    lcd.clear(); 
    
    // --- 1. STATUS MODE SUB-MENU (DCBA Active) ---
    if (statusMode) {
      lcd.print("STAT: " + inputBuffer);
      if (inputBuffer == "1***") {
        String doorStatus = isLocking ? "Locking..." : (doorOpen ? "OPEN" : "CLOSED");
        showFullStatus("Front Door", doorStatus);
      }
      else if (inputBuffer == "2***") {
        showFullStatus("Lights", lightsOn ? "ON" : "OFF");
      }
      else if (inputBuffer == "3***") {
        String alarmStatus = alarmTriggered ? "ACTIVATED!" : (alarmArmed ? "Armed" : "Off");
        showFullStatus("Alarm", alarmStatus);
      }
      else if (inputBuffer == "4***") {
        String garageStatus = isLocking ? "Locking..." : (garageOpen ? "OPEN" : "CLOSED");
        showFullStatus("Garage", garageStatus);
      }
      if (inputBuffer.length() >= 4) inputBuffer = ""; // Reset buffer if no match
      return; 
    }

    // --- 2. ALARM DISARM (ABCD) ---
    if (inputBuffer == "ABCD") {
      alarmArmed = false; alarmTriggered = false; wrongAttemptCount = 0;
      noTone(BUZZER_PIN); digitalWrite(LED_PIN, LOW);
      inputBuffer = ""; lcd.print("Alarm Reset"); delay(1000); showMenu();
      return;
    }

    // --- 3. NORMAL COMMANDS AND PIN ENTRY ---
    lcd.print("In: "); lcd.print(inputBuffer);
    
    if (inputBuffer == "DCBA") { statusMode = true; inputBuffer = ""; lcd.clear(); lcd.print("STATUS MODE ON"); }
    else if (inputBuffer == "D***") { alarmArmed = true; inputBuffer = ""; lcd.clear(); lcd.print("System Armed"); delay(1000); showMenu(); }
    else if (inputBuffer == "A***") { doorServo.write(0); doorOpen = false; inputBuffer = ""; }
    else if (inputBuffer == "B***") { garageServo.write(0); garageOpen = false; inputBuffer = ""; }
    else if (inputBuffer == "C***") { lightsOn = !lightsOn; digitalWrite(LED_PIN, lightsOn ? HIGH : LOW); inputBuffer = ""; }
    else if (inputBuffer == "***B") { garageServo.write(90); garageOpen = true; lockTimer = millis(); inputBuffer = ""; }
    else if (inputBuffer == "5***") { changePassMode = true; inputBuffer = ""; lcd.clear(); lcd.print("Enter Master:"); }
    
    // Process PINs
    else if (inputBuffer.length() == 4) {
      if (inputBuffer == userPIN || inputBuffer == adminPIN) {
        wrongAttemptCount = 0; alarmTriggered = false; noTone(BUZZER_PIN);
        doorServo.write(90); doorOpen = true; 
        if (inputBuffer == adminPIN) { garageServo.write(90); garageOpen = true; }
        lockTimer = millis(); inputBuffer = "";
        lcd.clear(); lcd.print("Welcome!"); delay(1000); showMenu();
      } else {
        wrongAttemptCount++; inputBuffer = "";
        lcd.clear(); lcd.print("Wrong PIN!");
        if (alarmArmed && wrongAttemptCount >= 3) { alarmTriggered = true; lcd.clear(); lcd.print("ALARM TRIGGERED"); }
        else { delay(1000); showMenu(); }
      }
    }
  }
}

void showFullStatus(String label, String value) {
  lcd.clear();
  lcd.print(label + ":");
  lcd.setCursor(0, 1);
  lcd.print(value);
  delay(3000); 
  statusMode = false; // Exit status mode after showing
  inputBuffer = ""; 
  showMenu();
}

void processVisitor() {
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  float dist = (pulseIn(ECHO_PIN, HIGH, 30000) / 2.0) / 29.1;
  if (dist >= 2.5 && dist <= 120.0 && (digitalRead(BTN_VISITOR) == LOW)) {
    lcd.clear(); lcd.print("VISITOR DETECTED");
    tone(BUZZER_PIN, 1047, 300); delay(400); tone(BUZZER_PIN, 880, 500);
    delay(1000); if (!alarmTriggered) showMenu();
  }
}

void checkAutoLock() {
  if ((doorOpen || garageOpen) && (millis() - lockTimer >= AUTO_LOCK_DELAY)) {
    isLocking = true;
    doorServo.write(0); garageServo.write(0);
    delay(1000); // Small delay to show "Locking" status if checked
    doorOpen = false; garageOpen = false;
    isLocking = false;
    if (!alarmTriggered) showMenu();
  }
}
