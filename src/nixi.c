#include <pebble.h>

#define WIDTHf 144.0f
#define WIDTH 144
#define HEIGHTf 168.0f
#define HEIGHT 168
#define STEPS_DEFAULT 7500

static Window *s_main_window;
static TextLayer *s_time_text_layer, *s_date_text_layer, *s_steps_text_layer;
static int s_battery_level, s_hour_level, s_minute_level, s_steps_level, s_steps_average;
static Layer *s_battery_layer, *s_bluetooth_layer, *s_hour_layer, *s_minute_layer, *s_steps_layer;
static bool bluetooth_connected;

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

static void update_time()
{
  // Get a tm structure
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  s_hour_level = tick_time->tm_hour;
  s_minute_level = tick_time->tm_min;

  // Write the current hours and minutes into a buffer
  static char s_buffer[8];
  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ?
                                          "%H:%M" : "%I:%M", tick_time);

  // Display this time on the TextLayer
  text_layer_set_text(s_time_text_layer, s_buffer);

    // Show the date
  static char date_buffer[16];
  strftime(date_buffer, sizeof(date_buffer), "%a %d %b", tick_time);
  text_layer_set_text(s_date_text_layer, date_buffer);


  // Test for step count:

  // Define query parameters
  const HealthMetric metric = HealthMetricStepCount;
  const HealthServiceTimeScope scope = HealthServiceTimeScopeDaily;

  // Use the average daily value from midnight to the current time
  const time_t start = time_start_of_today();
  const time_t end = time(NULL);

  // Check that an averaged value is accessible
  HealthServiceAccessibilityMask mask =
            health_service_metric_averaged_accessible(metric, start, end, scope);
  if(mask & HealthServiceAccessibilityMaskAvailable) {
    // Average is available, read it
    HealthValue average = health_service_sum_averaged(metric, start, end, scope);

    s_steps_average = (int)average;
    if (s_steps_average < 1) {
      s_steps_average = STEPS_DEFAULT;
    }
  }


  static char steps_buffer[20];

  // Check the metric has data available for today
  mask = health_service_metric_accessible(metric,
    start, end);

  if(mask & HealthServiceAccessibilityMaskAvailable) {
    s_steps_level = (int)health_service_sum_today(metric);
    snprintf(steps_buffer, sizeof(steps_buffer), "steps: %d", s_steps_level);
  } else {
    snprintf(steps_buffer, sizeof(steps_buffer), "steps: N/A");
  }
  text_layer_set_text(s_steps_text_layer, steps_buffer);

  layer_mark_dirty(s_hour_layer);
  layer_mark_dirty(s_minute_layer);
  layer_mark_dirty(s_steps_layer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed)
{
  update_time();
}

static void battery_callback(BatteryChargeState state)
{
  s_battery_level = state.charge_percent;
  layer_mark_dirty(s_battery_layer);
}

static void bluetooth_callback(bool connected) {
  // Show icon if disconnected
  bluetooth_connected = connected;
  layer_mark_dirty(s_bluetooth_layer);

  if(!connected) {
    // Issue a vibrating alert
    vibes_double_pulse();
  }
}

