#include <pebble.h>
#include "constants.h"
#include "timedisplay.h"
#include "health.h"

// All of the global variables
static Window *s_main_window;

static TextLayer *s_battery_text_layer;

static int s_battery_level;
static Layer *s_bluetooth_layer;

// All functions used
static void battery_callback(BatteryChargeState state);
static void main_window_load(Window *window);
static void main_window_unload(Window *window);
static void init();
static void deinit();
static void bluetooth_callback(bool connected);
static void bluetooth_update_proc(Layer *layer, GContext *ctx);
static void add_text_layer(Layer *window_layer, TextLayer *text_layer, GTextAlignment alignment);

/**
 * Main function, the watch runs this function
 */
int main(void)
{
  init();
  app_event_loop();
  deinit();
}

/**
 * Main initializer, this needs to initialize all the general components
 */
static void init()
{
  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers)
  {
    .load = main_window_load,
    .unload = main_window_unload
  });
  window_stack_push(s_main_window, true);
  bluetooth_callback(connection_service_peek_pebble_app_connection());

  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = bluetooth_callback
  });
  timedisplay_init();
  health_init();

  battery_callback(battery_state_service_peek());
  battery_state_service_subscribe(battery_callback);
}

/**
 * This functions only job is to request that the window must be destroyed
 */
static void deinit()
{
  timedisplay_deinit();
  window_destroy(s_main_window);
}

/**
 * Function to be run when the window needs to be loaded
 *
 * @param window The window to load
 */
static void main_window_load(Window *window)
{
  // Get information about the Window
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  int hh = bounds.size.h>>1;
  int hw = bounds.size.w>>1;

  // Create bluetooth meter Layer
  s_bluetooth_layer = layer_create(GRect(0, 0, bounds.size.w, bounds.size.h));
  layer_set_update_proc(s_bluetooth_layer, bluetooth_update_proc);
  layer_add_child(window_layer, s_bluetooth_layer);

  timedisplay_load_graphics(window);
  health_load_graphics(window);
  timedisplay_load_text(window);
  health_load_text(window);

  // Create the battery percentage display
  s_battery_text_layer = text_layer_create(GRect(0, -5, bounds.size.w, SUB_TEXT_HEIGHT));
  text_layer_set_font(s_battery_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  add_text_layer(window_layer, s_battery_text_layer, GTextAlignmentRight);
}

static void add_text_layer(Layer *window_layer, TextLayer *text_layer, GTextAlignment alignment)
{
  text_layer_set_background_color(text_layer, GColorClear);
  text_layer_set_text_color(text_layer, GColorBlack);
  text_layer_set_text_alignment(text_layer, alignment);
  layer_add_child(window_layer, text_layer_get_layer(text_layer));
}

/**
 * Destroy the window
 *
 * @param window The window to destroy
 */
static void main_window_unload(Window *window)
{
  timedisplay_unload(window);
  health_unload(window);
  text_layer_destroy(s_battery_text_layer);
  layer_destroy(s_bluetooth_layer);
}

/**
 * Run this if the battery level changes
 *
 * @param state The current battery state
 */
static void battery_callback(BatteryChargeState state)
{
  static char battery_buffer[8];
  s_battery_level = state.charge_percent;
  snprintf(battery_buffer, sizeof(battery_buffer), "%d%% %s", state.charge_percent, state.is_charging ? "+" : "");
  text_layer_set_text(s_battery_text_layer, battery_buffer);
  update_time();
}

/**
 * Run this if the bluetooth connection changes
 *
 * @param connected Whether we are connected or not
 */
static void bluetooth_callback(bool connected)
{
  layer_set_hidden(s_bluetooth_layer, connected);
  if(!connected)
    vibes_double_pulse();
}

/**
 * When the bluetooth layer is marked as dirty run thi
 *
 * @param layer The layer to update
 * @param ctx   The context
 */
static void bluetooth_update_proc(Layer *layer, GContext *ctx)
{
  graphics_context_set_fill_color(ctx, GColorRed);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}

/*


  graphics_context_set_fill_color(ctx, GColorLavenderIndigo);
  graphics_fill_radial(ctx, GRect((bounds.size.w >> 3) + PIE_THICKNESS, (bounds.size.h >> 3) + PIE_THICKNESS, ((3 * bounds.size.w) >> 2) - (PIE_THICKNESS << 1), ((3 * bounds.size.h) >> 2) - (PIE_THICKNESS << 1)), GOvalScaleModeFitCircle, PIE_THICKNESS >> 1, 0, s_battery_level * DEG_TO_TRIGANGLE(3.6));
 */