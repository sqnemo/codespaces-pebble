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

static int decay = 0;   // 荷物の劣化（0〜10）
#define MAX_DECAY 15

static bool game_over = false;
static bool passed_check = false;
static bool game_clear = false;




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
  {TILE_GOAL, TILE_MOUNTAIN, TILE_MOUNTAIN, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_RIVER, TILE_RIVER, TILE_RIVER},
  {TILE_EMPTY, TILE_MOUNTAIN, TILE_MOUNTAIN, TILE_EMPTY, TILE_EMPTY, TILE_RIVER, TILE_RIVER, TILE_RIVER, TILE_MOUNTAIN, TILE_MOUNTAIN},
  {TILE_EMPTY, TILE_EMPTY, TILE_STRANDED, TILE_CHECK, TILE_RIVER, TILE_RIVER, TILE_MOUNTAIN, TILE_MOUNTAIN, TILE_MOUNTAIN, TILE_MOUNTAIN},
  {TILE_EMPTY, TILE_EMPTY, TILE_RIVER, TILE_RIVER, TILE_RIVER, TILE_MOUNTAIN, TILE_MOUNTAIN, TILE_MOUNTAIN, TILE_MOUNTAIN, TILE_MOUNTAIN},
  {TILE_EMPTY, TILE_RIVER, TILE_RIVER, TILE_STRANDED, TILE_STRANDED, TILE_MOUNTAIN, TILE_MOUNTAIN, TILE_MOUNTAIN, TILE_MOUNTAIN, TILE_MOUNTAIN},
  {TILE_RIVER, TILE_RIVER, TILE_STRANDED, TILE_STRANDED, TILE_STRANDED, TILE_EMPTY, TILE_MOUNTAIN, TILE_MOUNTAIN, TILE_MOUNTAIN, TILE_MOUNTAIN},
  {TILE_MOUNTAIN, TILE_RIVER, TILE_MOUNTAIN, TILE_STRANDED, TILE_STRANDED, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY},
  {TILE_MOUNTAIN, TILE_RIVER, TILE_MOUNTAIN, TILE_MOUNTAIN, TILE_MOUNTAIN, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY},
  {TILE_MOUNTAIN, TILE_MOUNTAIN, TILE_MOUNTAIN, TILE_MOUNTAIN, TILE_MOUNTAIN, TILE_MOUNTAIN, TILE_EMPTY, TILE_EMPTY, TILE_EMPTY, TILE_START}
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

//ダイスの出目制限
static int roll_dice_for_tile(TileType tile) {
  switch(tile) {
    case TILE_MOUNTAIN:
      return (rand() % 2) + 1;   // 1〜2
    case TILE_RIVER:
      return (rand() % 3) + 1;   // 1〜3
    case TILE_STRANDED:
      return 1;                  // 1固定
    default:
      return (rand() % 4) + 1;   // 1〜4
  }
}


//マップ内チェック
static bool is_in_map(int x, int y) {
  return (x >= 0 && x < MAP_COLS && y >= 0 && y < MAP_ROWS);
}

