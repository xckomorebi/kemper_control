#include <portmidi.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "kemper_manager.h"

#define QUERY_BYTES_FOR_SLOT(slot_num)                                         \
    {0xF0, 0x00, 0x20, 0x33, 0x02, 0x7F,     0x47,                             \
     0x00, 0x00, 0x00, 0x01, 0x00, slot_num, 0xF7}

static unsigned char PERF_QUERY_BYTES[] = QUERY_BYTES_FOR_SLOT(0x00);
static unsigned char SLOT1_QUERY_BYTES[] = QUERY_BYTES_FOR_SLOT(0x01);
static unsigned char SLOT2_QUERY_BYTES[] = QUERY_BYTES_FOR_SLOT(0x02);
static unsigned char SLOT3_QUERY_BYTES[] = QUERY_BYTES_FOR_SLOT(0x03);

static void send_all_query(KemperManager *r) {

#define SEND_QUERY(query_bytes)                                                \
    err = Pm_WriteSysEx(r->midi_out, 0, query_bytes);                          \
    if (err != pmNoError) {                                                    \
        pthread_mutex_lock(&r->lock);                                          \
        r->error_msg = Pm_GetErrorText(err);                                   \
        r->state = KEMPER_STATE_ERROR;                                         \
        pthread_mutex_unlock(&r->lock);                                        \
        return;                                                                \
    }

    PmError err;
    SEND_QUERY(PERF_QUERY_BYTES);
    SEND_QUERY(SLOT1_QUERY_BYTES);
    SEND_QUERY(SLOT2_QUERY_BYTES);
    SEND_QUERY(SLOT3_QUERY_BYTES);

#undef SEND_QUERY
}

#define LOADING_STR "load"

int kemper_connect(KemperManager *r) {
    PmError err;
    PmDeviceID in_device_id = pmNoDevice;
    PmDeviceID out_device_id = pmNoDevice;

    while (true) {
        err = Pm_Initialize();
        if (err != pmNoError) {
            pthread_mutex_lock(&r->lock);
            r->error_msg = Pm_GetErrorText(err);
            r->state = KEMPER_STATE_ERROR;
            pthread_mutex_unlock(&r->lock);
            return 1;
        }

        for (int i = 0; i < Pm_CountDevices(); i++) {
            const PmDeviceInfo *info = Pm_GetDeviceInfo(i);
            if (strncmp(info->name, PROFILER_DEVICE_PREFIX, PROFILER_DEVICE_PREFIX_LEN) != 0) {
                continue;
            }
            if (info->input) {
                in_device_id = i;
            }
            if (info->output) {
                out_device_id = i;
            }
        }

        if (in_device_id != pmNoDevice && out_device_id != pmNoDevice) {
            break;
        }
        Pm_Terminate();
        // sleep for 1 second before checking again
        sleep(1);
    }

    err = Pm_OpenInput(&r->midi_in, in_device_id, NULL, 100, NULL, NULL);
    if (err != pmNoError) {
        pthread_mutex_lock(&r->lock);
        r->error_msg = Pm_GetErrorText(err);
        r->state = KEMPER_STATE_ERROR;
        pthread_mutex_unlock(&r->lock);
        return 1;
    }
    err = Pm_OpenOutput(&r->midi_out, out_device_id, NULL, 100, NULL, NULL, 0);
    if (err != pmNoError) {
        pthread_mutex_lock(&r->lock);
        r->error_msg = Pm_GetErrorText(err);
        r->state = KEMPER_STATE_ERROR;
        pthread_mutex_unlock(&r->lock);
        return 1;
    }

    pthread_mutex_lock(&r->lock);
    r->state = KEMPER_STATE_CONNECTED;
    r->current_slot = 0;
    strcpy(r->perf_name, LOADING_STR);
    for (int i = 0; i < MAX_SLOTS_NUM; i++) {
        strcpy(r->rig_name[i], LOADING_STR);
    }
    pthread_mutex_unlock(&r->lock);

    send_all_query(r);

    return 0;
}

