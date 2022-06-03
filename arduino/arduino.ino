// https://github.com/Arduino-IRremote/Arduino-IRremote
#include <IRremote.hpp>
#include <LiquidCrystal.h>
#include "clocktime.h"

// LIQUIDCRYSTAL
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
String lines[2];
bool lcd_change;

// IR REMOTE
bool ir_justWritten;
char ir_lastCommand;

// Elegoo Remote Mappings
#define IR_POWER 0x45
#define IR_VOLPL 0x46
#define IR_FUNC  0x47
#define IR_BACK  0x44
#define IR_PLAY  0x40
#define IR_SKIP  0x43
#define IR_DOWN  0x07
#define IR_VOLMN 0x15
#define IR_UP    0x09
#define IR_0     0x16
#define IR_EQ    0x19
#define IR_STREP 0x0D
#define IR_1     0x0C
#define IR_2     0x18
#define IR_3     0x5E
#define IR_4     0x08
#define IR_5     0x1C
#define IR_6     0x5A
#define IR_7     0x42
#define IR_8     0x52
#define IR_9     0x4A

// TIME
#define ULONG_MAX 4294967295
const unsigned long msInSec = 1000;
const unsigned long msInMin = 60000;
const unsigned long msInHr = 3600000;
const unsigned long msInDay = 86400000;
unsigned long day_time;
unsigned long last_timestamp;

// CLOCK
enum ClockMode { CL_SET, CL_SHOW, CL_SET_ALARM };
int clock_mode;

// TASK SCHEDULER
typedef struct task {
  int state;
  unsigned long period;
  unsigned long elapsedTime;
  int (*TickFct)(int);
} task;
const unsigned short tasksNum = 3;
task tasks[tasksNum];

// Debug for IR receiving commands
enum DebugIR_States { DebugIR_Init, DebugIR_S0 };
int DebugIR_TickFct(int state) {
  switch (state) {
    case DebugIR_Init:
    case DebugIR_S0:
      state = DebugIR_S0;
      break;
  }

  switch (state) {
    case DebugIR_S0:
      if (ir_justWritten) {
        lines[0] = formatForLCD("Command");
        lines[1] = formatForLCD(mapCharToCommand(acceptIRInput()));
        lcd_change = true; // queue the lcd to change
      }
      break;
  }
  return state;
}

