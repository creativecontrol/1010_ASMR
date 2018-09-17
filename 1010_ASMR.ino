/*
 * 1010 Euroshield ASMR 
 * Ambient Spectrum Multiosc Resonator
 * inspired by the 4ms Spectral Multiband Resonator
 * 
 * Author : t@creativecontrol.cc
 * Last revision : 2018.09.16
 * Processor : Teensy 3.2
 * USB Type : Serial + MIDI + Audio
 * Status : Tested, needs work on input scaling
 * 
 * The ASMR consists of 8 oscillators. The user sets: 
 * - the scale type (major, minor, chormatic, or pentatonic)
 * - starting note (Chromatic notes from C-B. Starting octave based on midiOffset variable.)
 * - the space between each of the notes (from adjacent to the scale length)
 * - the rotation of the oscillator group (Offsets the starting note and all other oscillators relative to it. 
 *    Wraps around the entire group of scale notes.)
 * 
 * Knob 1 - Start Note
 * Knob 2 - Spread
 * Button - Rotate once in the positive direction
 * CV 1 - Rotate
 * CV 2 - Scale
 * 
 */

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

// GUItool: begin automatically generated code
AudioInputI2S            i2s1;           //xy=143.3333282470703,106.33333396911621
AudioAnalyzePeak         cvIn1;          //xy=315.3333282470703,90.33333396911621
AudioAnalyzePeak         cvIn2;          //xy=315.3333282470703,125.33333396911621
AudioSynthWaveformSine   sine2;          //xy=553.3333358764648,131.666672706604
AudioSynthWaveformSine   sine4;          //xy=553.3333587646484,214.99999237060547
AudioSynthWaveformSine   sine1;          //xy=555.0000305175781,86.66669464111328
AudioSynthWaveformSine   sine3;          //xy=554.9999847412109,173.33334922790527
AudioMixer4              mixer1;         //xy=745.0000305175781,103.33332443237305
AudioOutputI2S           i2s2;           //xy=905,104.99999809265137
AudioConnection          patchCord1(i2s1, 0, cvIn1, 0);
AudioConnection          patchCord2(i2s1, 1, cvIn2, 0);
AudioConnection          patchCord3(sine2, 0, mixer1, 1);
AudioConnection          patchCord4(sine4, 0, mixer1, 3);
AudioConnection          patchCord5(sine1, 0, mixer1, 0);
AudioConnection          patchCord6(sine3, 0, mixer1, 2);
AudioConnection          patchCord7(mixer1, 0, i2s2, 0);
AudioConnection          patchCord8(mixer1, 0, i2s2, 1);
AudioControlSGTL5000     sgtl5000_1;     //xy=194.3333282470703,249.3333339691162
// GUItool: end automatically generated code




// Use the 1010VCOCalibration sketch to learn how these values were measured.
#define cvLow_IN1          0.2100
#define cvHigh_IN1         0.7689
#define cvLow_IN2          0.2089
#define cvHigh_IN2         0.7686
#define kC1VFrequency      32.703
#define octave             12

const int upperPotInput = 20;
const int lowerPotInput = 21;
const int potRange  = 1024;

const int buttonInput = 2;
const int topLED = 6;
const int botLED = 3;
const int delayTime = 20;

#define ledPinCount 4
int ledPins[ledPinCount] = { 3, 4, 5, 6 };
int ledPos = 0;


// CV
float cvCenterPoint_IN1 = (cvHigh_IN1 - cvLow_IN1) / 2;
float cvCenterPoint_IN2 = (cvHigh_IN2 - cvLow_IN2) / 2;
float cvDeadZone = 0.1 / 2;  // value must deviate more than 0.05 around the center point

// Scales
const int scaleLength = 24;
const int numScales = 4;
const int major[24] = {0,2,4,5,7,9,11,12,14,16,17,19,21,23,24,26,28,29,31,33,35,36,38,40};
const int minor[24] = {0,2,3,5,7,8,10,12,14,15,17,19,20,22,24,26,27,29,31,32,34,36,38,39};
const int chromatic[24] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23};
const int pentatonic[24] = {0,2,4,7,9,12,14,16,19,21,24,26,28,31,33,36,38,40,43,45,46,48,50,53};

// Musical variables
const int midiOffset = 24;  // C0
const int spreadMin = 1;
int startNote = 0;
int lastStartNote = 0;
int spread = 1;
int lastSpread = 1;
int *scale = (int *) major; 
int rotatePosition = 0;

// Flag to update oscillators 
bool readyForUpdate = false;

/*
 * 
 */
