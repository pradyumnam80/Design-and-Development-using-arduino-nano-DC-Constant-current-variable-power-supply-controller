#include <LiquidCrystal.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_MCP4725.h>

Adafruit_MCP4725 dac;
int Contrast = 80;  
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
//const int lcdContrastPin = 6;   // PWM pin

// Pin assignments
const byte EditButton = A0;
const byte IncrementButton = A1;
const byte ShiftButton = A2;
const byte OutputControlPin = A7;
const byte currentSensePin = A7;
const byte shuntStartCol = 0;
const int MAX_SET_AMP = 500;   // 5.00 A represented as 500
//int sensorValue = A7;
const int pwmPin = 9; // PWM-capable pin
int dutyCycle = 0;

// User data
int password[4] = {1, 2, 3, 4};
int calibrationPassword[4] = {0, 0, 3, 0};
int inputPassword[4] = {0, 0, 0, 0};
int inputCurrent[4] = {0, 0, 0, 0};
int inputTime[5] = {0, 0, 0, 0, 0}; // MMMSS
int setSec = 0; // seconds extracted from input
int shuntMilliVolt = 75;// default 075, you can replace
int passIndex = 0;
int MFSV = 1600;   // 0–8V full scale
int MFSI = 5000; // Max Full-Scale Current
int ampLimit = 500;              // 05.00A default (×100)
int ampLimitDigits[4] = {0, 5, 0, 0};
bool ampLimitSettingMode = false;
//bool enterAhSettingMode = false; 
const int EEPROM_SHUNT_ADDR = 64;
const int EEPROM_MFSV_ADDR = 60;
const int EEPROM_MFSI_ADDR = 62;

float voltageMeasured = 0.0;
float volt, v;
float currentMeasured = 0.0;
float dacSetAmp = 0.0;   // actual commanded current
float displayCurrent = 0.0;
float lastValidVolt = 0.0;
int ahDigits[5] = {0,0,0,0,0};
byte ahDigitIndex = 0;
bool ahSettingMode = false;
bool specialPasswordHandled = false;
bool resumeScreenShown = false;
bool ahTripLatched = false;

byte displayScreen = 0;   // 0..3

//const float ShuntAmps = 50;

// Timer
//int rawCurrent = 0.0;
int setMin = 0;
int setAmp = 0;
int remainingTime = 0;
bool calibrationJustFinished = false;
bool noLoadDetected = false;
bool rectifierFaultDetected = false;
bool RUN = false;
bool passwordMode = false;
bool shuntSettingMode = false;
bool enterPassword = false;
bool passwordChecked = false;
bool timeSettingMode = false;
bool ampSettingMode = false;
bool timerStarted = false;
bool isCalibrationMode = false;
bool waitingForStart = false;
bool postCompleteMenu = false;   // Post-completion YES/NO menu
bool completedHold = false;   // holds COMPLETED screen
bool timeConfirmed = false;
bool ampConfirmed  = false;
bool passwordChangeMode = false;
int newPassword[4] = {0, 0, 0, 0};
int newPassIndex = 0;
bool confirmPasswordChange = false;
bool confirmYesSelected = true;   // true = Y, false = N
bool confirmAmpLimitChange = false;
bool postCompleteHold = false;
bool showSetAmpScreen = true;   // true = SA preview, false = normal screen
bool waitingForResume = false;
bool inRampMode = false;
bool justResumed = false;
bool editMenuMode = false;
bool noLoadLatched = false;
bool rectifierLatched = false;
bool ahSetMode = false;
bool ahTotalMode = false;
bool ahActMode = false;
byte shuntDigitPos = 0;
float ahActual = 0.0;          // resets every batch
float ahTotal = 0.0;           // lifetime totalizer
float ahSet = 0.0;             // user set value
bool rampStarted = false;
unsigned long lastAhMillis = 0;
const byte relayPin = 13;       // choose relay pin
bool relayActive = false;
unsigned long relayStartTime = 0;


const byte buzzerPin = 8;   // choose any free digital pin

// Debounce
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 200;
bool lastEditButtonState = HIGH;
bool lastIncrementButtonState = HIGH;
bool lastShiftButtonState = HIGH;
#define EEPROM_PASSWORD_ADDR 0
#define EEPROM_AMP_LIMIT_ADDR 70
#define EEPROM_RUN_FLAG_ADDR     100
#define EEPROM_REMAIN_TIME_ADDR  101   // needs 2 bytes (int)
#define EEPROM_SETAMP_ADDR       105   // needs 2 bytes (int)
#define EEPROM_SETMIN_ADDR 110
#define EEPROM_SETSEC_ADDR 112
#define EEPROM_AH_TOTAL_ADDR 120
#define EEPROM_AH_SET_ADDR   124
#define EEPROM_AH_SET_ADDR  10   // any safe address
#define EEPROM_AH_ACT_ADDR 50
void setup() {
  //pinMode(lcdContrastPin, OUTPUT);
  analogWrite(6,Contrast);
  lcd.begin(16, 2);
  EEPROM.get(EEPROM_AH_TOTAL_ADDR, ahTotal);
  EEPROM.get(EEPROM_AH_SET_ADDR, ahSet);
  EEPROM.get(EEPROM_AH_ACT_ADDR, ahActual);   // restore Ah actual
  Wire.begin();
  dac.begin(0x60);
  // ✅ FORCE DAC OFF AT POWER-UP
dacSetAmp = 0.0;
setOutputVoltage(0.0);   // DAC = 0V
  dac.setVoltage(0, false);
  displayWelcomeScreen();
  pinMode(EditButton, INPUT_PULLUP);
  pinMode(IncrementButton, INPUT_PULLUP);
  pinMode(ShiftButton, INPUT_PULLUP);
  pinMode(A6, INPUT);
  pinMode(currentSensePin, INPUT);
 // pinMode(sensorValue, INPUT);
  pinMode(OutputControlPin, OUTPUT);
  pinMode(pwmPin, OUTPUT); // For PWM LED effect
  digitalWrite(OutputControlPin, HIGH);
  pinMode(buzzerPin, OUTPUT);
digitalWrite(buzzerPin, LOW);   // buzzer OFF initially
  pinMode(relayPin, OUTPUT);
digitalWrite(relayPin, HIGH);   // OFF (because active LOW)
 relayActive = false;


   for (int i = 0; i < 4; i++) {
    int val = EEPROM.read(EEPROM_PASSWORD_ADDR + i);
    if (val == 255) val = i+1; // default 1,2,3,4 if EEPROM empty
    password[i] = val;
  }
  if (EEPROM.read(EEPROM_AMP_LIMIT_ADDR) == 255 &&
    EEPROM.read(EEPROM_AMP_LIMIT_ADDR + 1) == 255) {
  EEPROM.put(EEPROM_AMP_LIMIT_ADDR, ampLimit);
} else {
  EEPROM.get(EEPROM_AMP_LIMIT_ADDR, ampLimit);
}

// Convert to digits
ampLimitDigits[0] = (ampLimit / 1000) % 10;
ampLimitDigits[1] = (ampLimit / 100) % 10;
ampLimitDigits[2] = (ampLimit / 10) % 10;
ampLimitDigits[3] = ampLimit % 10;

  // EEPROM for time
 /* if (EEPROM.read(50) != 50) {
    EEPROM.write(50, 50);
    EEPROM.write(51, 0);
  } else {
    setMin = EEPROM.read(51);
  }*/

 // setMin = inputTime[0] * 100 + inputTime[1] * 10 + inputTime[2];
 // setAmp = inputCurrent[0] * 100 + inputCurrent[1] * 10 + inputCurrent[2];

  // EEPROM for calibration
  if (EEPROM.read(EEPROM_MFSV_ADDR) == 255 && EEPROM.read(EEPROM_MFSV_ADDR + 1) == 255) {
    EEPROM.put(EEPROM_MFSV_ADDR, MFSV);
  } else {
    EEPROM.get(EEPROM_MFSV_ADDR, MFSV);
  }

  if (EEPROM.read(EEPROM_MFSI_ADDR) == 255 && EEPROM.read(EEPROM_MFSI_ADDR + 1) == 255) {
    EEPROM.put(EEPROM_MFSI_ADDR, MFSI);
  } else {
    EEPROM.get(EEPROM_MFSI_ADDR, MFSI);
  }

  if (EEPROM.read(EEPROM_SHUNT_ADDR) == 255) {
    EEPROM.write(EEPROM_SHUNT_ADDR, shuntMilliVolt);
  } else {
    shuntMilliVolt = EEPROM.read(EEPROM_SHUNT_ADDR);
  }
  // ===== POWER FAIL RESUME CHECK =====
byte wasRunning = EEPROM.read(EEPROM_RUN_FLAG_ADDR);
if (wasRunning == 1) {
  EEPROM.get(EEPROM_REMAIN_TIME_ADDR, remainingTime);
  EEPROM.get(EEPROM_SETAMP_ADDR, setAmp);

  if (remainingTime > 0 && setAmp > 0) {
    showResumeScreen();
  }
}
// ===== RESTORE LAST SET VALUES =====
  EEPROM.get(EEPROM_SETMIN_ADDR, setMin);
  EEPROM.get(EEPROM_SETSEC_ADDR, setSec);
  EEPROM.get(EEPROM_SETAMP_ADDR, setAmp);
// Restore digits for ST preview
  inputTime[0] = (setMin / 100) % 10;
    inputTime[1] = (setMin / 10)  % 10;
    inputTime[2] = setMin % 10;
    inputTime[3] = (setSec / 10) % 10;
    inputTime[4] = setSec % 10;
  // Restore digits for SA preview
inputCurrent[0] = (setAmp / 1000) % 10;
inputCurrent[1] = (setAmp / 100)  % 10;
inputCurrent[2] = (setAmp / 10)   % 10;
inputCurrent[3] = setAmp % 10;

if (!waitingForResume) {
  showSetAmpScreen = true;
  updateMainDisplay();
} else {
  return;   // 🔥 STOP screen refresh
}
EEPROM.get(EEPROM_AH_TOTAL_ADDR, ahTotal);
if (isnan(ahTotal) || ahTotal < 0) ahTotal = 0.0;
EEPROM.get(EEPROM_AH_SET_ADDR, ahSet);
if (isnan(ahSet) || ahSet < 0) ahSet = 0.0;
EEPROM.get(EEPROM_AH_ACT_ADDR, ahActual);
if (isnan(ahActual) || ahActual < 0) ahActual = 0.0;
  // convert value → digits for editor
  loadAhDigitsFromValue();   // ✅ ADD HERE
  updateMainDisplay();
  // ===== TEST MODE =====
 // ahActual = 0000.1;   // FORCE Ah Actual = 0000.1
}
void killAllEditModes() {
  timeSettingMode = false;
  ampSettingMode  = false;
  shuntSettingMode = false;
  passwordChangeMode = false;
  ampLimitSettingMode = false;
  confirmPasswordChange = false;
  confirmAmpLimitChange = false;
  editMenuMode = false;
  waitingForStart = false;
  RUN = false;
}

