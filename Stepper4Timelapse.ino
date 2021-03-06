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
 * LCD A pin to digital pin 12 - used to power the LCD backlight / can be disabled
 * LCD K pin to GND
 *    
 * Infrared signal to digital pin 13
 * Stepper motor digital pins: 8, 9, 10, 11 Custom library for the 5V arduino stepper motor http://playground.arduino.cc/Main/CustomStepper
 * 
 */
 
#include <LiquidCrystal.h>
#include <IRremote.h>
#include <TimerOne.h>
#include <CustomStepper.h>

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(7, 6, 5, 4, 3, 2);
IRrecv irrecv(13);
decode_results results;
//Stepper stepperMotor(200, 8, 9, 10, 11); // stepsPerRevolution = 200 (360 angle)
CustomStepper stepperMotor(8, 9, 10, 11, (byte[]){8, B1000, B1100, B0100, B0110, B0010, B0011, B0001, B1001}, 4075.7728395, 14, CW); // taken from example code

// Constants
#define ENABLE_SERIAL 0 // when developing write program debug infos over SERIAL port
#define POWER_SAVING 0  // disable LCD light after 10 sec, stop stepper motor after step

#define GEAR_RATIO 3.1

#define LCD_LIGHT_PIN 12

#define MODE_MANUAL 0
#define MODE_AUTO 1
#define MODE_MANUAL_LABEL "SET"
#define MODE_AUTO_LABEL "RUN"
#define DIRECTION_CLOCKWISE 0
#define DIRECTION_COUNTERCLOCKWISE 1
#define DIRECTION_CLOCKWISE_LABEL "->"
#define DIRECTION_COUNTERCLOCKWISE_LABEL "<-"
#define DELAY_MAX 999
#define ANGLE_MAX 3600 // this is displayed as 360.0 on LCD

#define EDITOR_DIR_INDEX 1
#define EDITOR_DELAY_INDEX 2
#define EDITOR_ANGLE_INDEX 3

#define ANGLE_EDITOR_CURSOR_POSITION 15
#define DIR_EDITOR_CURSOR_POSITION 5
#define DELAY_EDITOR_CURSOR_POSITION 9

#define BTN_ZERO 1000
#define BTN_MINUS 10
#define BTN_PLUS 11
#define BTN_EQ 12
#define BTN_PLAY 13

const int angleIncPerStep[9] = {4, 9, 18, 36, 72, 144, 288, 576, 900}; // value / 10 - angle to increase in each step
const int lcdLightPowerOffTime = 1000 * 5000;
int lcdLightPowerOffCounter = lcdLightPowerOffTime;
boolean lcdLightFlag = true;

int mod = MODE_MANUAL;
int dly = 9; // default value
int angIndex = 1; // angleIncPerStep[2] = 9  default
int dir = DIRECTION_CLOCKWISE;

// run time display variables
volatile unsigned int rundly = dly;
volatile unsigned int runAngle = 0;

int editorIndex = EDITOR_DELAY_INDEX;

void setup() {
  if (ENABLE_SERIAL) {
    Serial.begin(9600);
  }
  irrecv.enableIRIn(); // Start the receiver
  
  // initialize LCD and set up the number of columns and rows:
  lcd.begin(16, 2);
  
  writeLCDHeaders();
  writeMode();
  writeDelay(dly);
  writeDirection();
  writeAngle(angleIncPerStep[angIndex]);

  Timer1.initialize(1000000);       // 1000000 = 1 sec
  Timer1.attachInterrupt(runTimer); // runTimer to run every 1 second

  pinMode(LCD_LIGHT_PIN, OUTPUT);
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
    if (mod == MODE_AUTO) {
      lcdLightFlag ^= true;
    } else {
      nextEditor();
    }
  } else if (button == BTN_ZERO) {
    if (mod == MODE_AUTO) {
      writeAngle(runAngle = 0);
    }
  }

  if (mod == MODE_MANUAL) {
    showEditor();
  }
  stepperMotor.run();
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
    writeAngle(runAngle / GEAR_RATIO);
    writeDebugInfo("active\t");
  }
  writeDebugInfo("auto cycle\n");
}

// ---------------------------------------------------------------
// STATE - CONTROL
// ---------------------------------------------------------------

