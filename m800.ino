#pragma SPARK_NO_PREPROCESSOR // disable spark preprocssor to avoid issues with callbacks
#include "application.h"

// time sync
#include "TimeSync.h";
TimeSync* ts = new TimeSync();

// debugging options
#define CLOUD_DEBUG_ON
#define WEBHOOKS_DEBUG_ON
//#define STATE_DEBUG_ON
#define DATA_DEBUG_ON
#define SERIAL_DEBUG_ON
//#define LCD_DEBUG_ON

// keep track of installed version
#define STATE_VERSION    1 // update whenver structure changes
#define DEVICE_VERSION  "0.2.1" // update with every code update

// M800 controller
#include "M800Controller.h"

// lcd
DeviceDisplay* lcd = &LCD_20x4;

// initial state of the M800
M800State* state = new M800State(
  /* locked */                    false,
  /* state_logging */             true,
  /* data_logging */              false,
  /* data_reading_period_min */   1000, // in ms
  /* data_reading_period */       5000, // in ms
  /* data_logging_period */       3600, // in seconds
  /* data_logging_type */         LOG_BY_TIME // log by time
);

// M800 controller
M800Controller* M800 = new M800Controller(
  /* reset pin */         A5,
  /* lcd screen */        lcd,
  /* error wait */        500,
  /* pointer to state */  state
);

// user interface
char lcd_buffer[21];

void update_gui_state() {
}

void update_gui_data() {
  // lcd
  if (lcd) {
    // FIXME
  }
}

// callback function for commands
void report_command () {
  update_gui_state();
  update_gui_data();
}

// callback function for data
void report_data() {
  update_gui_data();
}

// using system threading to speed up restart after power out
SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(SEMI_AUTOMATIC);

// setup
void setup() {

  // serial
  Serial.begin(9600);
  delay(1000);

  // time sync
  ts->init();

  // lcd temporary messages
  lcd->setTempTextShowTime(4); // how many seconds temp time

  // M800
  Serial.println("INFO: initialize M800 version " + String(DEVICE_VERSION));
  M800->setCommandCallback(report_command);
  M800->setDataCallback(report_data);
  M800->init();

  // connect device to cloud
  Serial.println("INFO: connecting to cloud");
  Particle.connect();

  // initial user interface update
  update_gui_state();
  update_gui_data();

}

// loop
void loop() {
  ts->update();
  M800->update();
}
