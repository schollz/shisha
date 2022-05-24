#include "Utils.h"
#include <cstdlib>
#include <libraries/ADSR/ADSR.h>
#include <libraries/Biquad/Biquad.h>
#include <math.h>

#pragma once

class Saw {
  public:
    Saw(){};
    Saw(float fs) { setup(fs); }
    ~Saw(){};

    void setup(float fs);

    float process();
    void process(int n, float* buf[2]);
    void setNote(float note) {
        note_ = note;
        phaseinc_ =
            2.0f * (float)M_PI * midi_to_freq(note_ + detune_) * invSampleRate_;
    }
    // setDetune sets the detune in midi notes
    void setDetune(float detune) {
        detune_ = detune;
        setNote(note_);
    }
    void setPan(float pan) {
        // convert [-1,1] to [0,1]
        pan_ = (pan + 1.0) / 2.0;
    }
    void setLPF(float note) { lpFilter.setFc(midi_to_freq(note)); };
    void setPhase(float phase) { phase_ = phase; }
    void setMul(float mul) { mul_ = mul / M_PI; }
    float getPhase() { return phase_; }
    float getFrequency() { return frequency_; }
    void attack(float val) { env_.setAttackRate(val * fs_); }
    void decay(float val) { env_.setDecayRate(val * fs_); }
    void release(float val) { env_.setReleaseRate(val * fs_); }
    void sustain(float val) { env_.setSustainLevel(val); }
    void gate(bool val) {
        gate_ = val;
        env_.gate(val);
        env_.process();
    }
    void toggle() { gate(!gate_); }
    bool playing() { return gate_; }

  private:
    ADSR env_;
    Biquad lpFilter;
    bool gate_;
    float fs_;
    float detune_; // in cents
    float phase_, phaseinc_;
    float frequency_, note_;
    float invSampleRate_;
    float mul_;
    float pan_;
};
