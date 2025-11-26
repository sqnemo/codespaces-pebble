#include <pebble.h>

static Window *s_main_window;
static TextLayer *s_time_layer;
static TextLayer *s_date_layer;
static Layer *s_indicator_layer;

static bool is_vibrating = false;

// ===============================
//  プロトタイプ宣言
// ===============================
static void set_vibe_on(void *data);
static void set_vibe_off(void *data);
static void schedule_vibe_indicator(uint32_t *segments, int count);

// ===============================
//  描画（右上の黒丸）
// ===============================
static void indicator_update_proc(Layer *layer, GContext *ctx) {
  if(!is_vibrating) return;

  graphics_context_set_fill_color(ctx, GColorBlack);

  GRect r = GRect(130, 10, 10, 10);
  graphics_fill_rect(ctx, r, 5, GCornerNone);
}

// ===============================
//  時刻表示
// ===============================
static void update_time() {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  static char time_buffer[8];
  strftime(time_buffer, sizeof(time_buffer), "%H:%M", tick_time);
  text_layer_set_text(s_time_layer, time_buffer);

  static char date_buffer[16];
  strftime(date_buffer, sizeof(date_buffer), "%Y-%m-%d", tick_time);
  text_layer_set_text(s_date_layer, date_buffer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

// ===============================
//  インジケータの制御関数
// ===============================
static void set_vibe_on(void *data) {
  is_vibrating = true;
  layer_mark_dirty(s_indicator_layer);
}

static void set_vibe_off(void *data) {
  is_vibrating = false;
  layer_mark_dirty(s_indicator_layer);
}

static void schedule_vibe_indicator(uint32_t *segments, int count) {
  uint32_t elapsed = 0;
  bool state_on = true;

  for (int i = 0; i < count; i++) {
    if(state_on) {
      app_timer_register(elapsed, set_vibe_on, NULL);
    } else {
      app_timer_register(elapsed, set_vibe_off, NULL);
    }

    elapsed += segments[i];
    state_on = !state_on;
  }

  // 最後は必ずOFFで終了
  app_timer_register(elapsed, set_vibe_off, NULL);
}

// ===============================
//  バイブレーションパターン
// ===============================
static void send_time_vibration() {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  int hour = t->tm_hour % 12;
  if(hour == 0) hour = 12;

  int tens = t->tm_min / 10;
  int ones = t->tm_min % 10;

  const uint32_t pulse = 300;
  const uint32_t gap_short = 180;
  const uint32_t gap_long = 500;

  uint32_t segments[64];
  int idx = 0;

  // 時
  for(int i = 0; i < hour; i++){
    segments[idx++] = pulse;
    segments[idx++] = gap_short;
  }
  segments[idx++] = gap_long;

  // 10分
  if(tens == 0) {
    segments[idx++] = pulse * 2;
    segments[idx++] = gap_short;
  } else {
    for(int i = 0; i < tens; i++){
      segments[idx++] = pulse;
      segments[idx++] = gap_short;
    }
  }
  segments[idx++] = gap_long;

  // 1分
  if(ones == 0) {
    segments[idx++] = pulse * 2;
    segments[idx++] = gap_short;
  } else {
    for(int i = 0; i < ones; i++){
      segments[idx++] = pulse;
      segments[idx++] = gap_short;
    }
  }

  VibePattern pattern = {
    .durations = segments,
    .num_segments = idx
  };

  vibes_enqueue_custom_pattern(pattern);

  // ★ インジケータと同期
  schedule_vibe_indicator(segments, idx);
}

// ===============================
//  ボタン
// ===============================
static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  send_time_vibration();
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
}

// ===============================
//  Window
// ===============================
static void main_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_time_layer = text_layer_create(GRect(0, 40, bounds.size.w, 50));
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_time_layer));

  s_date_layer = text_layer_create(GRect(0, 100, bounds.size.w, 30));
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_date_layer));

  // ★ インジケータレイヤー
  s_indicator_layer = layer_create(bounds);
  layer_set_update_proc(s_indicator_layer, indicator_update_proc);
  layer_add_child(root, s_indicator_layer);
}

static void main_window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_date_layer);
  layer_destroy(s_indicator_layer);
}

// ===============================
//  main
// ===============================
static void init() {
  s_main_window = window_create();
  window_set_click_config_provider(s_main_window, click_config_provider);

  window_set_window_handlers(s_main_window, (WindowHandlers){
    .load = main_window_load,
    .unload = main_window_unload
  });

  window_stack_push(s_main_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  update_time();
}

static void deinit() {
  window_destroy(s_main_window);
}


int main(void) {
  init();
  app_event_loop();
  deinit();
}

