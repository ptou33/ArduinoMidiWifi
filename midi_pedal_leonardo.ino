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
  int _channel;
  bool _invert;
  int lastsensorvalue=0;
  int last_midivalue=0;
  int prev_midivalue=0;
  
  Sensor(byte sensor, int max_sensor, int channel, int midi_cc, bool invert=0) {
    _sensor = sensor;
    _max_sensor = max_sensor;
    _midi_cc = midi_cc;
    _channel = channel-1;
    _invert = invert;
  };

  int manage_sensor(){
    int sensorValue=analogRead(_sensor);
    
    //if(sensorValue-lastsensorvalue != 0){
    if(abs(sensorValue-lastsensorvalue) >= 8){//ignore if sensor change is too small
      lastsensorvalue=sensorValue;
      int midivalue = min(1.0* sensorValue / _max_sensor * 127, 127);
      if (_invert == 1){
        midivalue = 127-midivalue;
      }
      if (midivalue != last_midivalue && (NO_DEBOUNCE || midivalue != prev_midivalue) ) { //smooth midi + avoid simple bounce
        prev_midivalue = last_midivalue;
        last_midivalue = midivalue;
        controlChange(_channel, _midi_cc, midivalue); 
        Serial.println(sensorValue);
        //Serial.print(midivalue);
        MidiUSB.flush(); //send all the data!
      }
    }  
  };
};

static int MAX_A=928;
static int CHANNEL=2; // Typically reported to the user as 1-16. In reality will be sent 0-15. 
Sensor sensor_a3(A3, MAX_A, CHANNEL, 64); //sustain pedal:64
Sensor sensor_a2(A2, MAX_A, CHANNEL, 7, true); //volume:7
Sensor sensor_a1(A1, MAX_A, CHANNEL, 8, true); //output:8
Sensor sensor_a0(A0, MAX_A, CHANNEL, 9, true); //key release noise:9

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