// SM for setting the clock
// Runs when clock_mode == CL_SET
bool clset_blinking = false;
clocktime clset_time;
bool clset_modified = false;
enum CLSet_States { CLSet_Init, CLSet_OtherMode, CLSet_Slot1, CLSet_Slot2, CLSet_Slot3, CLSet_Slot4, CLSet_Slot5 };
int CLSet_TickFct(int state) {
  // switch cursor blinking
  clset_blinking = !clset_blinking;
  
  switch (state) {
    case CLSet_Init:
      // Since we start on CL_SET just switch to slot 1
      state = CLSet_Slot1;
      clset_time.hour = 0;
      clset_time.minute = 0;
      clset_time.second = 0;
      clset_time.am = false;
      clset_modified = true;
      break;
    case CLSet_OtherMode:
      if (clock_mode == CL_SET) {
        state = CLSet_Slot1;
        // we receive the time from the current time
        clset_time = timestampToClockTime(day_time);
        clset_modified = false;
      }
      break;
    case CLSet_Slot1:
    case CLSet_Slot2:
    case CLSet_Slot3:
    case CLSet_Slot4:
    case CLSet_Slot5:
      if (clock_mode == CL_SET) {
        // Wait for command
        if (ir_justWritten) {
          // Three possible actions
          // FUNC - switch to next state
          // LEFT - switch to slot to the left (or wrap around)
          // RIGHT - switch to slot to the right (or wrap around)
          char clset_com = acceptIRInput();
          switch (clset_com) {
            case IR_FUNC:
              state = CLSet_OtherMode;
              clock_mode = CL_SHOW;
              if (clset_modified) {
                day_time = clockTimeToTimestamp(clset_time);
              }
              break;
            case IR_BACK:
              state--;
              if (state < CLSet_Slot1) {
                state = CLSet_Slot5;
              }
              break;
            case IR_SKIP:
              state++;
              if (state > CLSet_Slot5) {
                state = CLSet_Slot1;
              }
              break;
            default:
              // We did not get a valid command, so pass it on just in case
              ir_justWritten = true;
              break;
          }
        }
      }
      break;
  }

  switch (state) {
    case CLSet_Slot1:
      if (ir_justWritten) {
        char clset_slot1_cmd = acceptIRInput();
        clset_modified = true;
        clset_blinking = false;
        switch (clset_slot1_cmd) {
          case IR_0:
            switch (clset_time.hour) {
              case 10:
              case 11:
                clset_time.hour -= 10;
                break;
              case 0:
                clset_time.hour = 2;
                break;
            }
            break;
          case IR_1:
            switch (clset_time.hour) {
              case 0:
              case 10:
              case 11:
                break;
              case 1:
                clset_time.hour = 11;
                break;
              case 2:
                clset_time.hour = 0;
                break;
              default:
                clset_time.hour = 10;
                break;
            }
            break;
          default:
            break;
        }
      }
      lines[0] = formatForLCD("Set time:");
      lines[1] = formatForLCD(dayTimeToSetDisplay(clockTimeToTimestamp(clset_time), 1, clset_blinking));
      lcd_change = true;
      break;
    case CLSet_Slot2:
      if (ir_justWritten) {
        char clset_slot2_cmd = acceptIRInput();
        clset_modified = true;
        clset_blinking = false;
        switch (clset_slot2_cmd) {
          case IR_0:
            switch (clset_time.hour) {
              case 10:
              case 11:
              case 0:
                clset_time.hour = 10;
                break;
              default:
                // Invalid interaction if there is no tens place
                break;
            }
            break;
          case IR_1:
            switch (clset_time.hour) {
              case 10:
              case 11:
              case 0:
                clset_time.hour = 11;
                break;
              default:
                clset_time.hour = 1;
                break;
            }
            break;
          case IR_2:
            switch (clset_time.hour) {
              case 10:
              case 11:
              case 0:
                clset_time.hour = 0;
                break;
              default:
                clset_time.hour = 2;
                break;
            }
            break;
          case IR_3:
          case IR_4:
          case IR_5:
          case IR_6:
          case IR_7:
          case IR_8:
          case IR_9:
            switch (clset_time.hour) {
              case 10:
              case 11:
              case 0:
                // Invalid interaction if there is tens place
                break;
              default:
                clset_time.hour = mapIRNum(clset_slot2_cmd);
                break;
            }
            break;
          default:
            break;
        }
      }
      lines[0] = formatForLCD("Set time:");
      lines[1] = formatForLCD(dayTimeToSetDisplay(clockTimeToTimestamp(clset_time), 2, clset_blinking));
      lcd_change = true;
      break;
    case CLSet_Slot3:
      if (ir_justWritten) {
        char clset_slot3_cmd = acceptIRInput();
        clset_modified = true;
        clset_blinking = false;
        switch (clset_slot3_cmd) {
          case IR_0:
          case IR_1:
          case IR_2:
          case IR_3:
          case IR_4:
          case IR_5:
            clset_time.minute = (clset_time.minute % 10) + (10 * mapIRNum(clset_slot3_cmd));
            break;
          default:
            break;
        }
      }
      lines[0] = formatForLCD("Set time:");
      lines[1] = formatForLCD(dayTimeToSetDisplay(clockTimeToTimestamp(clset_time), 3, clset_blinking));
      lcd_change = true;
      break;
    case CLSet_Slot4:
      if (ir_justWritten) {
        char clset_slot4_cmd = acceptIRInput();
        clset_modified = true;
        clset_blinking = false;
        switch (clset_slot4_cmd) {
          case IR_0:
          case IR_1:
          case IR_2:
          case IR_3:
          case IR_4:
          case IR_5:
          case IR_6:
          case IR_7:
          case IR_8:
          case IR_9:
            clset_time.minute = ((clset_time.minute / 10) * 10) + mapIRNum(clset_slot4_cmd);
            break;
          default:
            break;
        }
      }
      lines[0] = formatForLCD("Set time:");
      lines[1] = formatForLCD(dayTimeToSetDisplay(clockTimeToTimestamp(clset_time), 4, clset_blinking));
      lcd_change = true;
      break;
    case CLSet_Slot5:
      lines[0] = formatForLCD("Set time:");
      lines[1] = formatForLCD(dayTimeToSetDisplay(clockTimeToTimestamp(clset_time), 5, clset_blinking));
      lcd_change = true;
      if (ir_justWritten) {
        char clset_slot5_cmd = acceptIRInput();
        clset_modified = true;
        clset_blinking = false;
        switch (clset_slot5_cmd) {
          case IR_PLAY:
            clset_time.am = !clset_time.am;
            break;
          default:
            break;
        }
      }
      break;
  }
  return state;
}

