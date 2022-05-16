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

float Saw::process() {
  phase_ += phaseinc_;
  if (phase_ >= M_PI)
    phase_ -= 2.0f * (float)M_PI;

  return phase_ * mul_;
}
