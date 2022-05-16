#include "Saw.h"
#include "math.h"

void Saw::setup(float fs) {
  add_ = 0.0;
  mul_ = 1.f / (float)M_PI;
  invSampleRate_ = 1.0 / fs;
  phase_ = M_PI;
}

float Saw::process(float frequency) {
  frequency_ = frequency;
  return process();
}

void Saw::process(int n, float *buf) {
  for (unsigned int i = 0; i < n; i++) {
    float val = process();
    buf[0][i] = val * pan;
    buf[1][i] = val * (1.0 - pan);
  }
}

float Saw::process() {
  phase_ += phaseinc_;
  if (phase_ >= M_PI)
    phase_ -= 2.0f * (float)M_PI;
  // TODO add filter
  return phase_ * mul_;
}