// SM for showing the time
// Runs when clock_mode == CL_SHOW
enum CLShow_States { CLShow_Init, CLShow_OtherMode, CLShow_Display };
int CLShow_TickFct(int state) {
  switch (state) {
    case CLShow_Init:
      state = CLShow_OtherMode;
      break;
    case CLShow_OtherMode:
      if (clock_mode == CL_SHOW) {
        state = CLShow_Display;
      }
      break;
    case CLShow_Display:
      if (ir_justWritten) {
        char clshow_com = acceptIRInput();
        switch (clshow_com) {
          case IR_FUNC:
            state = CLShow_OtherMode;
            clock_mode = CL_SET;
            break;
          default:
            // We did not get a valid command, pass it on just in case
            ir_justWritten = true;
            break;
        }
      }
      break;
  }

  switch (state) {
    case CLShow_Display:
      lines[0] = formatForLCD(dayTimeToShowDisplay(day_time));
      lines[1] = formatForLCD("Have a good day!");
      lcd_change = true;
      // Accept input just in case (from if it was passed)
      if (ir_justWritten) acceptIRInput();
      break;
  }
  return state;
}

// SM for setting the alarm
// Runs when clock_mode == CL_SET_ALARM

enum UpdateScreen_States { UpdateScreen_Init, UpdateScreen_Wait, UpdateScreen_Write };
int UpdateScreen_TickFct(int state) {
  switch (state) {
    case UpdateScreen_Wait:
      if (lcd_change) {
        state = UpdateScreen_Write;
      }
      break;
    default:
      state = UpdateScreen_Wait;
      break;
  }

  switch (state) {
    case UpdateScreen_Write:
      lcd.begin(16, 2);
      lcd.print(lines[0]);
      lcd.setCursor(0,1);
      lcd.print(lines[1]);
      lcd_change = false;
      break;
  }
  return state;
}

void setup() {
  Serial.begin(9600);

  // LIQUIDCRYSTAL
  lines[0] = formatForLCD("Initializing...");
  lines[1] = formatForLCD("");
  lcd_change = true;
  
  // IR REMOTE
  IrReceiver.begin(7, false);
  ir_justWritten = false;
  ir_lastCommand = 0;

  // TIME
  day_time = 0;
  last_timestamp = 0;

  // CLOCK
  clock_mode = CL_SET;

  // TASK SCHEDULER
  unsigned char i = 0;
  /*
  tasks[i].state = DebugIR_Init;
  tasks[i].period = 500;
  tasks[i].elapsedTime = 0;
  tasks[i].TickFct = &DebugIR_TickFct;
  i++;
  */
  tasks[i].state = CLSet_Init;
  tasks[i].period = 500;
  tasks[i].elapsedTime = 0;
  tasks[i].TickFct = &CLSet_TickFct;
  i++;
  tasks[i].state = CLShow_Init;
  tasks[i].period = 1000;
  tasks[i].elapsedTime = 0;
  tasks[i].TickFct = &CLShow_TickFct;
  i++;
  tasks[i].state = UpdateScreen_Init;
  tasks[i].period = 50;
  tasks[i].elapsedTime = 0;
  tasks[i].TickFct = &UpdateScreen_TickFct;
  i++;
}

void loop() {
  // TIME
  unsigned long now = millis();
  // Handle unsigned long overflow
  unsigned long diff = now - last_timestamp;
  if (diff > msInDay) { // choose some unreasonable obscenely large number
    diff = ULONG_MAX - diff + 1; // correct for overflow
  }
  day_time += diff;
  day_time %= msInDay;
  last_timestamp = now;
  
  // IR REMOTE
  if (IrReceiver.decode()) {
    // flags = 1 will tell us if it is repeat
    // so check if it is 0 to do anything with input
    if (IrReceiver.decodedIRData.protocol == NEC && !IrReceiver.decodedIRData.flags) {
      ir_lastCommand = IrReceiver.decodedIRData.command;
      Serial.println(ir_lastCommand, HEX);
      // IrReceiver.printIRResultShort(&Serial); // optional use new print version
      ir_justWritten = true;
    }
    IrReceiver.resume(); // Enable receiving of the next value
  }

  // TASK SCHEDULER
  unsigned char i;
  for (i = 0; i < tasksNum; ++i) {
    if ( (millis() - tasks[i].elapsedTime) >= tasks[i].period) {
      tasks[i].state = tasks[i].TickFct(tasks[i].state);
      tasks[i].elapsedTime = millis(); // Last time this task was ran
    }
  }
}

