#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "kemper_manager.h"
#include "tui.h"

int main(void) {
    init_tui();

    KemperManager r = KEMPER_MANAGER_INIT;

    pthread_t tui_thread;

    int ret = pthread_create(&tui_thread, NULL, tui_main_loop, (void *)&r);
    if (ret != 0) {
        perror("Failed to create display thread");
        return 1;
    }

    ret = kemper_connect(&r);
    if (ret != 0) {
        sleep(5);
        return EXIT_FAILURE;
    }

    pthread_t midi_thread;
    int ret2 =
        pthread_create(&midi_thread, NULL, midi_process_main_loop, (void *)&r);
    if (ret2 != 0) {
        perror("Failed to create midi thread");
        return 1;
    }

    while (true) {
        int c = getchar();
        if (c == 'q') {
            break;
        }
    }

    pthread_cancel(midi_thread);
    pthread_cancel(tui_thread);

    cleanup_tui();
    return EXIT_SUCCESS;
}