void loop() {
  // 🔥 AH SET MODE HAS TOP PRIORITY
/*if (ahSettingMode) {
    handleAhSetting(lastIncrementButtonState, lastShiftButtonState);
    return;   // ⛔ STOP other screens from drawing
}*/

  // ===== WAITING FOR POWER-FAIL RESUME =====
if (waitingForResume) {
  bool editState = digitalRead(EditButton);

  if (editState == LOW && millis() - lastDebounceTime > debounceDelay) {
    lastDebounceTime = millis();

  lcd.clear();               // 🔥 remove EDIT:RESUME
  resumeScreenShown = false;

    waitingForResume = false;
    RUN = true;
    timerStarted = true;
    justResumed = true;   // 🔥 ADD THIS
  
     // ✅ RESTORE ORIGINAL SET TIME
     EEPROM.get(EEPROM_SETMIN_ADDR, setMin);
     EEPROM.get(EEPROM_SETSEC_ADDR, setSec);
     EEPROM.get(EEPROM_AH_TOTAL_ADDR, ahTotal);
     EEPROM.get(EEPROM_AH_SET_ADDR, ahSet);
     EEPROM.get(EEPROM_AH_ACT_ADDR, ahActual);
    inputTime[0] = (setMin / 100) % 10;
    inputTime[1] = (setMin / 10)  % 10;
    inputTime[2] = setMin % 10;
    inputTime[3] = (setSec / 10) % 10;
    inputTime[4] = setSec % 10;

    // Restore AH digits
int ahTemp = ahSet * 10;
ahDigits[0] = (ahTemp / 10000) % 10;
ahDigits[1] = (ahTemp / 1000) % 10;
ahDigits[2] = (ahTemp / 100) % 10;
ahDigits[3] = (ahTemp / 10) % 10;
ahDigits[4] = ahTemp % 10;

    // ===================================
    // ===== RESTORE DIGITS FOR SA DISPLAY =====
    lcd.clear();
    inputCurrent[0] = (setAmp / 1000) % 10;
    inputCurrent[1] = (setAmp / 100)  % 10;
    inputCurrent[2] = (setAmp / 10)   % 10;
    inputCurrent[3] = setAmp % 10;
    // ========================================

    lcd.clear();
    lcd.setCursor(3,0);
    lcd.print("Resuming...");
    delay(500);

    // ===== RAMP AGAIN LIKE NORMAL START =====
    lcd.clear();
    startCurrentRamp(setAmp / 100.0);
    // =======================================

    // Re-arm power fail flag
    EEPROM.write(EEPROM_RUN_FLAG_ADDR, 1);
  }
  return;
}
  // Voltage
  float a = analogRead(A6);
  v = (a * 5.0) / 1023.0;
  volt = (v * MFSV) / 1000.0;

  // Latch last valid voltage
if (volt > 0.05) {   // threshold
  lastValidVolt = volt;
}

 // ===== CURRENT ADC READ (A7) =====
if (RUN && setAmp > 0) { 
  int rawCurrent = analogRead(currentSensePin); 
  voltageMeasured = rawCurrent * (5.0 / 1023.0); 
  currentMeasured = voltageMeasured * (MFSI / 5.0); } 
  else { 
    voltageMeasured = 0.0;
     currentMeasured = 0.0; 
     }
   /* Serial.println("Relay ON");
  digitalWrite(relayPin, LOW);   // RELAY ON (active LOW)
  delay(1000);                   // 3 seconds ON

  Serial.println("Relay OFF");
  digitalWrite(relayPin, HIGH);  // RELAY OFF
  delay(1000); */
  if (RUN) {

  unsigned long now = millis();

  if (now - lastAhMillis >= 1000) {

    lastAhMillis = now;

    float deltaAh = currentMeasured / 3600.0;

    ahActual += deltaAh;
    ahTotal  += deltaAh;

    static unsigned long lastAhSave = 0;

    if (millis() - lastAhSave > 10000) {

      lastAhSave = millis();

      EEPROM.put(EEPROM_AH_TOTAL_ADDR, ahTotal);
      EEPROM.put(EEPROM_AH_ACT_ADDR, ahActual);
}
  
    char ahStr[8];
    dtostrf(ahActual, 6, 1, ahStr);
    for (int i = 0; i < 6; i++) if (ahStr[i] == ' ') ahStr[i] = '0';
    lcd.setCursor(0, 1);
    lcd.print(ahStr);
}
float ahActualDisp = round(ahActual * 10.0) / 10.0;

   // ===== AH TRIP RELAY =====
if (RUN && !ahTripLatched && ahActualDisp >= ahSet && ahSet > 0) {
  digitalWrite(relayPin, LOW);
  relayActive = true;
  relayStartTime = millis();
  ahTripLatched = true;
  EEPROM.put(EEPROM_AH_TOTAL_ADDR, ahTotal);
}
// ===== AUTO RESET AFTER 1 MIN =====
if (relayActive && millis() - relayStartTime >= 60000UL) {
  digitalWrite(relayPin, HIGH);  // OFF
  relayActive = false;
  ahTripLatched = false;
  ahActual = 0.0;
}
} 
// ===== LOAD / RECTIFIER FAULT CHECK =====
if (RUN) {
/*int dacValue = (int)((currentMeasured / 5.0) * 4095); 
dacValue = constrain(dacValue, 0, 4095); 
dac.setVoltage(dacValue, false); */
 
  // Both voltage & current zero → Rectifier Fault
  // -------- RECTIFIER FAULT --------
  if (voltageMeasured < 0.05 && volt < 0.05) {
    if (!rectifierLatched) {
      rectifierLatched = true;
      noLoadLatched = false;

      dacSetAmp = 0;
      setOutputVoltage(0);

      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Rectifier Fault");
    }
    return;   // stay locked
  }
// -------- NO LOAD --------
if (voltageMeasured < 0.05 && volt >= 0.05) {

  if (!noLoadLatched) {
    noLoadLatched = true;
    rectifierLatched = false;

   // dacSetAmp = 0;
   // setOutputVoltage(0);

    // Draw screen ONCE
    lcd.clear();
    lcd.noCursor();
    lcd.noBlink();
    lcd.setCursor(4, 1);
    lcd.print("NO LOAD");
  }

  // Update voltage only (no clear)
  float voltLive = lastValidVolt;   // use latched value

  char voltStr[7];
  dtostrf(voltLive, 5, 2, voltStr);
  if (voltStr[0] == ' ') voltStr[0] = '0';

  // Center voltage
  int col = (16 - 6) / 2;   // "00.00V"
  lcd.setCursor(col, 0);
  lcd.print(voltStr);
  lcd.print("V");

  return;   // stay locked
}
  // -------- LOAD RESTORED --------
  if (noLoadLatched || rectifierLatched) {
    noLoadLatched = false;
    rectifierLatched = false;
    
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Load Detected");
    delay(500);
      
    // 🔥 RESTART RAMP
    lcd.clear(); // remove fault screen
     lcd.noCursor(); 
     lcd.noBlink();
    startCurrentRamp(setAmp / 100.0);
    lcd.clear(); // remove fault screen
     lcd.noCursor(); 
     lcd.noBlink(); 
     updateMainDisplay();
  }
}

  // Buttons
  bool editButtonState = digitalRead(EditButton);
  bool incrementButtonState = digitalRead(IncrementButton);
  bool shiftButtonState = digitalRead(ShiftButton);

    // 2️⃣ HANDLE SHIFT BUTTON  ✅ PUT YOUR CODE HERE
 // 2️⃣ GLOBAL SCREEN SHIFT  ✅ ADD IT HERE
  if (!ahSettingMode &&
      !timeSettingMode &&
      !ampSettingMode &&
      !ampLimitSettingMode &&
      !enterPassword &&
      !editMenuMode &&
      shiftButtonState == LOW &&
      millis() - lastDebounceTime > debounceDelay) {

    lastDebounceTime = millis();

    // 🔒 ramp lock → always SA
    if (inRampMode) {
      displayScreen = 0;
    } else {
      displayScreen++;
      if (displayScreen > 3) displayScreen = 0;
    }

    lcd.clear();
  }
      // 3️⃣ AH SETTING MODE (DIGIT EDIT SCREEN)
  if (ahSettingMode) {
    handleAhSetting(incrementButtonState, shiftButtonState);
    lastEditButtonState = editButtonState;
    lastIncrementButtonState = incrementButtonState;
    lastShiftButtonState = shiftButtonState;
    return;   // 🔴 exits loop()
  }
  
  // ===== AMP LIMIT CONFIRM (Y / N) =====
if (confirmAmpLimitChange) {

  // Make sure cursor is visible
  lcd.cursor();
  lcd.noBlink();

  // SHIFT → toggle Y / N
  if (shiftButtonState == LOW && millis() - lastDebounceTime > debounceDelay) {
    lastDebounceTime = millis();
    confirmYesSelected = !confirmYesSelected;

    if (confirmYesSelected)
      lcd.setCursor(0,1);   // Y
    else
      lcd.setCursor(2,1);   // N (correct column)
  }

  // EDIT → confirm
  if (editButtonState == LOW && millis() - lastDebounceTime > debounceDelay) {
    lastDebounceTime = millis();

    confirmAmpLimitChange = false;

    if (confirmYesSelected) {
      // ✅ YES → enter amp limit setting
      ampLimitSettingMode = true;
      passIndex = 0;

      lcd.clear();
      lcd.noBlink();
      lcd.cursor();
      lcd.setCursor(0,0);
      lcd.print("Set Amp Limit");
      displayAmpLimit();
    } else {
      // ❌ NO → return
      lcd.clear();
      lcd.noCursor();
      lcd.noBlink();
      updateMainDisplay();
    }
  }

  return;   // 🔒 block all other logic
}
// ===== CHANGE PASSWORD CONFIRM (Y / N) =====
if (confirmPasswordChange) {

  // SHIFT → move cursor
  if (shiftButtonState == LOW && millis() - lastDebounceTime > debounceDelay) {
    lastDebounceTime = millis();

    confirmYesSelected = !confirmYesSelected;

    if (confirmYesSelected)
      lcd.setCursor(0,1);   // Y
    else
      lcd.setCursor(2,1);   // N
  }

  // EDIT → confirm selection
  if (editButtonState == LOW && millis() - lastDebounceTime > debounceDelay) {
    lastDebounceTime = millis();

    confirmPasswordChange = false;

    if (confirmYesSelected) {
      // YES → Enter new password
      passwordChangeMode = true;
      newPassIndex = 0;

      for (int i = 0; i < 4; i++)
        newPassword[i] = password[i];

      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Enter New Pass");
      lcd.setCursor(0,1);
      displayDigits(newPassword, newPassIndex);

      lcd.cursor();
      lcd.noBlink();
    } else {
      // NO → Return to final screen
       lcd.clear();
      lcd.noCursor();
      lcd.noBlink();
      updateMainDisplay();
    }
  }

  return;   // block all other logic
}
  // 🔴 ABSOLUTE PRIORITY
if (ampLimitSettingMode) {
    handleAmpLimitSetting(incrementButtonState, shiftButtonState);
    lastEditButtonState = editButtonState;
    lastIncrementButtonState = incrementButtonState;
    lastShiftButtonState = shiftButtonState;
    return;   // 🔒 block everything else
}
  
  // ===== EDIT DURING RUN =====
if (RUN &&
    editButtonState == LOW &&
    millis() - lastDebounceTime > debounceDelay) {

  lastDebounceTime = millis();

  // 🔴 HARD STOP OUTPUT
  RUN = false;
  timerStarted = false;
  remainingTime = 0;

  dacSetAmp = 0.0;
  setOutputVoltage(0.0);   // DAC = 0V
  dac.setVoltage(0, false);

  digitalWrite(buzzerPin, LOW);   // safety

  // Clear power-fail resume flag
  EEPROM.write(EEPROM_RUN_FLAG_ADDR, 0);

  // Go to password
  lcd.clear();
  lcd.noCursor();
  lcd.noBlink();

  startPasswordEntry();   // → Enter Password

  return;   // 🔒 block rest of loop
}

  // 3. EDIT MENU TRIGGER (from SA screen)
  if (!RUN && !editMenuMode && !timeSettingMode && !confirmPasswordChange && !ampSettingMode &&
    !ampLimitSettingMode &&!confirmAmpLimitChange && !passwordChangeMode && !ahSetMode &&
    !enterPassword && !postCompleteMenu && !completedHold &&
    editButtonState == LOW &&
    millis() - lastDebounceTime > debounceDelay) {

    lastDebounceTime = millis();
    editMenuMode = true;

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Edit:Amp&Time");
    lcd.setCursor(0,1);
    lcd.print("SHIFT:Start");
    return;
  }

  // 4. EDIT MENU MODE  ← (PUT YOUR BLOCK HERE)
  if (editMenuMode) {

    // EDIT → password → edit
    if (editButtonState == LOW &&
        millis() - lastDebounceTime > debounceDelay) {

      lastDebounceTime = millis();
      editMenuMode = false;
      startPasswordEntry();
      return;
    }

    // SHIFT → start immediately
    if (shiftButtonState == LOW &&
        millis() - lastDebounceTime > debounceDelay) {

      lastDebounceTime = millis();
      editMenuMode = false;

      remainingTime = setMin * 60 + setSec;
      RUN = true;
      timerStarted = true;

      lcd.clear();
      lcd.print("Plating Started");
      delay(500);
      lcd.clear();

      startCurrentRamp(setAmp / 100.0);
      return;
    }

    return;   // 🔒 block everything else
  }
  // ===== EXCLUSIVE POST-COMPLETE MODE =====
/*if (postCompleteMenu) {

  // EDIT → Edit Time & Amp
  if (editButtonState == LOW && millis() - lastDebounceTime > debounceDelay) {
    lastDebounceTime = millis();

    postCompleteMenu = false;
    lcd.clear();
    lcd.noCursor();
    lcd.noBlink();

    enterTimeSettingMode();
    return;
  }

  // SHIFT → Repeat immediately (NO screen toggle)
  if (shiftButtonState == LOW && millis() - lastDebounceTime > debounceDelay) {
    lastDebounceTime = millis();

    postCompleteMenu = false;
    lcd.clear();
    lcd.noCursor();
    lcd.noBlink();

    remainingTime = setMin * 60 + setSec;
    RUN = true;
    timerStarted = true;

    digitalWrite(buzzerPin, LOW);
    startCurrentRamp(setAmp / 100.0);
    return;
  }

  return;   // 🔒 BLOCK ALL OTHER LOGIC
}*/

  // ===== SHIFT BUTTON → TOGGLE DISPLAY SCREEN ONLY =====
if (shiftButtonState != lastShiftButtonState &&
    shiftButtonState == LOW &&
    millis() - lastDebounceTime > debounceDelay) {

    lastDebounceTime = millis();

    /* ================= RUN MODE ================= */
    if (RUN) {
        // During RUN → ONLY toggle display screens
       displayScreen = (displayScreen + 1) % 4;
        updateMainDisplay();
    }

    /* =============== NOT RUNNING ================= */
    else {
       if (ahSetMode) {                     // 🔥 ADD THIS
        passIndex = (passIndex + 5) % 5;       // for 0000.0
        displayAhSet();
    }
       else if (enterPassword) {
            passIndex = (passIndex + 1) % 4;
            displayDigits(inputPassword, passIndex);
        }
        else if (passwordChangeMode) {
            newPassIndex = (newPassIndex + 1) % 4;
            displayDigits(newPassword, newPassIndex);
        }
        else if (ampLimitSettingMode) {
            passIndex = (passIndex + 1) % 4;
            displayAmpLimit();
        }
        else if (timeSettingMode) {
            passIndex = (passIndex + 1) % 5;
            displaySetTime();
        }
        else if (ampSettingMode) {
            passIndex = (passIndex + 1) % 4;
            displaySetAmps();
        }
        else if (shuntSettingMode) {
            shuntDigitPos = (shuntDigitPos + 1) % 3;
           enterShuntSettingMode();   // ← no parameter
        }
        else {
            // normal idle toggle
            showSetAmpScreen = !showSetAmpScreen;
            updateMainDisplay();
        }
    }
}

lastShiftButtonState = shiftButtonState;

 // ===== COMPLETED HOLD SCREEN =====
// ===== COMPLETED HOLD SCREEN =====
if (completedHold) {

  // Wait ONLY for EDIT
  if (editButtonState == LOW && millis() - lastDebounceTime > debounceDelay) {
    lastDebounceTime = millis();
    // 🔴 NOW turn DAC OFF
    dacSetAmp = 0.0;
    setOutputVoltage(0.0);
    digitalWrite(buzzerPin, LOW);   // 🔇 buzzer OFF
    completedHold = false;
    postCompleteMenu = true;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("EDIT:Time &Amp");
    lcd.setCursor(0, 1);
    lcd.print("SHIFT:Repeat");
  }

  return;   // block all other logic
}

  // ===== POST-COMPLETION MENU (YES / NO) =====
  if (postCompleteMenu) {
  // EDIT → Edit Time & Amp
  if (editButtonState == LOW && millis() - lastDebounceTime > debounceDelay) {
    lastDebounceTime = millis();

    postCompleteMenu = false;
    // 🔥 CRITICAL FIX
    passwordMode = false;
    enterPassword = false;
    lcd.clear();
    lcd.noCursor();
    lcd.noBlink();
    enterTimeSettingMode();   // ✅ goes to "Edit Time & Amp"
    return;
  }
  
  // SHIFT → Repeat same cycle
  if (shiftButtonState == LOW && millis() - lastDebounceTime > debounceDelay) {
    lastDebounceTime = millis();

    postCompleteMenu = false;
    lcd.clear();
    lcd.noCursor();
    lcd.noBlink();

    remainingTime = setMin * 60 + setSec;
    RUN = true;
    timerStarted = true;

    float finalAmp = setAmp / 100.0;
    digitalWrite(buzzerPin, LOW);
    startCurrentRamp(finalAmp);
    return;
  }
 return;   // block other logic
}

  // ===== FINAL START CONFIRMATION =====
if (waitingForStart) {
  if (editButtonState == LOW && millis() - lastDebounceTime > debounceDelay) {
    lastDebounceTime = millis();

    waitingForStart = false;
    RUN = true;
    timerStarted = true;
    remainingTime = setMin * 60 + setSec;

    // ✅ SAVE ORIGINAL SET TIME
    EEPROM.put(EEPROM_SETMIN_ADDR, setMin);
    EEPROM.put(EEPROM_SETSEC_ADDR, setSec);

    lcd.clear();
    lcd.print("Plating Started");
    delay(500);
    lcd.clear();

    // ✅ DAC STARTS ONLY HERE
    startCurrentRamp(setAmp / 100.0);

    digitalWrite(OutputControlPin, HIGH);
  }
  return;
}


  // Edit button
  // ===== GLOBAL EDIT BUTTON HANDLER =====
if ( editButtonState != lastEditButtonState &&
    editButtonState == LOW &&
    millis() - lastDebounceTime > debounceDelay) {

    // 🔒 BLOCK EDIT while setting Amp Limit
    /*if (ampLimitSettingMode) {
        lastDebounceTime = millis();
        return;
    }*/

    lastDebounceTime = millis();

    // If we are in password change mode, ignore normal password checks
    if (passwordChangeMode) {
        // Handle inside handlePasswordChange() function
    }
    else if (enterPassword && passIndex < 3) {
        passIndex++;
        displayDigits(inputPassword, passIndex);
    }
    else if (enterPassword) {

    enterPassword = false;
    passwordChecked = isPasswordCorrect();

    // 🔐 SPECIAL PASSWORDS (0081 / 0080 / calibration)
    if (ampLimitSettingMode || confirmPasswordChange || confirmAmpLimitChange || isCalibrationMode) {
        return;   // ⛔ do NOT show Wrong Pass
    }
    
    if (calibrationJustFinished) {
        calibrationJustFinished = false;
        lcd.noBlink();
        lcd.noCursor();
        lcd.clear();
        displayFinalScreen();
    }
  else if (passwordChecked) {

    passwordMode = false;
    editMenuMode = false;

    // 🔥 IF special password (0020 / etc) already opened its screen
    if (specialPasswordHandled) {
        specialPasswordHandled = false;
        return;   // DO NOT go to Time & Amp
    }

    // normal password (1234)
    lcd.clear();
    lcd.noCursor();
    lcd.noBlink();
    lcd.setCursor(4, 0);
    lcd.print("Pass Ok");
    delay(1000);
    lcd.clear();
    enterTimeSettingMode();
}


    else {
        passwordMode = true;
        lcd.clear();
        lcd.noBlink();
        lcd.noCursor();
        lcd.setCursor(4, 0);
        lcd.print("Wrong Pass");
        delay(1000);
        lcd.clear();
        passIndex = 0;
        displayFinalScreen();
    }
}
     // Only here:
if (editMenuMode) {
    // EDIT → password
}

}
  // Mode handlers
  if (enterPassword && !passwordChecked) handlePasswordEntry(incrementButtonState, shiftButtonState);
  if (passwordChangeMode) handlePasswordChange(incrementButtonState, shiftButtonState);
 // if (ampLimitSettingMode)handleAmpLimitSetting(incrementButtonState, shiftButtonState);
  //if (enterAhSettingMode)handleAhSetting(incrementButtonState, shiftButtonState);
  if (timeSettingMode) updateTimeSetting(incrementButtonState, shiftButtonState);
  if (ampSettingMode) updateAmpSetting(incrementButtonState, shiftButtonState);
  // Timer countdown
if (RUN) {
  if (justResumed) {
    justResumed = false;   // skip first tick
  }
  else if (remainingTime > 0) {
    remainingTime--;
    // ===== SAVE STATE FOR POWER-FAIL RESUME =====
    EEPROM.write(EEPROM_RUN_FLAG_ADDR, 1);
    EEPROM.put(EEPROM_REMAIN_TIME_ADDR, remainingTime);
    EEPROM.put(EEPROM_SETAMP_ADDR, setAmp);
    // ===========================================
    updateMainDisplay();
    delay(1000); 
  }
else {
  RUN = false;
  // ===== CLEAR POWER-FAIL RESUME FLAG =====
  EEPROM.write(EEPROM_RUN_FLAG_ADDR, 0);
  postCompleteHold = true;

  float finalAmp = setAmp / 100.0;
  float holdAmp  = finalAmp * 0.05;
  if (holdAmp < 0.05) holdAmp = 0.05;

  // 🔴 HARD ZERO before going to 5%
  dacSetAmp = 0.0;
  setOutputVoltage(0.0);
  delay(500);

  // 🟢 RAMP TO 5%
  rampToHoldCurrent(holdAmp);

  // 🟡 HOLD 5%
  holdFivePercentForThirtySeconds(finalAmp);

  // ✅ KEEP 5% DURING COMPLETED
  dacSetAmp = holdAmp;
  setOutputVoltage(dacSetAmp);
    lcd.clear();
lcd.setCursor(3, 0);
lcd.print("COMPLETED");
timeConfirmed = false;
ampConfirmed  = false;
digitalWrite(buzzerPin, HIGH);   // 🔊 buzzer ON
completedHold = true;     // stay on COMPLETED
postCompleteMenu = false;
return;
 //showPostCompleteMenu();
return;

    }
  } else if (!enterPassword && !timeSettingMode && !ampSettingMode && !ampLimitSettingMode && !RUN && !passwordChangeMode && !ahSetMode) {
    updateMainDisplay();
    delay(500);
}

  lastEditButtonState = editButtonState;
  lastIncrementButtonState = incrementButtonState;
  lastShiftButtonState = shiftButtonState;
  if (!RUN && !enterPassword && !timeSettingMode && !ampSettingMode && !isCalibrationMode) {
    //pwmFade();
  }
}

