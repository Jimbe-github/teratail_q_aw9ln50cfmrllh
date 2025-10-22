//#define PDCURSES //PDCursesを使う場合はコレを有効に

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <locale.h>

#ifdef PDCURSES
#include <windows.h>
#include <pdcurses.h>
#else
#include <unistd.h>
#include <ncurses.h>
#endif

//#define DEBUG

#define BOARD_XOFF 14
#define BOARD_YOFF 1
#define CAREA1_XOFF (BOARD_XOFF+30)
#define CAREA1_YOFF (BOARD_YOFF+8)
#define CAREA2_XOFF (BOARD_XOFF-12)
#define CAREA2_YOFF (BOARD_YOFF+1)

#define lengthof(array) (sizeof(array)/sizeof((array)[0]))
#define dirsign(side) ((side)==1?-1:(side)==2?1:0)

char const *TURN[] = { NULL, "先手", "後手" };

int min(int a, int b) { return a<b ? a : b; }
int max(int a, int b) { return a>b ? a : b; }

void init() {
  setlocale(LC_ALL, ""); //日本語の化け対策
#ifdef PDCURSES
  SetConsoleOutputCP(CP_UTF8);
#endif
  initscr();
  noecho();
  cbreak();
  curs_set(0);
  keypad(stdscr, TRUE);

  start_color();
  init_pair(1, COLOR_BLUE, COLOR_BLACK);
  //init_pair(2, COLOR_RED, COLOR_BLACK);
  init_pair(3, COLOR_MAGENTA, COLOR_BLACK);
  //init_pair(4, COLOR_GREEN, COLOR_BLACK);
  init_pair(5, COLOR_CYAN, COLOR_BLACK);
  //init_pair(6, COLOR_YELLOW, COLOR_BLACK);
  init_pair(7, COLOR_WHITE, COLOR_BLACK);
  bkgd(COLOR_PAIR(7));
}

void term() {
  echo();
  timeout(-1);
  endwin();
}

void print_surface() {
  const char *r[] = {"一","二","三","四","五","六","七","八","九"};

  attrset(COLOR_PAIR(1));
  int y = BOARD_YOFF;
  mvaddstr(y++, BOARD_XOFF+1, "９ ８ ７ ６ ５ ４ ３ ２ １");
  for(int i=0; ; ++i) {
    mvaddstr(y++, BOARD_XOFF, "+");
    for(int j=0; j<9; ++j) addstr("--+");
    if(i >= 9) break;
    mvaddstr(y++, BOARD_XOFF, "|");
    for(int j=0; j<9; ++j) addstr("　|");
    addstr(r[i]);
  }

  for(int y=CAREA1_YOFF, x=CAREA1_XOFF, i=0; i<2; y=CAREA2_YOFF, x=CAREA2_XOFF, i++) {
    mvaddstr(y++, x, "+--------+");
    for(int j=0; j<10; ++j) mvaddstr(y++, x, "|　　　　|");
    mvaddstr(y, x, "+--------+");
  }
}

typedef struct {
  int count;
  int f[9][9];
} MF_TABLE;

void mf_clear(MF_TABLE *mf) {
  memset(mf, 0, sizeof(MF_TABLE));
}
void mf_set(MF_TABLE *mf, int y, int x) {
  if(mf->f[y][x]) return;
  mf->f[y][x] = 1;
  ++mf->count;
}
int mf_is_set(MF_TABLE *mf, int y, int x) {
  return mf->f[y][x];
}

struct _board;

typedef struct {
  char *face;
  void (*mm_func)(struct _board*,int,int,MF_TABLE*);
  void (*mmc_func)(struct _board*,int,MF_TABLE*);
  int is_king;
  int is_pawn;
  int use_straight; //棋譜"直"表示有無
  int use_relative_LR; //棋譜"相対的左右"表示有無
  int promoted;
  char *log_face;
  int original;
} PTYPE;

typedef struct {
  PTYPE const *ptype;
  int side;
} SQUARE;

typedef struct _board {
  SQUARE s[9][9];
} BOARD;

