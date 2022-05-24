#include "Voices.h"
#include <vector>

void Voices::setup(float fs) {
    midi.readFrom(midi_port);
    midi.writeTo(midi_port);
    midi.enableParser(true);

    std::vector<float> detuning_cents = {0.0,  -0.06, -0.1, -0.04,
                                         0.05, 0.02,  0.07};

    for (unsigned int i = 0; i < MAX_VOICES; i++) {
        voice[i] = Saw(fs);
        voice[i].setDetune(detuning_cents[i]);
        voice[i].setPan(randfloat(-0.25, 0.25));
        voice[i].setLPF(80);
        ev[i] = NoteEv();
    }
}

void Voices::note_on(float note, float velocity) {
    // find either the oldest note not being played
    // or the overall oldest note (if notes all being played)
    NoteEv old = NoteEv();
    old.event_num = 100000000;
    int i2 = -1;
    for (int i = 0; i < MAX_VOICES; i++) {
        if (ev[i].event_num < old.event_num && !voice[i].playing()) {
            old.event_num = ev[i].event_num;
            old.note = ev[i].note;
            i2 = i;
        }
    }
    if (i2 == -1) {
        for (int i = 0; i < MAX_VOICES; i++) {
            if (ev[i].event_num < old.event_num) {
                old.event_num = ev[i].event_num;
                old.note = ev[i].note;
                i2 = i;
            }
        }
    }

    // update the voice
    voice[i2].gate(false);
    voice[i2].setNote(note);
    voice[i2].gate(true);
    voice[i2].sustain(0.5); // todo: replace by the velocity

    // update the events
    event_num++;
    ev[i].note = note;
    ev[i].event_num = event_num;
}

void Voices::note_off(float note) {
    for (int i = 0; i < MAX_VOICES; i++) {
        if (ev[i].note == note) {
            voice[i].gate(false);
        }
    }
}

void Voices::process(int n float* buf[2]) {
    int num;
    while ((num = midi.getParser()->numAvailableMessages()) > 0) {
        static MidiChannelMessage message;
        message = midi.getParser()->getNextChannelMessage();
        message.prettyPrint();
        if (message.getType() == kmmNoteOn) {
            float note = (float)message.getDataByte(0);
            float velocity = (float)message.getDataByte(1);
            if (velocity > 0) {
                rt_printf("note_on: %2.0f %2.0f", note, velocity);
                note_on(note, velocity);
            } else {
                rt_printf("note_off: %2.0f %2.0f", note, velocity);
                note_off(note);
            }
        }
    }

    for (unsigned int i = 0; i < MAX_VOICES; i++) {
        voice[i].process(n, buf);
    }
}
