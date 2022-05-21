#include "I1P.h"
#include "LFNoise1.h"
#include "MonoFilePlayer.h"
#include "Saw.h"
#include "Sequencer.h"
#include "Utils.h"
#include <Bela.h>
#include <chrono>
#include <cmath>
#include <vector>

// profiling
BelaCpuData gCpuRender = {
    .count = 100,
};
std::chrono::steady_clock::time_point timingStart;
std::chrono::steady_clock::time_point timingEnd;

// oscillators
static const int NUM_VOICES = 6;
Saw voice[NUM_VOICES];
I1P* dcBlock[2];

// audio buffer (2 x block)
float *buf[2], *bufsnd[2];

// analog inputs
static const int NUM_ANALOG_INPUTS = 5;
float analogInput[NUM_ANALOG_INPUTS];
I1P* analogInputFilter[NUM_ANALOG_INPUTS];
int gAudioFramesPerAnalogFrame = 0;

// mono file player
std::string gFilename = "drums.wav";
MonoFilePlayer gPlayer;

// chorus
LFNoise1 chorus_noise[2];
// sequencers
Sequencer sequencer_notes[NUM_VOICES];

static void loop(void*) {
    while (!Bela_stopRequested()) {
        BelaCpuData* data = Bela_cpuMonitoringGet();
        // printf("total: %.2f%%, render: %.2f%%\n", data->percentage,
        //       gCpuRender.percentage);
        for (unsigned int i = 0; i < 5; i++) {
            printf("(%d %2.3f) ", i, analogInput[i]);
        }
        printf("\n");
        usleep(300000);
    }
}

bool setup(BelaContext* context, void* userData) {
    // hello world
    rt_printf("ahni shisha");

    // initialize the random number generator
    srand(time(NULL));

    // enable CPU monitoring for the whole audio thread
    Bela_cpuMonitoringInit(100);
    Bela_runAuxiliaryTask(loop);

    // setup file player
    if (!gPlayer.setup(gFilename)) {
        rt_printf("Error loading audio file '%s'\n", gFilename.c_str());
        return false;
    }
    rt_printf("Loaded the audio file '%s' with %d frames (%.1f seconds)\n",
              gFilename.c_str(), gPlayer.size(),
              gPlayer.size() / context->audioSampleRate);

    // setup analog inputs
    if (context->analogFrames)
        gAudioFramesPerAnalogFrame =
            context->audioFrames / context->analogFrames;
    for (unsigned int i = 0; i < NUM_ANALOG_INPUTS; i++) {
        analogInputFilter[i] = new I1P(0.3 / context->analogSampleRate);
    }

    // setup audio things
    for (unsigned int channel = 0; channel < 2; channel++) {
        dcBlock[channel] = new I1P(10.0 / context->audioSampleRate);
        buf[channel] = (float*)malloc(sizeof(float) * context->audioFrames);
        bufsnd[channel] = (float*)malloc(sizeof(float) * context->audioFrames);
        chorus_noise[channel] =
            LFNoise1(randfloat(5.0, 10.0),
                     context->audioSampleRate / context->audioFrames, true);
    }

    // setup sequencer_notes
    float bpm = 49.5;
    std::vector<float> beats = {3.0, 5.0, 6.0, 3.0, 3.0};
    std::vector<std::vector<float>> notes = {
        {28, 28, 29, 26, 26}, {43, 48, 45, 50, 50}, {59, 57, 57, 47, 48},
        {64, 69, 65, 59, 64}, {59, 52, 48, 55, 52}, {79, 72, 72, 71, 79},
    };
    for (unsigned int i = 0; i < NUM_VOICES; i++) {
        sequencer_notes[i] =
            Sequencer(bpm, context->audioFrames, context->audioSampleRate,
                      beats, notes[i]);
    }
    /*
    sequencer_notes[0] =
        Sequencer(bpm, context->audioFrames, context->audioSampleRate, beats,
                  std::vector<float>{28, 28, 29, 26});
    sequencer_notes[1] =
        Sequencer(bpm, context->audioFrames, context->audioSampleRate, beats,
                  std::vector<float>{43, 48, 45, 50});
    sequencer_notes[2] =
        Sequencer(bpm, context->audioFrames, context->audioSampleRate, beats,
                  std::vector<float>{59, 57, 57, 59 - 12});
    sequencer_notes[3] =
        Sequencer(bpm, context->audioFrames, context->audioSampleRate, beats,
                  std::vector<float>{64, 69, 65, 59});
    sequencer_notes[4] =
        Sequencer(bpm, context->audioFrames, context->audioSampleRate, beats,
                  std::vector<float>{59, 52, 48, 55});
    sequencer_notes[5] =
        Sequencer(bpm, context->audioFrames, context->audioSampleRate, beats,
                  std::vector<float>{79, 72, 72, 71 - 12});
*/

    // setup oscillators
    std::vector<float> detuning_cents = {0.0,  -0.06, -0.1, -0.04,
                                         0.05, 0.02,  0.07};
    for (unsigned int i = 0; i < NUM_VOICES; i++) {
        voice[i] = Saw(context->audioSampleRate);
        voice[i].setDetune(detuning_cents[i]);
        voice[i].setPan(randfloat(-0.25, 0.25));
        voice[i].setLPF(80);
    }
    voice[0].setMul(-1.0);

    return true;
}