PTYPE const *bd_set(BOARD *bd, int y, int x, PTYPE const *ptype, int side) {
  SQUARE *s = &bd->s[y][x];
  PTYPE const *old = s->ptype;
  s->ptype = ptype;
  s->side = ptype ? side : 0;
  return old;
}
int bd_on_board(int y, int x) {
  return (0 <= y && y < 9) && (0 <= x && x < 9);
}
int bd_get_side(BOARD *bd, int y, int x) {
  return bd->s[y][x].side;
}
PTYPE const *bd_get_ptype(BOARD *bd, int y, int x) {
  return bd->s[y][x].ptype;
}
void bd_mm_func(BOARD *bd, int y, int x, MF_TABLE *movable) {
  bd_get_ptype(bd, y, x)->mm_func(bd, y, x, movable);
}

typedef struct {
  int count[7]; //PTYPE別持ち駒数
  int oy, ox; //表示原点
  int dy, dx; //表示向き
  SQUARE s[10][4]; //表示用([0][0]が左上)
} CAPTURED;

void ca_set(CAPTURED *ca, int w, int y, int x, PTYPE const *ptype, int side) {
  SQUARE *s = &ca[w-1].s[y][x];
  s->ptype = ptype;
  s->side = ptype ? side : 0;
}
int ca_get_side(CAPTURED *ca, int w, int y, int x) {
  return ca[w-1].s[y][x].side;
}
PTYPE const *ca_get_ptype(CAPTURED *ca, int w, int y, int x) {
  return ca[w-1].s[y][x].ptype;
}
void ca_mmc_func(CAPTURED *ca, int w, int y, int x, BOARD *board, MF_TABLE *movable) {
  ca_get_ptype(ca, w, y, x)->mmc_func(board, w, movable);
}

typedef struct {
  int x, y, w; //位置xy, ウインドウw:盤=0,持ち駒1=1,持ち駒2=2
} POINT;

void point_setv(POINT *p, int w, int y, int x) {
  p->w=w, p->y=y, p->x=x;
}
void point_clr(POINT *p) {
  p->w = -1;
}
int point_is_clr(POINT *p) {
  return p->w < 0;
}
void point_cpy(POINT *p1, POINT *p2) {
  memcpy(p1, p2, sizeof(POINT));
}
int point_cmp(POINT *p1, POINT *p2) {
  return p1->w==p2->w && p1->y==p2->y && p1->x==p2->x;
}
int point_cmpv(POINT *p, int w, int y, int x) {
  return p->w==w && p->y==y && p->x==x;
}

typedef struct {
  WINDOW *w;
  int last_x, last_y;
} LOG;

void init_logwin(LOG *log) {
  log->w = subwin(stdscr, 22, 22, 1, 55);
  //wborder(log->w, '|','|','-','-','+','+','+','+');
  //box(log->w, 0, 0);
  scrollok(log->w, TRUE);
  log->last_x = log->last_y = -1;
}

typedef struct _game {
  BOARD board;
  MF_TABLE movable;
  POINT cursor; //カーソル
  POINT selected; //選択中
  CAPTURED captured[2]; //持ち駒
  LOG log;
} GAME;

#define GOLD_GENERAL 0 //金
//#define SILVER_GENERAL 1 //銀
//#define KNIGHT 2 //桂
//#define LANCE 3 //香
#define BISHOP 4 //角
#define ROOK 5 //飛
#define PAWN 6 //歩

void mm_pattern(BOARD *board, int y, int x, const int *d, int size, MF_TABLE *movable) {
  int side = bd_get_side(board, y, x);
  int ds = dirsign(side);
  for(int i=0; i<size; ) {
    int yy = y + d[i++] * ds;
    int xx = x + d[i++] * ds;
    if(!bd_on_board(yy,xx) || bd_get_side(board,yy,xx)==side) continue;
    mf_set(movable, yy, xx);
  }
}

