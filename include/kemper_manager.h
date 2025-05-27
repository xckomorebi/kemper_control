#ifndef KEMPER_MANAGER_H
#define KEMPER_MANAGER_H

#include <portmidi.h>
#include <pthread.h>

#define MAX_RIG_NAME_LEN 32
#define PROFILER_DEVICE_PREFIX "Profiler"
#define PROFILER_DEVICE_PREFIX_LEN 8

// in fact one kemper performance has 5 slots
// but I only use 3 of them
#define MAX_SLOTS_NUM 3

#define SYSEX_BUFFER_SIZE 48
#define MIDI_TYPE_SYSEX 0xF0
#define MIDI_TYPE_CC 0xB0

#define KEMPER_CTRL_PERF_CHANGE 47
#define KEMPER_CTRL_SLOT_CHANGE_START 50
#define KEMPER_CTRL_SLOT_CHANGE_END 52

typedef enum {
    KEMPER_STATE_DISCONNECTED,
    KEMPER_STATE_ERROR,
    KEMPER_STATE_CONNECTED,
} KemperState;

typedef struct {
    KemperState state;

    PortMidiStream *midi_in;
    PmDeviceID midi_in_id;
    PortMidiStream *midi_out;

    pthread_mutex_t lock;
    int current_perf;
    int current_slot;
    char perf_name[MAX_RIG_NAME_LEN];
    char rig_name[MAX_SLOTS_NUM][MAX_RIG_NAME_LEN];

    const char *error_msg;
} KemperManager;

#define KEMPER_MANAGER_INIT                                                    \
    {.state = KEMPER_STATE_DISCONNECTED,                                       \
     .current_perf = -1,                                                       \
     .current_slot = -1,                                                       \
     .midi_in_id = pmNoDevice,                                                 \
     .lock = PTHREAD_MUTEX_INITIALIZER}

int kemper_connect(KemperManager *r);
void *midi_process_main_loop(void *handle);

#endif // KEMPER_MANAGER_H
