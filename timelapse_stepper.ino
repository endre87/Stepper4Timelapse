/*
 * Timer0 is used by functions like delay()
 * We need a different timer to count seconds for stepper motor. This will be Timer1. Library from https://github.com/PaulStoffregen/TimerOne
 *
 * The circuit:
 * LCD VSS pin to GND
 * LCD VDD pin to +5V
 * LCD V0 pin to 10K potentiometer (connected to +5V and ground) / ~2K Resistor connected to ground
 * LCD RS pin to digital pin 7
 * LCD RW pin to GND
 * LCD Enable pin to digital pin 6
 * LCD D4 pin to digital pin 5
 * LCD D5 pin to digital pin 4
 * LCD D6 pin to digital pin 3
 * LCD D7 pin to digital pin 2
 * LCD A pin to digital pin 13 - used to power the LCD backlight / can be disabled
 * LCD K pin to GND
 *    
 * Infrared signal to digital pin 12
 * Stepper motor digital pins: 8, 9, 10, 11
 * 
 * 
 */
 
#include <LiquidCrystal.h>
#include <IRremote.h>
#include <TimerOne.h>
#include <Stepper.h>

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(7, 6, 5, 4, 3, 2);
IRrecv irrecv(12);
decode_results results;
Stepper stepperMotor(200, 8, 9, 10, 11); // stepsPerRevolution = 200 (360 angle)

// Constants
#define ENABLE_SERIAL 0 // when developing write program debug infos over SERIAL port
#define POWER_SAVING 1  // disable LCD light after 10 sec, stop stepper motor after step

#define LCD_LIGHT_PIN 13
#define LCD_LIGHT_POWER_OFF_SEC 10
#define MAIN_LOOP_DELAY_MS 50

#define MODE_MANUAL 0
#define MODE_AUTO 1
#define MODE_MANUAL_LABEL "MAN"
#define MODE_AUTO_LABEL "RUN"
#define DIRECTION_CLOCKWISE 0
#define DIRECTION_COUNTERCLOCKWISE 1
#define DIRECTION_CLOCKWISE_LABEL "-->"
#define DIRECTION_COUNTERCLOCKWISE_LABEL "<--"
#define DELAY_MAX 999
#define STEP_MAX 199 // on a 1.8 angle stepper motor 200 steps is 360 angle, 200 = 0

#define EDITOR_DIR_INDEX 1
#define EDITOR_DELAY_INDEX 2
#define EDITOR_STEP_INDEX 3

#define STEP_EDITOR_CURSOR_POSITION 15
#define DIR_EDITOR_CURSOR_POSITION 6
#define DELAY_EDITOR_CURSOR_POSITION 10

#define BTN_ZERO 1000
#define BTN_MINUS 10
#define BTN_PLUS 11
#define BTN_EQ 12
#define BTN_PLAY 13

const int lcdLightPowerOffTime = (LCD_LIGHT_POWER_OFF_SEC * 1000) / MAIN_LOOP_DELAY_MS;
int lcdLightPowerOffCounter = lcdLightPowerOffTime;

int mod = MODE_MANUAL;
int dly = 36; // 36 default
int dir = DIRECTION_CLOCKWISE;

// run time display variables
volatile unsigned int rundly = dly;
volatile unsigned int stp = 0;

int editorIndex = EDITOR_STEP_INDEX;

void setup() {
  if (ENABLE_SERIAL) {
    Serial.begin(9600);
  }
  irrecv.enableIRIn(); // Start the receiver
  
  // initialize LCD and set up the number of columns and rows:
  lcd.begin(16, 2);

  // set the stepper motor speed at 60 rpm:
//  stepperMotor.setSpeed(60);
  
  writeLCDHeaders();
  writeMode();
  writeDelay(dly);
  writeDirection();
  writeStep(stp);

  Timer1.initialize(1000000);       // 1000000 = 1 sec
  Timer1.attachInterrupt(runTimer); // runTimer to run every 1 second

  pinMode(LCD_LIGHT_PIN, OUTPUT);
  turnLCDLight(true);
}

// ---------------------------------------------------------------
// MAIN LOOP
// ---------------------------------------------------------------
void loop() {
  int button = readRemote();
  handleLCDPowerSaving();

  if (button == BTN_PLAY) {
    switchMode();
  } else if (button == BTN_PLUS) {
    changeEditorValue(1);
  } else if (button == BTN_MINUS) {
    changeEditorValue(-1);
  } else if (button == BTN_EQ) {
    nextEditor();
  } else if (button == BTN_ZERO) {
    writeStep(stp = 0);
  }

  showEditor();
  delay(MAIN_LOOP_DELAY_MS);
}

// ---------------------------------------------------------------
// TIMER - RUN MODE
// ---------------------------------------------------------------
void runTimer() {
  if (mod == MODE_AUTO) {
    rundly--;
    if (rundly == 0) {
      rundly = dly;
      doStep(1);
    }
    writeDelay(rundly);
    writeDebugInfo("active\t");
  }
  writeDebugInfo("auto cycle\n");
}

// ---------------------------------------------------------------
// STATE - CONTROL
// ---------------------------------------------------------------
void handleLCDPowerSaving() {
  if (POWER_SAVING) {
    lcdLightPowerOffCounter--;
    if (lcdLightPowerOffCounter == 0) {
      turnLCDLight(false);
      lcdLightPowerOffCounter = lcdLightPowerOffTime;
    }
  }
}

