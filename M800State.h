#pragma once
#include "device/SerialDeviceState.h"

// additional commands
#define CMD_PAGE          "page" // flip to next lcd page

// M800 state
struct M800State : public SerialDeviceState {

  uint page;

  M800State() {};

  M800State (bool locked, bool state_logging, bool data_logging, uint data_reading_period_min, uint data_reading_period, uint data_logging_period, uint8_t data_logging_type) :
    SerialDeviceState(locked, state_logging, data_logging, data_reading_period_min, data_reading_period, data_logging_period, data_logging_type), page(1) {};

};

/**** textual translations of state values ****/
