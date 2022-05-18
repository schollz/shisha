#include "I1P.h"
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

// oscillators
static const int NUM_VOICES = 6;
Saw voice[NUM_VOICES];
I1P* dcBlock[2];

// timing measurements
std::chrono::steady_clock::time_point timingStart;
std::chrono::steady_clock::time_point timingEnd;

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

// sequencer
Sequencer sequence[NUM_VOICES];

static void loop(void*) {
    while (!Bela_stopRequested()) {
        BelaCpuData* data = Bela_cpuMonitoringGet();
        // printf("total: %.2f%%, render: %.2f%%\n", data->percentage,
        //       gCpuRender.percentage);
        printf("analogInput[0]: %2.4f\n", analogInput[0]);
        printf("analogInput[1]: %2.4f\n", analogInput[1]);
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
        analogInputFilter[i] = new I1P(1.0 / context->analogSampleRate);
    }

    // setup audio things
    for (unsigned int channel = 0; channel < 2; channel++) {
        dcBlock[channel] = new I1P(10.0 / context->audioSampleRate);
        buf[channel] = (float*)malloc(sizeof(float) * context->audioFrames);
        bufsnd[channel] = (float*)malloc(sizeof(float) * context->audioFrames);
    }

    // setup sequencer
    float bpm = 50;
    std::vector<float> beats = {3.0, 5.0, 4.0, 6.0};
    sequence[0] = Sequencer(bpm, context->audioFrames, context->audioSampleRate,
                            beats, std::vector<float>{28, 28, 29, 26});
    sequence[1] = Sequencer(bpm, context->audioFrames, context->audioSampleRate,
                            beats, std::vector<float>{43, 48, 45, 50});
    sequence[2] = Sequencer(bpm, context->audioFrames, context->audioSampleRate,
                            beats, std::vector<float>{59, 57, 57, 59 - 12});
    sequence[3] = Sequencer(bpm, context->audioFrames, context->audioSampleRate,
                            beats, std::vector<float>{64, 69, 65, 59});
    sequence[4] = Sequencer(bpm, context->audioFrames, context->audioSampleRate,
                            beats, std::vector<float>{59, 52, 48, 55});
    sequence[5] = Sequencer(bpm, context->audioFrames, context->audioSampleRate,
                            beats, std::vector<float>{79, 72, 72, 71 - 12});

    // setup oscillators
    std::vector<float> detuning_cents = {0.0,  -0.06, -0.1, -0.04,
                                         0.05, 0.02,  0.07};
    std::vector<float> notes = {45 - 12.0, 60, 64, 69, 74, 81};
    for (unsigned int i = 0; i < NUM_VOICES; i++) {
        voice[i] = Saw(context->audioSampleRate);
        voice[i].setNote(notes[i]);
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

    // process sequencers
    for (unsigned int i = 0; i < NUM_VOICES; i++) {
        if (sequencer[i].tick() == true) {
            rt_printf("%d[%2.1f] ", i, sequence[i].val());
            voice[i].setNote(sequence[i].val());
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
            buf[channel][n] /= NUM_VOICES * 4;
            // buf[channel][n] = sample;
            buf[channel][n] -= dcBlock[channel]->process(buf[channel][n]);
            bufsnd[channel][n] = buf[channel][n];
        }
    }

    // send the audio out
    for (unsigned int channel = 0; channel < 2; channel++) {
        for (unsigned int n = 0; n < context->audioFrames; n++) {
            float input = audioRead(context, n, channel) / 12.0;
            audioWrite(context, n, channel,
                       (1 - input) * buf[channel][n] +
                           input * analogInput[0]); // analogInput 0 is dry/wet
            analogWriteOnce(context, n, channel,
                            bufsnd[channel][n] +
                                (input * analogInput[1] *
                                 0.9)); // analogInput 1 is feedback
        }
    }

    // perform the timing on the first render
    Bela_cpuToc(&gCpuRender);
}

void cleanup(BelaContext* context, void* userData) { rt_printf("cleanup!"); }
