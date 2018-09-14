# 1010 ASMR
Ambient Spectrum Multiosc Resonator\
built on the 1010 Euroshield and Teensy 3.2\
inspired by the 4ms Spectral Multiband Resonator 

The ASMR consists of 8 oscillators. The user sets: 
- the scale type (major, minor, chormatic, or pentatonic)
- starting note (Chromatic notes from C-B. Starting octave based on midiOffset variable.)
- the space between each of the notes (from adjacent to the scale length)
- the rotation of the oscillator group (Offsets the starting note and all other oscillators relative to it. 
  Wraps around the entire group of scale notes.) 

## Dependancies
1010 Euroshield: https://1010music.com/product/euroshield1 \
Proto-8 Teensy Audio Libraries: https://github.com/marshalltaylorSFE/Proto-8
