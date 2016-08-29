#include <pebble.h>

// Screen sizing for pebble time
#define WIDTHf 144.0f
#define WIDTH 144
#define HEIGHTf 168.0f
#define HEIGHT 168

// Default value
#define STEPS_DEFAULT 7500

// Piece Sizing
#define BATTERY_HEIGHT 5
#define TIME_BAR_WIDTH 20
#define BT_ERROR_THICKNESS 5
#define PADDING 5
#define SUB_TEXT_HEIGHT 30
#define TITLE_TEXT_HEIGHT 50

// All of the global variables
static Window *s_main_window;
static TextLayer *s_time_text_layer, *s_date_text_layer, *s_steps_text_layer, *s_steps_perc_text_layer;
static int s_battery_level, s_hour_level, s_minute_level, s_steps_level, s_steps_average;
static Layer *s_battery_layer, *s_bluetooth_layer, *s_hour_layer, *s_minute_layer, *s_steps_layer;
static bool bluetooth_connected;

// All functions used
static void update_time();
static void tick_handler(struct tm *tick_time, TimeUnits units_changed);
static void battery_callback(BatteryChargeState state);
static void main_window_load(Window *window);
static void main_window_unload(Window *window);
static void battery_update_proc(Layer *layer, GContext *ctx);
static void init();
static void deinit();
static void bluetooth_callback(bool connected);
static void bluetooth_update_proc(Layer *layer, GContext *ctx);
static void hour_update_proc(Layer *layer, GContext *ctx);
static void minute_proc_layer(Layer *layer, GContext *ctx);
static void steps_proc_layer(Layer *layer, GContext *ctx);
static void add_text_layer(Layer *window_layer, TextLayer *text_layer);
static void add_text_layer_right(Layer *window_layer, TextLayer *text_layer);
static void add_text_layer_left(Layer *window_layer, TextLayer *text_layer);

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
  s_steps_average = STEPS_DEFAULT;
  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers)
  {
    .load = main_window_load,
    .unload = main_window_unload
  });
  window_stack_push(s_main_window, true);

  update_time();
  battery_callback(battery_state_service_peek());
  bluetooth_callback(connection_service_peek_pebble_app_connection());

  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = bluetooth_callback
  });
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_callback);
}

/**
 * This functions only job is to request that the window must be destroyed
 */