clocktime timestampToClockTime(unsigned long timestamp) {
  clocktime ct;
  ct.am = timestamp < (msInDay / 2);
  ct.hour = timestamp / msInHr;
  ct.minute = (timestamp % msInHr) / msInMin;
  ct.second = (timestamp % msInMin) / msInSec;
  return ct;
}

unsigned long clockTimeToTimestamp(clocktime ct) {
  unsigned long result = 0;
  result += (ct.am ? 0 : msInDay / 2);
  result += ct.hour * msInHr;
  result += ct.minute * msInMin;
  result += ct.second * msInSec;
  return result;
}

String formatTimeSlot(int num) {
  if (num < 10) {
    Serial.println(num);
    return String("0" + String(num));
  }
  return String(num);
}

int convertHourToTwelveHour(int hour) {
  return (hour % 12 == 0 ? 12 : hour % 12);
}

String dayTimeToSetDisplay(unsigned long timestamp, int slot, bool isBlink) {
  clocktime t = timestampToClockTime(timestamp);
  String hour = formatTimeSlot(convertHourToTwelveHour(t.hour));
  String minute = formatTimeSlot(t.minute);
  String ampm = (t.am ? "AM" : "PM");
  if (isBlink) {
    switch (slot) {
      case 1:
        hour = String(char(255) + hour.substring(1));
        break;
      case 2:
        hour = String(hour.substring(0, 1) + String(char(255)));
        break;
      case 3:
        minute = String(char(255) + minute.substring(1));
        break;
      case 4:
        minute = String(minute.substring(0, 1) + String(char(255)));
        break;
      case 5:
        ampm = String(char(255) + String(char(255)));
        break;
    }
  }
  return String(hour + ":" + minute + " " + ampm);
}

String dayTimeToShowDisplay(unsigned long timestamp) {
  clocktime t = timestampToClockTime(timestamp);
  return String(formatTimeSlot(convertHourToTwelveHour(t.hour)) + ":" + formatTimeSlot(t.minute) + ":" + formatTimeSlot(t.second) + " " + (t.am ? "AM" : "PM"));
}

char acceptIRInput() {
  ir_justWritten = false;
  return ir_lastCommand;
}

int mapIRNum(char command) {
  switch (command) {
    case IR_0:
      return 0;
    case IR_1:
      return 1;
    case IR_2:
      return 2;
    case IR_3:
      return 3;
    case IR_4:
      return 4;
    case IR_5:
      return 5;
    case IR_6:
      return 6;
    case IR_7:
      return 7;
    case IR_8:
      return 8;
    case IR_9:
      return 9;
    default:
      return -1;
  }
}

String formatForLCD(String str) {
  String result = String(str);
  int spacesToAdd = 16 - str.length();
  for (int i = 0; i < spacesToAdd; i++) {
    result = String(str + " ");
  }
  return result;
}

String mapCharToCommand(char cmd) {
  switch (cmd) {
    case IR_POWER:
      return "POWER";
    case IR_VOLPL:
      return "VOLPL";
    case IR_FUNC:
      return "FUNC";
    case IR_BACK:
      return "BACK";
    case IR_PLAY:
      return "PLAY";
    case IR_SKIP:
      return "SKIP";
    case IR_DOWN:
      return "DOWN";
    case IR_VOLMN:
      return "VOLMN";
    case IR_UP:
      return "UP";
    case IR_0:
      return "0";
    case IR_EQ:
      return "EQ";
    case IR_STREP:
      return "STREP";
    case IR_1:
      return "1";
    case IR_2:
      return "2";
    case IR_3:
      return "3";
    case IR_4:
      return "4";
    case IR_5:
      return "5";
    case IR_6:
      return "6";
    case IR_7:
      return "7";
    case IR_8:
      return "8";
    case IR_9:
      return "9";
    default:
      return "UNKNOWN";
  }
}