void mm_line(BOARD *board, int y, int x, int dy, int dx, MF_TABLE *movable) {
  int side = bd_get_side(board, y, x);
  int ds = dirsign(side);
  do {
    y += dy * ds;
    x += dx * ds;
    if(!bd_on_board(y,x) || bd_get_side(board,y,x)==side) break;
    mf_set(movable, y, x);
  } while(!bd_get_side(board,y,x));
}

//marking_movable functions
//王/玉
void mm_0(BOARD *board, int y, int x, MF_TABLE *movable) {
  int const d[] = { 0,1, 1,1, 1,0, 1,-1, 0,-1, -1,-1, -1,0, -1,1 }; //8方向
  mm_pattern(board, y, x, d, lengthof(d), movable);
}
//金
void mm_1(BOARD *board, int y, int x, MF_TABLE *movable) {
  int const d[] = { 0,1, 1,1, 1,0, 1,-1, 0,-1, -1,0 }; //斜め後ろ以外
  mm_pattern(board, y, x, d, lengthof(d), movable);
}
//銀
void mm_2(BOARD *board, int y, int x, MF_TABLE *movable) {
  int const d[] = { 1,1, 1,0, 1,-1, -1,-1, -1,1 }; //横後ろ以外
  mm_pattern(board, y, x, d, lengthof(d), movable);
}
//桂
void mm_3(BOARD *board, int y, int x, MF_TABLE *movable) {
  int const d[] = { 2,1, 2,-1 }; //二つ前の左右
  mm_pattern(board, y, x, d, lengthof(d), movable);
}
//香
void mm_4(BOARD *board, int y, int x, MF_TABLE *movable) {
  mm_line(board, y, x, 1, 0, movable);
}
int const DIR_STRAIGHT[] = { 0,1, 1,0, 0,-1, -1,0 }; //前後左右
int const DIR_DIAGONAL[] = { 1,1, 1,-1, -1,-1, -1,1 }; //斜め
//角
void mm_5(BOARD *board, int y, int x, MF_TABLE *movable) {
  for(int i=0; i<lengthof(DIR_DIAGONAL); i+=2) {
    mm_line(board, y, x, DIR_DIAGONAL[i], DIR_DIAGONAL[i+1], movable);
  }
}
//飛
void mm_6(BOARD *board, int y, int x, MF_TABLE *movable) {
  for(int i=0; i<lengthof(DIR_STRAIGHT); i+=2) {
    mm_line(board, y, x, DIR_STRAIGHT[i], DIR_STRAIGHT[i+1], movable);
  }
}
//歩
void mm_7(BOARD *board, int y, int x, MF_TABLE *movable) {
  int side = bd_get_side(board, y, x);
  y += dirsign(side);
  if(bd_on_board(y, x) && bd_get_side(board,y,x)!=side) mf_set(movable, y, x);
}
//馬
void mm_8(BOARD *board, int y, int x, MF_TABLE *movable) {
  mm_5(board, y, x, movable);
  mm_pattern(board, y, x, DIR_STRAIGHT, lengthof(DIR_STRAIGHT), movable);
}
//龍
void mm_9(BOARD *board, int y, int x, MF_TABLE *movable) {
  mm_6(board, y, x, movable);
  mm_pattern(board, y, x, DIR_DIAGONAL, lengthof(DIR_DIAGONAL), movable);
}

void mark_spaces(BOARD *board, int side, int ystart, int yend_exclusive, int (*f)(BOARD*,int,int), MF_TABLE *movable) {
  for(int x=0; x<9; ++x) {
    if(f && f(board,side,x)) continue;
    for(int y=ystart; y<yend_exclusive; ++y) {
      if(bd_get_side(board,y,x)) continue;
      mf_set(movable, y, x);
    }
  }
}

void mmc_a(BOARD *board, int side, MF_TABLE *movable) {
  mark_spaces(board, side, 0, 9, NULL, movable);
}

void mmc_b(BOARD *board, int side, MF_TABLE *movable) {
  mark_spaces(board, side, (side==1?1:0), (side==1?9:8), NULL, movable);
}

void mmc_c(BOARD *board, int side, MF_TABLE *movable) {
  mark_spaces(board, side, (side==1?2:0), (side==1?9:7), NULL, movable);
}

