#include <pebble.h>

// ================================
//  定義
// ================================
#define NUM_HOUR_BLOCKS 12
#define NUM_TEN_BLOCKS 5
#define NUM_MIN_BLOCKS 9

// ------------------------------
// 座標定義
// ------------------------------

// 12時間（32x36）
static const GRect HOUR_RECTS[NUM_HOUR_BLOCKS] = {
  {{0,0}, {32,36}}, {{33,0}, {32,36}}, {{67,0}, {32,36}}, {{100,0}, {32,36}},
  {{134,0}, {32,36}}, {{167,0}, {32,36}},
  {{0,37}, {32,36}}, {{33,37}, {32,36}}, {{67,37}, {32,36}}, {{100,37}, {32,36}},
  {{134,37}, {32,36}}, {{167,37}, {32,36}},
};

// 10分（65x74）
static const GRect TEN_RECTS[NUM_TEN_BLOCKS] = {
  {{0,75}, {65,74}},
  {{67,75}, {65,74}},
  {{134,75}, {65,74}},
  {{0,151}, {65,74}},
  {{67,151}, {65,74}},
};

// 1分（32x14）×9個
static const GRect MIN_RECTS[NUM_MIN_BLOCKS] = {
  {{134,151}, {32,14}},
  {{134,166}, {32,14}},
  {{134,181}, {32,14}},
  {{134,196}, {32,14}},
  {{134,211}, {32,14}},
  {{167,151}, {32,14}},
  {{167,166}, {32,14}},
  {{167,181}, {32,14}},
  {{167,196}, {32,14}},
};

static const GRect SEC_RECT = {{167,211}, {32,14}};

// ================================
//  状態管理
// ================================
static Window *s_window;
static Layer  *s_layer;
static bool hour_active[NUM_HOUR_BLOCKS];
static bool ten_active[NUM_TEN_BLOCKS];
static bool min_active[NUM_MIN_BLOCKS];
static bool sec_on;
static bool invert_colors = false;

// ----------------------------------------------------------
// クリック
// ----------------------------------------------------------
static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  invert_colors = !invert_colors;
  layer_mark_dirty(s_layer);
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
}

// ★ これを追加
static void tick_handler(struct tm *tick_time, TimeUnits units_changed);

// ================================
//  描画処理
// ================================
static void layer_update_proc(Layer *layer, GContext *ctx) {

  GColor bg = invert_colors ? GColorWhite : GColorBlack;
  GColor fg = invert_colors ? GColorBlack : GColorWhite;

  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);

  graphics_context_set_fill_color(ctx, fg);

  // 時刻
  for(int i=0;i<NUM_HOUR_BLOCKS;i++){
    if(hour_active[i])
      graphics_fill_rect(ctx, HOUR_RECTS[i], 0, GCornerNone);
  }

  for(int i=0;i<NUM_TEN_BLOCKS;i++){
    if(ten_active[i])
      graphics_fill_rect(ctx, TEN_RECTS[i], 0, GCornerNone);
  }

  for(int i=0;i<NUM_MIN_BLOCKS;i++){
    if(min_active[i])
      graphics_fill_rect(ctx, MIN_RECTS[i], 0, GCornerNone);
  }

  if(sec_on)
    graphics_fill_rect(ctx, SEC_RECT, 0, GCornerNone);


  // ----------------------------------------
  // 小さいデジタル文字（50分ブロックの上）
  // ----------------------------------------
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  static char small_time[6];
  snprintf(small_time, sizeof(small_time), "%02d:%02d", t->tm_hour, t->tm_min);

  GRect text_rect = GRect(TEN_RECTS[4].origin.x, 207, 65, 20);

  bool block_on = ten_active[4];
  
  // ブロックがONなら背景は fg → 文字色は bg
  // ブロックがOFFなら背景は bg → 文字色は fg
  GColor text_color = block_on ? bg : fg;

  graphics_context_set_text_color(ctx, text_color);

  graphics_draw_text(
    ctx,
    small_time,
    fonts_get_system_font(FONT_KEY_GOTHIC_18),
    text_rect,
    GTextOverflowModeTrailingEllipsis,
    GTextAlignmentCenter,
    NULL
  );
}

// ================================
//  時刻ロジック
// ================================
static void update_time(struct tm *t) {
  int hour12 = t->tm_hour % 12;
  if(hour12 == 0) hour12 = 12;
  for(int i=0;i<NUM_HOUR_BLOCKS;i++){
    hour_active[i] = (i < hour12);
  }

  int ten = t->tm_min / 10;
  for(int i=0;i<NUM_TEN_BLOCKS;i++){
    ten_active[i] = (i < ten);
  }

  int one = t->tm_min % 10;
  for(int i=0;i<NUM_MIN_BLOCKS;i++){
    min_active[i] = (i < one);
  }

  sec_on = (t->tm_sec % 2 == 0);

  layer_mark_dirty(s_layer);
}

// ================================
//  Tick Handler
// ================================
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time(tick_time);
}

// ================================
//  Window
// ================================
static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_layer = layer_create(bounds);
  layer_set_update_proc(s_layer, layer_update_proc);
  layer_add_child(root, s_layer);

  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  update_time(t);
}

static void window_unload(Window *window) {
  layer_destroy(s_layer);
}

// ================================
//  main
// ================================
static void init(void) {
  s_window = window_create();
  window_set_click_config_provider(s_window, click_config_provider);

  window_set_background_color(s_window, GColorBlack);

  window_set_window_handlers(s_window, (WindowHandlers){
    .load = window_load,
    .unload = window_unload,
  });

  window_stack_push(s_window, true);
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
}

static void deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
