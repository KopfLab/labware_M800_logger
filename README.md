# Commands

#### `DeviceController` commands (`<cmd>` options):

  - `state-log on` to turn web logging of state changes on (letter `S` shown in the state overview)
  - `state-log off` to turn web logging of state changes off (no letter `S` in state overview)
  - `data-log on` to turn web logging of data on (letter `D` in state overview)
  - `data-log off` to turn web logging of data off
  - `lock on` to safely lock the device (i.e. no commands will be accepted until `lock off` is called) - letter `L` in state overview
  - `lock off` to unlock the device if it is locked
  - `reset data` to reset the data stored in the device

#### Additional `SerialDeviceController` commands:

  - `read-period <options>` to specify how frequently data should be read (letter `R` + subsequent in state overview), `<options>`:
    - `manual` don't read data unless externally triggered in some way (device specific) - `RM` in state overview
    - `200 ms` read data every 200 (or any other number) milli seconds (`R200ms` in state overview)
    - `5 s` read data every 5 (or any other number) seconds (`R5s` in state overview)
  - `log-period <options>` to specify how frequently read data should be logged (after letter `D` in state overview, although the `D` only appears if data logging is actually enabled), `<options>`:
    - `3 x` log after every 3rd (or any other number) successful data read (`D3x` in state overview if logging is active, just `3x` if not), works with `manual` or time based `read-period`, set to `1 x` in combination with `manual` to log every externally triggered data event immediately
    - `2 s` log every 2 seconds (or any other number), must exceed the `read-period` (`D2s` in state overview if data logging is active, just `2s` if not)
    - `8 m` log every 8 minutes (or any other number)
    - `1 h` log every hour (or any other number)

#### Additional `M800Controller` commands:

  - `page` jump to the next page on the LCD
