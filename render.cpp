#include "I1P.h"
#include "LFNoise1.h"
#include "MonoFilePlayer.h"
#include "Utils.h"
#include "Voices.h"
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
Voices voices;
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

static void loop(void*) {
    while (!Bela_stopRequested()) {
        BelaCpuData* data = Bela_cpuMonitoringGet();
        printf("total: %.2f%%, render: %.2f%%\n", data->percentage,
               gCpuRender.percentage);
        for (unsigned int i = 0; i < 5; i++) {
            printf("%2.2f ", analogInput[i]);
        }
        printf("\n");
        usleep(3000000);
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
        analogInputFilter[i] = new I1P(0.05 / context->analogSampleRate);
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

    voices = Voices(context->audioSampleRate);

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
    voices.process(context->audioFrames, buf);

    for (unsigned int n = 0; n < context->audioFrames; n++) {
        // float sample = 0.2 * gPlayer.process();
        for (unsigned int channel = 0; channel < 2; channel++) {
            buf[channel][n] /= 12.0;
            // buf[channel][n] += sample;
            buf[channel][n] -= dcBlock[channel]->process(buf[channel][n]);
            bufsnd[channel][n] = buf[channel][n] / 4;
        }
    }

    for (unsigned int channel = 0; channel < 2; channel++) {
        float val =
            map(chorus_noise[channel].process(), -1, 1,
                0.5 - analogInput[1] / 25.0, 0.5 + analogInput[1] / 25.0);
        val += analogInput[2] - 0.5;
        val = clamp(val, 0, 1);
        analogWrite(context, 0, 2 + channel, val);
    }
    // send the audio out
    for (unsigned int channel = 0; channel < 2; channel++) {
        for (unsigned int n = 0; n < context->audioFrames; n++) {
            float input = audioRead(context, n, channel) / 2.0;
            audioWrite(context, n, channel,
                       (1 - analogInput[0]) * buf[channel][n] +
                           input * analogInput[0]); // analogInput 0

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
