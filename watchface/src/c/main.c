#include <pebble.h>
#include <stdlib.h>

#define ANIM_FRAME_PAUSE_MS 100
#define BEE_DISPLAY_PAUSE_MS 6000
#define BEE_ANIM_SECOND_MARK 55
#define DATE_FORMAT "%a-%b-%d"
#define TIME_FORMAT_24H "%H:%M"
#define TIME_FORMAT_12H "%I:%M"
#define MAX_BOMBS 8
#define BOMB_ANIMATION_MIN_INTERVAL_MS (1 * 60 * 60 * 1000) // 1 hour
#define BOMB_ANIMATION_MAX_INTERVAL_MS (8 * 60 * 60 * 1000) // 8 hours

static const uint32_t ANIM_RESOURCE_IDS[] = {
  RESOURCE_ID_IMAGE_ANIM_00,
  RESOURCE_ID_IMAGE_ANIM_01,
  RESOURCE_ID_IMAGE_ANIM_02,
  RESOURCE_ID_IMAGE_ANIM_03,
  RESOURCE_ID_IMAGE_ANIM_04,
  RESOURCE_ID_IMAGE_ANIM_05,
  RESOURCE_ID_IMAGE_ANIM_06,
  RESOURCE_ID_IMAGE_ANIM_07,
  RESOURCE_ID_IMAGE_ANIM_08,
  RESOURCE_ID_IMAGE_ANIM_09,
  RESOURCE_ID_IMAGE_ANIM_10,
  RESOURCE_ID_IMAGE_ANIM_11,
  RESOURCE_ID_IMAGE_ANIM_12,
  RESOURCE_ID_IMAGE_ANIM_13,
  RESOURCE_ID_IMAGE_ANIM_14,
  RESOURCE_ID_IMAGE_ANIM_15,
  RESOURCE_ID_IMAGE_ANIM_16,
  RESOURCE_ID_IMAGE_ANIM_17
};
#define ANIM_FRAME_COUNT ((int)(sizeof(ANIM_RESOURCE_IDS) / sizeof(ANIM_RESOURCE_IDS[0])))

static const uint16_t ANIM_FRAME_PAUSES[] = {
  3 * ANIM_FRAME_PAUSE_MS,
  1 * ANIM_FRAME_PAUSE_MS,
  1 * ANIM_FRAME_PAUSE_MS,
  1 * ANIM_FRAME_PAUSE_MS,
  1 * ANIM_FRAME_PAUSE_MS,
  1 * ANIM_FRAME_PAUSE_MS,
  1 * ANIM_FRAME_PAUSE_MS,
  1 * ANIM_FRAME_PAUSE_MS,
  1 * ANIM_FRAME_PAUSE_MS,
  1 * ANIM_FRAME_PAUSE_MS,
  1 * ANIM_FRAME_PAUSE_MS,
  1 * ANIM_FRAME_PAUSE_MS,
  2 * ANIM_FRAME_PAUSE_MS,
  1 * ANIM_FRAME_PAUSE_MS,
  1 * ANIM_FRAME_PAUSE_MS,
  7 * ANIM_FRAME_PAUSE_MS,
  54 * ANIM_FRAME_PAUSE_MS,
  1 * ANIM_FRAME_PAUSE_MS
};

// Compile-time check to ensure animation arrays are the same size
_Static_assert(sizeof(ANIM_FRAME_PAUSES) / sizeof(ANIM_FRAME_PAUSES[0]) == ANIM_FRAME_COUNT,
               "ANIM_FRAME_PAUSES and ANIM_RESOURCE_IDS must have the same number of elements.");

static Window *s_main_window;
static TextLayer *s_time_layer;
static TextLayer *s_date_layer;
static GFont s_time_font;
static GFont s_date_font;

static char s_time_buffer[8];
static char s_date_buffer[16];

static BitmapLayer *s_animation_layer;
static int s_animation_frame_index;
static GBitmap *s_current_animation_frame;
static AppTimer *s_animation_timer;
static bool s_animation_done = false;

static BitmapLayer *s_bee_layer;
static GBitmap *s_bee_bitmap;
static AppTimer *s_bee_timer;

static GBitmap *s_bomb_bitmap;
static BitmapLayer *s_bomb_layers[MAX_BOMBS];
static AppTimer *s_bomb_timer;
static int s_num_bombs = 0;

