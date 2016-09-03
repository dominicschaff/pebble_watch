#include <pebble.h>

void timedisplay_load_graphics(Window *window);
void timedisplay_load_text(Window *window);
void timedisplay_unload(Window *window);

void timedisplay_init();
void timedisplay_deinit();

void update_time();