// ---- マップ描画 ----
static void map_layer_update(Layer *layer, GContext *ctx) {

  if (game_over) {
      graphics_context_set_text_color(ctx, GColorBlack);

      graphics_draw_text(
          ctx,
          "GAME OVER",
          fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
          GRect(0, 90, 200, 40),
          GTextOverflowModeWordWrap,
          GTextAlignmentCenter,
          NULL
      );
      return; // ← マップ描画を停止
  }

  // ---- GAME CLEAR ----
  if (game_clear) {
      graphics_context_set_text_color(ctx, GColorBlack);
      graphics_draw_text(
          ctx,
          "CLEAR!",
          fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
          GRect(0, 90, 200, 40),
          GTextOverflowModeWordWrap,
          GTextAlignmentCenter,
          NULL
      );
      return;
  }



  // --- タイル描画 ---
  for(int y = 0; y < MAP_ROWS; y++) {
    for(int x = 0; x < MAP_COLS; x++) {

      TileType t = map[y][x];
      GRect rect = GRect(x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE);

      graphics_context_set_fill_color(ctx, tile_color(t));
      graphics_fill_rect(ctx, rect, 0, GCornerNone);

//      graphics_context_set_stroke_color(ctx, GColorBlack);
//      graphics_draw_rect(ctx, rect);
      // --- 中央の小さな黒ドット ---
      graphics_context_set_fill_color(ctx, GColorBlack);

      // このマスの中心座標
      int cx = rect.origin.x + TILE_SIZE / 2;
      int cy = rect.origin.y + TILE_SIZE / 2;

      // 2x2 の黒い四角として描画（好みで 3x3 にもできる）
      graphics_fill_rect(ctx, GRect(cx - 1, cy - 1, 2, 2), 0, GCornerNone);

      // START
      if(t == TILE_START) {
        graphics_context_set_fill_color(ctx, GColorRed);
        graphics_fill_circle(ctx, GPoint(rect.origin.x + 10, rect.origin.y + 10), 6);
      }

      // GOAL
      if(t == TILE_GOAL) {
        graphics_context_set_fill_color(ctx, GColorBlack);
        graphics_fill_circle(ctx, GPoint(rect.origin.x + 10, rect.origin.y + 10), 6);
      }

      // CHECK
      if(t == TILE_CHECK) {
        graphics_context_set_fill_color(ctx, GColorGreen);

        GPoint p1 = GPoint(rect.origin.x + 10, rect.origin.y + 4);
        GPoint p2 = GPoint(rect.origin.x + 4,  rect.origin.y + 16);
        GPoint p3 = GPoint(rect.origin.x + 16, rect.origin.y + 16);

        GPathInfo info = {
          .num_points = 3,
          .points = (GPoint[]){ p1, p2, p3 }
        };
        GPath *tri = gpath_create(&info);
        gpath_draw_filled(ctx, tri);
        gpath_destroy(tri);
      }
    }
  }

  // --- プレイヤー ---
  GPoint pc = GPoint(
    player_x * TILE_SIZE + TILE_SIZE / 2,
    player_y * TILE_SIZE + TILE_SIZE / 2
  );
  graphics_context_set_stroke_color(ctx, GColorRed);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_circle(ctx, pc, 7);
  graphics_draw_circle(ctx, pc, 6);

  // --- 移動カーソル ---
  if (moving_phase && dice_result > 0) {
    int cx, cy;
    get_cursor_position(&cx, &cy);
    if (is_in_map(cx, cy)) {

      GPoint p1 = GPoint(cx * TILE_SIZE + 10, cy * TILE_SIZE + 4);
      GPoint p2 = GPoint(cx * TILE_SIZE + 4,  cy * TILE_SIZE + 16);
      GPoint p3 = GPoint(cx * TILE_SIZE + 16, cy * TILE_SIZE + 16);

      GPathInfo info = {
        .num_points = 3,
        .points = (GPoint[]){ p1, p2, p3 }
      };

      GPath *path = gpath_create(&info);
      graphics_context_set_fill_color(ctx, GColorRed);
      gpath_draw_filled(ctx, path);
      gpath_destroy(path);
    }
  }

  // --- ダイス ---
  if (dice_result > 0) {

    TileType tile = map[player_y][player_x];
    GColor dice_color = GColorRed;

    switch(tile) {
      case TILE_MOUNTAIN: dice_color = GColorArmyGreen; break;
      case TILE_RIVER:    dice_color = GColorBlue;      break;
      case TILE_STRANDED: dice_color = GColorLightGray; break;
      default:            dice_color = GColorRed;       break;
    }

    int base_x = 100;
    int y = MAP_ROWS * TILE_SIZE + 4;

    for (int i = 0; i < dice_result; i++) {
      GRect r = GRect(base_x + i * 14, y, 12, 12);
      graphics_context_set_fill_color(ctx, dice_color);
      graphics_fill_rect(ctx, r, 0, GCornerNone);
    }
  }

  // --- 荷物劣化 ---
  int decay_x = 0;
  int decay_y = MAP_ROWS * TILE_SIZE + 20;

  for (int i = 0; i < decay; i++) {
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, GRect(decay_x + i * 14, decay_y, 12, 12),
                       0, GCornerNone);
  }
}