int on_pawn(BOARD *board, int side, int x) {
  for(int y=0; y<9; ++y) {
    if(bd_get_side(board,y,x)==side && bd_get_ptype(board,y,x)->is_pawn) return 1;
  }
  return 0;
}

void mmc_d(BOARD *board, int side, MF_TABLE *movable) {
  mark_spaces(board, side, (side==1?1:0), (side==1?9:8), on_pawn, movable);
}

PTYPE const PTYPES[] = {
  { "金", mm_1, mmc_a, 0, 0, 1, 0, -1, NULL,    0 },
  { "銀", mm_2, mmc_a, 0, 0, 1, 0,  9, NULL,    1 },
  { "桂", mm_3, mmc_c, 0, 0, 0, 0, 10, NULL,    2 },
  { "香", mm_4, mmc_b, 0, 0, 0, 0, 11, NULL,    3 },
  { "角", mm_5, mmc_a, 0, 0, 0, 0, 12, NULL,    4 },
  { "飛", mm_6, mmc_a, 0, 0, 0, 0, 13, NULL,    5 },
  { "歩", mm_7, mmc_d, 0, 1, 0, 0, 14, NULL,    6 },
  { "王", mm_0, NULL,  1, 0, 0, 0, -1, NULL,   -1 },
  { "玉", mm_0, NULL,  1, 0, 0, 0, -1, NULL,   -1 },
  { "ぎ", mm_1, NULL,  0, 0, 1, 0, -1, "成銀",  1 },
  { "き", mm_1, NULL,  0, 0, 1, 0, -1, "成桂",  2 },
  { "キ", mm_1, NULL,  0, 0, 1, 0, -1, "成香",  3 },
  { "馬", mm_8, NULL,  0, 0, 0, 1, -1, NULL,    4 },
  { "龍", mm_9, NULL,  0, 0, 0, 1, -1, NULL,    5 },
  { "と", mm_1, NULL,  0, 0, 1, 0, -1, NULL,    6 },
};

int const INIT_PTYPES[] = {
  //y,x,PTYPES_index
#ifdef DEBUG
  0,0,7,
          3,2,14,
  4,1,13,         4,3,13,
  5,1,12, 5,2,12,
#else
  0,0,3, 0,1,2, 0,2,1, 0,3,0, 0,4,8, 0,5,0, 0,6,1, 0,7,2, 0,8,3,
  1,1,5, 1,7,4,
  2,0,6, 2,1,6, 2,2,6, 2,3,6, 2,4,6, 2,5,6, 2,6,6, 2,7,6, 2,8,6,
  6,0,6, 6,1,6, 6,2,6, 6,3,6, 6,4,6, 6,5,6, 6,6,6, 6,7,6, 6,8,6,
  7,1,4, 7,7,5,
  8,0,3, 8,1,2, 8,2,1, 8,3,0, 8,4,7, 8,5,0, 8,6,1, 8,7,2, 8,8,3,
#endif
};

void exp_captured(CAPTURED captured[], int w) {
	CAPTURED *ca = &captured[w-1];
  memset(ca->s, 0, sizeof(ca->s));
  //金(0)・銀(1)・桂(2)・香(3)・角(4)はそれぞれ1行左詰め
  int y = ca->oy;
  for(int i=GOLD_GENERAL; i<=BISHOP; ++i) {
    if(ca->count[i] > 0) {
      for(int j=0, x=ca->ox; j<ca->count[i]; ++j, x+=ca->dx) {
        ca_set(captured, w, y, x, &PTYPES[i], w);
      }
      y += ca->dy;
    }
  }
  if(ca->count[ROOK] > 0) { //飛(ROOK)は、角(BISHOP)があったらその横、無かったら単独行
    int x = ca->ox + (ca->count[BISHOP] ? ca->dx*2 : 0);
    if(ca->count[BISHOP]) y -= ca->dy;
    for(int j=0; j<ca->count[ROOK]; ++j, x+=ca->dx) {
      ca_set(captured, w, y, x, &PTYPES[ROOK], w);
    }
    y += ca->dy;
  }
  if(ca->count[PAWN] > 0) { //歩(PAWN)は最大4列5行
    for(int j=0; j<ca->count[PAWN]; ++j) {
      int x = ca->ox + (j%4) * ca->dx;
      y += (j>0 && x==ca->ox) ? ca->dy : 0;
      ca_set(captured, w, y, x, &PTYPES[PAWN], w);
    }
  }
}

