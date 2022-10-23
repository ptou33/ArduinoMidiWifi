/*
 * Sustain analog pedal, 3 CC, Damper switch pedal
 * Pietro Tou
 * 2022_10_23
 * 
 * This project uses a Leonardo Arduino, because it has USB MIDI programmability
 */ 

#include "MIDIUSB.h"

//SENSOR CONFIG
const bool DEBOUNCE = true;        // used to debounce of midi message (eg: 80 82 80 last 80 is not sent)
const unsigned long MIN_IDLE_MS = 10;     // at least 10 ms before sending next Midi message, too many are not useful
const int MAX_A = 910;             // analog (A0-A3) sensor range in Leonardo should give maximum value 1000, but it does not reach 928 or less

//MIDI CC VALUES
#define SUSTAIN_CC      64;
#define SOFT_CC         67;
#define CLOSE_VOL_CC    16;
#define AMBIENT_VOL_CC  17;

//MIDI CONFIG
const int CHANNEL = 2;               // channel used to send messages, values are MIDI CH 1-16
const int PED_CC = SUSTAIN_CC;       // ANALOG JACK for sustain pedal
const int LEFT_CC = AMBIENT_VOL_CC;  // RIGHT KNOB (inverted)
const int MID_CC = CLOSE_VOL_CC;     // MIDDLE KNOB (inverted)
const int RIGHT_CC = SOFT_CC;        // LEFT KNOB (inverted)
const int DIGI_CC = SOFT_CC;         // DIGITAL JACK for soft pedal

// CC for Garritan https://usermanuals.garritan.com/CFXConcertGrand/Content/midi_controls_automation.htm
/*
Control                     MIDI CC number
  Master Volume             7
  Master Pan                10
  Close Stereo Width        14
  Ambient Stereo Width      15
Close Volume                16
Ambient Volume              17
  Close EQ Lo gain          22
  Close EQ Mid gain         23
  Close EQ Hi gain          24
  Close EQ on/off           25 (On from 1-64, and off from 65-127)
  Close EQ Mid frequency    26
  Ambient EQ Lo gain        27
  Ambient EQ Mid gain       28
  Ambient EQ Hi gain        29
  Ambient EQ on/off         30 (On from 1-64, and off from 65-127)
  Ambient EQ Mid frequency  31
Sustain Pedal on/off        64 (On from 1-127) GUI pedal light turns on at 64
  Sostenuto Pedal on/off    66 (On from 1-127) GUI pedal light turns on at 64
Soft Pedal on/off           67 (Off from 0-63, on from 64-127)
  Pedal Noise               76
  Sustain Resonance         84
  Sustain Resonance Bypass  85
  Sympathetic Resonance     87
  Reverb Send Level         91
  Saturation                93
  Stretch Tuning            100
  Room/Release Volume       256
  Room/Release Decay        257
  Release Crossfade         258
  Dynamic Range             259
  Close Mute                268
  Ambient Mute              269
  Stereo Image              270
  Limiter on/off            292
  Ambient Pre-Delay         300
  Timbre Effect             489
*/

void noteOn(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOn = {0x09, 0x90 | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOn);
}

void noteOff(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOff = {0x08, 0x80 | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOff);
}

void controlChange(byte channel, byte control, byte value) {
  midiEventPacket_t event = {0x0B, 0xB0 | channel, control, value};
  // First parameter is the event type (0x0B = control change).
  // Second parameter is the event type, combined with the channel.
  // Third parameter is the control number number (0-119).
  // Fourth parameter is the control value (0-127).
  MidiUSB.sendMIDI(event);
}


// -----------------------------------------------------------------------------
// Analog Sensor
// -----------------------------------------------------------------------------
class Sensor {
  byte _sensor;
  int _max_sensor;
  int _midi_cc;
  int _channel;
  bool _invert;
  unsigned long _last_midi_millis = 0;
  int _last_midivalue = 0;
  int _prev_midivalue = 0;