static void animation_timer_callback(void *data);
static void schedule_bee_animation();
static void bee_animation_handler(void *data);
static void schedule_bomb_animation();
static void bomb_animation_handler(void *data);
static void update_date(const struct tm *tick_time);
static void update_time(const struct tm *tick_time);
static void hide_bombs();

static void start_animation() {
  hide_bombs();
  s_animation_frame_index = 0;
  s_animation_done = false;
  layer_set_hidden(bitmap_layer_get_layer(s_animation_layer), false);
  layer_set_hidden(text_layer_get_layer(s_time_layer), true);
  layer_set_hidden(text_layer_get_layer(s_date_layer), true);
  animation_timer_callback(NULL);
}

static void hide_bee_handler(void *data) {
  layer_set_hidden(bitmap_layer_get_layer(s_bee_layer), true);
  schedule_bee_animation();
}

static void bee_animation_handler(void *data) {
  const Layer *window_layer = window_get_root_layer(s_main_window);
  const GRect bounds = layer_get_bounds(window_layer);
  const int16_t margin = 20;

  const GSize bee_size = gbitmap_get_bounds(s_bee_bitmap).size;

  const int16_t x_range = bounds.size.w - (2 * margin) - bee_size.w;
  const int16_t y_range = bounds.size.h - (2 * margin) - bee_size.h;

  const int16_t x = margin + (rand() % x_range);
  const int16_t y = margin + (rand() % y_range);

  layer_set_frame(bitmap_layer_get_layer(s_bee_layer), GRect(x, y, bee_size.w, bee_size.h));

  layer_set_hidden(bitmap_layer_get_layer(s_bee_layer), false);
  s_bee_timer = app_timer_register(BEE_DISPLAY_PAUSE_MS, hide_bee_handler, NULL);
}

static void schedule_bee_animation() {
  if (s_bee_timer) {
    app_timer_cancel(s_bee_timer);
  }

  const time_t now = time(NULL);
  const struct tm *current_time = localtime(&now);
  int seconds_until_next_55 = BEE_ANIM_SECOND_MARK - current_time->tm_sec;

  if (seconds_until_next_55 <= 0) {
    seconds_until_next_55 += 60;
  }

  s_bee_timer = app_timer_register(seconds_until_next_55 * 1000, bee_animation_handler, NULL);
}

static void bomb_animation_handler(void *data) {
  hide_bombs();

  Layer *window_layer = window_get_root_layer(s_main_window);
  const GRect bounds = layer_get_bounds(window_layer);
  const GSize bomb_size = gbitmap_get_bounds(s_bomb_bitmap).size;

  s_num_bombs = 2 + (rand() % (MAX_BOMBS - 2 + 1));

  for (int i = 0; i < s_num_bombs; i++) {
    const int16_t y = (bounds.size.h / 2) + 15 - (bomb_size.h / 2);
    const int16_t x = i * bomb_size.w;

    s_bomb_layers[i] = bitmap_layer_create(GRect(x, y, bomb_size.w, bomb_size.h));
    bitmap_layer_set_bitmap(s_bomb_layers[i], s_bomb_bitmap);
    bitmap_layer_set_compositing_mode(s_bomb_layers[i], GCompOpSet);
    layer_add_child(window_layer, bitmap_layer_get_layer(s_bomb_layers[i]));
  }

  vibes_double_pulse();
}

static void schedule_bomb_animation() {
  if (s_bomb_timer) {
    app_timer_cancel(s_bomb_timer);
  }
  uint32_t interval = BOMB_ANIMATION_MIN_INTERVAL_MS + (rand() % (BOMB_ANIMATION_MAX_INTERVAL_MS - BOMB_ANIMATION_MIN_INTERVAL_MS));
  s_bomb_timer = app_timer_register(interval, bomb_animation_handler, NULL);
}

static void hide_bombs() {
  for (int i = 0; i < s_num_bombs; i++) {
    if (s_bomb_layers[i]) {
      layer_remove_from_parent(bitmap_layer_get_layer(s_bomb_layers[i]));
      bitmap_layer_destroy(s_bomb_layers[i]);
      s_bomb_layers[i] = NULL;
    }
  }
  s_num_bombs = 0;
}

