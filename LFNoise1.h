#include "I1P.h"
#include "math.h"
#include <cstdlib>
#pragma once

class LFNoise1 {
  public:
    LFNoise1(){};
    LFNoise1(float frequency, float fs, bool filter) {
        max_samples_ = (int)(fs / frequency);
        current_ = 0;
        next_rand();
        filter_ = new I1P((float)(frequency / fs) / 3.0);
        do_filter = filter;
    };
    ~LFNoise1(){};
    void next_rand() {
        next_ = randfloat_(-1, 1);
        slope_ = (next_ - current_) / max_samples_;
        count_ = 0;
    };

    float process() {
        count_ += 1;
        if (count_ > max_samples_) {
            next_rand();
        }
        current_ += slope_;
        if (do_filter) {
            return filter_->process(current_);
        }
        return current_;
    };
    float val() { return current_; }
    inline float randfloat_(float a, float b) {
        return ((b - a) * ((float)rand() / RAND_MAX)) + a;
    }

  private:
    I1P* filter_;
    bool do_filter;
    int count_;
    int max_samples_;
    float current_, next_;
    float slope_;
};
