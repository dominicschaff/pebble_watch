#include <pebble.h>
#include "my_math.h"
#include "suncalc.h"
// Default value
#define STEPS_DEFAULT 1000

// Piece Sizing
#define SUB_TEXT_HEIGHT 30
#define TITLE_TEXT_HEIGHT 49
#define PIE_THICKNESS 10

static AppSync s_sync;
static uint8_t s_sync_buffer[64];

enum LocationKey {
  LONGITUDE = 0x0,
  LATITUDE = 0x1
};

static Window *s_main_window;

static GFont s_time_font;

static Layer *s_bluetooth_layer;
static Layer *s_steps_layer, *s_steps_now_layer;
static Layer *s_hour_layer, *s_minute_layer;

static TextLayer *s_battery_text_layer, *s_location_text_layer;
static TextLayer *s_steps_text_layer, *s_steps_perc_text_layer, *s_steps_now_average_text_layer, *s_steps_average_text_layer;
static TextLayer *s_time_hour_text_layer, *s_time_minute_text_layer, *s_date_text_layer, *s_day_text_layer;

static int s_battery_level;
static int s_hour_level, s_minute_level;
static int s_steps_level, s_steps_average, s_steps_average_now;

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
static void format_number(char *str, int size, int number);
static void update_watch();
static void update_health();
float calcSunRise(int year, int month, int day, float latitude, float longitude, float zenith);
float calcSunSet(int year, int month, int day, float latitude, float longitude, float zenith);

double atof(const char *nptr);

int current_sunrise_sunset_day = -1;
int current_moon_day = -1;
float sunriseTime;
float sunsetTime;
int phase;
double lat;
double lon;
double tz;

static void update_location(void)
{
  static char location_buffer[20];

  float sunriseTime = calcSunRise(time->tm_year, time->tm_mon+1, time->tm_mday, lat, lon, 91.0f);
  float sunsetTime = calcSunSet(time->tm_year, time->tm_mon+1, time->tm_mday, lat, lon, 91.0f);

  snprintf(location_buffer, sizeof(location_buffer),
           "%d,%d",
           (int)sunriseTime,
           (int)sunsetTime);
  text_layer_set_text(s_location_text_layer, location_buffer);
}

static void sync_error_callback(DictionaryResult dict_error, AppMessageResult app_message_error, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Sync Error: %d", app_message_error);
}

static void sync_tuple_changed_callback(const uint32_t key, const Tuple* new_tuple, const Tuple* old_tuple, void* context) {
  switch (key) {
    case LATITUDE:
      lat = atof(new_tuple->value->cstring);
      update_location();
      break;
    case LONGITUDE:
      lon = atof(new_tuple->value->cstring);
      update_location();
      break;
  }
}