// ==== PASSWORD ====
void startPasswordEntry() {
  passwordMode = true; 
  enterPassword = true;
  passIndex = 0;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter Password");
  resetPasswordInput();

  // Enable cursor underline and blinking
  lcd.cursor();
  lcd.noBlink();

  displayDigits(inputPassword, passIndex);
}

bool isPasswordCorrect() {
// ===== AH SET PASSWORD (0020) =====
if (inputPassword[0]==0 && inputPassword[1]==0 &&
    inputPassword[2]==2 && inputPassword[3]==0) {

  enterPassword = false;
  passwordMode = false;
  specialPasswordHandled = true;

  enterAhSetMode();   // 🔥 SET screen, not total
  return true;
}

  // 1️⃣ Check for special "change main password" code 0080
if (inputPassword[0]==0 && inputPassword[1]==0 &&
    inputPassword[2]==8 && inputPassword[3]==0) {

    confirmPasswordChange = true;
    passwordChangeMode = false;
    enterPassword = false;

    confirmYesSelected = true;   // default cursor on YES

    lcd.clear();
    lcd.noBlink();
    lcd.cursor();

    lcd.setCursor(0,0);
    lcd.print("Change Password");

    lcd.setCursor(0,1);
    lcd.print("Y/N");

    lcd.setCursor(0,1);   // cursor under Y

    // wait for EDIT release
    while (digitalRead(EditButton) == LOW);
    lastDebounceTime = millis();

    return false;
}
// ===== AMP LIMIT PASSWORD (0081) =====
if (inputPassword[0]==0 && inputPassword[1]==0 &&
    inputPassword[2]==8 && inputPassword[3]==1) {

    confirmAmpLimitChange = true;
    enterPassword = false;
    passwordMode = false;
    passwordChecked = false;

    confirmYesSelected = true;   // default Y

    lcd.clear();
    lcd.noBlink();
    lcd.cursor();

    lcd.setCursor(0,0);
    lcd.print("Change Amp Limit");

    lcd.setCursor(0,1);
    lcd.print("Y/N");

    lcd.setCursor(0,1);   // cursor under Y

    while (digitalRead(EditButton) == LOW);
    lastDebounceTime = millis();

    return false;   // ⛔ stop normal password flow
}
  // 2️⃣ Check for calibration password (e.g., 0030)
  bool isCalibration = true;
  for (int i = 0; i < 4; i++) {
    if (inputPassword[i] != calibrationPassword[i]) {
      isCalibration = false;
      break;
    }
  }
  if (isCalibration) {
    isCalibrationMode = true;
    lcd.noBlink();
    lcd.noCursor();
    enterCalibrationMode();
    return false;  // Stop normal password check
  }

  // 3️⃣ Check main password
  for (int i = 0; i < 4; i++) {
    if (inputPassword[i] != password[i]) return false;
  }

  return true;
}
void resetPasswordInput() {
  for (int i = 0; i < 4; i++) inputPassword[i] = 0;
  displayDigits(inputPassword, passIndex);
}