void add_captured(CAPTURED captured[], PTYPE const *ptype, int side) {
  ++captured[side-1].count[ptype->original];
  exp_captured(captured, side);
}

void remove_captured(CAPTURED captured[], PTYPE const *ptype, int side) {
  --captured[side-1].count[ptype->original];
  exp_captured(captured, side);
}

const char *get_face(GAME *game, int w, int y, int x) {
  if(w == 0) {
    PTYPE const *ptype = bd_get_ptype(&game->board, y, x);
    return ptype
             ? ptype->face
             : mf_is_set(&game->movable,y,x) ? "〇" : "　";
  }
  PTYPE const *ptype = ca_get_ptype(game->captured, w, y, x);
  return ptype ? ptype->face : "　";
}

const int COLOR[] = { 7, 5, 3 };

int get_attr(GAME *game, int w, int y, int x) {
  int attr = (point_cmpv(&game->selected,w,y,x) ? A_BOLD : 0) | //選択駒
             (point_cmpv(&game->cursor,w,y,x) ? A_REVERSE : 0); //カーソル
  if(w == 0) {
    attr |= (mf_is_set(&game->movable,y,x) ? A_BLINK : 0) | //移動可
            COLOR_PAIR(COLOR[bd_get_side(&game->board,y,x)]); //陣営別色
  } else {
    attr |= COLOR_PAIR(COLOR[ca_get_side(game->captured,w,y,x)]); //陣営別色
  }
  return attr;
}

typedef struct {
  int yoff, xoff;
  int height, width;
  int ystep, xstep;
} WINS;

WINS wins[] = {
	{ BOARD_YOFF+2,  BOARD_XOFF+1,   9, 9, 2, 3 },
	{ CAREA1_YOFF+1, CAREA1_XOFF+1, 10, 4, 1, 2 },
	{ CAREA2_YOFF+1, CAREA2_XOFF+1, 10, 4, 1, 2 }
};

void print_game(GAME *game) {
  for(int w=0; w<lengthof(wins); ++w) {
    WINS *win = &wins[w];
    for(int y=0, yy=win->yoff; y<win->height; ++y, yy+=win->ystep) {
      for(int x=0, xx=win->xoff; x<win->width; ++x, xx+=win->xstep) {
        attrset(get_attr(game,w,y,x));
        mvaddstr(yy, xx, get_face(game,w,y,x));
      }
    }
  }
}

void clear_select(GAME *game) {
  mf_clear(&game->movable);
  point_clr(&game->selected);
}

void init_game(GAME *game) {
  memset(game, 0, sizeof(GAME));
  for(int i=0; i<lengthof(INIT_PTYPES); ) {
    int y = INIT_PTYPES[i++];
    int x = INIT_PTYPES[i++];
    int j = INIT_PTYPES[i++];
		bd_set(&game->board, y, x, &PTYPES[j], y<=2?2:1);
  }
  clear_select(game);
  point_setv(&game->cursor, 0, 8, 4);

  game->captured[0].oy = 9; //左下から
  game->captured[0].ox = 0;
  game->captured[0].dy = -1; //右上に向かって積む
  game->captured[0].dx = 1;

  game->captured[1].oy = 0; //右上から
  game->captured[1].ox = 3;
  game->captured[1].dy = 1; //左下に向かって積む
  game->captured[1].dx = -1;

  init_logwin(&game->log);
}

void select_piece(GAME *game) {
  int cancel = point_cmp(&game->selected, &game->cursor);
  clear_select(game);
  if(!cancel) {
    int w = game->cursor.w, y = game->cursor.y, x = game->cursor.x;
    if(w == 0) {
      bd_mm_func(&game->board, y, x, &game->movable);
    } else {
      ca_mmc_func(game->captured, w, y, x, &game->board, &game->movable);
    }
    if(game->movable.count > 0) {
      point_cpy(&game->selected, &game->cursor);
    }
  }
}