static void main_window_load(Window *window)
{
  // Get information about the Window
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Create hour meter Layer
  s_hour_layer = layer_create(GRect(0, 0, 25, HEIGHT));
  layer_set_update_proc(s_hour_layer, hour_update_proc);
  layer_add_child(window_get_root_layer(window), s_hour_layer);

  // Create minute meter Layer
  s_minute_layer = layer_create(GRect(119, 0, 25, HEIGHT));
  layer_set_update_proc(s_minute_layer, minute_proc_layer);
  layer_add_child(window_get_root_layer(window), s_minute_layer);

  // Create steps meter Layer
  s_steps_layer = layer_create(GRect(25, 20, 94, 30));
  layer_set_update_proc(s_steps_layer, steps_proc_layer);
  layer_add_child(window_get_root_layer(window), s_steps_layer);



  s_time_text_layer = text_layer_create(GRect(0, 52, WIDTH, 50));
  text_layer_set_background_color(s_time_text_layer, GColorClear);
  text_layer_set_text_color(s_time_text_layer, GColorBlack);
  text_layer_set_text(s_time_text_layer, "--");
  text_layer_set_font(s_time_text_layer, fonts_get_system_font(FONT_KEY_ROBOTO_BOLD_SUBSET_49));
  text_layer_set_text_alignment(s_time_text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_time_text_layer));

  s_date_text_layer = text_layer_create(GRect(0, 138, WIDTH, 30));
  text_layer_set_background_color(s_date_text_layer, GColorClear);
  text_layer_set_text_color(s_date_text_layer, GColorBlack);
  text_layer_set_text(s_date_text_layer, "--");
  text_layer_set_font(s_date_text_layer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
  text_layer_set_text_alignment(s_date_text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_date_text_layer));

  s_steps_text_layer = text_layer_create(GRect(0, 20, WIDTH, 30));
  text_layer_set_background_color(s_steps_text_layer, GColorClear);
  text_layer_set_text_color(s_steps_text_layer, GColorBlack);
  text_layer_set_text(s_steps_text_layer, "--");
  text_layer_set_font(s_steps_text_layer, fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21));
  text_layer_set_text_alignment(s_steps_text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_steps_text_layer));

  // Create battery meter Layer
  s_battery_layer = layer_create(GRect(0, 0, WIDTH, 5));
  layer_set_update_proc(s_battery_layer, battery_update_proc);
  layer_add_child(window_get_root_layer(window), s_battery_layer);

  // Create bluetooth meter Layer
  s_bluetooth_layer = layer_create(GRect(25, 102, 94, 5));
  layer_set_update_proc(s_bluetooth_layer, bluetooth_update_proc);
  layer_add_child(window_get_root_layer(window), s_bluetooth_layer);
}

static void main_window_unload(Window *window)
{
  text_layer_destroy(s_time_text_layer);
  text_layer_destroy(s_date_text_layer);
  text_layer_destroy(s_steps_text_layer);
  layer_destroy(s_battery_layer);
  layer_destroy(s_bluetooth_layer);
  layer_destroy(s_hour_layer);
  layer_destroy(s_minute_layer);
  layer_destroy(s_steps_layer);
}

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

static void hour_update_proc(Layer *layer, GContext *ctx)
{
  GRect bounds = layer_get_bounds(layer);

  // Draw the background
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Draw the bar
  graphics_context_set_fill_color(ctx, GColorPictonBlue);
  graphics_fill_rect(ctx, GRect(0, HEIGHTf - (int)(float)(((float)s_hour_level / 24.0F) * HEIGHTf), bounds.size.w, HEIGHTf), 0, GCornerNone);
}

static void minute_proc_layer(Layer *layer, GContext *ctx)
{
  GRect bounds = layer_get_bounds(layer);

  // Draw the background
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  // Draw the bar
  graphics_context_set_fill_color(ctx, GColorMagenta);
  graphics_fill_rect(ctx, GRect(0, HEIGHTf - (int)(float)(((float)s_minute_level / 60.0F) * HEIGHTf), bounds.size.w, HEIGHTf), 0, GCornerNone);
}

static void steps_proc_layer(Layer *layer, GContext *ctx)
{
  GRect bounds = layer_get_bounds(layer);

  // Draw the background
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  float l = 1.0f*s_steps_level/s_steps_average;

  // Draw the bar
  if (l >= 1.0) {
    graphics_context_set_fill_color(ctx, GColorMalachite);
    graphics_fill_rect(ctx, GRect(0, 0, bounds.size.w, bounds.size.h), 0, GCornerNone);
  } else {
    graphics_context_set_fill_color(ctx, GColorLavenderIndigo);
    graphics_fill_rect(ctx, GRect(0, 0, l*bounds.size.w, bounds.size.h), 0, GCornerNone);
  }
}

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

static void deinit()
{
  window_destroy(s_main_window);
}

int main(void)
{
  init();
  app_event_loop();
  deinit();
}