/*
 * Pianoteq controls
 * Pietro Tou
 */ 

#include "MIDIUSB.h"

void noteOn(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOn = {0x09, 0x90 | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOn);
}

void noteOff(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOff = {0x08, 0x80 | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOff);
}


// First parameter is the event type (0x0B = control change).
// Second parameter is the event type, combined with the channel.
// Third parameter is the control number number (0-119).
// Fourth parameter is the control value (0-127).

void controlChange(byte channel, byte control, byte value) {
  midiEventPacket_t event = {0x0B, 0xB0 | channel, control, value};
  MidiUSB.sendMIDI(event);
}


static bool NO_DEBOUNCE=0;

class Sensor {
  public:
  byte _sensor;
  int _max_sensor;
  int _midi_cc;
  int _midi_cc2;
  int _channel;
  bool _invert;
  unsigned long _min_idle;
  unsigned long last_midi_millis=0;
  int lastsensorvalue=0;
  int last_midivalue=0;
  int prev_midivalue=0;
  
  Sensor(byte sensor, int max_sensor, int channel, int midi_cc, bool invert=0,int midi_cc2=-1, unsigned long min_idle=10) {
    _sensor = sensor;
    _max_sensor = max_sensor;
    _midi_cc = midi_cc;
    _channel = channel-1;
    _invert = invert;
    _midi_cc2 = midi_cc2;
    _min_idle = min_idle;
  };

  void manage_sensor(){
    int sensorValue=analogRead(_sensor);
    
    //if(sensorValue-lastsensorvalue != 0){
    if(abs(sensorValue-lastsensorvalue) >= 8){//ignore if sensor change is too small
      int midivalue = min(1.0* sensorValue / _max_sensor * 127, 127);
      if (_invert == 1){
        midivalue = 127-midivalue;
      }
      if (midivalue != last_midivalue && (NO_DEBOUNCE || midivalue != prev_midivalue) ) { //smooth midi + avoid simple bounce
        unsigned long now = millis();
        if (now - last_midi_millis <= _min_idle) {
          //Serial.println("skip");  
          return;
        }
        lastsensorvalue=sensorValue;
        prev_midivalue = last_midivalue;
        last_midivalue = midivalue;
        controlChange(_channel, _midi_cc, midivalue);
        if (_midi_cc2 != -1) {
          controlChange(_channel, _midi_cc2, midivalue); 
        }
        //Serial.println(sensorValue);
        //Serial.println(midivalue);
        MidiUSB.flush(); //send all the data!
        last_midi_millis = now;
      }
    }  
  };
};

static int MAX_A=910; //928
static int CHANNEL=2; // Typically reported to the user as 1-16. In reality will be sent 0-15. 
Sensor sensor_a3(A3, MAX_A, CHANNEL, 64); //sustain pedal:64
Sensor sensor_a2(A2, MAX_A, CHANNEL, 22, true, 27); //volume:7
Sensor sensor_a1(A1, MAX_A, CHANNEL, 23, true, 28); //output:8, ambient volume 17
Sensor sensor_a0(A0, MAX_A, CHANNEL, 24, true, 29); //key release noise:9, close volume 16

void setup()
{
  Serial.begin(115200);
}

void loop() {
   sensor_a0.manage_sensor();
   sensor_a1.manage_sensor();
   sensor_a2.manage_sensor();
   sensor_a3.manage_sensor();
}
