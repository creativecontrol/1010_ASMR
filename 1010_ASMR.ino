/*
 * 1010 Euroshield ASMR 
 * Ambient Spectrum Multiosc Resonator
 * inspired by the 4ms Spectral Multiband Resonator
 * 
 * Author : t@creativecontrol.cc
 * Last revision : 2018.09.13
 * Processor : Teensy 3.2
 * USB Type : Serial + MIDI + Audio
 * Status : Untested Draft
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

// TODO : run calibration routine and update cv colibration values

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

#include "synth_dc_binary.h"
#include "synth_multiosc.h"

// GUItool: begin automatically generated code
AudioInputI2S              i2s2;           //xy=138,242
AudioSynthWaveformDcBinary controlIn1;     //xy=169,594
AudioSynthWaveformDcBinary controlIn2;     //xy=169,681
AudioSynthWaveformDcBinary controlIn3;     //xy=170,638
AudioSynthWaveformDcBinary controlIn4;     //xy=170,730
AudioSynthWaveformDcBinary controlIn5;     //xy=199,400
AudioSynthWaveformDcBinary controlIn6;     //xy=199,474
AudioSynthWaveformDcBinary controlIn7;     //xy=200,438
AudioSynthWaveformDcBinary controlIn8;     //xy=200,515
AudioSynthMultiOsc         multiosc1;      //xy=356,400
AudioSynthMultiOsc         multiosc2;      //xy=356,590
AudioAnalyzePeak           cvIn1;          //xy=366,230
AudioAnalyzePeak           cvIn2;          //xy=374,270
AudioMixer4                mixer1;         //xy=530,470
AudioOutputI2S             i2s1;           //xy=679,476
AudioConnection            patchCord1(i2s2, 0, cvIn1, 0);
AudioConnection            patchCord2(i2s2, 1, cvIn2, 0);
AudioConnection            patchCord3(controlIn1, 0, multiosc1, 5);
AudioConnection            patchCord4(controlIn2, 0, multiosc1, 6);
AudioConnection            patchCord5(controlIn3, 0, multiosc1, 7);
AudioConnection            patchCord6(controlIn4, 0, multiosc1, 8);
AudioConnection            patchCord7(controlIn5, 0, multiosc2, 5);
AudioConnection            patchCord8(controlIn6, 0, multiosc2, 6);
AudioConnection            patchCord9(controlIn7, 0, multiosc2, 7);
AudioConnection            patchCord10(controlIn8, 0, multiosc2, 8);
AudioConnection            patchCord11(multiosc1, 0, mixer1, 0);
AudioConnection            patchCord12(multiosc2, 0, mixer1, 1);
AudioConnection            patchCord13(mixer1, 0, i2s1, 0);
AudioControlSGTL5000       sgtl5000_1;     //xy=381,791
// GUItool: end automatically generated code

// Use the 1010VCOCalibration sketch to learn how these values were measured.
#define cvLow_IN1          0.2088
#define cvHigh_IN1         0.7627
#define cvLow_IN2          0.2092
#define cvHigh_IN2         0.7649
#define kC1VFrequency      32.703
#define octave             12

const int upperPotInput = 20;
const int lowerPotInput = 21;
const int potRange  = 1024;

const int buttonInput = 2;
const int topLED = 6;
const int botLED = 3;
const int delayTime = 2;

// CV
float cvCenterPoint_IN1 = (cvHigh_IN1 - cvLow_IN1) / 2;
float cvCenterPoint_IN2 = (cvHigh_IN2 - cvLow_IN2) / 2;
float cvDeadZone = 0.05 / 2;  // value must deviate more than 0.05 around the center point

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
  AudioMemory(175);

  // Enable the audio shield, select input, and enable output
  sgtl5000_1.enable();
  sgtl5000_1.inputSelect(AUDIO_INPUT_LINEIN);
  sgtl5000_1.volume(0.82);
  sgtl5000_1.adcHighPassFilterDisable();
  sgtl5000_1.lineInLevel(0,0);
  sgtl5000_1.unmuteHeadphone();

  // Setup the Proto-8 multiosc object with standard waveform and full volume.
  multiosc1.amplitude(0, 255);
  multiosc2.amplitude(0, 255);

  multiosc1.begin();
  multiosc2.begin();

  // Start audio processing.
  AudioInterrupts();
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
      return (int *) major;
      break;
    case 1:
      return (int *) minor;
      break;
    case 2:
      return (int *) chromatic;
      break;
    case 3:
      return (int *) pentatonic;
      break;
    default:
      return (int *) major;
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
  controlIn1.amplitude_midi_key(bass + scale[rotatePosition]);
  controlIn2.amplitude_midi_key(bass + scale[((rotatePosition + (spread * 1)) % scaleLength)]);
  controlIn3.amplitude_midi_key(bass + scale[((rotatePosition + (spread * 2)) % scaleLength)]);
  controlIn4.amplitude_midi_key(bass + scale[((rotatePosition + (spread * 3)) % scaleLength)]);
  controlIn5.amplitude_midi_key(bass + scale[((rotatePosition + (spread * 4)) % scaleLength)]);
  controlIn6.amplitude_midi_key(bass + scale[((rotatePosition + (spread * 5)) % scaleLength)]);
  controlIn7.amplitude_midi_key(bass + scale[((rotatePosition + (spread * 6)) % scaleLength)]);
  controlIn8.amplitude_midi_key(bass + scale[((rotatePosition + (spread * 7)) % scaleLength)]);
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
    rotatePosition += rotatePosition;
    rotatePosition = rotatePosition % scaleLength;
    readyForUpdate = true;
    digitalWrite(topLED, HIGH);
  }
  
  if (cvIn1.available()) {
    float cvValue1 = cvIn1.read();
    float cvInput = calCvFromPeakValue(cvValue1, cvLow_IN1, cvHigh_IN1);
    if (cvInput < (cvCenterPoint_IN1 - cvDeadZone)){
      rotatePosition -= rotatePosition;
      digitalWrite(botLED, HIGH);
    } else if (cvInput > (cvCenterPoint_IN1 + cvDeadZone)) {
      rotatePosition += rotatePosition;
      digitalWrite(topLED, HIGH);
    }
    rotatePosition = rotatePosition % scaleLength;
    
    readyForUpdate = true;
  }

  if (cvIn2.available()) {
    float cvValue2 = cvIn2.read();
    float cvInput = calCvFromPeakValue(cvValue2, cvLow_IN2, cvHigh_IN2);
    int cvScale = int(map(cvInput, cvLow_IN2, cvHigh_IN2, 0, numScales - 1));
    scale = scaleType(cvScale); 
    readyForUpdate = true;
  }

  if (readyForUpdate) {
    updateOsc();
    readyForUpdate = false;
    
  }
  
  delay(delayTime);
}

