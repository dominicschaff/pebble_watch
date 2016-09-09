#include <pebble.h>
#include "timedisplay.h"
#include "health.h"
#include "constants.h"

static void tick_handler(struct tm *tick_time, TimeUnits units_changed);

static void time_update_proc(Layer *layer, GContext *ctx);
static void add_text_layer(Layer *window_layer, TextLayer *text_layer, GTextAlignment alignment);

static TextLayer *s_time_hour_text_layer, *s_time_minute_text_layer, *s_date_text_layer, *s_day_text_layer;

static int s_hour_level, s_minute_level;

static Layer *s_time_layer;

static GFont s_time_font;

void timedisplay_load_graphics(Window *window)
{
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Create hour meter Layer
  s_time_layer = layer_create(bounds);
  layer_set_update_proc(s_time_layer, time_update_proc);
  layer_add_child(window_layer, s_time_layer);
}
void timedisplay_load_text(Window *window)
{
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

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
}
void timedisplay_unload(Window *window)
{

  text_layer_destroy(s_time_hour_text_layer);
  text_layer_destroy(s_time_minute_text_layer);
  text_layer_destroy(s_date_text_layer);
  text_layer_destroy(s_day_text_layer);
  layer_destroy(s_time_layer);
  fonts_unload_custom_font(s_time_font);
}

void timedisplay_init()
{
  update_time();
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}
void timedisplay_deinit()
{

}


static void add_text_layer(Layer *window_layer, TextLayer *text_layer, GTextAlignment alignment)
{
  text_layer_set_background_color(text_layer, GColorClear);
  text_layer_set_text_color(text_layer, GColorBlack);
  text_layer_set_text_alignment(text_layer, alignment);
  layer_add_child(window_layer, text_layer_get_layer(text_layer));
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
  update_health();
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
}


/**
 * When the time is updated run this to update the various components
 */
void update_time()
{
  static char s_hour_buffer[4];
  static char s_minute_buffer[4];
  static char date_buffer[16];
  static char day_buffer[16];

  // Get the current time
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  // Save the minute and hour (for the bars)
  s_hour_level = tick_time->tm_hour;
  s_minute_level = tick_time->tm_min;

  // Create the string for the time display
  strftime(s_hour_buffer, sizeof(s_hour_buffer), clock_is_24h_style() ?"%H" : "%I", tick_time);
  text_layer_set_text(s_time_hour_text_layer, s_hour_buffer);
  strftime(s_minute_buffer, sizeof(s_minute_buffer), "%M", tick_time);
  text_layer_set_text(s_time_minute_text_layer, s_minute_buffer);

  // Create the string for the date display
  strftime(date_buffer, sizeof(date_buffer), "%d %b", tick_time);
  strftime(day_buffer, sizeof(day_buffer), "%a", tick_time);
  text_layer_set_text(s_date_text_layer, date_buffer);
  text_layer_set_text(s_day_text_layer, day_buffer);

  // Mark the layers as dirty
  layer_mark_dirty(s_time_layer);
}