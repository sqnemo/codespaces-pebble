#include <pebble.h>

#define TILE_SIZE 20
#define MAP_COLS 10
#define MAP_ROWS 9

// ---- グローバル変数（必ず先に置く） ----
static Window *s_main_window;
static Layer *s_map_layer;

static int player_x = 7;   // 列（0〜9）
static int player_y = 8;   // 行（0〜8）
static int dice_result = 0;

static int move_dir = 0;       // 0=上, 1=右, 2=下, 3=左
static bool moving_phase = false; // true=プレイヤーは移動待ち

// ---- 地形 enum ----
typedef enum {
  TILE_EMPTY = 0,
  TILE_MOUNTAIN,
  TILE_RIVER,
  TILE_STRANDED,
  TILE_START,
  TILE_CHECK,
  TILE_GOAL
} TileType;

// ---- マップデータ ----
static TileType map[MAP_ROWS][MAP_COLS] = {
  {TILE_GOAL, TILE_MOUNTAIN, TILE_MOUNTAIN, TILE_MOUNTAIN, TILE_EMPTY, TILE_RIVER, TILE_RIVER, TILE_RIVER, TILE_RIVER, TILE_RIVER},
  {TILE_EMPTY, TILE_MOUNTAIN, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_RIVER, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY},
  {TILE_EMPTY, TILE_EMPTY, TILE_CHECK, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY},
  {TILE_EMPTY, TILE_RIVER, TILE_RIVER, TILE_STRANDED, TILE_STRANDED, TILE_STRANDED, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY},
  {TILE_RIVER, TILE_RIVER, TILE_EMPTY, TILE_STRANDED, TILE_STRANDED, TILE_STRANDED, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY},
  {TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY},
  {TILE_MOUNTAIN, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY},
  {TILE_MOUNTAIN, TILE_MOUNTAIN, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY},
  {TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_START, TILE_EMPTY, TILE_EMPTY}
};


// ---- 色定義 ----
static GColor tile_color(TileType t) {
  switch(t) {
    case TILE_MOUNTAIN: return GColorArmyGreen;
    case TILE_RIVER: return GColorBlue;
    case TILE_STRANDED: return GColorLightGray;
    default: return GColorWhite;
  }
}

// ---- 関数プロトタイプ ----
static void get_cursor_position(int *cx, int *cy);

// ---- マップ描画 ----
static void map_layer_update(Layer *layer, GContext *ctx) {
  for(int y = 0; y < MAP_ROWS; y++) {
    for(int x = 0; x < MAP_COLS; x++) {
      TileType t = map[y][x];
      GRect rect = GRect(x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE);

      // 塗りつぶし
      graphics_context_set_fill_color(ctx, tile_color(t));
      graphics_fill_rect(ctx, rect, 0, GCornerNone);

      // 枠線
      graphics_context_set_stroke_color(ctx, GColorBlack);
      graphics_draw_rect(ctx, rect);

      // 特殊マーカー
      if(t == TILE_START) {
        graphics_context_set_fill_color(ctx, GColorRed);
        graphics_fill_circle(ctx, GPoint(rect.origin.x + 10, rect.origin.y + 10), 6);
      }

      if(t == TILE_GOAL) {
        graphics_context_set_fill_color(ctx, GColorBlack);
        graphics_fill_circle(ctx, GPoint(rect.origin.x + 10, rect.origin.y + 10), 6);
      }

      if(t == TILE_CHECK) {
        graphics_context_set_fill_color(ctx, GColorGreen);
        GPathInfo tri_info = {
          .num_points = 3,
          .points = (GPoint[]) {
            {rect.origin.x + 10, rect.origin.y + 4},
            {rect.origin.x + 4, rect.origin.y + 16},
            {rect.origin.x + 16, rect.origin.y + 16},
          }
        };
        GPath *tri = gpath_create(&tri_info);
        gpath_draw_filled(ctx, tri);
        gpath_destroy(tri);
      }
    }
  }

  // ← この下にプレイヤーを描画
  //-------------------------------
  GPoint center = GPoint(
    player_x * TILE_SIZE + TILE_SIZE / 2,
    player_y * TILE_SIZE + TILE_SIZE / 2
  );
  graphics_context_set_stroke_color(ctx, GColorRed);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_circle(ctx, center, 7);
  graphics_draw_circle(ctx, center, 6);

  // ---- 移動カーソル（赤い▲） ----
  if (moving_phase && dice_result > 0) {
    int cx, cy;
    get_cursor_position(&cx, &cy);

    // タイル外を描かないように（今は軽くガード）
    if (cx >= 0 && cx < MAP_COLS && cy >= 0 && cy < MAP_ROWS) {
      GPoint p1 = GPoint(cx * TILE_SIZE + 10, cy * TILE_SIZE + 4);
      GPoint p2 = GPoint(cx * TILE_SIZE + 4, cy * TILE_SIZE + 16);
      GPoint p3 = GPoint(cx * TILE_SIZE + 16, cy * TILE_SIZE + 16);

      graphics_context_set_fill_color(ctx, GColorRed);

      GPathInfo tri = {
        .num_points = 3,
        .points = (GPoint[]) { p1, p2, p3 }
      };
      GPath path = { .points = tri.points, .num_points = tri.num_points };
      gpath_draw_filled(ctx, &path);
    }
  }

  // ---- ダイス出目ゲージ（赤い四角） ----
  if (dice_result > 0) {
    int start_x = 100;  // スクショに合わせて右下寄せ。あとで微調整できる
    int y = MAP_ROWS * TILE_SIZE + 4;  // マップのすぐ下

    for (int i = 0; i < dice_result; i++) {
      GRect r = GRect(start_x + i * 14, y, 12, 12);
      graphics_context_set_fill_color(ctx, GColorRed);
      graphics_fill_rect(ctx, r, 0, GCornerNone);
    }
  }

}

static void get_cursor_position(int *cx, int *cy) {
  *cx = player_x;
  *cy = player_y;

  switch(move_dir) {
    case 0: *cy -= 1; break; // 上
    case 1: *cx += 1; break; // 右
    case 2: *cy += 1; break; // 下
    case 3: *cx -= 1; break; // 左
  }
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {

  if (!moving_phase) {
    // --- ダイスを振る（移動フェーズ開始） ---
    dice_result = (rand() % 4) + 1;
    move_dir = 0;              // 最初は上を指す
    moving_phase = true;
  } else {
    // --- 移動を確定する ---
    int cx, cy;
    get_cursor_position(&cx, &cy);

    // 今は地形チェックなしで移動
    player_x = cx;
    player_y = cy;
    dice_result--;

    if (dice_result <= 0) {
      moving_phase = false;   // 全部使い切ったら終了
    }
  }

  layer_mark_dirty(s_map_layer);
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (moving_phase) {
    move_dir = (move_dir + 1) % 4;  // 時計回り
    layer_mark_dirty(s_map_layer);
  }
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP,     up_click_handler);
}


// ---- ウィンドウ ----
static Window *s_main_window;
static Layer *s_map_layer;

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // map_layer を画面全体にする
  s_map_layer = layer_create(bounds);

  layer_set_update_proc(s_map_layer, map_layer_update);
  layer_add_child(window_layer, s_map_layer);
}


static void main_window_unload(Window *window) {
  layer_destroy(s_map_layer);
}

static void init() {
  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  window_stack_push(s_main_window, true);
  window_set_click_config_provider(s_main_window, click_config_provider);
}

static void deinit() {
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
