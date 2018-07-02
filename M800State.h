#pragma once
#include "device/SerialDeviceState.h"

// scale state
struct M800State : public SerialDeviceState {

  uint calc_rate;

  M800State() {};

  M800State (bool locked, bool state_logging, bool data_logging, uint data_reading_period_min, uint data_reading_period, uint data_logging_period, uint8_t data_logging_type) :
    SerialDeviceState(locked, state_logging, data_logging, data_reading_period_min, data_reading_period, data_logging_period, data_logging_type) {};

};

/**** textual translations of state values ****/
