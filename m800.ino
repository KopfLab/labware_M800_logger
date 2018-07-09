#pragma SPARK_NO_PREPROCESSOR // disable spark preprocssor to avoid issues with callbacks
#include "application.h"

// time sync
#include "TimeSync.h";
TimeSync* ts = new TimeSync();

// debugging options
#define CLOUD_DEBUG_ON
//#define WEBHOOKS_DEBUG_ON
//#define STATE_DEBUG_ON
#define DATA_DEBUG_ON
//#define SERIAL_DEBUG_ON
//#define LCD_DEBUG_ON

// keep track of installed version
#define STATE_VERSION    3 // update whenver structure changes
#define DEVICE_VERSION  "0.3.3" // update with every code update

// M800 controller
#include "M800Controller.h"

// lcd
DeviceDisplay* lcd = &LCD_20x4;

// initial state of the M800
M800State* state = new M800State(
  /* locked */                    false,
  /* state_logging */             true,
  /* data_logging */              false,
  /* data_reading_period_min */   2000, // in ms
  /* data_reading_period */       5000, // in ms
  /* data_logging_period */       600, // in seconds
  /* data_logging_type */         LOG_BY_TIME // log by time
);

// M800 controller
M800Controller* M800 = new M800Controller(
  /* reset pin */         A5,
  /* lcd screen */        lcd,
  /* error wait */        500,
  /* pointer to state */  state
);


// using system threading to speed up restart after power out
SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(SEMI_AUTOMATIC);

// setup
void setup() {

  // serial
  Serial.begin(9600);

  // time sync
  ts->init();

  // lcd temporary messages
  lcd->setTempTextShowTime(3); // how many seconds temp time

  // M800
  Serial.println("INFO: initialize M800 version " + String(DEVICE_VERSION));
  M800->init();

  // connect device to cloud
  Serial.println("INFO: connecting to cloud");
  Particle.connect();

}

// loop
void loop() {
  ts->update();
  M800->update();
}
