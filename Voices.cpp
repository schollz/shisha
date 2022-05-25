#include "Voices.h"
#include <libraries/Midi/Midi.h>
#include <vector>

Midi midi;

const char* gMidiPort0 = "hw:1,0,0";

void Voices::setup(float fs) {
    midi.readFrom(gMidiPort0);
    // midi.writeTo(midi_port);
    midi.enableParser(true);

    // setup overall filter
    Biquad::Settings settings{
        .fs = fs,
        .cutoff = 15000.0,
        .type = Biquad::lowpass,
        .q = 0.707,
        .peakGainDb = 3,
    };
    for (unsigned int channel = 0; channel < 2; channel++) {
        filter[channel].setup(settings);
        env[channel].setAttackRate(0.1 * fs);
        env[channel].setDecayRate(120 * fs);
        env[channel].setReleaseRate(16 * fs);
        env[channel].setSustainLevel(0.95);
        env[channel].gate(false);
    }

    std::vector<float> detuning_cents = {0.0,   -0.06, -0.1,  -0.04, 0.05, 0.02,
                                         0.07,  -0.08, -0.1,  0.02,  0.09, 0.11,
                                         -0.03, -0.05, -0.12, 0.03};

    for (unsigned int i = 0; i < MAX_VOICES; i++) {
        voice[i] = Saw(fs);
        voice[i].setDetune(detuning_cents[i]);
        voice[i].setPan(randfloat(-0.25, 0.25));
        voice[i].setLPF(80);
        voice[i].release(8);
        ev[i] = NoteEv();
    }
}

void Voices::note_on(float note, float velocity) {
    // find either the oldest note not being played
    // or the overall oldest note (if notes all being played)
    NoteEv old = NoteEv();
    old.event_num = 100000000;
    int i2 = -1;
    float lowest_note = 10000.0;
    for (int i = 0; i < MAX_VOICES; i++) {
        if (voice[i].playing() && ev[i].note < lowest_note) {
            lowest_note = ev[i].note;
        }
        if (ev[i].event_num < old.event_num && !voice[i].playing()) {
            old.event_num = ev[i].event_num;
            old.note = ev[i].note;
            i2 = i;
        }
    }
    if (i2 == -1) {
        i2 = 0;
        for (int i = 0; i < MAX_VOICES; i++) {
            if (ev[i].event_num < old.event_num) {
                old.event_num = ev[i].event_num;
                old.note = ev[i].note;
                i2 = i;
            }
        }
    }

    if (is_playing == false) {
        // gate on the main envelope
        env[0].gate(true);
        env[1].gate(true);
        is_playing = true;
    }

    // update the voice
    voice[i2].gate(false);
    // if lowest note, make it a sub note
    if (note < lowest_note) {
        // voice[i2].setMul(-1.0);
        voice[i2].setNote(note - 12);
    } else {
        // voice[i2].setMul(1.0);
        voice[i2].setNote(note);
    }
    voice[i2].gate(true);
    voice[i2].sustain(
        linlin(velocity, 0, 127, 0.4, 0.8)); // todo: replace by the velocity

    // update the events
    event_num++;
    ev[i2].note = note;
    ev[i2].event_num = event_num;
}

void Voices::note_off(float note) {
    for (int i = 0; i < MAX_VOICES; i++) {
        if (ev[i].note == note) {
            voice[i].gate(false);
        }
    }
    // if nothing else is playing then turn down filter
    is_playing = false;
    for (int i = 0; i < MAX_VOICES; i++) {
        if (voice[i].playing() == true) {
            is_playing = true;
            break;
        }
    }
    if (is_playing == false) {
        env[0].gate(false);
        env[1].gate(false);
    }
}

void Voices::process(int n, float* buf[2]) {
    int num;
    while ((num = midi.getParser()->numAvailableMessages()) > 0) {
        static MidiChannelMessage message;
        message = midi.getParser()->getNextChannelMessage();
        message.prettyPrint();
        if (message.getType() == kmmNoteOn) {
            float note = (float)message.getDataByte(0);
            float velocity = (float)message.getDataByte(1);
            rt_printf("note_on: %2.0f %2.0f\n", note, velocity);
            note_on(note, velocity);
        } else if (message.getType() == kmmNoteOff) {
            float note = (float)message.getDataByte(0);
            rt_printf("note_off: %2.0f\n", note);
            note_off(note);
        }
    }

    for (unsigned int i = 0; i < MAX_VOICES; i++) {
        voice[i].process(n, buf);
    }

    // update the filter and filter the output
    for (unsigned int channel = 0; channel < 2; channel++) {
        for (unsigned int i = 0; i < n - 1; i++) {
            env[channel].process();
        }
        filter[channel].setFc(
            linexp(env[channel].process(), 0.0, 1.0, 10.0, 18000.0));
        for (unsigned int i = 0; i < n; i++) {
            buf[channel][i] = filter[channel].process(buf[channel][i]);
        }
    }

    // TODO put a LPF over a "main envelope" that is always on as long as a note
    // is on (to improve aliasing)
    // TODO put a LPF on each voice with the envelope? (to improve aliasing)
}