void nextEditor() {
  if (mod == MODE_MANUAL) {
    if (editorIndex == 0 || editorIndex == EDITOR_ANGLE_INDEX) {
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
    changeValue(&dir, ammount, 0, 1);
    writeDirection();
  } else if (editorIndex == EDITOR_ANGLE_INDEX) {
    int maxIndex = sizeof(angleIncPerStep) / sizeof(*angleIncPerStep) - 1;
    int normAmmount = ammount > 0 ? 1 : -1;
    if (angIndex == maxIndex && normAmmount > 0) {
      angIndex = 0;
    } else if (angIndex == 0 && normAmmount < 0) {
      angIndex = maxIndex;
    } else {
      angIndex += normAmmount;
    }
    writeAngle(angleIncPerStep[angIndex]);
  }  
}

void doStep(int ammount) {
//  With default - arduino library stepper motor
//  int stepValue = 0;
//  if (dir == DIRECTION_CLOCKWISE) {
//    stepValue = ammount;
//  } else {
//    stepValue = -ammount;
//  }
//  stepperMotor.step(stepValue);

// With custom library
  if ((dir == DIRECTION_CLOCKWISE && ammount > 0) || (dir == DIRECTION_COUNTERCLOCKWISE && ammount < 0)) {
    // Clockwise case; CCW == normally clockwise ?!
    stepperMotor.setDirection(CCW);
  } else {
    // Counterclockwise case; CW == normally counterclockwise ?!
    stepperMotor.setDirection(CW);
  }
  float degree = ((float) angleIncPerStep[angIndex]) / 10.0;
  stepperMotor.rotateDegrees(degree * abs(ammount));
  
  changeValue(&runAngle, angleIncPerStep[angIndex], 0, ANGLE_MAX);
}

void changeValue(int *value, int ammount, int minimum, int maximum) {
  if (ammount > 0) { // increase
    if ((*value) == maximum || ((*value) + ammount) > maximum) {
      (*value) = (*value) + ammount - maximum;
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
    editorIndex = EDITOR_ANGLE_INDEX;
    writeDelay(dly);
    writeAngle(angleIncPerStep[angIndex]);
  } else {
    if (POWER_SAVING) {
      lcdLightFlag = false;
    }
    lcd.noCursor();
    editorIndex = 0;
    rundly = dly;
    writeAngle(runAngle = 0);
  }
  writeMode();
}

// ---------------------------------------------------------------
// LCD
// ---------------------------------------------------------------
void handleLCDPowerSaving() {
  if (POWER_SAVING && mod == MODE_MANUAL) {
    lcdLightPowerOffCounter--;
    if (lcdLightPowerOffCounter == 0) {
      lcdLightFlag = false;
      lcdLightPowerOffCounter = lcdLightPowerOffTime;
    }
  }
  turnLCDLight();
}

void turnLCDLight() {
  digitalWrite(LCD_LIGHT_PIN, lcdLightFlag ? HIGH : LOW);
}

void writeLCDHeaders() {
  lcd.setCursor(0, 0);
  lcd.print("Mod");
  lcd.setCursor(4, 0);
  lcd.print("Di");
  lcd.setCursor(7, 0);
  lcd.print("Dly");
  lcd.setCursor(11, 0);
  lcd.print("Angle");
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
  writeThreeDigitNumber(6, 1, value);
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

void writeAngle(int value) {
  writeThreeDigitNumber(10, 1, value / 10);
//  lcd.setCursor(14, 1);
  lcd.print('.');
  lcd.print(value % 10);
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
    case EDITOR_ANGLE_INDEX:
      lcd.cursor();
      lcd.setCursor(ANGLE_EDITOR_CURSOR_POSITION, 1);
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
    if (mod == MODE_MANUAL) {
      lcdLightFlag = true;
    }
    lcdLightPowerOffCounter = lcdLightPowerOffTime;
    switch (results.value) {
      case 16769055: // -
      case 1:
        result = lastPressed = BTN_MINUS;
        break;
      case 16754775: // +
      case 2:
        result = lastPressed = BTN_PLUS;
        break;
      case 16748655: // eq
      case 3:
        result = BTN_EQ;
        lastPressed = 0;
        break;
      case 16761405: // play/pause
      case 4:
        result = BTN_PLAY;
        lastPressed = 0;
        break;
      case 16738455: // 0
      case 0:
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
    writeDebugInfo("\r\n");
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