void enterTimeSettingMode() {
  timeSettingMode = true;
  ampSettingMode = false;
  passIndex = 0;

  lcd.clear();
  lcd.print("Set Time:MM:SS");
  displaySetTime();

  lcd.cursor();     // ✅ enable cursor ONLY here
  lcd.noBlink();
}


void enterAmpSettingMode() {
  timeSettingMode = false;
  ampSettingMode = true;
  passIndex = 0;
  lcd.clear();
  lcd.print("Set Amps:");

  // enable cursor underline and blinking
  lcd.cursor();
  lcd.noBlink();

  displaySetAmps();
  setAmp = inputCurrent[0] * 100 + inputCurrent[1] * 10 + inputCurrent[2];
 // setOutputVoltage(setAmp / 100.0);
}

// Display functions
void displayDigits(int *digits, int activePosition) {
  lcd.setCursor(0, 1);
  for (int i = 0; i < 4; i++) {
    lcd.print(digits[i]);
  }
  // position cursor under active digit (0..3)
  lcd.setCursor(activePosition, 1);
}

void handlePasswordEntry(bool incBtn, bool shiftBtn) {
  if (incBtn != lastIncrementButtonState && incBtn == LOW && millis() - lastDebounceTime > debounceDelay) {
    lastDebounceTime = millis();
    inputPassword[passIndex] = (inputPassword[passIndex] + 1) % 10;
    displayDigits(inputPassword, passIndex);
  }
  if (shiftBtn != lastShiftButtonState && shiftBtn == LOW && millis() - lastDebounceTime > debounceDelay) {
    lastDebounceTime = millis();
    passIndex = (passIndex + 1) % 4;
    displayDigits(inputPassword, passIndex);
  }
}