static void process_program_change(KemperManager *r, int control, int value) {
    // perf change
    if (control == KEMPER_CTRL_PERF_CHANGE) {
        if (value == r->current_perf) {
            return;
        }

        pthread_mutex_lock(&r->lock);
        r->current_perf = value;
        r->current_slot = 0;
        strcpy(r->perf_name, LOADING_STR);
        pthread_mutex_unlock(&r->lock);

        send_all_query(r);
        return;
    }

    // slot change
    if (value != 1) {
        return;
    }

    if (control >= KEMPER_CTRL_SLOT_CHANGE_START &&
        control <= KEMPER_CTRL_SLOT_CHANGE_END) {
        int slot_num = control - KEMPER_CTRL_SLOT_CHANGE_START;
        pthread_mutex_lock(&r->lock);
        r->current_slot = slot_num;
        pthread_mutex_unlock(&r->lock);
    }
}

#define SYSEX_PREFIX_LEN 13
#define SYSEX_TO_STR(x) ((const char *)((x) + SYSEX_PREFIX_LEN))

static void process_sysex(KemperManager *r, unsigned char *sysex_buffer,
                          size_t sysex_index) {
    if (sysex_index < 11) {
        return;
    }

    // some magic numbers going on here, i don't know what they mean
    // but they are needed to identify the sysex message
    if (sysex_buffer[0] != 240 || sysex_buffer[1] != 0 ||
        sysex_buffer[2] != 32 || sysex_buffer[3] != 51 ||
        sysex_buffer[4] != 0 || sysex_buffer[5] != 0 || sysex_buffer[6] != 7 ||
        sysex_buffer[7] != 0 || sysex_buffer[8] != 0 || sysex_buffer[9] != 0 ||
        sysex_buffer[10] != 1 || sysex_buffer[11] != 0) {
        return;
    }

    unsigned char slot_num = sysex_buffer[12];
    if (slot_num > MAX_SLOTS_NUM) {
        return;
    }

    if (slot_num == 0) {
        pthread_mutex_lock(&r->lock);
        strncpy(r->perf_name, SYSEX_TO_STR(sysex_buffer), sysex_index - 14);
        r->perf_name[sysex_index - 14] = '\0';
        pthread_mutex_unlock(&r->lock);
        return;
    }

    pthread_mutex_lock(&r->lock);
    strncpy(r->rig_name[slot_num - 1], SYSEX_TO_STR(sysex_buffer),
            sysex_index - 14);
    r->rig_name[slot_num - 1][sysex_index - 14] = '\0';
    pthread_mutex_unlock(&r->lock);
}

#undef SYSEX_TO_STR
#undef SYSEX_PREFIX_LEN

void *midi_process_main_loop(void *handle) {
    PmEvent buffer;

    KemperManager *r = (KemperManager *)handle;
    while (true) {
        if (Pm_Poll(r->midi_in)) {
            int count = Pm_Read(r->midi_in, &buffer, 1);

            if (count > 0) {
                PmMessage msg = buffer.message;
                int status = Pm_MessageStatus(msg);
                int control = Pm_MessageData1(msg);
                int value = Pm_MessageData2(msg);
                size_t sysex_index = 0;
                unsigned char sysex_buffer[SYSEX_BUFFER_SIZE] = {0};

                switch (status) {
                case MIDI_TYPE_SYSEX:
                    do {
                        msg = buffer.message;
                        sysex_buffer[sysex_index++] = msg & 0xFF;
                        if (sysex_buffer[sysex_index - 1] == 0xF7)
                            break;
                        sysex_buffer[sysex_index++] = (msg >> 8) & 0xFF;
                        if (sysex_buffer[sysex_index - 1] == 0xF7)
                            break;
                        sysex_buffer[sysex_index++] = (msg >> 16) & 0xFF;
                        if (sysex_buffer[sysex_index - 1] == 0xF7)
                            break;
                        sysex_buffer[sysex_index++] = (msg >> 24) & 0xFF;
                        if (sysex_buffer[sysex_index - 1] == 0xF7)
                            break;
                    } while (Pm_Read(r->midi_in, &buffer, 1) > 0);
                    process_sysex(r, sysex_buffer, sysex_index);
                    break;
                case MIDI_TYPE_CC:
                    control = Pm_MessageData1(msg);
                    value = Pm_MessageData2(msg);
                    process_program_change(r, control, value);
                    break;
                }
            }
        }
        usleep(50000); // sleep for 50ms
    }
}
