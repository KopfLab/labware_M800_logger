#pragma once
#include <vector>
#include "M800State.h"
#include "device/SerialDeviceController.h"

// serial communication constants
#define M800_DATA_REQUEST  "D00Z" // data request command
#define M800_DATA_N_MAX    9
// NOTE: consider making this dynamic but keep in mind that around 12 the 600 chars overflow!
// FIXME: not unusal to have 12 variables from the M800 (4 devices x 3 vars, what to do in this case?)

// controll information
#define RS485TxControl    D3
#define RS485Baud         19200
#define RS485Transmit     HIGH
#define RS485Receive      LOW
#define RS485SerialConfig SERIAL_8N1
#define M800_DATA_DELIM 		   9  // value/variable pair delimiter (tab)
#define M800_DATA_END         13 // end of data stream (return character)

// controller class
class M800Controller : public SerialDeviceController {

  private:

    // state
    M800State* state;
    SerialDeviceState* dss = state;
    DeviceState* ds = state;

    // serial communication
    int data_pattern_pos;
    int var_used;
    int var_counter;
    bool expect_value_next;
    void construct();

  public:

    // constructors
    M800Controller();

    // without LCD
    M800Controller (int reset_pin, const int error_wait, M800State* state) :
      SerialDeviceController(reset_pin, RS485Baud, RS485SerialConfig, M800_DATA_REQUEST , error_wait), state(state) { construct(); }

    // with LCD
    M800Controller (int reset_pin, DeviceDisplay* lcd, const int error_wait, M800State* state) :
      SerialDeviceController(reset_pin, lcd, RS485Baud, RS485SerialConfig, M800_DATA_REQUEST , error_wait), state(state) { construct(); }

    // setup & loop
    void init();

    // serial
    void sendRequestCommand();
    void startSerialData();
    bool checkVariableBuffer(const char* pattern, bool units_first = false);
    int processSerialData(byte b);
    void completeSerialData(int error_count);

    // data
    void updateDataInformation();

    // state
    void assembleStateInformation();
    DeviceState* getDS() { return(ds); }; // return device state
    SerialDeviceState* getDSS() { return(dss); }; // return device state serial
    void saveDS(); // save device state to EEPROM
    bool restoreDS(); // load device state from EEPROM

    // particle commands
    void parseCommand (); // parse a cloud command
    bool parsePage();
    bool nextPage();

};

/**** CONSTRUCTION & init ****/

void M800Controller::construct() {
  // start data vector

  data.resize(M800_DATA_N_MAX);
  for (int i=0; i<M800_DATA_N_MAX; i++) {
    data[i] = DeviceData(i+1);
  }

  // FIXME: this should essentially be possible restored from state and changred by command
  int i=0;
  data[i].setVariable("pH"); data[i++].setUnits("");
  data[i].setVariable("ORP"); data[i++].setUnits("mV");
  data[i].setVariable("T"); data[i++].setUnits("DegC");
  data[i].setVariable("pH"); data[i++].setUnits("");
  data[i].setVariable("ORP"); data[i++].setUnits("mV");
  data[i].setVariable("T"); data[i++].setUnits("DegC");
  data[i].setVariable("O2"); data[i++].setUnits("mg/L");
  data[i].setVariable("O2"); data[i++].setUnits("nA");
  data[i].setVariable("T"); data[i++].setUnits("DegC");
  var_used = i;
}

// init function
void M800Controller::init() {
  pinMode(RS485TxControl, OUTPUT);
  SerialDeviceController::init();
  digitalWrite(RS485TxControl, RS485Receive);
}

/**** SERIAL COMMUNICATION ****/

void M800Controller::sendRequestCommand() {
  digitalWrite(RS485TxControl, RS485Transmit);
  Serial1.println(request_command);
  delay(6); // transmit the data before switching back to receive
  digitalWrite(RS485TxControl, RS485Receive);
}

void M800Controller::startSerialData() {
  SerialDeviceController::startSerialData();
  var_counter = 0;
  expect_value_next = false;
}

bool M800Controller::checkVariableBuffer(const char* pattern, bool units_first) {
  char var_units_buffer[20];
  if (units_first)
    snprintf(var_units_buffer, sizeof(var_units_buffer), pattern,
      data[var_counter - 2].units, data[var_counter - 2].variable);
  else
    snprintf(var_units_buffer, sizeof(var_units_buffer), pattern,
      data[var_counter - 2].variable, data[var_counter - 2].units);
  return (strcmp(variable_buffer, var_units_buffer) == 0);
}