void updateTimeSetting(bool incBtn, bool shiftBtn) {

  // Increase selected digit
  if (incBtn != lastIncrementButtonState && incBtn == LOW &&
      millis() - lastDebounceTime > debounceDelay) {

    lastDebounceTime = millis();
    // Increase selected digit
if (passIndex <= 2) {                 // Minutes digits (MMM)
  inputTime[passIndex] = (inputTime[passIndex] + 1) % 10;
}
else if (passIndex == 3) {            // Seconds tens (0–9)
  inputTime[3] = (inputTime[3] + 1) % 10;
}

else if (passIndex == 4) {            // Seconds ones (0–9)
  inputTime[4] = (inputTime[4] + 1) % 10;
}

    displaySetTime();
  }

  // Shift cursor (0–4)
  if (shiftBtn != lastShiftButtonState && shiftBtn == LOW &&
      millis() - lastDebounceTime > debounceDelay) {

    lastDebounceTime = millis();
    passIndex = (passIndex + 1) % 5;
    displaySetTime();
  }

  // Confirm -> move to Amp setting
  if (digitalRead(EditButton) == LOW && millis() - lastDebounceTime > debounceDelay) {
  lastDebounceTime = millis();

  // ⛔ Time NOT set → show warning and STAY in Set Time
  if (setMin == 0 && setSec == 0) {
    lcd.noCursor();
    lcd.noBlink();
    showWarning("Set Time...");
    lcd.clear();
    lcd.cursor();
    lcd.print("Set Time:MM:SS");
    displaySetTime();
    return;
  }

  // ✅ Time is valid → allow Amp setting
  timeConfirmed = true;
  // 🔥 THIS IS THE FIX
  remainingTime = setMin * 60 + setSec;
//showWarning("Set Amp...");
  enterAmpSettingMode();
}
}

void updateAmpSetting(bool incBtn, bool shiftBtn) {

  // INCREMENT
  if (incBtn != lastIncrementButtonState &&
      incBtn == LOW &&
      millis() - lastDebounceTime > debounceDelay) {

    lastDebounceTime = millis();
    inputCurrent[passIndex] = (inputCurrent[passIndex] + 1) % 10;
    displaySetAmps();
  }

  // SHIFT
  if (shiftBtn != lastShiftButtonState &&
      shiftBtn == LOW &&
      millis() - lastDebounceTime > debounceDelay) {

    lastDebounceTime = millis();
    passIndex = (passIndex + 1) % 4;   // 🔥 4 digits
    displaySetAmps();
  }

  // CONFIRM
  if (digitalRead(EditButton) == LOW &&
      millis() - lastDebounceTime > debounceDelay) {

    lastDebounceTime = millis();

    setAmp =
      inputCurrent[0] * 1000 +
      inputCurrent[1] * 100 +
      inputCurrent[2] * 10 +
      inputCurrent[3];
      //dacSetAmp = setAmp / 100.0;   // STORE target current
      //setOutputVoltage(dacSetAmp);
     // currentMeasured = finalAmp;        // keep display synced
    if (setAmp == 0) {
      lcd.noCursor(); 
      lcd.noBlink();
      showWarning("Set Amp...");
      lcd.clear();
      lcd.print("Set Amps:");
      lcd.cursor();
      displaySetAmps();
      return;
    }

    if (setAmp > ampLimit) {   // 5.00A limit
      lcd.noCursor(); 
      lcd.noBlink();
      showWarning("Over Range");
      lcd.clear();
      lcd.print("Set Amps:");
      lcd.cursor();
      displaySetAmps();
      return;
    }

    ampConfirmed = true;
    // ✅ FORCE DAC OFF
dacSetAmp = 0.0;
setOutputVoltage(0.0);
    lcd.noCursor();
    lcd.noBlink();
    enterShuntSettingMode();
  }
}