static void deinit()
{
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

  // Create bluetooth meter Layer
  s_bluetooth_layer = layer_create(GRect(TIME_BAR_WIDTH, bounds.size.w/2 - TITLE_TEXT_HEIGHT/2, WIDTH - TIME_BAR_WIDTH*2, TITLE_TEXT_HEIGHT));
  layer_set_update_proc(s_bluetooth_layer, bluetooth_update_proc);
  layer_add_child(window_get_root_layer(window), s_bluetooth_layer);

  // Create steps meter Layer
  s_steps_layer = layer_create(GRect(TIME_BAR_WIDTH, BATTERY_HEIGHT + PADDING, bounds.size.w - TIME_BAR_WIDTH*2, SUB_TEXT_HEIGHT + PADDING*4));
  layer_set_update_proc(s_steps_layer, steps_proc_layer);
  layer_add_child(window_get_root_layer(window), s_steps_layer);

  // Create hour meter Layer
  s_hour_layer = layer_create(GRect(0, HEIGHT/2, bounds.size.w/2, HEIGHT/2));
  layer_set_update_proc(s_hour_layer, hour_update_proc);
  layer_add_child(window_get_root_layer(window), s_hour_layer);

  // Create minute meter Layer
  s_minute_layer = layer_create(GRect(WIDTH/2, HEIGHT/2, bounds.size.w/2, HEIGHT/2));
  layer_set_update_proc(s_minute_layer, minute_proc_layer);
  layer_add_child(window_get_root_layer(window), s_minute_layer);

  // Create the time display
  // I can't seem to calculate 52 from the other values, I will find a way (or at least in a way that makes sense)
  s_time_text_layer = text_layer_create(GRect(0, 52, WIDTH, TITLE_TEXT_HEIGHT));
  text_layer_set_font(s_time_text_layer, fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49));
  add_text_layer(window_layer, s_time_text_layer);

  // Create the date display
  s_date_text_layer = text_layer_create(GRect(0, 52 + TITLE_TEXT_HEIGHT, WIDTH, SUB_TEXT_HEIGHT));
  text_layer_set_font(s_date_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  add_text_layer_right(window_layer, s_date_text_layer);

  // Create the steps display
  s_steps_text_layer = text_layer_create(GRect(0, HEIGHT - SUB_TEXT_HEIGHT, WIDTH, SUB_TEXT_HEIGHT));
  text_layer_set_font(s_steps_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  add_text_layer_left(window_layer, s_steps_text_layer);

  // Create the steps percentage display
  s_steps_perc_text_layer = text_layer_create(GRect(0, HEIGHT - SUB_TEXT_HEIGHT, WIDTH, SUB_TEXT_HEIGHT));
  text_layer_set_font(s_steps_perc_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  add_text_layer_right(window_layer, s_steps_perc_text_layer);

  // Create battery meter Layer
  s_battery_layer = layer_create(GRect(0, 0, WIDTH, BATTERY_HEIGHT));
  layer_set_update_proc(s_battery_layer, battery_update_proc);
  layer_add_child(window_get_root_layer(window), s_battery_layer);
}

static void add_text_layer(Layer *window_layer, TextLayer *text_layer)
{
  text_layer_set_background_color(text_layer, GColorClear);
  text_layer_set_text_color(text_layer, GColorBlack);
  text_layer_set_text(text_layer, "--");
  text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(text_layer));
}

static void add_text_layer_right(Layer *window_layer, TextLayer *text_layer)
{
  text_layer_set_background_color(text_layer, GColorClear);
  text_layer_set_text_color(text_layer, GColorBlack);
  text_layer_set_text(text_layer, "--");
  text_layer_set_text_alignment(text_layer, GTextAlignmentRight);
  layer_add_child(window_layer, text_layer_get_layer(text_layer));
}

static void add_text_layer_left(Layer *window_layer, TextLayer *text_layer)
{
  text_layer_set_background_color(text_layer, GColorClear);
  text_layer_set_text_color(text_layer, GColorBlack);
  text_layer_set_text(text_layer, "--");
  text_layer_set_text_alignment(text_layer, GTextAlignmentLeft);
  layer_add_child(window_layer, text_layer_get_layer(text_layer));
}

/**
 * Destroy the window
 *
 * @param window The window to destroy
 */
static void main_window_unload(Window *window)
{
  text_layer_destroy(s_time_text_layer);
  text_layer_destroy(s_date_text_layer);
  text_layer_destroy(s_steps_text_layer);
  text_layer_destroy(s_steps_perc_text_layer);
  layer_destroy(s_battery_layer);
  layer_destroy(s_bluetooth_layer);
  layer_destroy(s_hour_layer);
  layer_destroy(s_minute_layer);
  layer_destroy(s_steps_layer);
}

/**
 * On every tick run this function
 *
 * @param tick_time     The orriginal time
 * @param units_changed What units changed
 */
static void tick_handler(struct tm *tick_time, TimeUnits units_changed)
{
  update_time();
}

/**
 * Run this if the battery level changes
 *
 * @param state The current battery state
 */
static void battery_callback(BatteryChargeState state)
{
  s_battery_level = state.charge_percent;
  layer_mark_dirty(s_battery_layer);
}

/**
 * Run this if the bluetooth connection changes
 *
 * @param connected Whether we are connected or not
 */
static void bluetooth_callback(bool connected) {
  // Show icon if disconnected
  bluetooth_connected = connected;
  layer_mark_dirty(s_bluetooth_layer);

  if(!connected) {
    // Issue a vibrating alert
    vibes_double_pulse();
  }
}

/**
 * When the battery layer is marked as dirty run this
 *
 * @param layer The layer to update
 * @param ctx   The context
 */
static void battery_update_proc(Layer *layer, GContext *ctx)
{
  GRect bounds = layer_get_bounds(layer);

  // Draw the background
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Draw the bar
  graphics_context_set_fill_color(ctx, GColorBlue);
  graphics_fill_rect(ctx, GRect(0, 0, (int)(float)(((float)s_battery_level / 100.0F) * WIDTHf), bounds.size.h), 0, GCornerNone);
}

/**
 * When the bluetooth layer is marked as dirty run thi
 *
 * @param layer The layer to update
 * @param ctx   The context
 */
static void bluetooth_update_proc(Layer *layer, GContext *ctx)
{
  GRect bounds = layer_get_bounds(layer);

  // Draw the background
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Draw the bar
  graphics_context_set_fill_color(ctx, GColorRed);
  graphics_fill_rect(ctx, GRect(0, 0, bluetooth_connected ? 0 : bounds.size.w, bounds.size.h), 0, GCornerNone);
}

/**
 * When the hour layer is marked as dirty run thi
 *
 * @param layer The layer to update
 * @param ctx   The context
 */
static void hour_update_proc(Layer *layer, GContext *ctx)
{
  GRect bounds = layer_get_bounds(layer);

  // Draw the background
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  float l = s_minute_level / 24.0f;
  
  graphics_context_set_fill_color(ctx, GColorPictonBlue);
  graphics_fill_radial(ctx, GRect(0, 0, bounds.size.w, bounds.size.h), GOvalScaleModeFitCircle, 20, 0, DEG_TO_TRIGANGLE(l*360));
}

/**
 * When the minute layer is marked as dirty run thi
 *
 * @param layer The layer to update
 * @param ctx   The context
 */
static void minute_proc_layer(Layer *layer, GContext *ctx)
{
  GRect bounds = layer_get_bounds(layer);

  // Draw the background
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  
  float l = s_minute_level / 60.0f;
  
  graphics_context_set_fill_color(ctx, GColorMagenta);
  graphics_fill_radial(ctx, GRect(0, 0, bounds.size.w, bounds.size.h), GOvalScaleModeFitCircle, 20, 0, DEG_TO_TRIGANGLE(l*360));
}

/**
 * When the steps layer is marked as dirty run thi
 *
 * @param layer The layer to update
 * @param ctx   The context
 */
static void steps_proc_layer(Layer *layer, GContext *ctx)
{
  GRect bounds = layer_get_bounds(layer);

  // Draw the background
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  float l = 1.0f*s_steps_level/s_steps_average;

  graphics_context_set_fill_color(ctx, l >= 1.0 ? GColorMalachite : GColorShockingPink);
  graphics_fill_radial(ctx, GRect(0, 0, bounds.size.w, bounds.size.h), GOvalScaleModeFitCircle, SUB_TEXT_HEIGHT/2, 0, DEG_TO_TRIGANGLE(l*360));
}

/**
 * When the time is updated run this to update the various components
 */
static void update_time()
{
  // Get the current time
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  // Save the minute and hour (for the bars)
  s_hour_level = tick_time->tm_hour;
  s_minute_level = tick_time->tm_min;

  // Create the string for the time display
  static char s_buffer[8];
  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ?"%H:%M" : "%I:%M", tick_time);
  text_layer_set_text(s_time_text_layer, s_buffer);

  // Create the string for the date display
  static char date_buffer[16];
  strftime(date_buffer, sizeof(date_buffer), "%a %d %b", tick_time);
  text_layer_set_text(s_date_text_layer, date_buffer);

  // Now for the health data

  static char steps_buffer[10];
  static char steps_perc_buffer[10];

  // Can we get the info, if we can get it:
  s_steps_average = (int)health_service_sum_averaged(HealthMetricStepCount, time_start_of_today(), time_start_of_today() + SECONDS_PER_DAY, HealthServiceTimeScopeDaily);
  if (s_steps_average < 1) {
    s_steps_average = STEPS_DEFAULT;
  }

  // Can we get the info, if we can get it:
  s_steps_level = (int)health_service_sum_today(HealthMetricStepCount);
  int step_perc = (int)(100.0f*s_steps_level/s_steps_average);
  if (s_steps_level > 1000) {
    snprintf(steps_buffer, sizeof(steps_buffer), "%d,%03d", s_steps_level/1000, s_steps_level%1000);
  } else {
    snprintf(steps_buffer, sizeof(steps_buffer), "%d", s_steps_level);
  }
  if (step_perc > 1000) {
    snprintf(steps_perc_buffer, sizeof(steps_buffer), "%d,%03d%%", step_perc/1000, step_perc%1000);
  } else {
    snprintf(steps_perc_buffer, sizeof(steps_buffer), "%d%%", step_perc);
  }
  

  // Set the steps display
  text_layer_set_text(s_steps_text_layer, steps_buffer);
  text_layer_set_text(s_steps_perc_text_layer, steps_perc_buffer);

  // Mark the layers as dirty
  layer_mark_dirty(s_hour_layer);
  layer_mark_dirty(s_minute_layer);
  layer_mark_dirty(s_steps_layer);
}