int M800Controller::processSerialData(byte b) {
  // keep track of all data
  SerialDeviceController::processSerialData(b);

  // pattern interpretation
  char c = (char) b;

  if (c == 0) {
    // do nothing when 0 character encountered
  } else if (c == M800_DATA_DELIM) {
    // end of value or variable
    bool valid_char = true;
    if (var_counter == 0) {
      // date
      var_counter++;
      expect_value_next = false;
    } else if (var_counter == 1) {
      // time
      var_counter++;
      expect_value_next = true;
    } else if (expect_value_next) {
      // variable finished, value next
      expect_value_next = false;
    } else {
      // value/variable pair finished --> check variable
      bool valid_key = checkVariableBuffer("%s");
      if (!valid_key) valid_key = checkVariableBuffer("%s", true);
      if (!valid_key) valid_key = checkVariableBuffer("%s %s", true);
      if (!valid_key) valid_key = checkVariableBuffer("%s%s", true);
      // --> set / check value (infer decimals + 2 for better precision)
      bool valid_value = data[var_counter - 2].setNewestValue(value_buffer, true, true, 2L);
      // check if problem requires throwing an error
      if (!valid_value) {
        if (strcmp(value_buffer, "----  ") == 0) valid_value = true;
        if (strcmp(value_buffer, "****  ") == 0) valid_value = true;
        // not entirely clear why this might happen but it is a frequent pattern
        if (strcmp(value_buffer, "KKKKJ") == 0) valid_value = true;
      }
      if (!valid_key || !valid_value) valid_char = false;

      // debugging info
      #ifdef SERIAL_DEBUG_ON
        appendToSerialDataBuffer(124);
        appendToSerialDataBuffer(75);
        if (valid_key) appendToSerialDataBuffer(71);
        else appendToSerialDataBuffer(66);
        appendToSerialDataBuffer(86);
        if (valid_value) appendToSerialDataBuffer(71);
        else appendToSerialDataBuffer(66);
      #endif

      if (!valid_char) {
        // problem with value/var pair
        error_counter++;
        Serial.printf("WARNING: problem %d with serial data for key/variable pair: %s = %s\n", error_counter, variable_buffer, value_buffer);
        snprintf(lcd_buffer, sizeof(lcd_buffer), "M800: %d value error", error_counter);
        if (lcd) lcd->printLineTemp(1, lcd_buffer);
      }

      // reset for next value/variable pair
      appendToSerialDataBuffer(44); // add a comma to buffer for easier readability
      resetSerialVariableBuffer();
      resetSerialValueBuffer();
      var_counter++;
      // value next
      expect_value_next = true;
    }

  } else if (c >= 32 && c <= 126) {
    // only consider regular ASCII range
    if (var_counter >= 2) {
      // donce with date & time
      if (expect_value_next) appendToSerialValueBuffer(c);
      else appendToSerialVariableBuffer(c);
    }
  } else if (c == M800_DATA_END) {
    // end of the data transmission
    #ifdef SERIAL_DEBUG_ON
      appendToSerialDataBuffer(124);
      if (var_counter - 2 == var_used) {
        appendToSerialDataBuffer(89);
      } else {
        appendToSerialDataBuffer(78);
      }
    #endif
    if (var_counter - 2 != var_used) {
      Serial.printf("WARNING: failed to receive expected number (%d) of value/variable pairs, only %d\n", var_counter - 2, var_used);
      if (lcd) lcd->printLineTemp(1, "M800: incomplete msg");
      error_counter++;
    }
    return(SERIAL_DATA_COMPLETE);
  } else {
    // unrecognized part of data --> ignore (happens too frequently to throw an error)
    //return(SERIAL_DATA_ERROR);
  }

  return(SERIAL_DATA_WAITING);
}

void M800Controller::completeSerialData(int error_count) {
  // only save values if all of them where received properly
  if (error_count == 0) {
    for (int i=0; i<var_used; i++) data[i].saveNewestValue(true); // average for all valid data
  }
}

/****** STATE INFORMATION *******/

void M800Controller::assembleStateInformation() {
  SerialDeviceController::assembleStateInformation();
  char pair[60];
}


/***** DATA INFORMATION ****/

void M800Controller::updateDataInformation() {
  SerialDeviceController::updateDataInformation();
  // LCD update
  if (lcd) {

    int i;
    for (int line = 2; line <= 4; line++) {
      i = (state->page - 1) * 3 + line - 2;
      if (i < var_used) {
        if (data[i].getN() > 0)
          getDataDoubleText(data[i].idx, data[i].variable, data[i].getValue(), data[i].units, data[i].getN(),
            lcd_buffer, sizeof(lcd_buffer), PATTERN_IKVUN_SIMPLE, data[i].getDecimals());
        else
          getInfoIdxKeyValue(lcd_buffer, sizeof(lcd_buffer), data[i].idx, data[i].variable, "no data yet", PATTERN_IKV_SIMPLE);
        lcd->printLine(line, lcd_buffer);
      } else {
        lcd->printLine(line, "");
      }
    }
  }
}

/**** STATE PERSISTENCE ****/

// save device state to EEPROM
void M800Controller::saveDS() {
  EEPROM.put(STATE_ADDRESS, *state);
  #ifdef STATE_DEBUG_ON
    Serial.println("INFO: M800 state saved in memory (if any updates were necessary)");
  #endif
}

// load device state from EEPROM
bool M800Controller::restoreDS(){
  M800State saved_state;
  EEPROM.get(STATE_ADDRESS, saved_state);
  bool recoverable = saved_state.version == STATE_VERSION;
  if(recoverable) {
    EEPROM.get(STATE_ADDRESS, *state);
    Serial.printf("INFO: successfully restored state from memory (version %d)\n", STATE_VERSION);
  } else {
    Serial.printf("INFO: could not restore state from memory (found version %d), sticking with initial default\n", saved_state.version);
    saveDS();
  }
  return(recoverable);
}


/****** WEB COMMAND PROCESSING *******/

void M800Controller::parseCommand() {

  SerialDeviceController::parseCommand();

  if (command.isTypeDefined()) {
    // command processed successfully by parent function
  } else if (parsePage()) {
    // parse page command
  }

  // more additional, device specific commands

}

bool M800Controller::parsePage() {
  if (command.parseVariable(CMD_PAGE)) {
    // just page by one
    command.success(nextPage());
  }
  return(command.isTypeDefined());
}

bool M800Controller::nextPage() {

  int max_pages = floor(var_used / 3.0);
  state->page = (state->page % max_pages) + 1;
  updateDataInformation();

  #ifdef STATE_DEBUG_ON
    Serial.printf("INFO: flipping to next page (%d) on LCD %d\n", state->page);
  #endif

  saveDS();

  return(true);
}