void displayWelcomeScreen() {
  lcd.setCursor(3, 0);
  lcd.print("WELCOME TO");
  lcd.setCursor(4, 1);
  lcd.print("OXFORD");
  delay(1000);
  lcd.clear();
  showSetAmpScreen = true;
  updateMainDisplay();
}

void displaySetTime() {
  lcd.setCursor(0, 1);
  lcd.print("ST: ");

  // Print minutes (3 digits) at columns 4,5,6
  lcd.setCursor(4, 1);
  lcd.print(inputTime[0]);
  lcd.print(inputTime[1]);
  lcd.print(inputTime[2]);

  lcd.setCursor(7, 1);
  lcd.print(":");

  // Print seconds (2 digits) at columns 8,9
  lcd.setCursor(8, 1);
  lcd.print(inputTime[3]);
  lcd.print(inputTime[4]);

  // Calculate totals
  setMin = inputTime[0] * 100 + inputTime[1] * 10 + inputTime[2];
  setSec = inputTime[3] * 10 + inputTime[4];

  // Cursor mapping for time positions
  int cursorMap[5] = {4, 5, 6, 8, 9};
  lcd.setCursor(cursorMap[passIndex], 1);
}

void displaySetAmps() { 
  lcd.setCursor(0, 1); 
  lcd.print("A: "); 

  float amp =
    (inputCurrent[0] * 1000 +
     inputCurrent[1] * 100 +
     inputCurrent[2] * 10 +
     inputCurrent[3]) / 100.0;

  char ampStr[7];
  dtostrf(amp, 5, 2, ampStr);
  if (ampStr[0] == ' ') ampStr[0] = '0';

  lcd.print(ampStr);
  //lcd.print("A");

  // Cursor columns under digits
  int cursorMap[4] = {3, 4, 6, 7};
  lcd.setCursor(cursorMap[passIndex], 1);
}

void setOutputVoltage(float setAmp) { 
  // 0–5A → full DAC range 
  float amp = constrain(setAmp, 0.0, 5.0); 
  int dacValue = (int)((amp / 5.0) * 4098); 
  dacValue = constrain(dacValue, 0, 4098); 
  dac.setVoltage(dacValue, false);
   }

void enterCalibrationMode() {
  bool refreshCalScreen = true;
  isCalibrationMode = true;
  int calIndex = 0; // 0 = MFSV, 1 = MFSI
  int cursorPos = 0; // Start from leftmost digit (position 0)
  bool calibrating = true;// 🔒 Clear pending EDIT press
  while (digitalRead(EditButton) == LOW);
  lastDebounceTime = millis();

  while (calibrating) {
    if (refreshCalScreen) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(calIndex == 0 ? "MFSV:" : "MFSI:");
    refreshCalScreen = false;
    }
    int value = (calIndex == 0) ? MFSV : MFSI;

    // Extract digits into array
    int digits[4];
    digits[0] = (value / 1000) % 10;
    digits[1] = (value / 100) % 10;
    digits[2] = (value / 10) % 10;
    digits[3] = value % 10;

    // Display digits and position the cursor
    for (int i = 0; i < 4; i++) {
      lcd.setCursor(i, 1);
      lcd.print(digits[i]);
    }
    lcd.setCursor(cursorPos, 1); // Move to active digit

    // Ensure cursor is visible but not blinking in calibration
    lcd.cursor();
    lcd.noBlink();

    // Read buttons
    bool incState = digitalRead(IncrementButton);
    bool shiftState = digitalRead(ShiftButton);
    bool editState = digitalRead(EditButton);

    if (incState == LOW && millis() - lastDebounceTime > debounceDelay) {
      lastDebounceTime = millis();
      digits[cursorPos] = (digits[cursorPos] + 1) % 10;

      // Reconstruct value
      value = digits[0] * 1000 + digits[1] * 100 + digits[2] * 10 + digits[3];
      if (calIndex == 0) MFSV = value;
      else MFSI = value;
    }

    if (shiftState == LOW && millis() - lastDebounceTime > debounceDelay) {
      lastDebounceTime = millis();
      cursorPos = (cursorPos + 1) % 4; // Move left to right
    }

   if (editState == LOW && millis() - lastDebounceTime > debounceDelay) {

  lastDebounceTime = millis();

  if (calIndex == 0) {
    calIndex = 1;        // ➜ switch to MFSI
    cursorPos = 0;
    refreshCalScreen = true;
  } else {
    EEPROM.put(EEPROM_MFSV_ADDR, MFSV);
    EEPROM.put(EEPROM_MFSI_ADDR, MFSI);
    calibrating = false;
  }

  // wait for button release to avoid double trigger
  while (digitalRead(EditButton) == LOW);
}

    delay(150); // reduce flicker and debounce
  }

  lcd.clear();
  displayFinalScreen();
  isCalibrationMode = false;
  calibrationJustFinished = true; 
  lcd.noCursor();
  lcd.noBlink();
}

void enterShuntSettingMode() {
  shuntSettingMode = true;  
  lcd.clear();
  lcd.print("Set Shunt mV:");

  int digits[3] = {
    shuntMilliVolt / 100,
    (shuntMilliVolt / 10) % 10,
    shuntMilliVolt % 10
  };

  int shuntDigitPos = 0;
  bool done = false;
  lastDebounceTime = millis();

  // show cursor and blinking for shunt editing
  lcd.cursor();
  lcd.noBlink();

  while (!done) {
    lcd.setCursor(shuntStartCol, 1);
for (int i = 0; i < 3; i++) {
  lcd.print(digits[i]);
}
// Cursor exactly under active digit
lcd.setCursor(shuntStartCol + shuntDigitPos, 1);

    bool incState = digitalRead(IncrementButton);
    bool shiftState = digitalRead(ShiftButton);
    bool editState = digitalRead(EditButton);

    if (incState == LOW && millis() - lastDebounceTime > debounceDelay) {
      lastDebounceTime = millis();
      digits[shuntDigitPos] = (digits[shuntDigitPos] + 1) % 10;
    }

    if (shiftState == LOW && millis() - lastDebounceTime > debounceDelay) {
      lastDebounceTime = millis();
      shuntDigitPos = (shuntDigitPos + 1) % 3;
    }

    if (editState == LOW && millis() - lastDebounceTime > debounceDelay) {
      lastDebounceTime = millis();
      shuntMilliVolt = digits[0] * 100 + digits[1] * 10 + digits[2];
      EEPROM.write(EEPROM_SHUNT_ADDR, shuntMilliVolt);
      done = true;
      lcd.noBlink();
    }
    delay(150);
  }

  // Start run after shunt entry
 // ----- WAIT FOR FINAL CONFIRMATION -----
 shuntSettingMode = false;     // ✅ ADD THIS

waitingForStart = true;

lcd.clear();
lcd.setCursor(2, 0);
lcd.print("All Setted");
lcd.setCursor(0, 1);
lcd.print("Press EDIT to go");

lcd.noBlink();
lcd.noCursor();


  // turn off cursor and blink when running
  lcd.noCursor();
  lcd.noBlink();
}

void startCurrentRamp(float finalAmp) {

  inRampMode = true;   // 🔥 START ramp mode

  float step = 0.05;
  int rampDelay = 250;
  float current = 0.10;

  while (current <= finalAmp) {
    dacSetAmp = current;
    setOutputVoltage(dacSetAmp);
  //  displayCurrent = dacSetAmp;   // 👈 force ramp value
    updateMainDisplay();
    delay(rampDelay);
    current += step;
  }
 
  dacSetAmp = finalAmp;
  setOutputVoltage(dacSetAmp);

  // 🔥 SYNC ADC IMMEDIATELY
  int rawCurrent = analogRead(currentSensePin);
  voltageMeasured = rawCurrent * (5.0 / 1023.0);
  currentMeasured = voltageMeasured * (MFSI / 5.0);

  inRampMode = false;  // 🔥 END ramp mode
 // displayCurrent = currentMeasured;   // 🔥 THIS LINE
}


void holdFivePercentCurrent(float finalAmp) {

  float holdAmp = finalAmp * 0.05;   // 5% current
  if (holdAmp < 0.05) holdAmp = 0.05; // minimum readable

  float step = 0.02;
  int rampDelay = 200;

  float current = 0.01;

  // Ensure cursor/blink off while holding
  lcd.noBlink();
  lcd.noCursor();

  while (current <= holdAmp) {

    setOutputVoltage(current);
    currentMeasured = current;
    updateMainDisplay();
    delay(rampDelay);

    current += step;
  }

  // lock the 5% value
  setOutputVoltage(holdAmp);
  currentMeasured = holdAmp;
  updateMainDisplay();
}
void holdFivePercentForThirtySeconds(float finalAmp) {

  float holdAmp = finalAmp * 0.05;
  if (holdAmp < 0.05) holdAmp = 0.05;

  setOutputVoltage(holdAmp);
  currentMeasured = holdAmp;

  unsigned long startTime = millis();

  while (millis() - startTime < 5000UL) {   // 1 minute = 60000 ms
   updateMainDisplay();
    delay(500);             // refresh every 0.5s
  }
}

