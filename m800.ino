#pragma SPARK_NO_PREPROCESSOR // disable spark preprocssor to avoid issues with callbacks
#include "application.h"

/*
TODO include EEPROM settings that define
 - number of recorded values (parse them properly in a struct of variable/value pairs)
 - maybe: the webhook to use to log the data
 - implement particle command structure to allow settings thes -> particle call <device> M800 "receive 8"
TODO separate M800 class that sends commands and parses the information
*/

//#define SD_LOG_ON 1
#define WEB_LOG_ON 1

/**** DISPLAY ****/
#include "Display.h"
Display display (
  /* i2c address */  0x27,
  /* lcd width */      20,
  /* lcd height */      4,
  /* message width */   7,
  /* sg show time */ 3000); // how long info messages are shown

/**** General settings ****/
#define REQUEST_INTERVAL 	5000        // data request interval in milli seconds
unsigned long last_request_time = 0;  // data requested / request sent to M800

#define DISPLAY_INTERVAL	3000        // how long each information screen is displayed (in ms)
unsigned long last_display_time = 0;  // last time display was changed

#define RS485TxControl   D3
#define RS485Baud        19200
#define RS485Transmit    HIGH
#define RS485Receive     LOW

#define MAX_MSG_SIZE     120 // length of read buffer (enough for ~10vars)
#define MAX_VAR_SIZE     25 // length of variable buffer
#define MAX_VAL_SIZE     25 // length of value buffer
#define DATE_TIME_SIZE   21 // length of date_time part of M800 message, format DD/MMM/YYYY HH:MM:SS

#define TIMEOUT_COUNTER  5 // how many timeouts until re-issuing request to M800
uint8_t request_timeout = 0;

#define DATA_DELIM 		   9  // value/variable pair delimiter (tab)
#define DATA_END         13 // end of data stream (return character)
#define DATA_SEP         44 // user's choice, 124 is the pipe (|), 44 is comma (,)

String last_message = "";	// whole read buffer
String temp_message = "";	// whole read buffer
String variable = "";   	// variable buffer
String value = "";      	// value buffer
time_t last_message_time; // time last message was received

// message information
bool message_received = true; // whether request was awnsered or not
bool message_error = false;   // whether there was any trouble with the message
int variable_counter;         // counts how many variables were measured


/***** Device name *****/

// device name
char device_name[20];
void name_handler(const char *topic, const char *data) {
  strncpy ( device_name, data, sizeof(device_name) );
  Serial.println("INFO: device ID " + String(device_name));
  display.print_line(1, "Device: " + String(device_name));
}

/****** CLOCK *****/

// time zone and time sync
#define TIMEZONE -6 // Mountain Daylight Time -6 / Mountain Standard Time -7
#define ONE_DAY_MILLIS (24 * 60 * 60 * 1000)
unsigned long last_sync = millis();

/*********** LOGGING *************/
#define SAVE_INTERVAL    (2 * 60 * 1000) // in milli seconds (every 2 minutes)
unsigned long last_save_time = 0;
char date_time_buffer[21];
char last_message_buffer[120];

#ifdef WEB_LOG_ON
  const char WEBHOOK[] = "geom_m800";

  // send to google
  char json_buffer[255];
  bool log_to_web() {
    Time.format(last_message_time, "%m/%d/%Y %H:%M:%S").toCharArray(date_time_buffer, sizeof(date_time_buffer));
    last_message.substring(last_message.indexOf(DATA_SEP)+1, last_message.length()).toCharArray(last_message_buffer, sizeof(last_message_buffer));
    snprintf(json_buffer, sizeof(json_buffer),
      "{\"datetime\":\"%s\",\"device\":\"%s\",\"type\":\"%s\",\"data\":\"%s\"}",
      date_time_buffer, device_name, "data", last_message_buffer);
    Serial.print("INFO: trying to send the following JSON to webhook ");
    Serial.println(WEBHOOK);
    Serial.println(json_buffer);
    return(Particle.publish(WEBHOOK, json_buffer));
  }

#endif

