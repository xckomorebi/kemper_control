#ifndef TUI_H
#define TUI_H

#include "kemper_manager.h"

void init_tui(void);
void display_rig_status(KemperManager *r);
void *tui_main_loop(void *handle);

void cleanup_tui(void);

#endif // TUI_H