void render(BelaContext* context, void* userData) {
    // cpu start clock
    Bela_cpuTic(&gCpuRender);

    // process sequencer_notess
    for (unsigned int i = 0; i < NUM_VOICES; i++) {
        if (sequencer_notes[i].tick() == true) {
            rt_printf("%d[%2.1f] ", i, sequencer_notes[i].val());
            voice[i].setNote(sequencer_notes[i].val());
            voice[i].toggle();
        }
    }

    // process analog pins
    for (unsigned int n = 0; n < context->audioFrames; n++) {
        // read analog inputs and update frequency and amplitude
        if (gAudioFramesPerAnalogFrame && !(n % gAudioFramesPerAnalogFrame)) {
            for (unsigned int i = 0; i < NUM_ANALOG_INPUTS; i++) {
                analogInput[i] =
                    // analogInputFilter[i]->process(
                    analogRead(context, n / gAudioFramesPerAnalogFrame, i);
                //);
            }
        }
    }

    // reset block
    for (unsigned int channel = 0; channel < 2; channel++) {
        for (unsigned int n = 0; n < context->audioFrames; n++) {
            buf[channel][n] = 0.0;
            bufsnd[channel][n] = 0.0;
        }
    }

    // process audio
    for (unsigned int i = 0; i < NUM_VOICES; i++) {
        voice[i].process(context->audioFrames, buf);
    }

    // dc blocking
    for (unsigned int n = 0; n < context->audioFrames; n++) {
        float sample = 0.2 * gPlayer.process();
        for (unsigned int channel = 0; channel < 2; channel++) {
            buf[channel][n] /= NUM_VOICES;
            // buf[channel][n] += sample;
            buf[channel][n] -= dcBlock[channel]->process(buf[channel][n]);
            bufsnd[channel][n] = buf[channel][n] / 4;
        }
    }

    for (unsigned int channel = 0; channel < 2; channel++) {
        analogWrite(context, 0, 2 + channel,
                    map(chorus_noise[channel].process(), -1, 1,
                        0.5 - analogInput[1] / 20.0,
                        0.5 + analogInput[1] / 20.0));
    }
    // send the audio out
    for (unsigned int channel = 0; channel < 2; channel++) {
        for (unsigned int n = 0; n < context->audioFrames; n++) {
            float input = audioRead(context, n, channel) / 2.0;
            audioWrite(context, n, channel,
                       (1 - analogInput[0]) * buf[channel][n] +
                           input * analogInput[0]); // analogInput 0 is dry/wet
            analogWriteOnce(context, n, channel,
                            bufsnd[channel][n] +
                                (input * analogInput[3] * 0.9 /
                                 5.0)); // analogInput 1 is feedback
        }
    }

    // perform the timing on the first render
    Bela_cpuToc(&gCpuRender);
}

void cleanup(BelaContext* context, void* userData) { rt_printf("cleanup!"); }