void displayFinalScreen() {
//  lcd.clear(); 
  lcd.setCursor(0, 0);

  // --- Voltage (fixed width 6 columns) ---
  char voltStr[7];
  dtostrf(volt, 5, 2, voltStr);
  // Replace leading space with zero
  if (voltStr[0] == ' ') {
  voltStr[0] = '0';
}
lcd.print(voltStr);
lcd.print("V");

  // --- Current (fixed width 6 columns) ---
 // --- Current (fixed width 7 columns → 00.00A) ---
// --- Current (always 00.00A) ---
// --- Current (exactly 00.00A) ---
lcd.setCursor(10, 0);
char currStr[7];
dtostrf(getDisplayCurrent(), 5, 2, currStr);
if (currStr[0] == ' ') currStr[0] = '0';
lcd.print(currStr);
lcd.print("A");

  // --- Second line ---
  lcd.setCursor(0, 1);

  lcd.print("ST:");
  if (setMin < 100) lcd.print("0");
  if (setMin < 10) lcd.print("0");
  lcd.print(setMin);

  lcd.print(" RT:");

  int minutes = remainingTime / 60;
  int seconds = remainingTime % 60;

  if (minutes < 100) lcd.print("0");
  if (minutes < 10) lcd.print("0");
  lcd.print(minutes);

  lcd.print(":");
  if (seconds < 10) lcd.print("0");
  lcd.print(seconds);
}

void updatePWMFromSetAmp(float setAmpValue) {

  // 0–5A  →  0–255 PWM
  int pwmValue = (int)((setAmpValue / 5.0) * 255);

  pwmValue = constrain(pwmValue, 0, 255);
  analogWrite(pwmPin, pwmValue);
}

void dacSweepThreeTimes() {

  for (int cycle = 0; cycle < 3; cycle++) {

    // Ramp up
    for (int i = 0; i <= 4095; i += 64) {
      dac.setVoltage(i, false);
      delay(2);
    }

    // Ramp down
    for (int i = 4095; i >= 0; i -= 64) {
      dac.setVoltage(i, false);
      delay(2);
    }
  }

  dac.setVoltage(0, false); // final OFF
}
/*void showPostCompleteMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Edit Time &Amp?");

  lcd.setCursor(0, 1);
  lcd.print("Y/N");

  lcd.cursor();
  lcd.noBlink();

  lcd.setCursor(editChoiceYes ? 0 : 6, 1);
}*/
void showWarning(const char *msg) {
  lcd.clear();
  int len = strlen(msg);
  int col = (16 - len) / 2;
  lcd.setCursor(col, 0);
  lcd.print(msg);
  delay(1000);
  lcd.clear();
}

void handlePasswordChange(bool incBtn, bool shiftBtn) {
  // Increment digit
  if (incBtn != lastIncrementButtonState && incBtn == LOW && millis() - lastDebounceTime > debounceDelay) {
    lastDebounceTime = millis();
    newPassword[newPassIndex] = (newPassword[newPassIndex] + 1) % 10;
    displayDigits(newPassword, newPassIndex);
  }

  // Shift cursor
  if (shiftBtn != lastShiftButtonState && shiftBtn == LOW && millis() - lastDebounceTime > debounceDelay) {
    lastDebounceTime = millis();
    newPassIndex = (newPassIndex + 1) % 4;
    displayDigits(newPassword, newPassIndex);
  }

  // Confirm with Edit button
  if (digitalRead(EditButton) == LOW && millis() - lastDebounceTime > debounceDelay) {
    lastDebounceTime = millis();

    // Save new password
    for (int i = 0; i < 4; i++) password[i] = newPassword[i];

    // ✅ Save new password to EEPROM
    for (int i = 0; i < 4; i++) {
        EEPROM.write(EEPROM_PASSWORD_ADDR + i, newPassword[i]);
    }


    // Clear mode
    passwordChangeMode = false;
    enterPassword = false;
    passwordChecked = false;
    passIndex = 0;

    lcd.clear();
    lcd.noCursor();     // ✅ turn cursor OFF
    lcd.noBlink();      // ✅ safety
    lcd.setCursor(4, 0);
    lcd.print("Password");
    lcd.setCursor(4, 1);
    lcd.print("Changed");
    delay(1000);
    lcd.clear();
    displayFinalScreen();
  }
}
void displayAmpLimit() {
  lcd.setCursor(0, 1);
  lcd.print("L:");

  float limit =
    (ampLimitDigits[0] * 1000 +
     ampLimitDigits[1] * 100 +
     ampLimitDigits[2] * 10 +
     ampLimitDigits[3]) / 100.0;

  char buf[7];
  dtostrf(limit, 5, 2, buf);
  if (buf[0] == ' ') buf[0] = '0';

  lcd.print(buf);
  lcd.print("A");

  int cursorMap[4] = {2, 3, 5, 6};
  lcd.setCursor(cursorMap[passIndex], 1);
}
void handleAmpLimitSetting(bool incBtn, bool shiftBtn) {

  // Increment
  if (incBtn != lastIncrementButtonState &&
      incBtn == LOW &&
      millis() - lastDebounceTime > debounceDelay) {

    lastDebounceTime = millis();
    ampLimitDigits[passIndex] = (ampLimitDigits[passIndex] + 1) % 10;
    displayAmpLimit();
  }

  // Shift
  if (shiftBtn != lastShiftButtonState &&
      shiftBtn == LOW &&
      millis() - lastDebounceTime > debounceDelay) {

    lastDebounceTime = millis();
    passIndex = (passIndex + 1) % 4;
    displayAmpLimit();
  }
 // Confirm
if (digitalRead(EditButton) == LOW &&
    millis() - lastDebounceTime > debounceDelay) {

    lastDebounceTime = millis();

    ampLimit =
      ampLimitDigits[0] * 1000 +
      ampLimitDigits[1] * 100 +
      ampLimitDigits[2] * 10 +
      ampLimitDigits[3];

    if (ampLimit < 50) ampLimit = 50;
    if (ampLimit > 1000) ampLimit = 1000;

    EEPROM.put(EEPROM_AMP_LIMIT_ADDR, ampLimit);

    // ===== EXIT AMP LIMIT MODE =====
    ampLimitSettingMode = false;
    enterPassword = false;
    passwordMode = false;
    passwordChecked = false;
    passIndex = 0;

    lcd.clear();
    lcd.noCursor();
    lcd.noBlink();

    lcd.setCursor(2,0);
    lcd.print("Limit Saved");
    delay(1000);
    while (digitalRead(EditButton) == LOW);
    lastDebounceTime = millis();
    lcd.clear();
    displayFinalScreen();
}
} 
void rampToHoldCurrent(float targetAmp) {

  inRampMode = true;   // 🔥 enable ramp display

  float step = 0.05;
  int delayMs = 200;
  float current = 0.10;

  lcd.noCursor();
  lcd.noBlink();

  while (current < targetAmp) {
    current += step;
    if (current > targetAmp) current = targetAmp;

    dacSetAmp = current;
    setOutputVoltage(dacSetAmp);
    lcd.clear();
    updateMainDisplay();
    delay(delayMs);
  }

  inRampMode = false;  // 🔥 back to real ADC
}

void updateMainDisplay() {
  if (waitingForResume) return;   // 🔒 STOP any LCD redraw
  static byte lastScreen = 255;

  // 🔒 lock SA during ramp
  if (inRampMode) {
    displayScreen = 0;
  }

  if (displayScreen != lastScreen) {
    lcd.clear();
    lastScreen = displayScreen;
  }

  switch (displayScreen) {

    case 0:   // 1️⃣ SA screen
      displaySetAmpPreviewScreen();
      break;

    case 1:   // 2️⃣ Voltage screen
      displayFinalScreen();
      break;

    case 2:   // 3️⃣ AH Set + AH Actual
       displayAhSetScreen();
      break;

    case 3:   // 4️⃣ AH Totalizer
      displayAhTotalScreen();
      break;
  }
}