void nextEditor() {
  if (mod == MODE_MANUAL) {
    if (editorIndex == 0 || editorIndex == EDITOR_STEP_INDEX) {
      editorIndex = 1;
    } else {
      editorIndex++;
    }
  }
}

void changeEditorValue(int ammount) {
  if (editorIndex == EDITOR_DELAY_INDEX) {
    changeValue(&dly, ammount, 1, DELAY_MAX);
    writeDelay(dly);
  } else if (editorIndex == EDITOR_DIR_INDEX) {
    changeValue(&dir, ammount, 0, DIRECTION_COUNTERCLOCKWISE);
    writeDirection();
  } else if (editorIndex == EDITOR_STEP_INDEX) {
    doStep(ammount);
  }  
}

void doStep(int ammount) {
  int stepValue = 0;
  if (dir == DIRECTION_CLOCKWISE) {
    stepValue = ammount;
  } else {
    stepValue = -ammount;
  }
  changeValue(&stp, ammount, 0, STEP_MAX);
  stepperMotor.step(stepValue);
  writeStep(stp);
}

void changeValue(int *value, int ammount, int minimum, int maximum) {
  if (ammount > 0) { // increase
    if ((*value) == maximum || ((*value) + ammount) > maximum) {
      (*value) = minimum;
    } else {
      (*value) += ammount;
    }
  } else { // decrease
    if ((*value) == minimum || ((*value) + ammount) < minimum) {
      (*value) = maximum;
    } else {
      (*value) += ammount;
    }
  }  
}

void switchMode() {
  mod ^= 1;
  if (mod == MODE_MANUAL) {
    editorIndex = EDITOR_STEP_INDEX;
    writeDelay(dly);
  } else {
    editorIndex = 0;
    rundly = dly;
//    stp = 0;
//    writeStep();
  }
  writeMode();
}

// ---------------------------------------------------------------
// LCD
// ---------------------------------------------------------------
void turnLCDLight(boolean state) {
  digitalWrite(LCD_LIGHT_PIN, state ? HIGH : LOW);
}

void writeLCDHeaders() {
  lcd.setCursor(0, 0);
  lcd.print("Mod");
  lcd.setCursor(4, 0);
  lcd.print("Dir");
  lcd.setCursor(8, 0);
  lcd.print("Dly");
  lcd.setCursor(12, 0);
  lcd.print("Step");
}

void writeMode() {
  lcd.setCursor(0, 1);
  switch (mod) {
    case MODE_MANUAL:
      lcd.print(MODE_MANUAL_LABEL);
      break;
    case MODE_AUTO:
      lcd.print(MODE_AUTO_LABEL);
      break;
  }
}

void writeDelay(int value) {
  writeThreeDigitNumber(7, 1, value);
}

void writeDirection() {
  lcd.setCursor(4, 1);
  switch (dir) {
    case DIRECTION_CLOCKWISE:
      lcd.print(DIRECTION_CLOCKWISE_LABEL);
      break;
    case DIRECTION_COUNTERCLOCKWISE:
      lcd.print(DIRECTION_COUNTERCLOCKWISE_LABEL);
      break;
  }
}

void writeStep(int value) {
  writeThreeDigitNumber(12, 1, value);
}

void writeThreeDigitNumber(int cursorColumn, int cursorRow, int value) { // align right
  lcd.setCursor(cursorColumn, cursorRow);
  lcd.write(' ');
  if (value / 100 == 0) {
    lcd.write(' ');  
  }
  if (value / 10 == 0) {
    lcd.write(' ');
  }
  lcd.print(value);
}

void showEditor() {
  switch(editorIndex) {
    case EDITOR_STEP_INDEX:
      lcd.cursor();
      lcd.setCursor(STEP_EDITOR_CURSOR_POSITION, 1);
      break;
    case EDITOR_DIR_INDEX:
      lcd.cursor();
      lcd.setCursor(DIR_EDITOR_CURSOR_POSITION, 1);
      break;
    case EDITOR_DELAY_INDEX:
      lcd.cursor();
      lcd.setCursor(DELAY_EDITOR_CURSOR_POSITION, 1);
      break;
    default: // OFF
      lcd.noCursor();
      break;
  }
}

// ---------------------------------------------------------------
// REMOTE CONTROL
// ---------------------------------------------------------------
int readRemote() {
  static int lastPressed = 0;
  int result = 0;
  if (irrecv.decode(&results)) {
    turnLCDLight(true);
    lcdLightPowerOffCounter = lcdLightPowerOffTime;
    switch (results.value) {
      case 16769055: // -
        result = lastPressed = BTN_MINUS;
        break;
      case 16754775: // +
        result = lastPressed = BTN_PLUS;
        break;
      case 16748655: // eq
        result = BTN_EQ;
        lastPressed = 0;
        break;
      case 16761405: // play/pause
        result = BTN_PLAY;
        lastPressed = 0;
        break;
      case 16738455: // 0
        result = BTN_ZERO;
        lastPressed = 0;
        break;
      case 4294967295: // continuosly pressed
        result = lastPressed;
        break;
      default:
        result = lastPressed = 0;
    }
    writeDebugInfo("pressed key code: ");
    writeDebugInfo(results.value);
    writeDebugInfo("\n");
    irrecv.resume(); // Receive the next value
  }
  return result;
}

// ---------------------------------------------------------------
// DEBUG
// ---------------------------------------------------------------
void writeDebugInfo(char *value) {
  if (ENABLE_SERIAL) {
    Serial.print(value);
  }
}

void writeDebugInfo(unsigned long value) {
  if (ENABLE_SERIAL) {
    Serial.print(value);
  }
}

