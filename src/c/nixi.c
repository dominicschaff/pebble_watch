#include <pebble.h>
#include "utilities.h"
// Default value
#define STEPS_DEFAULT 1000

// Piece Sizing
#define SUB_TEXT_HEIGHT 30
#define TITLE_TEXT_HEIGHT 49
#define PIE_THICKNESS 10

static AppSync s_sync;
static uint8_t s_sync_buffer[64];

static Window *s_main_window;

static GFont s_time_font;

static Layer *background_layer;
static Layer *steps_layer, *steps_now_layer;
static Layer *hour_layer, *minute_layer;

static TextLayer *battery_text_layer, *location_text_layer;

static TextLayer *steps_text_layer, *steps_perc_text_layer, *steps_now_average_text_layer, *steps_average_text_layer;

static TextLayer *time_hour_text_layer, *time_minute_text_layer, *date_text_layer, *day_text_layer;

static int current_hour, current_minute, current_time_minutes;

static int current_steps, steps_day_average, steps_average_now;

static int battery_level;
static int phone_battery = -1;
static bool bluetooth_connected = false, battery_charging = false, phone_battery_charging = false;

static bool dayTime = true;

static double lat = 0.0, lon = 0.0;
static int sunriseMinutes = 0, sunsetMinutes = 1500;

static void main_window_load(Window *window);
static void main_window_unload(Window *window);

static void battery_callback(BatteryChargeState state);
static void bluetooth_callback(bool connected);

static void bluetooth_update_proc(Layer *layer, GContext *ctx);
static void steps_now_proc_layer(Layer *layer, GContext *ctx);
static void steps_proc_layer(Layer *layer, GContext *ctx);
static void time_hour_update_proc(Layer *layer, GContext *ctx);
static void time_minute_update_proc(Layer *layer, GContext *ctx);

static void tick_handler(struct tm *tick_time, TimeUnits units_changed);

static void add_text_layer(Layer *window_layer, TextLayer *text_layer, GTextAlignment alignment);
static void update_watch();
static void update_health();
static void setTextColour();
static void battery_update();

static void update_location()
{
  static char location_buffer[20];
  if (lat == 0 && lon == 0) {
      text_layer_set_text(location_text_layer, "Locating...");
    return;
  }

  time_t temp = time(NULL);
  struct tm *local_time = localtime(&temp);
  struct tm *gmt_time = gmtime(&temp);

  int tz = local_time->tm_hour - gmt_time->tm_hour;

  if (gmt_time->tm_yday < local_time->tm_yday) {
    tz += 24;
  } else if (gmt_time->tm_yday > local_time->tm_yday) {
    tz -= 24;
  }

  float sunriseTime = calcSunRise(2016, 10, 28, lat, lon, ZENITH_OFFICIAL) + tz;
  float sunsetTime = calcSunSet(2016, 10, 28, lat, lon, ZENITH_OFFICIAL) + tz;

  int sunriseHour = (int)sunriseTime;
  int sunriseMinute = (int)((((int)(sunriseTime*100))%100)*0.6);
  int sunsetHour = (int)sunsetTime;
  int sunsetMinute = (int)((((int)(sunsetTime*100))%100)*0.6);

  sunriseMinutes = sunriseHour * 60 + sunriseMinute;
  sunsetMinutes = sunsetHour * 60 + sunsetMinute;

  snprintf(location_buffer, sizeof(location_buffer),
           "%d:%d - %d:%d",
           sunriseHour, sunriseMinute,
           sunsetHour, sunsetMinute);
  text_layer_set_text(location_text_layer, location_buffer);
  update_watch();
}

static void inbox_received_callback(DictionaryIterator *iter, void *context) {
  Tuple *tup;

  tup = dict_find(iter, MESSAGE_KEY_Latitude);
  if(tup) {
    lat = atof(tup->value->cstring);
    update_location();
  }

  tup = dict_find(iter, MESSAGE_KEY_Longitude);
  if(tup) {
    lon = atof(tup->value->cstring);
    update_location();
  }

  tup = dict_find(iter, MESSAGE_KEY_PhoneBattery);
  if(tup) {
    phone_battery = tup->value->int32;
    battery_update();
  }

  tup = dict_find(iter, MESSAGE_KEY_PhoneBatteryCharging);
  if(tup) {
    phone_battery_charging = tup->value->int32 == 1;
    battery_update();
  }
}

