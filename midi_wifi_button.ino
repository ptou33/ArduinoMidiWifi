#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>

#define SerialMon Serial
#define APPLEMIDI_DEBUG SerialMon
#include <AppleMIDI.h>
APPLEMIDI_CREATE_DEFAULTSESSION_INSTANCE();

// constants
const char ssid[] = "FASTWEB-2.4GHZ";   // network SSID (name)
const char pass[] = "pietrotou";        // network password (use for WPA, or use as key for WEP)
const int ledPin =  2;                  // 2=internal LED, -1=disable
const int buttonPin = 0;                // 0=internal right button, 5=D1 input, 1=TX input
const int inertiaMillisecs = 50;        // ignore changes if too close, to avoid button bouncing
const bool useSustain = true;            // false=send midi note, true=send sustain
const byte note = 45;
const byte velocity = 55;
const byte channel = 1;

// global variables
uint8_t isConnected = 0;                 // counter of MIDI sessions


// -----------------------------------------------------------------------------
// LED
// -----------------------------------------------------------------------------
class Led {
  public: 
  int _pin;
  Led(int inputPin) {
    _pin = inputPin;
    if (_pin != -1) {
      pinMode(_pin, OUTPUT);  // if not initialized, the led will not work
    }
  };
  void ledOn(){
    DBG(F("Led on"), _pin);
    digitalWrite(_pin, LOW);
  };
  void ledOff(){
    DBG(F("Led off"), _pin);
    digitalWrite(_pin, HIGH);
  };
};
Led myLed = Led(ledPin);


// -----------------------------------------------------------------------------
// Button
// -----------------------------------------------------------------------------
class Button {
  public: 
  int _pin;
  Button(int inputPin, int input_mode = INPUT_PULLUP) {
    _pin = inputPin;
    if (_pin != -1) {
      pinMode(_pin, input_mode);  // INPUT or INPUT_PULLUP
    }
  };
  int read(){
    return digitalRead(_pin);
  };
};
Button myButton = Button(buttonPin);



// -----------------------------------------------------------------------------
// MIDI
// -----------------------------------------------------------------------------
class Midi {
  public:
  bool _sustain;
  byte _channel;
  
  Midi(bool input_sustain = true, byte input_channel = 1){
    _sustain = input_sustain;
    _channel = input_channel;
  };
  void setup_midi(){
    DBG(F("OK, now make sure you an rtpMIDI session that is Enabled"));
    DBG(F("Add device named Arduino with Host"), WiFi.localIP(), "Port", AppleMIDI.getPort(), "(Name", AppleMIDI.getName(), ")");
    DBG(F("Select and then press the Connect button"));
    DBG(F("Then open a MIDI listener and monitor incoming notes"));
    DBG(F("Listen to incoming MIDI commands"));
    DBG(F("LED will shut down when connected to Midi session"));
    MIDI.begin();
    AppleMIDI.setHandleConnected([](const APPLEMIDI_NAMESPACE::ssrc_t & ssrc, const char* name) {
      isConnected++;
      DBG(F("Connected to session"), ssrc, name);
      myLed.ledOff();
    });
    AppleMIDI.setHandleDisconnected([](const APPLEMIDI_NAMESPACE::ssrc_t & ssrc) {
      isConnected--;
      DBG(F("Disconnected"), ssrc);
    });
    MIDI.setHandleNoteOn([](byte channel, byte note, byte velocity) {
      DBG(F("NoteOn"), note);
    });
    MIDI.setHandleNoteOff([](byte channel, byte note, byte velocity) {
      DBG(F("NoteOff"), note);
    });
  };
  void midiOn() {
    switch (_sustain) {
      case 0:
        MIDI.sendNoteOn(note, velocity, _channel);
        break;
      case 1:
        MIDI.sendControlChange(64, 127, _channel);
        break;
    }
  };
  void midiOff() {
    switch (_sustain) {
      case 0:
        MIDI.sendNoteOff(note, velocity, _channel);
        break;
      case 1:
        MIDI.sendControlChange(64, 0, _channel);
        break;
    }
  }
};
Midi myMidi = Midi(useSustain, channel);

// -----------------------------------------------------------------------------
// SETUP
// -----------------------------------------------------------------------------
void setup()
{
  DBG_SETUP(115200);
  DBG(F("Booting"));
  setup_wifi();
  myLed.ledOn();
  myMidi.setup_midi();
}

void setup_wifi()
{
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    DBG(F("Establishing connection to WiFi.."));
  }
  DBG(F("Connected to network with IP:"), WiFi.localIP());
}


// -----------------------------------------------------------------------------
// LOOP
// -----------------------------------------------------------------------------
void loop()
{
  static unsigned long lastChangeTime = millis();
  static unsigned long now = millis();
  static int buttonLastState = digitalRead(buttonPin); // will be reset to off at first loop;
  static int buttonNewState;

  MIDI.read();    // Listen to incoming notes, necessary for connection
  
  buttonNewState = myButton.read();
  if (buttonNewState != buttonLastState) {
    now = millis();
    if (now - lastChangeTime >= inertiaMillisecs) { // ignore if change happened too close
      DBG(F("- duration"), now - lastChangeTime);
      buttonChangedAction(buttonNewState);    
      buttonLastState = buttonNewState;
      lastChangeTime = now;
    } else {
      DBG(F("Ignore elapsed ms"), now - lastChangeTime);
    }    
  }
}


void buttonChangedAction(int buttonNewState)
{
  static int buttonPressed;         // depends on type of button/pedal
  switch (buttonPin) { 
     case 0:
       buttonPressed = LOW;         // internal button
       break;
     default:
       buttonPressed = HIGH;       // HIGH=external switch, Yamaha pedal is inverted  
       break;
  }
  if (buttonNewState == buttonPressed) {
    pressedAction();
  } else {
    releasedAction();
  }
}


void pressedAction()
{
  static int pressCounter = 0;
  pressCounter++;
  DBG("Button press ", pressCounter);
  myMidi.midiOn();
  myLed.ledOn();
}


void releasedAction()
{
  DBG("off");
  myMidi.midiOff();
  myLed.ledOff();
}