static void animation_timer_callback(void *data) {
  if (s_animation_frame_index == ANIM_FRAME_COUNT) {
    s_animation_done = true;
    layer_set_hidden(text_layer_get_layer(s_time_layer), false);
    layer_set_hidden(text_layer_get_layer(s_date_layer), false);
    schedule_bee_animation();
    return;
  }

  if (s_current_animation_frame) {
    gbitmap_destroy(s_current_animation_frame);
    s_current_animation_frame = NULL;
  }

  s_current_animation_frame = gbitmap_create_with_resource(ANIM_RESOURCE_IDS[s_animation_frame_index]);
  if (s_current_animation_frame) {
    bitmap_layer_set_bitmap(s_animation_layer, s_current_animation_frame);

    const int duration = ANIM_FRAME_PAUSES[s_animation_frame_index];

    s_animation_frame_index++;
    s_animation_timer = app_timer_register(duration, animation_timer_callback, NULL);
  } else {
    // Failed to load a frame, fallback to showing the time
    s_animation_done = true;
    layer_set_hidden(bitmap_layer_get_layer(s_animation_layer), true);
    layer_set_hidden(text_layer_get_layer(s_time_layer), false);
    layer_set_hidden(text_layer_get_layer(s_date_layer), false);
    schedule_bee_animation();
  }
}

static void update_time(const struct tm *tick_time) {
  strftime(s_time_buffer, sizeof(s_time_buffer), clock_is_24h_style() ? TIME_FORMAT_24H : TIME_FORMAT_12H, tick_time);
  text_layer_set_text(s_time_layer, s_time_buffer);
}

static void update_date(const struct tm *tick_time) {
  strftime(s_date_buffer, sizeof(s_date_buffer), DATE_FORMAT, tick_time);
  text_layer_set_text(s_date_layer, s_date_buffer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time(tick_time);
  if (units_changed & DAY_UNIT) {
    update_date(tick_time);
  }
}

static void main_window_appear(Window *window) {
  start_animation();
  schedule_bomb_animation();
}
static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  const GRect bounds = layer_get_bounds(window_layer);

  // Create animation layer
  s_animation_layer = bitmap_layer_create(GRect(0, 0, bounds.size.w, bounds.size.h));
  layer_add_child(window_layer, bitmap_layer_get_layer(s_animation_layer));

  // Create time TextLayer
  s_time_layer = text_layer_create(GRect(0, (bounds.size.h / 2) - 40, bounds.size.w, 50));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorBlack);
  s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TIME_42));
  text_layer_set_font(s_time_layer, s_time_font);
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));

  // Create date TextLayer
  s_date_layer = text_layer_create(GRect(0, (bounds.size.h / 2) + 20, bounds.size.w, 30));
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorBlack);
  s_date_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DATE_24));
  text_layer_set_font(s_date_layer, s_date_font);
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));

  // Create GBitmap for bee
  s_bee_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BEE);

  // Create BitmapLayer for bee
  s_bee_layer = bitmap_layer_create(GRect(0, 0, bounds.size.w, bounds.size.h));
  bitmap_layer_set_bitmap(s_bee_layer, s_bee_bitmap);
  bitmap_layer_set_compositing_mode(s_bee_layer, GCompOpSet);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_bee_layer));
  layer_set_hidden(bitmap_layer_get_layer(s_bee_layer), true);

  // Create GBitmap for bomb
  s_bomb_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BOMB);
}

static void main_window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_date_layer);
  fonts_unload_custom_font(s_time_font);
  fonts_unload_custom_font(s_date_font);

  bitmap_layer_destroy(s_animation_layer);
  if (s_current_animation_frame) {
    gbitmap_destroy(s_current_animation_frame);
  }
  if (s_animation_timer) {
    app_timer_cancel(s_animation_timer);
  }

  gbitmap_destroy(s_bee_bitmap);
  bitmap_layer_destroy(s_bee_layer);
  if (s_bee_timer) {
    app_timer_cancel(s_bee_timer);
  }

  gbitmap_destroy(s_bomb_bitmap);
  hide_bombs();
  if (s_bomb_timer) {
    app_timer_cancel(s_bomb_timer);
  }
}

static void init() {
  srand(time(NULL));
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorBlack);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
    .appear = main_window_appear,
  });
  window_stack_push(s_main_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  // Make sure the time is displayed from the start
  const time_t temp = time(NULL);
  const struct tm *tick_time = localtime(&temp);
  update_time(tick_time);
  update_date(tick_time);
}

static void deinit() {
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