static void request_data(void)
{
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);

  if (!iter) {
    // Error creating outbound message
    return;
  }

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
  update_watch();
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  s_steps_average = STEPS_DEFAULT;

  battery_callback(battery_state_service_peek());
  battery_state_service_subscribe(battery_callback);
  update_health();

  Tuplet initial_values[] = {
    TupletCString(LONGITUDE, "0"),
    TupletCString(LATITUDE, "0")
  };

  app_sync_init(&s_sync, s_sync_buffer, sizeof(s_sync_buffer),
      initial_values, ARRAY_LENGTH(initial_values),
      sync_tuple_changed_callback, sync_error_callback, NULL);

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
  s_bluetooth_layer = layer_create(bounds);
  layer_set_update_proc(s_bluetooth_layer, bluetooth_update_proc);
  layer_add_child(window_layer, s_bluetooth_layer);

  // Create hour meter Layer
  s_hour_layer = layer_create(GRect(PIE_THICKNESS, PIE_THICKNESS, bounds.size.w - (PIE_THICKNESS<<1), bounds.size.h - (PIE_THICKNESS<<1)));
  layer_set_update_proc(s_hour_layer, time_hour_update_proc);
  layer_add_child(window_layer, s_hour_layer);

  // Create hour meter Layer
  s_minute_layer = layer_create(GRect(bounds.size.w >> 3, bounds.size.h >> 3, (3 * bounds.size.w) >> 2, (3 * bounds.size.h) >> 2));
  layer_set_update_proc(s_minute_layer, time_minute_update_proc);
  layer_add_child(window_layer, s_minute_layer);

  // Create steps meter Layer
  s_steps_layer = layer_create(bounds);
  layer_set_update_proc(s_steps_layer, steps_proc_layer);
  layer_add_child(window_layer, s_steps_layer);

  // Create now steps meter Layer
  s_steps_now_layer = layer_create(GRect(hw>>1, hh>>1, hw, hh));
  layer_set_update_proc(s_steps_now_layer, steps_now_proc_layer);
  layer_add_child(window_layer, s_steps_now_layer);

  s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_PIXELS_49));

  // Create the time display
  s_time_hour_text_layer = text_layer_create(GRect(0, SUB_TEXT_HEIGHT, bounds.size.w, TITLE_TEXT_HEIGHT));
  text_layer_set_font(s_time_hour_text_layer, s_time_font);
  add_text_layer(window_layer, s_time_hour_text_layer, GTextAlignmentCenter);

  s_time_minute_text_layer = text_layer_create(GRect(0, SUB_TEXT_HEIGHT +  TITLE_TEXT_HEIGHT, bounds.size.w, TITLE_TEXT_HEIGHT));
  text_layer_set_font(s_time_minute_text_layer, s_time_font);
  add_text_layer(window_layer, s_time_minute_text_layer, GTextAlignmentCenter);

  // Create the date display
  s_date_text_layer = text_layer_create(GRect(0, -5, bounds.size.w, SUB_TEXT_HEIGHT));
  text_layer_set_font(s_date_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  add_text_layer(window_layer, s_date_text_layer, GTextAlignmentLeft);

  // Create the date display
  s_day_text_layer = text_layer_create(GRect(0, SUB_TEXT_HEIGHT - 13, bounds.size.w, SUB_TEXT_HEIGHT));
  text_layer_set_font(s_day_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  add_text_layer(window_layer, s_day_text_layer, GTextAlignmentLeft);

  // Create the steps display
  s_steps_text_layer = text_layer_create(GRect(0, bounds.size.h - SUB_TEXT_HEIGHT, bounds.size.w, SUB_TEXT_HEIGHT));
  text_layer_set_font(s_steps_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  add_text_layer(window_layer, s_steps_text_layer, GTextAlignmentLeft);

  // Create the steps percentage display
  s_steps_perc_text_layer = text_layer_create(GRect(0, bounds.size.h - SUB_TEXT_HEIGHT, bounds.size.w, SUB_TEXT_HEIGHT));
  text_layer_set_font(s_steps_perc_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  add_text_layer(window_layer, s_steps_perc_text_layer, GTextAlignmentRight);

  // Create the current average steps display
  s_steps_now_average_text_layer = text_layer_create(GRect(0, bounds.size.h - SUB_TEXT_HEIGHT - 13, bounds.size.w, SUB_TEXT_HEIGHT));
  text_layer_set_font(s_steps_now_average_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  add_text_layer(window_layer, s_steps_now_average_text_layer, GTextAlignmentLeft);

  // Create the average steps display
  s_steps_average_text_layer = text_layer_create(GRect(0, bounds.size.h - SUB_TEXT_HEIGHT - 13, bounds.size.w, SUB_TEXT_HEIGHT));
  text_layer_set_font(s_steps_average_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  add_text_layer(window_layer, s_steps_average_text_layer, GTextAlignmentRight);

  // Create the battery percentage display
  s_battery_text_layer = text_layer_create(GRect(0, -5, bounds.size.w, SUB_TEXT_HEIGHT));
  text_layer_set_font(s_battery_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  add_text_layer(window_layer, s_battery_text_layer, GTextAlignmentRight);

  s_location_text_layer = text_layer_create(GRect(0, SUB_TEXT_HEIGHT - 13, bounds.size.w, SUB_TEXT_HEIGHT));
  text_layer_set_font(s_location_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  add_text_layer(window_layer, s_location_text_layer, GTextAlignmentRight);
}

/**
 * Destroy the window
 *
 * @param window The window to destroy
 */
static void main_window_unload(Window *window)
{
  text_layer_destroy(s_time_hour_text_layer);
  text_layer_destroy(s_time_minute_text_layer);
  text_layer_destroy(s_date_text_layer);
  text_layer_destroy(s_day_text_layer);
  text_layer_destroy(s_location_text_layer);
  layer_destroy(s_hour_layer);
  layer_destroy(s_minute_layer);
  fonts_unload_custom_font(s_time_font);

  text_layer_destroy(s_steps_text_layer);
  text_layer_destroy(s_steps_perc_text_layer);
  text_layer_destroy(s_steps_now_average_text_layer);
  text_layer_destroy(s_steps_average_text_layer);
  layer_destroy(s_steps_layer);
  layer_destroy(s_steps_now_layer);

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
  snprintf(battery_buffer, sizeof(battery_buffer), "%d%%%s", state.charge_percent, state.is_charging ? " +" : "");
  text_layer_set_text(s_battery_text_layer, battery_buffer);
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
  graphics_context_set_fill_color(ctx, GColorRed);
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
  graphics_context_set_fill_color(ctx, GColorPictonBlue);
  graphics_fill_radial(ctx, layer_get_bounds(layer), GOvalScaleModeFitCircle, PIE_THICKNESS, 0, (s_hour_level % 12) * DEG_TO_TRIGANGLE(30));
}

/**
 * When the minute layer is marked as dirty run thi
 *
 * @param layer The layer to update
 * @param ctx   The context
 */
static void time_minute_update_proc(Layer *layer, GContext *ctx)
{
  graphics_context_set_fill_color(ctx, GColorMagenta);
  graphics_fill_radial(ctx, layer_get_bounds(layer), GOvalScaleModeFitCircle, PIE_THICKNESS, 0, s_minute_level * DEG_TO_TRIGANGLE(6));
}

/**
 * When the steps layer is marked as dirty run this
 *
 * @param layer The layer to update
 * @param ctx   The context
 */
static void steps_proc_layer(Layer *layer, GContext *ctx)
{
  GRect bounds = layer_get_bounds(layer);

  float l = 1.0f * s_steps_level / s_steps_average;

  graphics_context_set_fill_color(ctx, l >= 1.0 ? GColorMalachite : GColorShockingPink);
  graphics_fill_radial(ctx, bounds, GOvalScaleModeFitCircle, PIE_THICKNESS, 0, l * DEG_TO_TRIGANGLE(360));

  l = 1.0f * s_steps_average_now / s_steps_average;
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
  GRect bounds = layer_get_bounds(layer);

  float l = 1.0f * s_steps_level / s_steps_average_now;

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
  int tmp_hour = s_hour_level;

  // Get the current time
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  // Save the minute and hour (for the bars)
  s_hour_level = tick_time->tm_hour;
  s_minute_level = tick_time->tm_min;

  // Create the string for the time display
  strftime(s_hour_buffer, sizeof(s_hour_buffer), clock_is_24h_style() ? "%H" : "%I", tick_time);
  text_layer_set_text(s_time_hour_text_layer, s_hour_buffer);
  strftime(s_minute_buffer, sizeof(s_minute_buffer), "%M", tick_time);
  text_layer_set_text(s_time_minute_text_layer, s_minute_buffer);

  // Create the string for the date display
  strftime(date_buffer, sizeof(date_buffer), "%d %b", tick_time);
  strftime(day_buffer, sizeof(day_buffer), "%a", tick_time);
  text_layer_set_text(s_date_text_layer, date_buffer);
  text_layer_set_text(s_day_text_layer, day_buffer);

  // Mark the layers as dirty
  if (tmp_hour != s_hour_level) layer_mark_dirty(s_hour_layer);
  layer_mark_dirty(s_minute_layer);
  if (s_minute_level & 1)
    update_health();

  if (s_minute_level % 60) {

  }
}

static void update_health()
{
  static char steps_buffer[10];
  static char steps_perc_buffer[10];
  static char steps_now_buffer[10];
  static char steps_average_buffer[10];

  int start = time_start_of_today();

  s_steps_average = (int)health_service_sum_averaged(HealthMetricStepCount, start, start + SECONDS_PER_DAY, HealthServiceTimeScopeDailyWeekdayOrWeekend);
  if (s_steps_average < 1) s_steps_average = STEPS_DEFAULT;

  s_steps_average_now = (int)health_service_sum_averaged(HealthMetricStepCount, start, time(NULL), HealthServiceTimeScopeDailyWeekdayOrWeekend);

  if (s_steps_average_now < 1) s_steps_average_now = STEPS_DEFAULT;

  s_steps_level = (int)health_service_sum_today(HealthMetricStepCount);

  format_number(steps_buffer, sizeof(steps_buffer), s_steps_level);
  format_number(steps_average_buffer, sizeof(steps_average_buffer), s_steps_average);
  format_number(steps_now_buffer, sizeof(steps_now_buffer), s_steps_average_now);

  snprintf(steps_perc_buffer, sizeof(steps_perc_buffer), "%d%%", (int)(100.0f * s_steps_level / s_steps_average));

  // Set the steps display
  text_layer_set_text(s_steps_text_layer, steps_buffer);
  text_layer_set_text(s_steps_perc_text_layer, steps_perc_buffer);
  text_layer_set_text(s_steps_now_average_text_layer, steps_now_buffer);
  text_layer_set_text(s_steps_average_text_layer, steps_average_buffer);

  layer_mark_dirty(s_steps_layer);
  layer_mark_dirty(s_steps_now_layer);
}

/* Helper functions */

static void format_number(char *str, int size, int number)
{
  if (number > 1000)
    snprintf(str, size, "%d,%03d", number / 1000, number % 1000);
  else
    snprintf(str, size, "%d", number);
}

static void add_text_layer(Layer *window_layer, TextLayer *text_layer, GTextAlignment alignment)
{
  text_layer_set_background_color(text_layer, GColorClear);
  text_layer_set_text_color(text_layer, GColorBlack);
  text_layer_set_text_alignment(text_layer, alignment);
  layer_add_child(window_layer, text_layer_get_layer(text_layer));
}


double atof(const char *nptr)
{
    return (strtod(nptr, (char **)NULL));
}
int isspace(int c)
{
  if(((char)c)==' ')
    return 1;
  return 0;
}

int isdigit(int c)
{
  char cc = (char)c;
  if(cc>='0' && cc<='9')
    return 1;
  return 0;
}

double strtod(const char *nptr, char **endptr)
{
    double x = 0.0;
    double xs= 1.0;
    double es = 1.0;
    double xf = 0.0;
    double xd = 1.0;
    while( isspace( (unsigned char)*nptr ) ) ++nptr;
    if(*nptr == '-')
    {
        xs = -1;
        nptr++;
    }
    else if(*nptr == '+')
    {
        nptr++;
    }

    while (1)
    {
        if (isdigit((unsigned char)*nptr))
        {
            x = x * 10 + (*nptr - '0');
            nptr++;
        }
        else
        {
            x = x * xs;
            break;
        }
    }
    if (*nptr == '.')
    {
        nptr++;
        while (1)
        {
            if (isdigit((unsigned char)*nptr))
            {
                xf = xf * 10 + (*nptr - '0');
                xd = xd * 10;
            }
            else
            {
                x = x + xs * (xf / xd);
                break;
            }
            nptr++;
        }
    }
    if ((*nptr == 'e') || (*nptr == 'E'))
    {
        nptr++;
        if (*nptr == '-')
        {
            es = -1;
            nptr++;
        }
        xd = 1;
        xf = 0;
        while (1)
        {
            if (isdigit((unsigned char)*nptr))
            {
                xf = xf * 10 + (*nptr - '0');
                nptr++;
            }
            else
            {
                while (xf > 0)
                {
                    xd *= 10;
                    xf--;
                }
                if (es < 0.0)
                {
                    x = x / xd;
                }
                else
                {
                    x = x * xd;
                }
                break;
            }
        }
    }
    if (endptr != NULL)
    {
        *endptr = (char *)nptr;
    }
    return (x);
}