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
    env_.setAttackRate(randfloat(0.5, 1.0) * fs);
    env_.setDecayRate(randfloat(1.0, 2.0) * fs);
    env_.setReleaseRate(randfloat(2, 4) * fs);
    env_.setSustainLevel(randfloat(0.5, 0.6));
    gate_ = false;
}

void Saw::process(int n, float* buf[2]) {
    for (unsigned int i = 0; i < n; i++) {
        float val = process();
        buf[0][i] += val * pan_;
        buf[1][i] += val * (1.0 - pan_);
    }
}

float Saw::process() {
    float env_current = env_.process();
    if (env_current < 0.0000001) {
        return 0.0;
    }
    phase_ += phaseinc_;
    while (phase_ >=
           M_PI) // TODO: reduce this by env_current to avoid aliasing?
        phase_ -= 2.0f * (float)M_PI;
    return lpFilter.process(phase_ * mul_) * env_current;
}
