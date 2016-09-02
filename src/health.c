#include <pebble.h>
#include "health.h"
#include "constants.h"

static void steps_proc_layer(Layer *layer, GContext *ctx);
static void steps_now_proc_layer(Layer *layer, GContext *ctx);
static void add_text_layer(Layer *window_layer, TextLayer *text_layer, GTextAlignment alignment);

static TextLayer *s_steps_text_layer, *s_steps_perc_text_layer, *s_steps_now_average_text_layer, *s_steps_average_text_layer;

static int s_steps_level, s_steps_average, s_steps_average_now;
static Layer *s_steps_layer, *s_steps_now_layer;

void health_load_graphics(Window *window)
{
  // Get information about the Window
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  int hh = bounds.size.h>>1;
  int hw = bounds.size.w>>1;

  // Create steps meter Layer
  s_steps_layer = layer_create(bounds);
  layer_set_update_proc(s_steps_layer, steps_proc_layer);
  layer_add_child(window_layer, s_steps_layer);

  // Create steps meter Layer
  s_steps_now_layer = layer_create(GRect(bounds.size.w/3, hh + 3*(hh>>2), bounds.size.w/3, (hh-3*(hh>>2))));
  layer_set_update_proc(s_steps_now_layer, steps_now_proc_layer);
  layer_add_child(window_layer, s_steps_now_layer);
}
void health_load_text(Window *window)
{
  // Get information about the Window
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  int hh = bounds.size.h>>1;
  int hw = bounds.size.w>>1;

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
}
void health_unload(Window *window)
{

  text_layer_destroy(s_steps_text_layer);
  text_layer_destroy(s_steps_perc_text_layer);
  text_layer_destroy(s_steps_now_average_text_layer);
  text_layer_destroy(s_steps_average_text_layer);
  layer_destroy(s_steps_layer);
  layer_destroy(s_steps_now_layer);
}
void health_init()
{
  s_steps_average = STEPS_DEFAULT;
}
void health_deinit()
{

}

void update_health()
{
  static char steps_buffer[10];
  static char steps_perc_buffer[10];
  static char steps_now_buffer[15];
  static char steps_average_buffer[10];

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
  layer_mark_dirty(s_steps_layer);
  layer_mark_dirty(s_steps_now_layer);
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
  graphics_fill_radial(ctx, GRect(0, 0, bounds.size.w, bounds.size.h), GOvalScaleModeFitCircle, PIE_THICKNESS >> 2, 0, l * DEG_TO_TRIGANGLE(360));

  l = 1.0f * s_steps_average_now / s_steps_average;
  if (l <= 1.0) {
    graphics_context_set_fill_color(ctx, GColorLavenderIndigo);
    graphics_fill_radial(ctx, GRect(0, 0, bounds.size.w, bounds.size.h), GOvalScaleModeFitCircle, PIE_THICKNESS, l * DEG_TO_TRIGANGLE(360) - DEG_TO_TRIGANGLE(2), l * DEG_TO_TRIGANGLE(360) + DEG_TO_TRIGANGLE(2));
    graphics_context_set_fill_color(ctx, GColorImperialPurple);
    graphics_fill_radial(ctx, GRect(0, 0, bounds.size.w, bounds.size.h), GOvalScaleModeFitCircle, PIE_THICKNESS, l * DEG_TO_TRIGANGLE(360) - DEG_TO_TRIGANGLE(1), l * DEG_TO_TRIGANGLE(360) + DEG_TO_TRIGANGLE(1));
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

  l = (((l > 1.5) ? 0.5 : ((l < -0.5) ? -0.5 : l - 1.0)) + 0.5) * bounds.size.w;

  // draw the meters:
  graphics_context_set_fill_color(ctx, GColorFolly);
  graphics_fill_rect(ctx, GRect(0, 0, 2, bounds.size.h), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(bounds.size.w/2, 0, 2, bounds.size.h), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(bounds.size.w-2, 0, 2, bounds.size.h), 0, GCornerNone);

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(l-1, 0, 2, bounds.size.h), 0, GCornerNone);
}


static void add_text_layer(Layer *window_layer, TextLayer *text_layer, GTextAlignment alignment)
{
  text_layer_set_background_color(text_layer, GColorClear);
  text_layer_set_text_color(text_layer, GColorBlack);
  text_layer_set_text_alignment(text_layer, alignment);
  layer_add_child(window_layer, text_layer_get_layer(text_layer));
}