/*
 * 1010 Euroshield ASMR 
 * Ambient Spectrum Multiosc Resonator
 * inspired by the 4ms Spectral Multiband Resonator
 * 
 * Author : t@creativecontrol.cc
 * Last revision : 2018.09.29
 * Processor : Teensy 3.2
 * USB Type : Serial + MIDI + Audio
 * Status : Changing to using String rather than Oscillator
 * Version: Pluck ASMR
 * 
 * K1 - Start Note
 * K2 - Rotation
 * CV1 - Note Trigger (which of the 24 scale notes -or- which of the eight oscs)
 * CV2 - Spread 
 * Button - Change Scale (scale should be changed to chord types as well M7, m7, M9, m9, etc.)
 * 
 */

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

// GUItool: begin automatically generated code
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

// GUItool: begin automatically generated code
AudioInputI2S            i2s1;           //xy=143.3333282470703,106.33333396911621
AudioAnalyzePeak         cvIn1;          //xy=315.3333282470703,90.33333396911621
AudioAnalyzePeak         cvIn2;          //xy=315.3333282470703,125.33333396911621
AudioSynthKarplusStrong  string1;        //xy=588,84
AudioSynthKarplusStrong  string7;        //xy=587,336
AudioSynthKarplusStrong  string2;        //xy=588,122
AudioSynthKarplusStrong  string5;        //xy=588,249
AudioSynthKarplusStrong  string6;        //xy=588,292
AudioSynthKarplusStrong  string8;        //xy=588,377
AudioSynthKarplusStrong  string3;        //xy=589,161
AudioSynthKarplusStrong  string4;        //xy=589,201
AudioMixer4              mixer1;         //xy=745.0000305175781,103.33332443237305
AudioMixer4              mixer2;         //xy=771,293
AudioMixer4              mixer3;         //xy=958,297
AudioOutputI2S           i2s2;           //xy=1098,297
AudioConnection          patchCord1(i2s1, 0, cvIn1, 0);
AudioConnection          patchCord2(i2s1, 1, cvIn2, 0);
AudioConnection          patchCord3(string1, 0, mixer1, 0);
AudioConnection          patchCord4(string7, 0, mixer2, 2);
AudioConnection          patchCord5(string2, 0, mixer1, 1);
AudioConnection          patchCord6(string5, 0, mixer2, 0);
AudioConnection          patchCord7(string6, 0, mixer2, 1);
AudioConnection          patchCord8(string8, 0, mixer2, 3);
AudioConnection          patchCord9(string3, 0, mixer1, 2);
AudioConnection          patchCord10(string4, 0, mixer1, 3);
AudioConnection          patchCord11(mixer1, 0, mixer3, 0);
AudioConnection          patchCord12(mixer2, 0, mixer3, 1);
AudioConnection          patchCord13(mixer3, 0, i2s2, 0);
AudioConnection          patchCord14(mixer3, 0, i2s2, 1);
AudioControlSGTL5000     sgtl5000_1;     //xy=194.3333282470703,249.3333339691162
// GUItool: end automatically generated code


// Use the 1010VCOCalibration sketch to learn how these values were measured.
#define cvLow_IN1          0.2100
#define cvHigh_IN1         0.7689
#define cvLow_IN2          0.2089
#define cvHigh_IN2         0.7686
#define kC1VFrequency      32.703
#define octave             12
#define velocity           0.8
#define numStrings         8

const int upperPotInput = 20;
const int lowerPotInput = 21;
const int potRange  = 1024;

const int buttonInput = 2;
const int topLED = 6;
const int botLED = 3;
const int delayTime = 50;

#define ledPinCount 4
int ledPins[ledPinCount] = { 3, 4, 5, 6 };
int ledPos = 0;


// CV
float cvCenterPoint_IN1 = (cvHigh_IN1 - cvLow_IN1) / 2;
float cvCenterPoint_IN2 = (cvHigh_IN2 - cvLow_IN2) / 2;
float cvDeadZone = 0.1 / 2;  // value must deviate more than 0.05 around the center point
float cvRangeLow = 0.0;
float cvRangeHigh = 3.52;

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
int *scale = (int *) pentatonic; 
int rotatePosition = 0;
int lastRotation = 0;
int string = 0;
int chosenScale = 0;

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

//  sine1.frequency(0);
//  sine1.amplitude(0.5);
//  sine2.frequency(0);
//  sine2.amplitude(0.5);
//  sine3.frequency(0);
//  sine3.amplitude(0.5);
//  sine4.frequency(0);
//  sine4.amplitude(0.5);

  string1.noteOn(0, 0);
  string2.noteOn(0, 0);
  string3.noteOn(0, 0);
  string4.noteOn(0, 0);
  string5.noteOn(0, 0);
  string6.noteOn(0, 0);
  string7.noteOn(0, 0);
  string8.noteOn(0, 0);
  
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