int can_promote(PTYPE const *ptype, int side, int cy, int sy) {
  return ptype->promoted>=0 &&
         ((side==1 && (cy<=2 || sy<=2)) || //敵陣内へ/から移動したか
          (side==2 && (cy>=6 || sy>=6))); //     〃
}

int get_cursor_side(GAME *game) {
  int cw = game->cursor.w, cy = game->cursor.y, cx = game->cursor.x;
  return cw == 0
           ? bd_get_side(&game->board, cy, cx)
           : ca_get_side(game->captured, cw, cy, cx);
}

char const *MOVE_STRS[] = { "引", "寄", "上" }; //-1/0/1

int move_val(int cy, int sy) {
  return cy==sy ? 0 : sy<cy ? -1 : 1;
}

char const *DIR_STRS[] = { "左", "", "右" }; //-1/0/1

int dir_val(int cx, int sx, int ds) {
  return (cx==sx ? 0 : sx<cx ? -1 : 1) * -ds;
}

char const *PROMOTE_STRS[] = { "不成", "", "成" }; //-1/0/1

void print_movelog(GAME *game, PTYPE const *ptype, int side, int promote) {
  int cw = game->cursor.w, cy = game->cursor.y, cx = game->cursor.x;
  int sw = game->selected.w, sy = game->selected.y, sx = game->selected.x;

  int ds = dirsign(side);

  int move = move_val(cy, sy);
  int dir = dir_val(cx, sx, ds);

  wattrset(game->log.w, COLOR_PAIR(COLOR[side]));
#ifdef DEBUG
  wprintw(game->log.w, "\n----\n" );
#endif

  MF_TABLE mf;
  int cnt = 0, move_same_cnt = 0, dir_same_cnt = 0;
  int relative = -1;
  for(int y=0; y<9; y++) {
    for(int x=0; x<9; x++) {
      if((y==sy && x==sx) ||
         bd_get_ptype(&game->board,y,x)!=ptype ||
         bd_get_side(&game->board,y,x)!=side) continue;
      mf_clear(&mf);
      ptype->mm_func(&game->board, y, x, &mf);
      if(!mf_is_set(&mf, cy, cx)) continue;

      ++cnt;
      if(move_val(cy,y) == move) ++move_same_cnt; //同じ動き
      if(dir_val(cx,x,ds) == dir) ++dir_same_cnt; //同じ方向
      if(ptype->use_relative_LR) dir = dir_val(x, sx, ds); //駒同士の相対
    }
  }

#ifdef DEBUG
  wprintw(game->log.w, "move=%d\n", move);
  wprintw(game->log.w, "dir=%d\n", dir);
  wprintw(game->log.w, "cnt=%d\n", cnt);
  wprintw(game->log.w, "move_same_cnt=%d\n", move_same_cnt);
  wprintw(game->log.w, "dir_same_cnt=%d\n", dir_same_cnt);
#endif

  char const *move_str = "";
  char const *dir_str = "";
  if(cnt != 0) {
    if(move_same_cnt==0) {
      move_str = MOVE_STRS[move+1];
    } else if(ptype->use_straight && cy+1==sy && dir==0) {
      dir_str = "直";
    } else {
      if(dir_same_cnt != 0) move_str = MOVE_STRS[move+1];
      dir_str = DIR_STRS[dir+1];
    }
  }

  char const *promotion = PROMOTE_STRS[promote + 1];
  char const *drop_str = sw&&cnt ? "打" : "";

  char dist[16];
  if(game->log.last_x==cx && game->log.last_y==cy) {
    strcpy(dist, "同");
  } else {
    sprintf(dist, "%d%d", 9-cx, cy+1);
    game->log.last_x = cx;  game->log.last_y = cy;
  }

#ifdef DEBUG
  wprintw(game->log.w, "----\n");
#endif
  wprintw(game->log.w, "%s%s%s%s%s%s\n", dist,
          ptype->log_face ? ptype->log_face : ptype->face,
          dir_str, move_str, promotion, drop_str);
  wrefresh(game->log.w);
}