static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  int cx, cy;
  get_cursor_position(&cx, &cy);

  if (game_over) {
      // ---- リセット処理 ----
      decay = 0;
      dice_result = 0;
      moving_phase = false;
      move_dir = 0;
      player_x = 7;
      player_y = 8;
      game_over = false;

      layer_mark_dirty(s_map_layer);
      return;
  }

  // ---- GAME CLEAR ----
  if (game_clear) {
      // ---- リセット ----
      decay = 0;
      dice_result = 0;
      moving_phase = false;
      move_dir = 0;
      player_x = 7;
      player_y = 8;
      passed_check = false;
      game_clear = false;
      game_over = false;

      layer_mark_dirty(s_map_layer);
      return;
  }


  if (!moving_phase) {
    // 現在地の地形を判定
    TileType current = map[player_y][player_x];
    decay++;
    if (decay >= 15) {
      game_over = true;
    }
    // 地形に応じたダイスを振る
    dice_result = roll_dice_for_tile(current);

    move_dir = 0;
    moving_phase = true;

    // ★方向補正ループ
    for (int i = 0; i < 4; i++) {
      int nx, ny;
      get_cursor_position(&nx, &ny);
      if (is_in_map(nx, ny)) break;
      move_dir = (move_dir + 1) % 4;
    }

  } else {
    // --- 移動処理（既存） ---
    if (is_in_map(cx, cy)) {

        TileType before = map[player_y][player_x];  // 元の地形
        TileType after  = map[cy][cx];              // 移動先の地形

        // ★ チェックポイント通過（立ち止まる必要なし）
        if (map[player_y][player_x] == TILE_CHECK) {
            passed_check = true;
        }
        
        // ★ 座礁地帯に侵入したら decay+2 ★
        if (after == TILE_STRANDED && before != TILE_STRANDED) {
            decay += 2;
            if (decay >= 15) {
              game_over = true;
            }
            if (decay > MAX_DECAY) decay = MAX_DECAY;
        }

        // 移動
        player_x = cx;
        player_y = cy;

        // --- ペナルティチェック ---
        bool stop_tile = false;

        if (after == TILE_MOUNTAIN || after == TILE_RIVER || after == TILE_STRANDED) {
            if (before != after) {   // 異なる地形に移動した時だけ停止
                stop_tile = true;
            }
        }

        // ★ ゴール判定（チェック通過してなければ無効）
        if (map[player_y][player_x] == TILE_GOAL) {
            if (passed_check) {
              game_clear = true;
              moving_phase = false;  // 動きを止める
            } else {
                // チェック通過してない → ゴール無効（何も起きない）
            }
        }

        if (stop_tile) {
            dice_result = 0;
            moving_phase = false;

        } else {

            // 通常のダイス消費
            dice_result--;

            if (dice_result <= 0) {
                moving_phase = false;
            } else {
                // --- 移動後の方向補正 ---
                for (int i = 0; i < 4; i++) {
                    int nx, ny;
                    get_cursor_position(&nx, &ny);
                    if (is_in_map(nx, ny)) break;
                    move_dir = (move_dir + 1) % 4;
                }
            }
        }
    }

  }

  layer_mark_dirty(s_map_layer);
}



static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (!moving_phase) {
      return;   // 移動フェーズ外では UP は完全に無視
  }

//  if (!moving_phase) {

      // 現在地タイル判定
//      TileType current = map[player_y][player_x];

      // ダイス出目決定
//      dice_result = roll_dice_for_tile(current);

      // ★★ 荷物劣化：ターン開始時に +1 ★★
//      if (decay < MAX_DECAY) decay++;

//      move_dir = 0;
//      moving_phase = true;

      // 方向補正（既存）
//      for (int i = 0; i < 4; i++) {
//        int nx, ny;
//        get_cursor_position(&nx, &ny);
//        if (is_in_map(nx, ny)) break;
//        move_dir = (move_dir + 1) % 4;
//      }
//  }

  for (int i = 0; i < 4; i++) {
    move_dir = (move_dir + 1) % 4;

    int cx, cy;
    get_cursor_position(&cx, &cy);

    if (is_in_map(cx, cy)) {
      break;  // 有効方向が見つかった！
    }
  }

  layer_mark_dirty(s_map_layer);
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

  // --- 画面全体のレイヤーにする（縦228px） ---
  s_map_layer = layer_create(GRect(0, 0, 200, 228));

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