#ifdef SD_LOG_ON
  char log_file_name[] = "12345678.123";  // prototype file name in required 8.3 format
  const char FILE_EXT = "CSV";
  char file_buffer[255];
  //#include "SDCard.h"
  //DataLogger logger (SD_SPI);

  bool log_to_SD() {
    // filename
    Time.format(last_message_time, "%Y%m%d").toCharArray(date_time_buffer, sizeof(date_time_buffer));
    snprintf(log_file_name, sizeof(log_file_name), "%s.%s", date_time_buffer, FILE_EXT);

    // file content
    Time.format(last_message_time, "%m/%d/%Y %H:%M:%S").toCharArray(date_time_buffer, sizeof(date_time_buffer));
    last_message.substring(last_message.indexOf(DATA_SEP)+1, last_message.length()).toCharArray(last_message_buffer, sizeof(last_message_buffer));
    snprintf(file_buffer, sizeof(file_buffer), "%s,%s", date_time_buffer, last_message_buffer);

    Serial.print("INFO: trying to write the following line to SD card file ");
    Serial.println(log_file_name);
    Serial.println(file_buffer);
    //return (logger.write_to_file(log_file_name, last_message));

    return(false);
  }

#endif


/****** SETUP ******/

// using system threading to make sure to log to SD even if internet is out
SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(SEMI_AUTOMATIC);
bool name_handler_registered = false;

void setup(){
  delay(3000);

	// memory allocation
	last_message.reserve(MAX_MSG_SIZE); // reserve string size to avoid dynamic allocation and heap fragmentation
	temp_message.reserve(MAX_MSG_SIZE); // reserve string size to avoid dynamic allocation and heap fragmentation
	variable.reserve(MAX_VAR_SIZE); // reserve string size to avoid dynamic allocation and heap fragmentation
	value.reserve(MAX_VAL_SIZE); // reserve string size to avoid dynamic allocation and heap fragmentation

	// start serial monitor
	Serial.begin(9600);
	Serial.println();
	Serial.println("INFO: Initializing sensor module...");

  // time
  Time.zone(-6); // Mountain Daylight Time -6 / Mountain Standard Time -7
  Serial.print("INFO: starting up at time ");
  Serial.println(Time.format("%Y-%m-%d %H:%M:%S"));


	// initialize display
  Serial.print("INFO: Initializing LCD screen");
	display.init();
	display.print_line(1, "Device: waiting...");
	display.print_line(2, "Starting up...");
	Serial.println(" - LCD initialized");

  // RS485 setup
  pinMode(RS485TxControl, OUTPUT);
  Serial.print("INFO: Initializing communication via RS485 with Baud rate ");
  Serial.print(RS485Baud);
  Serial1.begin(RS485Baud, SERIAL_8N1);
  digitalWrite(RS485TxControl, RS485Receive);
	Serial.println(" - RS485 initialized");
  display.print_line(3, "RS485 initialized");

	// initialize SD card
  Serial.print("INFO: Initializing SD card");
	#ifdef SD_LOG_ON
    pinMode(SPI_PIN, OUTPUT); // enable SPI pin
    display.print_line(3, "Starting SD...");

		// see if the card is present and can be initialized:
		if (!logger.init()) {
			Serial.println(" - WARNING: SD Card failed, or not present");
		} else {
			Serial.println(" - SD card initialized. Existing log files:");
			logger.print_files(Serial, FILE_EXT);

      // TODO make this a remote command instead
			#ifdef DELETE_ALL_LOG_FILES
				Serial.println("\nWARNING: DELETING ALL LOG FILES ON CARD in 10 seconds!");
				delay(10000);
				logger.delete_all_log_files(FILE_EXT);
				Serial.println("All done, continuing normal operation.");
			#endif
		}
	#else
		 Serial.println(" - SD card is disabled");
     display.print_line(3, "SD disabled");
	#endif

  // connect device to cloud and register for listeners
  Serial.println("INFO: registering spark variables and connecting to cloud");
  Particle.subscribe("spark/", name_handler);
  Particle.connect();

  // startup complete
  Serial.println("INFO: Startup completed.");
  display.print_line(2, "Startup completed.");
  display.print_line(3, "");
  Serial.println();
}

/******* LOOP *******/