  public:
  Sensor(byte sensor, int max_sensor, int user_channel, int midi_cc, bool invert=false) {
    _sensor = sensor;
    _max_sensor = max_sensor;
    _midi_cc = midi_cc;
    _channel = user_channel - 1;  // send 0-15 as channel. Typically reported to the user as MIDI CH 1-16
    _invert = invert;             // true to map 0-127 to 127-0
  }

  void manage_sensor(bool debug) {
    unsigned long now = millis();
    if (now - _last_midi_millis <= MIN_IDLE_MS) {                     // At least _min_idle ms before sending next Midi message
      return;
    }
    int sensorValue = analogRead(_sensor);
    int midivalue = min(1.0 * sensorValue/_max_sensor * 127, 127);    // Scale sensor reading to 0-127
    if (_invert){
      midivalue = 127 - midivalue;                                    // Invert value if sensor is mounted in reverse
    }    
    if ( (midivalue == _last_midivalue) ||                            // Skip if midi value not changed, even if sensorValue was changed
      (DEBOUNCE == true && midivalue == _prev_midivalue)) {           // Avoid also simple bounce back of MIDI message. eg: 80 82 80, last 80 is not sent. Allows smooth monotone ramps.
      return;
    }
    _prev_midivalue = _last_midivalue;
    _last_midivalue = midivalue;
    controlChange(_channel, _midi_cc, midivalue);
    MidiUSB.flush();                                                  //send all the data!
    _last_midi_millis = now;
    
    if (debug) {
      Serial.println(sensorValue);
      //Serial.println(midivalue);
    }
  }
};


// -----------------------------------------------------------------------------
// Button
// -----------------------------------------------------------------------------
class Button {
  int _pin;
  int _midi_cc;
  int _channel;
  int _read_pressed = HIGH;         // LOW for internal buttons
  bool _last_button_state = false;  // true = pressed
  unsigned long _last_midi_millis = 0;
  
  public:
  Button(int inputPin, int channel, int midi_cc) {
    _pin = inputPin;
    _channel = channel;
    _midi_cc = midi_cc;
    pinMode(_pin, INPUT_PULLUP);    // INPUT or INPUT_PULLUP (without pullup resistor)
    if (_pin == 0) { 
      _read_pressed = LOW;          // internal button pressed for ESP8266 is LOW
    }
  }

  void manage_button(bool debug) {
    unsigned long now = millis();
    if (now - _last_midi_millis <= MIN_IDLE_MS) {  // At least _min_idle ms before sending next Midi message
      return;
    }
    bool current_button_state = false;
    int current_read = digitalRead(_pin);
    if (current_read == _read_pressed) {
      current_button_state = true;
    }
    if (current_button_state == _last_button_state){
      return;
    }
    _last_button_state = current_button_state;
    if (current_button_state) {
      controlChange(_channel, _midi_cc, 127); 
    } else {
      controlChange(_channel, _midi_cc, 0); 
    }
    MidiUSB.flush();                             //send all the data!
    _last_midi_millis = now;

    if (debug) {
      Serial.println(current_button_state);
    }
  }
};



// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------
Sensor s_a3(A3, MAX_A, CHANNEL, PED_CC);
Sensor s_a2(A2, MAX_A, CHANNEL, RIGHT_CC, true); 
Sensor s_a1(A1, MAX_A, CHANNEL, MID_CC, true);
Sensor s_a0(A0, MAX_A, CHANNEL, LEFT_CC, true);

Button b_10(10, CHANNEL, DIGI_CC);

void setup() {            //needed to call methods once
  Serial.begin(115200);
}

// -----------------------------------------------------------------------------
// Loop
// -----------------------------------------------------------------------------
void loop() {
  s_a0.manage_sensor(false);
  s_a1.manage_sensor(false);
  s_a2.manage_sensor(false);
  s_a3.manage_sensor(false);    //sustain
  
  b_10.manage_button(false);
}
