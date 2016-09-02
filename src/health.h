#include <pebble.h>

void update_health();

void health_load_graphics(Window *window);
void health_load_text(Window *window);
void health_unload(Window *window);
void health_init();
void health_deinit();