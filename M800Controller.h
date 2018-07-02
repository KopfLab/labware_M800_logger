#pragma once
#include <vector>
#include "M800State.h"
#include "device/SerialDeviceController.h"

// serial communication constants
#define M800_DATA_REQUEST  "D00Z" // data request command
#define M800_DATA_N_MAX    12 // NOTE: consider making this dynamic

// controll information
#define RS485TxControl    D3
#define RS485Baud         19200
#define RS485Transmit     HIGH
#define RS485Receive      LOW
#define RS485SerialConfig SERIAL_8N1

// controller class
class M800Controller : public SerialDeviceController {

  private:

    // state
    M800State* state;
    SerialDeviceState* dss = state;
    DeviceState* ds = state;


    // serial communication
    int data_pattern_pos;
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
    int processSerialData(byte b);
    void completeSerialData();

    // state
    void assembleStateInformation();
    DeviceState* getDS() { return(ds); }; // return device state
    SerialDeviceState* getDSS() { return(dss); }; // return device state serial
    void saveDS(); // save device state to EEPROM
    bool restoreDS(); // load device state from EEPROM

    // particle commands
    void parseCommand (); // parse a cloud command

};

/**** CONSTRUCTION & init ****/

void M800Controller::construct() {
  // start data vector

  data.resize(M800_DATA_N_MAX);
  for (int i=0; i<M800_DATA_N_MAX; i++) {
    data[i] = DeviceData(i+1);
  }

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

/* // FIXME

// pattern pieces
#define P_VAL       -1 // [ +-0-9]
#define P_UNIT      -2 // [GOC]
#define P_STABLE    -3 // [ S]
#define P_BYTE       0 // anything > 0 is specific byte

// specific ascii characters (actual byte values)
#define B_SPACE      32 // \\s
#define B_CR         13 // \r
#define B_NL         10 // \n
#define B_0          48 // 0
#define B_9          57 // 9

const int data_pattern[] = {P_VAL, P_VAL, P_VAL, P_VAL, P_VAL, P_VAL, P_VAL, P_VAL, P_VAL, B_SPACE, B_SPACE, P_UNIT, P_STABLE, B_CR, B_NL};
const int data_pattern_size = sizeof(data_pattern) / sizeof(data_pattern[0]) - 1;

*/ //FIXME

#define DATA_DELIM 		   9  // value/variable pair delimiter (tab)
#define DATA_END         13 // end of data stream (return character)
#define DATA_SEP         44 // user's choice, 124 is the pipe (|), 44 is comma (,)


void M800Controller::startSerialData() {
  SerialDeviceController::startSerialData();
  //data_pattern_pos = 0; // FIXME
}

int M800Controller::processSerialData(byte b) {
  // keep track of all data
  SerialDeviceController::processSerialData(b);

  // pattern interpretation
  char c = (char) b;

  if (c == 0) {
    // do nothing when 0 character encountered
  } else if (c == DATA_DELIM) {
    // end of value or variable
    /* FIXME
    if (variable_counter == 0)
      temp_message += " "; // space between date and time
    else
      temp_message += (char) DATA_SEP;
    variable_counter++; // first real value/variable pair found
    */
  } else if (c >= 32 && c <= 126) {
    // only consider regular ASCII range
    // temp_message += (char) c; // FIXME
  } else if (c == DATA_END) {
    // end of the data transmission
    /* FIXME
    message_received = true;

    if (message_error) {
      // message received but there were errors in it!
      Serial.println("ignored because of ERRORS. Partial message:");
      Serial.println(temp_message);
    } else {
      // no errors, make this the new message
      last_message = temp_message;
      last_message_time = Time.now();
      Serial.print("SUCCESSFULLY retrieved with ");
      Serial.println(String(variable_counter/2) + " variable/value pairs:");
      Serial.println(last_message);
    }*/
    return(SERIAL_DATA_COMPLETE);
  } else {
    // unrecognized part of data --> error
    return(SERIAL_DATA_ERROR);
  }

  return(SERIAL_DATA_WAITING);
}

void M800Controller::completeSerialData() {
  for (int i=0; i<M800_DATA_N_MAX; i++) {
    if (data[i].newest_value_valid)
      data[i].saveNewestValue(true); // average for all valid data
  }
  SerialDeviceController::completeSerialData();
}

/****** STATE INFORMATION *******/

void M800Controller::assembleStateInformation() {
  SerialDeviceController::assembleStateInformation();
  char pair[60];
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
  }

  // more additional, device specific commands

}