void setup(){
  // Turn off Audio processing.
  AudioNoInterrupts();
  // Memory is required for audio processing.
  // This may be tweaked up to about 195 before running out of memory on the Teensy 3.2.
  AudioMemory(150);

  // Enable the audio shield, select input, and enable output
  sgtl5000_1.enable();
  sgtl5000_1.inputSelect(AUDIO_INPUT_LINEIN);
  sgtl5000_1.volume(0.82);
  sgtl5000_1.adcHighPassFilterDisable();
  sgtl5000_1.lineInLevel(0,0);
  sgtl5000_1.unmuteHeadphone();

  sine1.frequency(0);
  sine1.amplitude(0.5);
  sine2.frequency(0);
  sine2.amplitude(0.5);
  sine3.frequency(0);
  sine3.amplitude(0.5);
  sine4.frequency(0);
  sine4.amplitude(0.5);
  
  updateOsc();

  for (int i = 0; i < ledPinCount; i++)
    pinMode(ledPins[i], OUTPUT);
  pinMode(buttonInput, INPUT);

  // Start audio processing.
  AudioInterrupts();

  
}

float midiToFreq(float note){
  return (440.0 * powf(2.0, (float)(note - 69) * 0.08333333));
}
/*
 *  
 */
float calcFreqFromCV(float cvValue) {
  return pow(2, cvValue) * kC1VFrequency; 
}

/*
 * 
 */
float calCvFromPeakValue(float peakValue, float peakLow, float peakHigh){
  float peakPerOctave = (peakHigh - peakLow) / 3;
  float cvValue = (peakValue - peakLow) / peakPerOctave;
  return cvValue;
}

/*
 * Choose the scale based on cv input.
 */
int * scaleType(int cv) {
  switch (cv){
    case 0:
      Serial.println("pentatonic");
      return (int *) pentatonic;
      break;
    case 1:
      Serial.println("major");
      return (int *) major;
      break;
    case 2:
      Serial.println("minor");
      return (int *) minor;
      break;
    case 3:
      Serial.println("chromatic");
      return (int *) chromatic;
      break;
    default:
      Serial.println("pentatonic");
      return (int *) pentatonic;
      break;
  }
}

void turnOffLEDs() {
  digitalWrite(topLED, LOW);
  digitalWrite(botLED, LOW);
}

/*
 * Update all oscillators via the software control inputs.
 */
void updateOsc() {
  int bass = startNote + midiOffset;

  sine1.frequency(midiToFreq(bass + scale[rotatePosition]));
  sine2.frequency(midiToFreq(bass + scale[((rotatePosition + (spread * 1)) % scaleLength)]));
  sine3.frequency(midiToFreq(bass + scale[((rotatePosition + (spread * 2)) % scaleLength)]));
  sine4.frequency(midiToFreq(bass + scale[((rotatePosition + (spread * 3)) % scaleLength)]));
  

//  Serial.print("bass ");
//  Serial.println(bass);
//  Serial.print("start note ");
//  Serial.print(lastStartNote);
//  Serial.print(" ");
//  Serial.println(startNote);
//  Serial.print("spread ");
//  Serial.println(spread);
//  Serial.print("rotation ");
//  Serial.println(rotatePosition);
//  Serial.println(bass + scale[rotatePosition]);

  turnOffLEDs();
}

/*
 * On each cycle, check the knobs, button, and cvInputs.
 * Update oscillators if necessary.
 */
void loop() {
  // Read knobs controlling Tonic and Spread of oscillators.
  int upperKnob = analogRead(upperPotInput);
  int lowerKnob = analogRead(lowerPotInput);
  
  startNote = map(upperKnob, 0, potRange, 0, octave);
  spread = map(lowerKnob, 0, potRange, 1, scaleLength); // not sure about this value
  
  if(startNote != lastStartNote) {
    lastStartNote = startNote;
    readyForUpdate = true;
  }

  if(spread != lastSpread) {
    lastSpread = spread;
    readyForUpdate = true;
  }

  // Button input reads low when pressed.
  if(digitalRead(buttonInput) == LOW) {
    rotatePosition += 1;
    rotatePosition = rotatePosition % scaleLength;
    readyForUpdate = true;
    digitalWrite(topLED, HIGH);
  }
  
  if (cvIn1.available()) {
    float cvValue1 = cvIn1.read();
    float cvInput1 = calCvFromPeakValue(cvValue1, cvLow_IN1, cvHigh_IN1);
    int rotationTemp = rotatePosition;
    if (cvInput1 < (cvCenterPoint_IN1 - cvDeadZone)){
      //rotationTemp -= 1;
      //digitalWrite(botLED, HIGH);
    } else if (cvInput1 > (cvCenterPoint_IN1 + cvDeadZone)) {
      rotationTemp += 1;
      digitalWrite(topLED, HIGH);
    }
    rotatePosition = rotationTemp % scaleLength;
    
    readyForUpdate = true;
  }

  if (cvIn2.available()) {
    float cvValue2 = cvIn2.read();
//    Serial.println(cvValue2);
    float cvInput2 = calCvFromPeakValue(cvValue2, cvLow_IN2, cvHigh_IN2);
//    Serial.println(cvInput);
    int cvScale = int(map(cvInput2, 0, 3, 0, numScales-1));
//    Serial.println(cvScale);
    //int cvScale = int(map(cvValue2, cvLow_IN2, cvHigh_IN2, 0, numScales - 1));
    scale = scaleType(cvScale); 
    readyForUpdate = true;
  }

  if (readyForUpdate) {
    updateOsc();
    readyForUpdate = false;
    
  }
  
  delay(delayTime);
}

