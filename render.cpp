#include "I1P.h"
#include "Saw.h"
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

static void loop(void*) {
    while (!Bela_stopRequested()) {
        BelaCpuData* data = Bela_cpuMonitoringGet();
        // printf("total: %.2f%%, render: %.2f%%\n", data->percentage,
        //       gCpuRender.percentage);
        printf("analogInput[0]: %2.4f\n", analogInput[0]);
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

    // process analog pins
    for (unsigned int n = 0; n < context->audioFrames; n++) {
        // read analog inputs and update frequency and amplitude
        if (gAudioFramesPerAnalogFrame && !(n % gAudioFramesPerAnalogFrame)) {
            for (unsigned int i = 0; i < NUM_ANALOG_INPUTS; i++) {
                analogInput[i] = analogInputFilter[i]->process(
                    analogRead(context, n / gAudioFramesPerAnalogFrame, i));
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
    for (unsigned int channel = 0; channel < 2; channel++) {
        for (unsigned int n = 0; n < context->audioFrames; n++) {
            buf[channel][n] /= NUM_VOICES;
            buf[channel][n] -= dcBlock[channel]->process(buf[channel][n]);
            bufsnd[channel][n] = buf[channel][n];
        }
    }

    // send the audio out
    for (unsigned int channel = 0; channel < 2; channel++) {
        for (unsigned int n = 0; n < context->audioFrames; n++) {
            float input = audioRead(context, n, channel);
            audioWrite(context, n, channel,
                       buf[channel][n] + input * analogInput[channel]);
            analogWriteOnce(context, n, channel, bufsnd[channel][n]);
        }
    }

    // perform the timing on the first render
    Bela_cpuToc(&gCpuRender);
}

void cleanup(BelaContext* context, void* userData) { rt_printf("cleanup!"); }