PTYPE const *move_piece(GAME *game, int side) {
  PTYPE const *ptype;
  int cy = game->cursor.y, cx = game->cursor.x;
  int sy = game->selected.y, sx = game->selected.x, sw = game->selected.w;
  if(sw == 0) { //盤内の移動
    ptype = bd_get_ptype(&game->board, sy, sx);
    const PTYPE *promoted = NULL;
    int promote = can_promote(ptype, side, cy, sy);
    if(promote) {
      promoted = &PTYPES[ptype->promoted]; //成る TODO: 選択出来るようにする？
      promote = promoted ? 1 : -1; //成か不成か
    }
    print_movelog(game, ptype, side, promote);
    bd_set(&game->board, sy, sx, NULL, 0); //消す(print_movelogより先に消さないこと)
    if(promoted) ptype = promoted;
  } else { //持ち駒から打ち
    ptype = ca_get_ptype(game->captured, sw, sy, sx);
    remove_captured(game->captured, ptype, side); //消す
    print_movelog(game, ptype, side, 0);
  }

  clear_select(game);
  return bd_set(&game->board, cy, cx, ptype, side);
}

void move_cursor(POINT *cursor, int turn, int key) {
   if(key == KEY_UP) {
     if(cursor->y > 0) --cursor->y;
   } else if(key == KEY_DOWN) {
     if(cursor->y < (cursor->w==0?9:10)-1) ++cursor->y;
   } else if(key == KEY_LEFT && cursor->x > 0) {
     --cursor->x;
   } else if(key == KEY_RIGHT && cursor->x < (cursor->w==0?9:4)-1) {
     ++cursor->x;
   } else if(cursor->w == 0) {
     if(key == KEY_LEFT && turn == 2) {
       point_setv(cursor, 2, min(cursor->y*2,10-1), 4-1); //持ち駒2←盤
     } else if(key == KEY_RIGHT && turn == 1) {
       point_setv(cursor, 1, max((cursor->y-4)*2+1,0), 0); //盤→持ち駒1
     }
   } else if(cursor->w == 1) {
     if(key == KEY_LEFT) {
       point_setv(cursor, 0, (cursor->y-1)/2+4, 9-1); //盤←持ち駒1
     }
   } else if(cursor->w == 2) {
     if(key == KEY_RIGHT) {
       point_setv(cursor, 0, cursor->y/2, 0); //持ち駒2→盤
     }
   }
}

void print_turnlog(WINDOW *w, int turn) {
  wattrset(w, COLOR_PAIR(COLOR[turn]));
  wprintw(w, "%s ", TURN[turn]);
  wrefresh(w);
}

int main(int argc, char *argv[]) {
  init();

  GAME game;
  init_game(&game);

  int turn = 1; //1or2

  print_surface();
  print_game(&game);

  wattrset(game.log.w, COLOR_PAIR(7));
  wprintw(game.log.w, "将棋ゲーム\n");
  print_turnlog(game.log.w, turn);

  refresh();

  for(int end=0, key; !end && (key=getch())!='q'; ) {
    if(key==KEY_UP || key==KEY_DOWN || key== KEY_LEFT || key==KEY_RIGHT) {
      move_cursor(&game.cursor, turn, key);
    } else if(key == ' ') {
      if(get_cursor_side(&game) == turn) {
        select_piece(&game);
      } else if(game.cursor.w==0 && mf_is_set(&game.movable,game.cursor.y,game.cursor.x)) {
        PTYPE const *ptype = move_piece(&game, turn);
        if(ptype != NULL) {
          if(ptype->is_king) end = 1;
          add_captured(game.captured, ptype, turn);
        }
        if(!end) {
          turn ^= 3; //攻守交替
          print_turnlog(game.log.w, turn);
        }
      }
    }
    print_game(&game);
  }

  term();

  return 0;
}