void displaySetAmpPreviewScreen() {
 // lcd.clear();
  // ---------- TOP LINE ----------
  lcd.setCursor(0, 0);
  lcd.print("SA:");

  // Set Amp (from inputCurrent)
  float setAmpValue =
    (inputCurrent[0] * 1000 +
     inputCurrent[1] * 100 +
     inputCurrent[2] * 10 +
     inputCurrent[3]) / 100.0;

  char saStr[7];
  dtostrf(setAmpValue, 5, 2, saStr);
  if (saStr[0] == ' ') saStr[0] = '0';
  lcd.print(saStr);
  lcd.print("A");

  // Actual current (DAC)
  lcd.setCursor(10, 0);
  char currStr[7];
  dtostrf(getDisplayCurrent(), 5, 2, currStr);
  if (currStr[0] == ' ') currStr[0] = '0';
  lcd.print(currStr);
  lcd.print("A");

  // ---------- BOTTOM LINE ----------
  lcd.setCursor(0, 1);
  lcd.print("ST:");

  if (setMin < 100) lcd.print("0");
  if (setMin < 10)  lcd.print("0");
  lcd.print(setMin);

  lcd.print(" RT:");

  int minutes = remainingTime / 60;
  int seconds = remainingTime % 60;

  if (minutes < 100) lcd.print("0");
  if (minutes < 10)  lcd.print("0");
  lcd.print(minutes);
  lcd.print(":");
  if (seconds < 10) lcd.print("0");
  lcd.print(seconds);
} 
void showResumeScreen() {
  if (!resumeScreenShown) {
    lcd.clear();
    lcd.noCursor();
    lcd.noBlink();
    lcd.setCursor(3,0);
    lcd.print("EDIT:RESUME");

    resumeScreenShown = true;
  }

  waitingForResume = true;
}
float getDisplayCurrent() {
  if (inRampMode) {
    return dacSetAmp;        // show ramp (SET)
  } else {
    return currentMeasured; // show real (ADC)
  }
}
void enterAhSettingMode() {
    ahSettingMode = true;
    ahDigitIndex = 0;   // start from leftmost digit

    // Load AH Set from EEPROM
    EEPROM.get(EEPROM_AH_SET_ADDR, ahSet);

    // Split AH into digits for editing (0000.0 → 5 digits)
    int ahInt = (int)(ahSet * 10); // e.g., 1.0 → 10
    ahDigits[0] = (ahInt / 10000) % 10;
    ahDigits[1] = (ahInt / 1000) % 10;
    ahDigits[2] = (ahInt / 100) % 10;
    ahDigits[3] = (ahInt / 10) % 10;
    ahDigits[4] = ahInt % 10;

    lcd.clear();
    lcd.setCursor(4,0);
    lcd.print("Set Ah");
    displayAhSet();
}

/*void displayAhScreen() {
    // Line 0: Title
    lcd.setCursor(4,0);
    lcd.print("AH Totalizer");

    // Line 1: show AH Set / AH Actual
    // Format: 00000.0
    char ahStr[8];
    float ahDisplay = ahSet;   // show set value while editing
    dtostrf(ahDisplay,6,1,ahStr); // 6 chars, 1 decimal
    lcd.setCursor(5,1);  // center approximately
    lcd.print(ahStr);
}*/

/*void handleAhSetting(bool incBtn, bool shiftBtn) {
    // INCREMENT digit
    if (incBtn != lastIncrementButtonState && incBtn == LOW && millis() - lastDebounceTime > debounceDelay) {
        lastDebounceTime = millis();
        ahDigits[ahDigitIndex] = (ahDigits[ahDigitIndex] + 1) % 10;
        updateAhFromDigits();
        displayAhScreen();
    }

    // SHIFT → move digit
    if (shiftBtn != lastShiftButtonState && shiftBtn == LOW && millis() - lastDebounceTime > debounceDelay) {
        lastDebounceTime = millis();
        ahDigitIndex = (ahDigitIndex + 1) % 5; // 5 digits 0000.0
        displayAhScreen();
    }

    // EDIT → confirm
    if (digitalRead(EditButton) == LOW && millis() - lastDebounceTime > debounceDelay) {
        lastDebounceTime = millis();
        ahSettingMode = false;

        // Save AH Set to EEPROM
        EEPROM.put(EEPROM_AH_SET_ADDR, ahSet);

        lcd.clear();
        lcd.setCursor(4,0);
        lcd.print("AH Saved!");
        delay(1000);
         lcd.clear();
        updateMainDisplay();  // return to main screen
    }
}*/

void displayAhSetScreen() {

  lcd.setCursor(0, 0);
  lcd.print("Ah Set");

  lcd.setCursor(9, 0);
  lcd.print("Ah Act");

  // ---- Values (fixed width) ----
  lcd.setCursor(0, 1);
  printAh4_1(ahSet);     // 0000.0

  lcd.setCursor(9, 1);
  printAh4_1(ahActual);  // 0000.0
}
void displayAhTotalScreen() {

  lcd.setCursor(1, 0);
  lcd.print("AH Totalizer");

  lcd.setCursor(3, 1);
  printAh5_1(ahTotal);   // 00000.0
}
void printAh4_1(float value) {
  int intPart = (int)value;                 // 0–9999
  int decPart = (int)((value - intPart) * 10 + 0.5);

  if (intPart < 10) lcd.print("000");
  else if (intPart < 100) lcd.print("00");
  else if (intPart < 1000) lcd.print("0");

  lcd.print(intPart);
  lcd.print(".");
  lcd.print(decPart);
}

void updateAhFromDigits() {
    int ahInt = ahDigits[0]*10000 + ahDigits[1]*1000 + ahDigits[2]*100 + ahDigits[3]*10 + ahDigits[4];
    ahSet = ahInt / 10.0;  // convert to float 0000.0
}
void printAh5_1(float value) {
  int intPart = (int)value;                  // 0–99999
  int decPart = (int)((value - intPart) * 10 + 0.5);

  if (intPart < 10) lcd.print("0000");
  else if (intPart < 100) lcd.print("000");
  else if (intPart < 1000) lcd.print("00");
  else if (intPart < 10000) lcd.print("0");

  lcd.print(intPart);
  lcd.print(".");
  lcd.print(decPart);
}
//float ahSet = 0.0;   // user target

void enterAhSetMode() {
  ahSettingMode = true;
  ahDigitIndex = 0;

  // reset button edge states
  lastIncrementButtonState = HIGH;
  lastShiftButtonState = HIGH;
   lastEditButtonState      = HIGH;   // if you have it
  lastDebounceTime = millis();

  loadAhDigitsFromValue();
  displayAhSet();
}


/*void displayAhSet() {
  lcd.setCursor(0, 0);
  lcd.print("   Set Ah   ");

  lcd.setCursor(3, 1);    // center
  char buf[8];
  dtostrf(ahSet, 6, 1, buf); // "0000.0"
  lcd.print(buf);
}
void adjustAhSet(int dir) {
  ahSet += dir * 0.1;   // 0.1 hour steps
  if (ahSet < 0) ahSet = 0;
  if (ahSet > 9999.9) ahSet = 9999.9;
}*/
void displayAhSet() {

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set Ah:");

  lcd.setCursor(0, 1);

  for (int i = 0; i < 5; i++) {
    if (i == 4) lcd.print(".");
    lcd.print(ahDigits[i]);
  }

  int cursorMap[5] = {0, 1, 2, 3, 5};
  lcd.setCursor(cursorMap[ahDigitIndex], 1);

  lcd.cursor();
}


void handleAhSetting(bool incBtn, bool shiftBtn) {

  // INCREMENT active digit
  if (incBtn != lastIncrementButtonState &&
      incBtn == LOW &&
      millis() - lastDebounceTime > debounceDelay) {

    lastDebounceTime = millis();
    ahDigits[ahDigitIndex] = (ahDigits[ahDigitIndex] + 1) % 10;  // 🔥 FIX
    updateAhFromDigits();
    displayAhSet();
  }

  // SHIFT → move cursor
  if (shiftBtn != lastShiftButtonState &&
      shiftBtn == LOW &&
      millis() - lastDebounceTime > debounceDelay) {

    lastDebounceTime = millis();
    ahDigitIndex = (ahDigitIndex + 1) % 5;
    displayAhSet();
  }

  // EDIT → confirm
  if (digitalRead(EditButton) == LOW &&
      millis() - lastDebounceTime > debounceDelay) {

    lastDebounceTime = millis();
    ahSettingMode = false;

    EEPROM.put(EEPROM_AH_SET_ADDR, ahSet);
    lcd.noCursor();     // 🔥 TURN OFF CURSOR
    lcd.clear();
    lcd.setCursor(3,0);
    lcd.print("AH Saved!");
    delay(1000);
    lcd.clear();
    updateMainDisplay();
  }
}
void loadAhDigitsFromValue() {

  int temp = ahSet * 10;

  ahDigits[4] = temp % 10; temp /= 10;
  ahDigits[3] = temp % 10; temp /= 10;
  ahDigits[2] = temp % 10; temp /= 10;
  ahDigits[1] = temp % 10; temp /= 10;
  ahDigits[0] = temp % 10;
}