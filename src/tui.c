#include <ncurses.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>

#include "kemper_manager.h"
#include "tui.h"

void init_tui(void) {
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);

    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_GREEN, COLOR_BLACK);
        init_pair(2, COLOR_BLUE, COLOR_BLACK);
        init_pair(3, COLOR_WHITE, COLOR_BLUE);
        init_pair(4, COLOR_RED, COLOR_RED);
    }
}

void cleanup_tui(void) {
    endwin();
}

void *tui_main_loop(void *handle) {
    KemperManager *r = (KemperManager *)handle;
    while (true) {
        display_rig_status(r);
        // tui refreshes every 100ms
        usleep(100000); 
    }
    return NULL;
}

static void print_to_center(const char *str) {
    int x, y;
    getmaxyx(stdscr, y, x);
    int len = strlen(str);
    int start_x = (x - len) / 2;
    mvprintw(y / 2, start_x, "%s", str);
}

void display_rig_status(KemperManager *r) {
    pthread_mutex_lock(&r->lock);
    clear();
    if (r->state == KEMPER_STATE_DISCONNECTED) {
        attron(A_BOLD | COLOR_PAIR(2));
        print_to_center("Not connected to kemper");
        attroff(A_BOLD | COLOR_PAIR(2));

        goto unlock_and_refresh;
    }

    if (r->state == KEMPER_STATE_ERROR) {
        attron(A_BOLD | COLOR_PAIR(4));
        print_to_center(r->error_msg);
        attroff(A_BOLD | COLOR_PAIR(4));

        goto unlock_and_refresh;
    }

    attron(A_BOLD | COLOR_PAIR(2));
    printw("Kemper Profiler Controller\n\n");
    attroff(A_BOLD | COLOR_PAIR(2));

    // Highlight current slot
    for (int i = 0; i < 3; i++) {
        if (i == r->current_slot) {
            attron(A_REVERSE | COLOR_PAIR(3));
            printw(" %s ", r->rig_name[i]);
            attroff(A_REVERSE | COLOR_PAIR(3));
        } else {
            printw(" %s ", r->rig_name[i]);
        }
    }

    printw("\n\n");

    attron(COLOR_PAIR(1));
    printw("Perf:\t%s\n", r->perf_name);
    attroff(COLOR_PAIR(1));

unlock_and_refresh:
    pthread_mutex_unlock(&r->lock);
    refresh();
}

void handle_key_command(KemperManager *r) {
    int c = getch();
    switch (c) {
    case 'q':
        endwin();
        exit(0);
    case 'h':
        r->current_slot = (r->current_slot + 2) % 3;
        break;
    case 'l':
        r->current_slot = (r->current_slot + 1) % 3;
        break;
    case '0':
        r->current_slot = -1;
        break;
    case '1':
        r->current_slot = 0;
        break;
    case '2':
        r->current_slot = 1;
        break;
    case '3':
        r->current_slot = 2;
        break;
    }
}