//  sine1.frequency(midiToFreq(bass + scale[rotatePosition]));
//  sine2.frequency(midiToFreq(bass + scale[((rotatePosition + (spread * 1)) % scaleLength)]));
//  sine3.frequency(midiToFreq(bass + scale[((rotatePosition + (spread * 2)) % scaleLength)]));
//  sine4.frequency(midiToFreq(bass + scale[((rotatePosition + (spread * 3)) % scaleLength)]));

  switch(string){
    case 0:
      break;
    case 1:
      string1.noteOn(midiToFreq(bass + scale[rotatePosition]), velocity);
      break;
    case 2:
      string2.noteOn(midiToFreq(bass + scale[((rotatePosition + (spread * 1)) % scaleLength)]), velocity);
      break;
    case 3:
      string3.noteOn(midiToFreq(bass + scale[((rotatePosition + (spread * 2)) % scaleLength)]), velocity);
      break;
    case 4:
      string4.noteOn(midiToFreq(bass + scale[((rotatePosition + (spread * 3)) % scaleLength)]), velocity);
      break;
    case 5:
      string5.noteOn(midiToFreq(bass + scale[((rotatePosition + (spread * 4)) % scaleLength)]), velocity);
      break;
    case 6:
      string6.noteOn(midiToFreq(bass + scale[((rotatePosition + (spread * 5)) % scaleLength)]), velocity);
      break;
    case 7:
      string7.noteOn(midiToFreq(bass + scale[((rotatePosition + (spread * 6)) % scaleLength)]), velocity);
      break;
    case 8:
      string8.noteOn(midiToFreq(bass + scale[((rotatePosition + (spread * 7)) % scaleLength)]), velocity);
      break;
  }

  Serial.print("bass ");
  Serial.println(bass);
  Serial.print("start note ");
  Serial.print(lastStartNote);
  Serial.print(" ");
  Serial.println(startNote);
  Serial.println(midiToFreq(bass + scale[rotatePosition]));
  Serial.println(midiToFreq(bass + scale[((rotatePosition + (spread * 1)) % scaleLength)]));
  Serial.println(midiToFreq(bass + scale[((rotatePosition + (spread * 2)) % scaleLength)]));
  Serial.println(midiToFreq(bass + scale[((rotatePosition + (spread * 3)) % scaleLength)]));
  Serial.println(midiToFreq(bass + scale[((rotatePosition + (spread * 4)) % scaleLength)]));
  Serial.println(midiToFreq(bass + scale[((rotatePosition + (spread * 5)) % scaleLength)]));
  Serial.println(midiToFreq(bass + scale[((rotatePosition + (spread * 6)) % scaleLength)]));
  Serial.println(midiToFreq(bass + scale[((rotatePosition + (spread * 7)) % scaleLength)]));
  
  Serial.print("string ");
  Serial.println(string);
  Serial.print("spread ");
  Serial.println(spread);
  Serial.print("rotation ");
  Serial.println(rotatePosition);
  Serial.println(bass + scale[rotatePosition]);

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
  rotatePosition = map(lowerKnob, 0, potRange, 1, scaleLength); 

  // Tonic
  if(startNote != lastStartNote) {
    lastStartNote = startNote;
    //readyForUpdate = true;
  }

  // Rotation
  if(rotatePosition != lastRotation) {
    lastRotation = rotatePosition;
    //readyForUpdate = true;
  }

  // Button input reads low when pressed.
  if(digitalRead(buttonInput) == LOW) {
    chosenScale += 1;
    chosenScale = chosenScale % numScales;
    scale = scaleType(chosenScale) ;
    //readyForUpdate = true;
    digitalWrite(topLED, HIGH);
  }

  // cv1 defines which of the 4 strings is plucked
  if (cvIn1.available()) {
    float cvValue1 = cvIn1.read();
    float cvInput1 = calCvFromPeakValue(cvValue1, cvLow_IN1, cvHigh_IN1);

//    Serial.print("cv Input: ");
//    Serial.println(cvInput1);
  
    string = map(cvInput1, cvRangeLow, cvRangeHigh, 0, numStrings );

//    Serial.print("string select: ");
//    Serial.println(string);

    if ((string > 0) && (readyForUpdate == false)){
      digitalWrite(botLED, HIGH);
      readyForUpdate = true;
    }
  }

//spread
  if (cvIn2.available()) {
    float cvValue2 = cvIn2.read();
//    Serial.println(cvValue2);
    float cvInput2 = calCvFromPeakValue(cvValue2, cvLow_IN2, cvHigh_IN2);
//    Serial.println(cvInput);
    int newSpread = int(map(cvInput2, cvRangeLow, cvRangeHigh, 0, scaleLength));

    
    if (newSpread != lastSpread){
      spread = newSpread;
      lastSpread = spread;
      
    }
    //readyForUpdate = true;
  }

  if (readyForUpdate) {
    AudioNoInterrupts();
    updateOsc();
    AudioInterrupts();
    readyForUpdate = false;
  }
  
  delay(delayTime);
  turnOffLEDs();
}

