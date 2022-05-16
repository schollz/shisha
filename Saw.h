#include "Utils.h"
#include <cstdlib>
#include <math.h>

#pragma once

class Saw {
public:
  Saw(){};
  Saw(float fs) { setup(fs); }
  ~Saw(){};

  void setup(float fs);

  float process();
  float process(float frequency);
  void setNote(float note) { setFrequency(midi_to_freq(note)); }
  void setFrequency(float frequency) {
    frequency_ = frequency + cents_away(frequency, detune_);
    phaseinc_ = 2.0f * (float)M_PI * frequency_ * invSampleRate_;
  }
  void setDetune(float detune) {
    detune_ = detune;
    setFrequency(frequency_);
  }
  void setPhase(float phase) { phase_ = phase; }
  void setMul(float mul) { mul_ = mul / M_PI; }
  float getPhase() { return phase_; }
  float getFrequency() { return frequency_; }

private:
  float detune_; // in cents
  float phase_, phaseinc_;
  float frequency_;
  float invSampleRate_;
  float mul_;
};
