#include <ncurses.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kemper_manager.h"
#include "tui.h"
#include "ascii_art.h"

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

static void print_to_scr_center(const char *str) {
    int x, y;
    getmaxyx(stdscr, y, x);
    int len = strlen(str);
    int start_x = (x - len) / 2;
    mvprintw(y / 2, start_x, "%s", str);
}

static void print_to_middle(const char *fmt, ...) {
    char tmp[MAX_RIG_NAME_LEN];
    va_list args;
    va_start(args, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, args);
    va_end(args);

    int max_x = getmaxx(stdscr);
    int length = strlen(tmp);
    int start_pos = (max_x - length) / 2;
    move(getcury(stdscr), start_pos);
    printw("%s", tmp);
    refresh();
}

static void print_to_section(int section,
                             int total_sections,
                             const char *fmt,
                             ...) {
    char tmp[MAX_RIG_NAME_LEN];
    va_list args;
    va_start(args, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, args);
    va_end(args);

    int max_x = getmaxx(stdscr);
    int length = strlen(tmp);
    int start_pos = max_x / total_sections / 2 * (section * 2 - 1);
    start_pos -= length / 2;
    move(getcury(stdscr), start_pos);
    printw("%s", tmp);
    refresh();
}

static void print_ascii_art(const char *str) {
    size_t len = strlen(str);
    size_t width = 0;
    for (size_t i = 0; i < len; i++) {
        width += wcslen(ascii_art[(unsigned char)str[i]][0]);
    }

    int max_x = getmaxx(stdscr);
    int start_pos = (max_x - width) / 2;

    for (int i = 0; i < ASCII_ART_HEIGHT; i++) {
        move(getcury(stdscr), start_pos);
        for (int j = 0; j < len; j++) {
            printw("%ls", ascii_art[(unsigned char)str[j]][i]);
        }
        printw("%ls", L"\n");
    }
}

void display_rig_status(KemperManager *r) {
    pthread_mutex_lock(&r->lock);
    clear();
    if (r->state == KEMPER_STATE_DISCONNECTED) {
        attron(A_BOLD | COLOR_PAIR(2));
        print_to_scr_center("Not connected to kemper");
        attroff(A_BOLD | COLOR_PAIR(2));

        goto unlock_and_refresh;
    }

    if (r->state == KEMPER_STATE_ERROR) {
        attron(A_BOLD | COLOR_PAIR(4));
        print_to_scr_center(r->error_msg);
        attroff(A_BOLD | COLOR_PAIR(4));

        goto unlock_and_refresh;
    }

    attron(A_BOLD | COLOR_PAIR(2));
    print_to_middle("Kemper Profiler Controller\n\n");
    attroff(A_BOLD | COLOR_PAIR(2));

    // Highlight current slot
    for (int i = 0; i < 3; i++) {
        if (i == r->current_slot) {
            attron(A_REVERSE | COLOR_PAIR(3));
            print_to_section(i + 1, 3, " %d. %s", i + 1, r->rig_name[i]);
            attroff(A_REVERSE | COLOR_PAIR(3));
        } else {
            print_to_section(i + 1, 3, " %d. %s", i + 1, r->rig_name[i]);
        }
    }

    printw("\n\n\n");

    attron(COLOR_PAIR(1));
    print_ascii_art(r->perf_name);
    attroff(COLOR_PAIR(1));

unlock_and_refresh:
    pthread_mutex_unlock(&r->lock);
    refresh();
}