static void request_data(void)
{
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);

  if (!iter)
    return;

  int value = 1;
  dict_write_int(iter, 1, &value, sizeof(int), true);
  dict_write_end(iter);

  app_message_outbox_send();
}

/**
 * Main function, the watch runs this function
 */
int main(void)
{
  setlocale(LC_ALL, "");

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

  app_message_open(64, 64);
  app_message_register_inbox_received(inbox_received_callback);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  steps_day_average = STEPS_DEFAULT;

  battery_callback(battery_state_service_peek());
  battery_state_service_subscribe(battery_callback);

  update_watch();
  update_health();
  update_location();
  request_data();

  app_event_loop();

  window_destroy(s_main_window);
  app_sync_deinit(&s_sync);
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
  background_layer = layer_create(bounds);
  layer_set_update_proc(background_layer, bluetooth_update_proc);
  layer_add_child(window_layer, background_layer);

  // Create hour meter Layer
  minute_layer = layer_create(GRect(PIE_THICKNESS, PIE_THICKNESS, bounds.size.w - (PIE_THICKNESS<<1), bounds.size.h - (PIE_THICKNESS<<1)));
  layer_set_update_proc(minute_layer, time_minute_update_proc);
  layer_add_child(window_layer, minute_layer);

  // Create hour meter Layer
  hour_layer = layer_create(GRect(bounds.size.w >> 3, bounds.size.h >> 3, (3 * bounds.size.w) >> 2, (3 * bounds.size.h) >> 2));
  layer_set_update_proc(hour_layer, time_hour_update_proc);
  layer_add_child(window_layer, hour_layer);

  // Create steps meter Layer
  steps_layer = layer_create(bounds);
  layer_set_update_proc(steps_layer, steps_proc_layer);
  layer_add_child(window_layer, steps_layer);

  // Create now steps meter Layer
  steps_now_layer = layer_create(GRect(hw>>1, hh>>1, hw, hh));
  layer_set_update_proc(steps_now_layer, steps_now_proc_layer);
  layer_add_child(window_layer, steps_now_layer);

  s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_PIXELS_49));

  // Create the time display
  time_hour_text_layer = text_layer_create(GRect(0, SUB_TEXT_HEIGHT, bounds.size.w, TITLE_TEXT_HEIGHT));
  text_layer_set_font(time_hour_text_layer, s_time_font);
  add_text_layer(window_layer, time_hour_text_layer, GTextAlignmentCenter);

  time_minute_text_layer = text_layer_create(GRect(0, SUB_TEXT_HEIGHT +  TITLE_TEXT_HEIGHT, bounds.size.w, TITLE_TEXT_HEIGHT));
  text_layer_set_font(time_minute_text_layer, s_time_font);
  add_text_layer(window_layer, time_minute_text_layer, GTextAlignmentCenter);

  // Create the date display
  date_text_layer = text_layer_create(GRect(0, -5, bounds.size.w, SUB_TEXT_HEIGHT));
  text_layer_set_font(date_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  add_text_layer(window_layer, date_text_layer, GTextAlignmentLeft);

  // Create the date display
  day_text_layer = text_layer_create(GRect(0, SUB_TEXT_HEIGHT - 13, bounds.size.w, SUB_TEXT_HEIGHT));
  text_layer_set_font(day_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  add_text_layer(window_layer, day_text_layer, GTextAlignmentLeft);

  // Create the steps display
  steps_text_layer = text_layer_create(GRect(0, bounds.size.h - SUB_TEXT_HEIGHT, bounds.size.w, SUB_TEXT_HEIGHT));
  text_layer_set_font(steps_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  add_text_layer(window_layer, steps_text_layer, GTextAlignmentLeft);

  // Create the steps percentage display
  steps_perc_text_layer = text_layer_create(GRect(0, bounds.size.h - SUB_TEXT_HEIGHT, bounds.size.w, SUB_TEXT_HEIGHT));
  text_layer_set_font(steps_perc_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  add_text_layer(window_layer, steps_perc_text_layer, GTextAlignmentRight);

  // Create the current average steps display
  steps_now_average_text_layer = text_layer_create(GRect(0, bounds.size.h - SUB_TEXT_HEIGHT - 13, bounds.size.w, SUB_TEXT_HEIGHT));
  text_layer_set_font(steps_now_average_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  add_text_layer(window_layer, steps_now_average_text_layer, GTextAlignmentLeft);

  // Create the average steps display
  steps_average_text_layer = text_layer_create(GRect(0, bounds.size.h - SUB_TEXT_HEIGHT - 13, bounds.size.w, SUB_TEXT_HEIGHT));
  text_layer_set_font(steps_average_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  add_text_layer(window_layer, steps_average_text_layer, GTextAlignmentRight);

  // Create the battery percentage display
  battery_text_layer = text_layer_create(GRect(0, -5, bounds.size.w, SUB_TEXT_HEIGHT));
  text_layer_set_font(battery_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  add_text_layer(window_layer, battery_text_layer, GTextAlignmentRight);

  location_text_layer = text_layer_create(GRect(0, SUB_TEXT_HEIGHT - 13, bounds.size.w, SUB_TEXT_HEIGHT));
  text_layer_set_font(location_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  add_text_layer(window_layer, location_text_layer, GTextAlignmentRight);
}

/**
 * Destroy the window
 *
 * @param window The window to destroy
 */
static void main_window_unload(Window *window)
{
  text_layer_destroy(time_hour_text_layer);
  text_layer_destroy(time_minute_text_layer);
  text_layer_destroy(date_text_layer);
  text_layer_destroy(day_text_layer);
  text_layer_destroy(location_text_layer);
  layer_destroy(hour_layer);
  layer_destroy(minute_layer);
  fonts_unload_custom_font(s_time_font);

  text_layer_destroy(steps_text_layer);
  text_layer_destroy(steps_perc_text_layer);
  text_layer_destroy(steps_now_average_text_layer);
  text_layer_destroy(steps_average_text_layer);
  layer_destroy(steps_layer);
  layer_destroy(steps_now_layer);

  text_layer_destroy(battery_text_layer);
  layer_destroy(background_layer);
}

/**
 * Run this if the battery level changes
 *
 * @param state The current battery state
 */
static void battery_callback(BatteryChargeState state)
{
  battery_level = state.charge_percent;
  battery_charging = state.is_charging;
  battery_update();
}

static void battery_update()
{
  static char battery_buffer[15];
  if (phone_battery > -1) {
    snprintf(battery_buffer, sizeof(battery_buffer), "%s%d%%:%s%d%%", battery_charging ? "+" : "", battery_level, phone_battery_charging ? "+" : "", phone_battery);
  } else {
    snprintf(battery_buffer, sizeof(battery_buffer), "%s%d%%", battery_charging ? "+" : "", battery_level);
  }
  text_layer_set_text(battery_text_layer, battery_buffer);
}

/**
 * Run this if the bluetooth connection changes
 *
 * @param connected Whether we are connected or not
 */
static void bluetooth_callback(bool connected)
{
  bluetooth_connected = connected;
  layer_mark_dirty(background_layer);

  if(!connected)
    vibes_double_pulse();
}

/**
 * On every tick run this function
 *
 * @param tick_time     The orriginal time
 * @param units_changed What units changed
 */
static void tick_handler(struct tm *tick_time, TimeUnits units_changed)
{
  update_watch();
}

/**
 * When the bluetooth layer is marked as dirty run this
 *
 * @param layer The layer to update
 * @param ctx   The context
 */
static void bluetooth_update_proc(Layer *layer, GContext *ctx)
{
  graphics_context_set_fill_color(ctx, bluetooth_connected ? (dayTime ? GColorWhite : GColorBlack) : GColorRed);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}

/**
 * When the hour layer is marked as dirty run thi
 *
 * @param layer The layer to update
 * @param ctx   The context
 */
static void time_hour_update_proc(Layer *layer, GContext *ctx)
{
  if (!dayTime) return;
  graphics_context_set_fill_color(ctx, GColorMagenta);
  graphics_fill_radial(ctx, layer_get_bounds(layer), GOvalScaleModeFitCircle, PIE_THICKNESS, 0, (current_hour % 12) * DEG_TO_TRIGANGLE(30));
}

/**
 * When the minute layer is marked as dirty run thi
 *
 * @param layer The layer to update
 * @param ctx   The context
 */
static void time_minute_update_proc(Layer *layer, GContext *ctx)
{
  if (!dayTime) return;
  graphics_context_set_fill_color(ctx, GColorPictonBlue);
  graphics_fill_radial(ctx, layer_get_bounds(layer), GOvalScaleModeFitCircle, PIE_THICKNESS, 0, current_minute * DEG_TO_TRIGANGLE(6));
}

/**
 * When the steps layer is marked as dirty run this
 *
 * @param layer The layer to update
 * @param ctx   The context
 */
static void steps_proc_layer(Layer *layer, GContext *ctx)
{
  if (!dayTime) return;
  GRect bounds = layer_get_bounds(layer);

  float l = 1.0f * current_steps / steps_day_average;

  graphics_context_set_fill_color(ctx, l >= 1.0 ? GColorMalachite : GColorShockingPink);
  graphics_fill_radial(ctx, bounds, GOvalScaleModeFitCircle, PIE_THICKNESS, 0, l * DEG_TO_TRIGANGLE(360));

  l = 1.0f * steps_average_now / steps_day_average;
  graphics_context_set_fill_color(ctx, GColorDukeBlue);
  graphics_fill_radial(ctx, bounds, GOvalScaleModeFitCircle, PIE_THICKNESS >> 2, 0, l * DEG_TO_TRIGANGLE(360));
}

/**
 * When the steps layer is marked as dirty run this
 *
 * @param layer The layer to update
 * @param ctx   The context
 */
static void steps_now_proc_layer(Layer *layer, GContext *ctx)
{
  if (!dayTime) return;
  GRect bounds = layer_get_bounds(layer);

  float l = 1.0f * current_steps / steps_average_now;

  l = DEG_TO_TRIGANGLE((((l > 2) ? 1.0 : ((l < 0) ? -1.0 : l - 1.0))) * 180);

  graphics_context_set_fill_color(ctx, GColorLavenderIndigo);
  if (l < 0) {
    graphics_fill_radial(ctx, bounds, GOvalScaleModeFitCircle, PIE_THICKNESS, DEG_TO_TRIGANGLE(360) + l, DEG_TO_TRIGANGLE(360));
  } else {
    graphics_fill_radial(ctx, bounds, GOvalScaleModeFitCircle, PIE_THICKNESS, 0, l);
  }

}

/* Watch update for tick: */

/**
 * When the time is updated run this to update the various components
 */
static void update_watch()
{
  static char s_hour_buffer[4];
  static char s_minute_buffer[4];
  static char date_buffer[10];
  static char day_buffer[5];
  int tmp_hour = current_hour;

  // Get the current time
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  // Save the minute and hour (for the bars)
  current_hour = tick_time->tm_hour;
  current_minute = tick_time->tm_min;
  current_time_minutes = current_hour * 60 + current_minute;

  // Create the string for the time display
  strftime(s_hour_buffer, sizeof(s_hour_buffer), clock_is_24h_style() ? "%H" : "%I", tick_time);
  text_layer_set_text(time_hour_text_layer, s_hour_buffer);
  strftime(s_minute_buffer, sizeof(s_minute_buffer), "%M", tick_time);
  text_layer_set_text(time_minute_text_layer, s_minute_buffer);

  // Create the string for the date display
  strftime(date_buffer, sizeof(date_buffer), "%d %b", tick_time);
  strftime(day_buffer, sizeof(day_buffer), "%a", tick_time);
  text_layer_set_text(date_text_layer, date_buffer);
  text_layer_set_text(day_text_layer, day_buffer);

  if (current_minute & 1)
    update_health();

  if (current_hour == 0) {
    request_data();
  }

  bool t = current_time_minutes <= sunsetMinutes && current_time_minutes >= sunriseMinutes;

  if (dayTime != t) {
    dayTime = t;
    setTextColour();
  }

  // Mark the layers as dirty
  if (tmp_hour != current_hour) layer_mark_dirty(hour_layer);
  layer_mark_dirty(minute_layer);
}

static void update_health()
{
  static char steps_buffer[10];
  static char steps_perc_buffer[10];
  static char steps_now_buffer[10];
  static char steps_average_buffer[10];

  int start = time_start_of_today();

  steps_day_average = (int)health_service_sum_averaged(HealthMetricStepCount, start, start + SECONDS_PER_DAY, HealthServiceTimeScopeDailyWeekdayOrWeekend);
  if (steps_day_average < 1) steps_day_average = STEPS_DEFAULT;

  steps_average_now = (int)health_service_sum_averaged(HealthMetricStepCount, start, time(NULL), HealthServiceTimeScopeDailyWeekdayOrWeekend);

  if (steps_average_now < 1) steps_average_now = STEPS_DEFAULT;

  current_steps = (int)health_service_sum_today(HealthMetricStepCount);

  format_number(steps_buffer, sizeof(steps_buffer), current_steps);
  format_number(steps_average_buffer, sizeof(steps_average_buffer), steps_day_average);
  format_number(steps_now_buffer, sizeof(steps_now_buffer), steps_average_now);

  snprintf(steps_perc_buffer, sizeof(steps_perc_buffer), "%d%%", (int)(100.0f * current_steps / steps_day_average));

  // Set the steps display
  text_layer_set_text(steps_text_layer, steps_buffer);
  text_layer_set_text(steps_perc_text_layer, steps_perc_buffer);
  text_layer_set_text(steps_now_average_text_layer, steps_now_buffer);
  text_layer_set_text(steps_average_text_layer, steps_average_buffer);

  if (dayTime) {
    layer_mark_dirty(steps_layer);
    layer_mark_dirty(steps_now_layer);
  }
}

/* Helper functions */

static void add_text_layer(Layer *window_layer, TextLayer *text_layer, GTextAlignment alignment)
{
  text_layer_set_background_color(text_layer, GColorClear);
  text_layer_set_text_color(text_layer, GColorBlack);
  text_layer_set_text_alignment(text_layer, alignment);
  layer_add_child(window_layer, text_layer_get_layer(text_layer));
}

static void setTextColour()
{
  text_layer_set_text_color(time_hour_text_layer, dayTime ? GColorBlack : GColorWhite);
  text_layer_set_text_color(time_minute_text_layer, dayTime ? GColorBlack : GColorWhite);
  text_layer_set_text_color(date_text_layer, dayTime ? GColorBlack : GColorWhite);
  text_layer_set_text_color(day_text_layer, dayTime ? GColorBlack : GColorWhite);
  text_layer_set_text_color(location_text_layer, dayTime ? GColorBlack : GColorWhite);
  text_layer_set_text_color(steps_text_layer, dayTime ? GColorBlack : GColorWhite);
  text_layer_set_text_color(steps_perc_text_layer, dayTime ? GColorBlack : GColorWhite);
  text_layer_set_text_color(steps_now_average_text_layer, dayTime ? GColorBlack : GColorWhite);
  text_layer_set_text_color(steps_average_text_layer, dayTime ? GColorBlack : GColorWhite);
  text_layer_set_text_color(battery_text_layer, dayTime ? GColorBlack : GColorWhite);
  layer_mark_dirty(background_layer);
}
