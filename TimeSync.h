#pragma once

class TimeSync {

  private:

    unsigned long sync_period = 24 * 60 * 60 * 1000; // one day by default
    unsigned long last_sync = millis();

  public:

    // constructor
    TimeSync () {};
    void init(); // to be run during setup()
    void update(); // to be run during loop()

};

void TimeSync::init() {
  last_sync = millis();
}

void TimeSync::update() {
  // time sync
  if ( (millis() - last_sync) > sync_period && Particle.connected() && !Particle.syncTimePending()) {
      // Request time synchronization from the Particle Cloud
      Particle.syncTime();
      last_sync = millis();
      Serial.println("INFO: time sync complete");
  }
}
