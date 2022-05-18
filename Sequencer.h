#include <cstdlib>
#include <math.h>
#include <vector>

#pragma once

class Sequencer {
  public:
    Sequencer(){};
    ~Sequencer(){};
    Sequencer(float bpm, float n, float fs, std::vector<float> durs,
              std::vector<float> vals) {
        _durs = durs;
        _vals = vals;
        _i = _durs.size() - 1;
        _dur = 10000.0;
        _bpm = bpm;
        _beat_inc = (_bpm / 60.0) * k / fs;
    };
    bool tick() {
        float beats_old = beats_;
        beats_ += (bpm / 60) * context->audioFrames / context->audioSampleRate;
        if (floor(beats) == floor(beats_old)) {
            return false
        }
        _dur += 1.0;
        if (_dur < _durs.at(_i)) {
            return false;
        }
        _dur = 0;
        _i++;
        if (_i >= _durs.size()) {
            _i = 0;
        }
        return true;
    };
    float val() { return _vals.at(_i); };

  private:
    unsigned int _i;
    float _dur;
    float _bpm;
    float _bpm_inc, _beats;
    std::vector<float> _durs;
    std::vector<float> _vals;
};
