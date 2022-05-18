#include "Saw.h"
#include "math.h"

void Saw::setup(float fs) {
    fs_ = fs;
    mul_ = 1.f / (float)M_PI;
    invSampleRate_ = 1.0 / fs;
    phase_ = M_PI;
    Biquad::Settings settings{
        .fs = fs,
        .cutoff = 20000.0,
        .type = Biquad::lowpass,
        .q = 0.707,
        .peakGainDb = 2,
    };
    lpFilter.setup(settings);
    // Set ADSR parameters
    env_.setAttackRate(1 * fs);
    env_.setDecayRate(8 * fs);
    env_.setReleaseRate(1.0 * fs);
    env_.setSustainLevel(0.01);
    gate(true);
}

void Saw::process(int n, float* buf[2]) {
    for (unsigned int i = 0; i < n; i++) {
        float val = process();
        buf[0][i] += val * pan_;
        buf[1][i] += val * (1.0 - pan_);
    }
}

float Saw::process() {
    phase_ += phaseinc_;
    if (phase_ >= M_PI)
        phase_ -= 2.0f * (float)M_PI;
    return lpFilter.process(phase_ * mul_);
}