void loop() {

  // once connected to cloud, register name and send first weblog
  if (!name_handler_registered && Particle.connected()){
    // running this here becaus we're in system thread mode and it won't work until connected
    name_handler_registered = Particle.publish("spark/device/name");
    Serial.println("INFO: name handler registered");
  }

  // time sync (once a day)
  if ( (millis() - last_sync > ONE_DAY_MILLIS || millis() < last_sync ) && Particle.connected()) {
      // Request time synchronization from the Particle Cloud
      Particle.syncTime();
      last_sync = millis();
      Serial.println("INFO: time sync complete");
      display.show_message("Clock", "SYNCED");
  }

  // display update
  display.update();

  // M800 request
	if ( (millis() - last_display_time) > DISPLAY_INTERVAL || millis() < last_display_time) {
		if (last_message.length() == 0) {
			display.print_line(1, "No data from M800");
		} else {
      // get date time
			Time.format(last_message_time, "%m/%d/%Y %H:%M:%S").toCharArray(date_time_buffer, sizeof(date_time_buffer));
      // get message without date time
      last_message.substring(last_message.indexOf(DATA_SEP)+1, last_message.length()).toCharArray(last_message_buffer, sizeof(last_message_buffer));
      display.print_line(1, date_time_buffer);

      // TODO: do this properly by parsing variable/value pairs when first reading the message
			int start = 0;
			value = "";
			for (int i = 0; i < 2 * display.rows - 1; i++) {
				int end = last_message.indexOf(DATA_SEP, start);
				if (end < 0) break;
				if (i > 0 && i % 2 == 0) {
					variable = last_message.substring(start, end);
					display.print_line( i/2 + 1, value + variable);
				} else
					value = last_message.substring(start, end) + " ";
				start = end + 1;
			}
		}
		last_display_time = millis();
	}

  // make periodic request to M800
  if ( (millis() - last_request_time) > REQUEST_INTERVAL || millis() < last_request_time) {
    if (!message_received) {
      Serial.println("WARNING: still waiting for answer to previous request");
      request_timeout++;
      if (request_timeout == TIMEOUT_COUNTER) {
        Serial.println("TIMEOUT: giving up on previous request and re-issuing new one");
        message_received = true; // repeat request if unanswered after 5x the request_interval
        last_message = "";
        display.show_message("M800", "TIMEOUT");
      }

    } else {
      Serial.print("Requesting data from M800... ");

      // make request
      digitalWrite(RS485TxControl, RS485Transmit);
      Serial1.println("D00Z"); // send request signal to M800
      delay(6); // transmit the data before switching back to receive
      digitalWrite(RS485TxControl, RS485Receive);
      message_received = false;
      message_error = false;
      variable_counter = 0;
      request_timeout = 0;
      temp_message = "";
    }
    last_request_time = millis();
  }


  /*** receiving data from M800 ***/
  while (Serial1.available()) {
    byte c = Serial1.read();

    if (c == 0) {
      // do nothing when 0 character encountered
    } else if (c == DATA_DELIM) {
      // end of value or variable
      if (variable_counter == 0)
        temp_message += " "; // space between date and time
      else
        temp_message += (char) DATA_SEP;
      variable_counter++; // first real value/variable pair found
    } else if (c >= 32 && c <= 126) {
      // only consider regular ASCII range
      temp_message += (char) c;
    } else if (c == DATA_END) {
      // end of the data transmission
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
      }
    } else {
      // non-ASCII and non-control character!
      message_error = true;
    }
  }


  /*** SAVE DATA ***/
  if ( ( (millis() - last_save_time) > SAVE_INTERVAL || millis() < last_save_time) && last_message.length() > 0) {

    #ifdef SD_LOG_ON
      if (log_to_SD()) {
        Serial.println("SUCCESS");
        display.show_message("SD log", "saved");
      } else {
        Serial.println("FAILED");
        display.show_message("SD log", "ERROR");
      }
    #endif

		#ifdef WEB_LOG_ON
			if (log_to_web()) {
				Serial.println("SUCCESS");
				display.show_message("Web log", "saved");
			} else {
				Serial.println("FAILED");
				display.show_message("Web log", "ERROR");
			}
		#endif

		last_save_time = millis();
	}

}
