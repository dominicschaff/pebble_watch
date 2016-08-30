#include <pebble.h>

// Default value
#define STEPS_DEFAULT 7500

// Piece Sizing
#define SUB_TEXT_HEIGHT 30
#define TITLE_TEXT_HEIGHT 49
#define PIE_THICKNESS 20

// All of the global variables
static Window *s_main_window;

static TextLayer *s_time_text_layer, *s_date_text_layer;
static TextLayer *s_steps_text_layer, *s_steps_perc_text_layer, *s_steps_now_average_text_layer, *s_steps_average_text_layer;
static TextLayer *s_battery_text_layer;

static int s_hour_level, s_minute_level, s_steps_level, s_steps_average, s_steps_average_now, s_battery_level;
static Layer *s_bluetooth_layer, *s_time_layer, *s_steps_layer, *s_steps_now_layer;

// All functions used
static void update_time();
static void tick_handler(struct tm *tick_time, TimeUnits units_changed);
static void battery_callback(BatteryChargeState state);
static void main_window_load(Window *window);
static void main_window_unload(Window *window);
static void init();
static void deinit();
static void bluetooth_callback(bool connected);
static void bluetooth_update_proc(Layer *layer, GContext *ctx);
static void time_update_proc(Layer *layer, GContext *ctx);
static void steps_proc_layer(Layer *layer, GContext *ctx);
static void steps_now_proc_layer(Layer *layer, GContext *ctx);
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

  int hh = bounds.size.h>>1;
  int hw = bounds.size.w>>1;

  // Create bluetooth meter Layer
  s_bluetooth_layer = layer_create(GRect(0, 0, bounds.size.w, bounds.size.h));
  layer_set_update_proc(s_bluetooth_layer, bluetooth_update_proc);
  layer_add_child(window_layer, s_bluetooth_layer);

  // Create hour meter Layer
  s_time_layer = layer_create(GRect(0, 0, bounds.size.w, bounds.size.h));
  layer_set_update_proc(s_time_layer, time_update_proc);
  layer_add_child(window_layer, s_time_layer);

  // Create steps meter Layer
  s_steps_layer = layer_create(GRect(0, 0, hw, hh));
  layer_set_update_proc(s_steps_layer, steps_proc_layer);
  layer_add_child(window_layer, s_steps_layer);

  // Create steps meter Layer
  s_steps_now_layer = layer_create(GRect(hw, 0, hw, hh));
  layer_set_update_proc(s_steps_now_layer, steps_now_proc_layer);
  layer_add_child(window_layer, s_steps_now_layer);

  // Create the time display
  // I can't seem to calculate 52 from the other values, I will find a way (or at least in a way that makes sense)
  s_time_text_layer = text_layer_create(GRect(0, 52, bounds.size.w, TITLE_TEXT_HEIGHT));
  text_layer_set_font(s_time_text_layer, fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49));
  add_text_layer(window_layer, s_time_text_layer, GTextAlignmentCenter);


  // Create the date display
  s_date_text_layer = text_layer_create(GRect(0, 45 + TITLE_TEXT_HEIGHT, bounds.size.w, SUB_TEXT_HEIGHT));
  text_layer_set_font(s_date_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  add_text_layer(window_layer, s_date_text_layer, GTextAlignmentRight);

  // Create the steps display
  s_steps_text_layer = text_layer_create(GRect(0, bounds.size.h - SUB_TEXT_HEIGHT, bounds.size.w, SUB_TEXT_HEIGHT));
  text_layer_set_font(s_steps_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  add_text_layer(window_layer, s_steps_text_layer, GTextAlignmentLeft);

  // Create the steps percentage display
  s_steps_perc_text_layer = text_layer_create(GRect(0, bounds.size.h - SUB_TEXT_HEIGHT, bounds.size.w, SUB_TEXT_HEIGHT));
  text_layer_set_font(s_steps_perc_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  add_text_layer(window_layer, s_steps_perc_text_layer, GTextAlignmentRight);

  // Create the battery percentage display
  s_battery_text_layer = text_layer_create(GRect(0, 0, bounds.size.w, SUB_TEXT_HEIGHT));
  text_layer_set_font(s_battery_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  add_text_layer(window_layer, s_battery_text_layer, GTextAlignmentLeft);

  // Create the current average steps display
  s_steps_now_average_text_layer = text_layer_create(GRect(0, bounds.size.h - SUB_TEXT_HEIGHT - 13, bounds.size.w, SUB_TEXT_HEIGHT));
  text_layer_set_font(s_steps_now_average_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  add_text_layer(window_layer, s_steps_now_average_text_layer, GTextAlignmentLeft);

  // Create the average steps display
  s_steps_average_text_layer = text_layer_create(GRect(0, bounds.size.h - SUB_TEXT_HEIGHT - 13, bounds.size.w, SUB_TEXT_HEIGHT));
  text_layer_set_font(s_steps_average_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  add_text_layer(window_layer, s_steps_average_text_layer, GTextAlignmentRight);
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
  text_layer_destroy(s_time_text_layer);
  text_layer_destroy(s_date_text_layer);
  text_layer_destroy(s_steps_text_layer);
  text_layer_destroy(s_steps_perc_text_layer);
  text_layer_destroy(s_battery_text_layer);
  text_layer_destroy(s_steps_now_average_text_layer);
  text_layer_destroy(s_steps_average_text_layer);
  layer_destroy(s_bluetooth_layer);
  layer_destroy(s_time_layer);
  layer_destroy(s_steps_layer);
  layer_destroy(s_steps_now_layer);
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

/**
 * When the hour layer is marked as dirty run thi
 *
 * @param layer The layer to update
 * @param ctx   The context
 */
static void time_update_proc(Layer *layer, GContext *ctx)
{
  GRect bounds = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, GColorPictonBlue);
  graphics_fill_radial(ctx, GRect(0, 0, bounds.size.w, bounds.size.h), GOvalScaleModeFitCircle, PIE_THICKNESS, 0, s_hour_level * DEG_TO_TRIGANGLE(15));

  graphics_context_set_fill_color(ctx, GColorMagenta);
  graphics_fill_radial(ctx, GRect(bounds.size.w >> 3, bounds.size.h >> 3, (3 * bounds.size.w) >> 2, (3 * bounds.size.h) >> 2), GOvalScaleModeFitCircle, PIE_THICKNESS, 0, s_minute_level * DEG_TO_TRIGANGLE(6));

  graphics_context_set_fill_color(ctx, GColorLavenderIndigo);
  graphics_fill_radial(ctx, GRect((bounds.size.w >> 3) + PIE_THICKNESS, (bounds.size.h >> 3) + PIE_THICKNESS, ((3 * bounds.size.w) >> 2) - (PIE_THICKNESS << 1), ((3 * bounds.size.h) >> 2) - (PIE_THICKNESS << 1)), GOvalScaleModeFitCircle, PIE_THICKNESS >> 1, 0, s_battery_level * DEG_TO_TRIGANGLE(3.6));
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

  float l = 1.0f * s_steps_level / s_steps_average;

  graphics_context_set_fill_color(ctx, l >= 1.0 ? GColorMalachite : GColorShockingPink);
  graphics_fill_radial(ctx, GRect(0, 0, bounds.size.w, bounds.size.h), GOvalScaleModeFitCircle, PIE_THICKNESS >> 1, 0, l * DEG_TO_TRIGANGLE(360));

  l = 1.0f * s_steps_average_now / s_steps_average;
  if (l <= 1.0) {
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_radial(ctx, GRect(0, 0, bounds.size.w, bounds.size.h), GOvalScaleModeFitCircle, PIE_THICKNESS << 1, l * DEG_TO_TRIGANGLE(360), l * DEG_TO_TRIGANGLE(360) + DEG_TO_TRIGANGLE(5));
  } else {
    graphics_context_set_fill_color(ctx, GColorFolly);
    graphics_fill_radial(ctx, GRect(bounds.size.w >> 3, bounds.size.h >> 3, (3 * bounds.size.w) >> 2, (3 * bounds.size.h) >> 2), GOvalScaleModeFitCircle, PIE_THICKNESS >> 2, 0, DEG_TO_TRIGANGLE(360));
  }
}

/**
 * When the steps layer is marked as dirty run thi
 *
 * @param layer The layer to update
 * @param ctx   The context
 */
static void steps_now_proc_layer(Layer *layer, GContext *ctx)
{
  GRect bounds = layer_get_bounds(layer);

  float l = 1.0f * s_steps_level / s_steps_average_now;

  l = DEG_TO_TRIGANGLE((((l > 1.3) ? 1.3 : ((l < 0.7) ? 0.7 : l)) - 1.0) * 180);

  // draw the meters:
  graphics_context_set_fill_color(ctx, GColorFolly);
  graphics_fill_radial(ctx, GRect(0, 0, bounds.size.w, bounds.size.h), GOvalScaleModeFitCircle, PIE_THICKNESS, DEG_TO_TRIGANGLE(0), DEG_TO_TRIGANGLE(5));
  graphics_fill_radial(ctx, GRect(0, 0, bounds.size.w, bounds.size.h), GOvalScaleModeFitCircle, PIE_THICKNESS, DEG_TO_TRIGANGLE(355), DEG_TO_TRIGANGLE(360));
  graphics_fill_radial(ctx, GRect(0, 0, bounds.size.w, bounds.size.h), GOvalScaleModeFitCircle, PIE_THICKNESS, DEG_TO_TRIGANGLE(40), DEG_TO_TRIGANGLE(50));
  graphics_fill_radial(ctx, GRect(0, 0, bounds.size.w, bounds.size.h), GOvalScaleModeFitCircle, PIE_THICKNESS, DEG_TO_TRIGANGLE(310), DEG_TO_TRIGANGLE(320));

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_radial(ctx, GRect(0, 0, bounds.size.w, bounds.size.h), GOvalScaleModeFitCircle, PIE_THICKNESS << 1, l, l + DEG_TO_TRIGANGLE(5));
}

/**
 * When the time is updated run this to update the various components
 */
static void update_time()
{
  static char s_buffer[8];
  static char date_buffer[16];
  static char steps_buffer[10];
  static char steps_perc_buffer[10];
  static char steps_now_buffer[15];
  static char steps_average_buffer[10];

  // Get the current time
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  // Save the minute and hour (for the bars)
  s_hour_level = tick_time->tm_hour;
  s_minute_level = tick_time->tm_min;

  // Create the string for the time display
  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ?"%H:%M" : "%I:%M", tick_time);
  text_layer_set_text(s_time_text_layer, s_buffer);

  // Create the string for the date display
  strftime(date_buffer, sizeof(date_buffer), "%a %d %b", tick_time);
  text_layer_set_text(s_date_text_layer, date_buffer);

  s_steps_average = (int)health_service_sum_averaged(HealthMetricStepCount, time_start_of_today(), time_start_of_today() + SECONDS_PER_DAY, HealthServiceTimeScopeDaily);
  if (s_steps_average < 1)
    s_steps_average = STEPS_DEFAULT;

  s_steps_average_now = (int)health_service_sum_averaged(HealthMetricStepCount, time_start_of_today(), time(NULL), HealthServiceTimeScopeDaily);
  if (s_steps_average_now < 1)
    s_steps_average_now = STEPS_DEFAULT;

  s_steps_level = (int)health_service_sum_today(HealthMetricStepCount);

  if (s_steps_level > 1000)
    snprintf(steps_buffer, sizeof(steps_buffer), "%d,%03d", s_steps_level / 1000, s_steps_level % 1000);
  else
    snprintf(steps_buffer, sizeof(steps_buffer), "%d", s_steps_level);

  if (s_steps_average > 1000)
    snprintf(steps_average_buffer, sizeof(steps_average_buffer), "/%d,%03d", s_steps_average / 1000, s_steps_average % 1000);
  else
    snprintf(steps_average_buffer, sizeof(steps_average_buffer), "/%d", s_steps_average);

  if (s_steps_average_now > 1000)
    snprintf(steps_now_buffer, sizeof(steps_now_buffer), "%d,%03d", s_steps_average_now / 1000, s_steps_average_now % 1000);
  else
    snprintf(steps_now_buffer, sizeof(steps_now_buffer), "%d", s_steps_average_now);

  snprintf(steps_perc_buffer, sizeof(steps_perc_buffer), "%d%%", (int)(100.0f * s_steps_level / s_steps_average));

  // Set the steps display
  text_layer_set_text(s_steps_text_layer, steps_buffer);
  text_layer_set_text(s_steps_perc_text_layer, steps_perc_buffer);
  text_layer_set_text(s_steps_now_average_text_layer, steps_now_buffer);
  text_layer_set_text(s_steps_average_text_layer, steps_average_buffer);

  // Mark the layers as dirty
  layer_mark_dirty(s_time_layer);
  layer_mark_dirty(s_steps_layer);
  layer_mark_dirty(s_steps_now_layer);
}
