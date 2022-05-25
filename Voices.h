#include "Saw.h"
#include "Utils.h"
#include <cstdlib>
#include <libraries/ADSR/ADSR.h>
#include <libraries/Biquad/Biquad.h>
#include <math.h>

#pragma once
#define MAX_VOICES 12

class NoteEv {
  public:
    NoteEv(){};
    ~NoteEv(){};
    int event_num;
    float note;

  private:
};

class Voices {
  public:
    Voices(){};
    Voices(int num_voices) { setup(num_voices); }
    ~Voices(){};

    void setup(float fs);

    void process(int n, float* buf[2]);
    void setDetune(int i, float detune) { voice[i].setDetune(detune); }
    void setPan(int i, float pan) { voice[i].setPan(pan); }
    void setLPF(float note) {
        for (int i = 0; i < MAX_VOICES; i++) {
            voice[i].setLPF(note);
        }
    }
    void attack(float val) {
        for (int i = 0; i < MAX_VOICES; i++) {
            voice[i].attack(val);
        }
    }
    void decay(float val) {
        for (int i = 0; i < MAX_VOICES; i++) {
            voice[i].decay(val);
        }
    }
    void release(float val) {
        for (int i = 0; i < MAX_VOICES; i++) {
            voice[i].release(val);
        }
    }
    void sustain(float val) {
        for (int i = 0; i < MAX_VOICES; i++) {
            voice[i].sustain(val);
        }
    }
    int oldest();
    void note_on(float note, float velocity);
    void note_off(float note);

  private:
    Saw voice[MAX_VOICES];
    NoteEv ev[MAX_VOICES];
    int event_num;
    ADSR env[2];
    Biquad filter[2];
    bool is_playing;
};
