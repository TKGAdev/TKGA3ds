// TKGA para Nintendo 3DS: GWW (Codenames) y Sintonia en modo "pasar la consola", con graficos y pantalla tactil.
#include <3ds.h>
#include <citro2d.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "data.h"

#define Z 0.5f
#define COL(rgb) C2D_Color32(((rgb) >> 16) & 255, ((rgb) >> 8) & 255, (rgb) & 255, 255)
#define PAPER 0xf2ecdf
#define INK 0x1c1b19
#define RED 0xc9473a
#define BLUE 0x2b57a8
#define YEL 0xf4b400
#define NEUT 0xd9d2c0
#define WHITE 0xfffdf5
#define GREEN 0x59d4a5
#define ORANGE 0xff8a3d
#define YWEDGE 0xffd166
#define GREY 0x8a8577
#define PI_F 3.14159265f

static C3D_RenderTarget *TOP, *BOT;
static C2D_TextBuf tb;
static u32 down, held;
static int tx, ty;
static bool tdown, theld;
static unsigned fc = 0;
static bool quitApp = false;

/* ------------------------------------------------------------------ dibujo ------------------------------------------------------------------ */

static void rrect(float x, float y, float w, float h, float r, u32 c) {
    if (r * 2 > h) r = h / 2;
    if (r * 2 > w) r = w / 2;
    C2D_DrawRectSolid(x + r, y, Z, w - 2 * r, h, c);
    C2D_DrawRectSolid(x, y + r, Z, w, h - 2 * r, c);
    C2D_DrawCircleSolid(x + r, y + r, Z, r, c);
    C2D_DrawCircleSolid(x + w - r, y + r, Z, r, c);
    C2D_DrawCircleSolid(x + r, y + h - r, Z, r, c);
    C2D_DrawCircleSolid(x + w - r, y + h - r, Z, r, c);
}

// Cartel con borde y sombra dura (estilo de la portada original)
static void card(float x, float y, float w, float h, u32 fill, float sh) {
    if (sh > 0) rrect(x + sh, y + sh + 1, w, h, 8, COL(INK));
    rrect(x, y, w, h, 8, COL(INK));
    rrect(x + 3, y + 3, w - 6, h - 6, 6, fill);
}

static void txt(const char *s, float x, float y, float sc, u32 col, int al) {
    C2D_Text t;
    C2D_TextParse(&t, tb, s);
    C2D_TextOptimize(&t);
    float w, h;
    C2D_TextGetDimensions(&t, sc, sc, &w, &h);
    float px = al == 1 ? x - w / 2 : al == 2 ? x - w : x;
    C2D_DrawText(&t, C2D_WithColor, px, y, Z, sc, sc, col);
}

// Texto centrado en (cx,cy) que se achica hasta entrar en maxw
static void fit(const char *s, float cx, float cy, float maxw, float sc, u32 col) {
    C2D_Text t;
    C2D_TextParse(&t, tb, s);
    C2D_TextOptimize(&t);
    float w, h;
    C2D_TextGetDimensions(&t, sc, sc, &w, &h);
    if (w > maxw) { sc *= maxw / w; C2D_TextGetDimensions(&t, sc, sc, &w, &h); }
    C2D_DrawText(&t, C2D_WithColor, cx - w / 2, cy - h / 2, Z, sc, sc, col);
}

static void para(const char *s, float x, float y, float w, float sc, u32 col) {
    C2D_Text t;
    C2D_TextParse(&t, tb, s);
    C2D_TextOptimize(&t);
    C2D_DrawText(&t, C2D_WithColor | C2D_WordWrap, x, y, Z, sc, sc, col, w);
}

static bool inside(float x, float y, float w, float h) { return tx >= x && tx < x + w && ty >= y && ty < y + h; }

// Boton tactil: devuelve true en el cuadro en que se toca
static bool btn(float x, float y, float w, float h, const char *label, u32 fill, u32 fg, bool on) {
    bool pressed = on && theld && inside(x, y, w, h);
    float off = pressed ? 3 : 0;
    card(x + off, y + off, w, h, on ? fill : COL(NEUT), pressed ? 0 : 3);
    fit(label, x + off + w / 2, y + off + h / 2, w - 14, 0.55f, on ? fg : COL(GREY));
    return on && tdown && inside(x, y, w, h);
}

// Boton que se mantiene apretado
static bool holdbtn(float x, float y, float w, float h, const char *label, u32 fill, u32 fg) {
    bool pressed = theld && inside(x, y, w, h);
    float off = pressed ? 3 : 0;
    card(x + off, y + off, w, h, fill, pressed ? 0 : 3);
    fit(label, x + off + w / 2, y + off + h / 2, w - 14, 0.55f, fg);
    return pressed;
}

static void fan(float cx, float cy, float r, float v0, float v1, u32 c) {
    int n = (int)((v1 - v0) * 40) + 1;
    for (int i = 0; i < n; i++) {
        float a0 = PI_F * (1 - (v0 + (v1 - v0) * i / n)), a1 = PI_F * (1 - (v0 + (v1 - v0) * (i + 1) / n));
        C2D_DrawTriangle(cx, cy, c, cx + r * cosf(a0), cy - r * sinf(a0), c, cx + r * cosf(a1), cy - r * sinf(a1), c, Z);
    }
}

// Cuadrante de Sintonia. target<0: no mostrar zona. needle en 0..1
static void dial(float cx, float cy, float r, float needle, float target, bool needleOn) {
    fan(cx, cy, r + 5, 0, 1, COL(INK));
    fan(cx, cy, r, 0, 1, COL(WHITE));
    if (target >= 0) {
        float W[3] = {.16f, .10f, .04f};
        u32 C[3] = {COL(GREEN), COL(YWEDGE), COL(ORANGE)};
        for (int i = 0; i < 3; i++) {
            float a = target - W[i], b = target + W[i];
            if (a < 0) a = 0;
            if (b > 1) b = 1;
            fan(cx, cy, r * 0.93f, a, b, C[i]);
        }
    }
    fan(cx, cy, r * 0.30f, 0, 1, COL(PAPER));
    if (needleOn) {
        float a = PI_F * (1 - needle);
        C2D_DrawLine(cx, cy, COL(INK), cx + r * 0.95f * cosf(a), cy - r * 0.95f * sinf(a), COL(INK), 6, Z);
        C2D_DrawCircleSolid(cx, cy, Z, 12, COL(INK));
        C2D_DrawCircleSolid(cx, cy, Z, 5, COL(WHITE));
    }
}

static void tiles(float cx, float y, const char *letters, int n, float sz, bool bounce) {
    float gap = sz * 0.08f, tot = n * sz + (n - 1) * gap, x0 = cx - tot / 2;
    char ch[2] = {0, 0};
    for (int i = 0; i < n; i++) {
        float dy = bounce ? sinf((fc + i * 14) * 0.07f) * 3 : 0;
        float x = x0 + i * (sz + gap);
        u32 f = (i % 2) ? COL(YEL) : COL(BLUE);
        rrect(x + 3, y + dy + 4, sz, sz * 1.25f, 5, COL(INK));
        rrect(x, y + dy, sz, sz * 1.25f, 5, f);
        ch[0] = letters[i];
        fit(ch, x + sz / 2, y + dy + sz * 0.625f, sz, sz / 36.0f, (i % 2) ? COL(INK) : COL(WHITE));
    }
}

/* ---------------------------------------------------------------- entrada / cuadro --------------------------------------------------------------- */

static bool beginFrame(void) {
    if (!aptMainLoop()) { quitApp = true; return false; }
    hidScanInput();
    down = hidKeysDown();
    held = hidKeysHeld();
    theld = held & KEY_TOUCH;
    tdown = down & KEY_TOUCH;
    if (theld) {
        touchPosition t;
        hidTouchRead(&t);
        tx = t.px;
        ty = t.py;
    } else { tx = ty = -100; }
    fc++;
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TextBufClear(tb);
    return true;
}
static void endFrame(void) { C3D_FrameEnd(0); }
static void sceneTop(void) { C2D_TargetClear(TOP, COL(PAPER)); C2D_SceneBegin(TOP); }
static void sceneBot(void) { C2D_TargetClear(BOT, COL(PAPER)); C2D_SceneBegin(BOT); }

static int rnd(int n) { return rand() % n; }
typedef enum { SC_MENU, SC_SIN, SC_GWW } Scr;
static Scr scr = SC_MENU;

// Boton "Menu" con confirmacion de dos toques
static unsigned menuArm = 0;
static bool menuBtn(float x, float y) {
    bool armed = menuArm && fc < menuArm;
    if (btn(x, y, 78, 28, armed ? "¿Salir?" : "< Menú", armed ? COL(RED) : COL(WHITE), armed ? COL(WHITE) : COL(INK), true)) {
        if (armed) { menuArm = 0; return true; }
        menuArm = fc + 180;
    }
    return false;
}

static const char *TEAMN[2] = {"ROJO", "AZUL"};
static u32 teamCol(int t) { return COL(t == 0 ? RED : BLUE); }

static void scoreBar(int s0, int s1, int turn, const char *mid) {
    char b[32];
    for (int t = 0; t < 2; t++) {
        float x = t == 0 ? 8 : 256;
        snprintf(b, sizeof b, "%s %d", TEAMN[t], t == 0 ? s0 : s1);
        card(x, 6, 136, 30, teamCol(t), turn == t ? 4 : 0);
        fit(b, x + 68, 21, 120, 0.6f, COL(WHITE));
    }
    if (mid) fit(mid, 200, 21, 100, 0.42f, COL(INK));
}

/* ================================================================ SINTONIA ================================================================ */

enum { SI_INTRO, SI_PASS, SI_PEEK, SI_GUESS, SI_REVEAL, SI_END };
static struct {
    int st, sc[2], sw[2], team, idx, flip, target, guess, order[N_PAIRS], pos, pts;
    bool seen, moved;
} S;

static void sinNew(void) {
    if (S.pos >= N_PAIRS) {
        for (int i = N_PAIRS - 1; i > 0; i--) { int j = rnd(i + 1), t = S.order[i]; S.order[i] = S.order[j]; S.order[j] = t; }
        S.pos = 0;
    }
    S.idx = S.order[S.pos++];
    S.flip = rnd(2);
    S.target = 10 + rnd(81);
    S.guess = 50;
    S.seen = S.moved = false;
}

static void sinReset(void) {
    memset(&S, 0, sizeof S);
    for (int i = 0; i < N_PAIRS; i++) S.order[i] = i;
    S.pos = N_PAIRS;
    S.sw[0] = S.sw[1] = 3;
    S.team = rnd(2);
    S.st = SI_INTRO;
}

static void sinFrame(void) {
    bool showT = false;
    // ---- pantalla de arriba
    sceneTop();
    if (S.st == SI_INTRO || S.st == SI_END) {
        fit("Sintonía", 200, 52, 330, 1.9f, COL(INK));
        if (S.st == SI_INTRO) {
            dial(200, 205, 125, 0.5f + 0.4f * sinf(fc * 0.04f), 0.62f, true);
        } else {
            char b[48];
            snprintf(b, sizeof b, "¡Gana %s!", TEAMN[S.team]);
            fit(b, 200, 130, 340, 1.4f, teamCol(S.team));
            snprintf(b, sizeof b, "Rojo %d  -  Azul %d", S.sc[0], S.sc[1]);
            fit(b, 200, 190, 340, 0.8f, COL(INK));
        }
    } else {
        scoreBar(S.sc[0], S.sc[1], S.team, "Primero a 10");
        showT = (S.st == SI_PEEK && theld && inside(10, 70, 300, 56)) || S.st == SI_REVEAL;
        dial(200, 214, 150, S.guess / 100.0f, showT ? S.target / 100.0f : -1, S.st != SI_PEEK && S.st != SI_PASS);
        card(6, 42, 118, 30, COL(WHITE), 0);
        fit(PAIRS[S.idx][S.flip], 65, 57, 106, 0.5f, COL(INK));
        card(276, 42, 118, 30, COL(WHITE), 0);
        fit(PAIRS[S.idx][!S.flip], 335, 57, 106, 0.5f, COL(INK));
        if (S.st == SI_PEEK && showT) fit("Objetivo", 200, 175, 150, 0.5f, COL(INK));
    }
    // ---- pantalla de abajo (tactil)
    sceneBot();
    char b[96];
    switch (S.st) {
    case SI_INTRO:
        para("Un equipo da una pista y ese mismo equipo mueve la aguja hasta donde cree que está el objetivo.\n\nGana el primero en llegar a 10 puntos. Cada equipo puede cambiar la escala 3 veces en toda la partida.\n\nCentro 4 · medio 3 · borde 2", 14, 10, 292, 0.5f, COL(INK));
        if (btn(40, 160, 240, 46, "Empezar a jugar", COL(RED), COL(WHITE), true)) { sinReset(); S.st = SI_PASS; sinNew(); }
        if (menuBtn(6, 210)) scr = SC_MENU;
        break;
    case SI_PASS:
        card(10, 10, 300, 44, teamCol(S.team), 3);
        snprintf(b, sizeof b, "Turno del equipo %s", TEAMN[S.team]);
        fit(b, 160, 32, 280, 0.65f, COL(WHITE));
        para("Un jugador de este equipo agarra la consola para dar la pista.\nLos demás miran para otro lado.", 14, 70, 292, 0.5f, COL(INK));
        if (btn(30, 150, 260, 48, "Yo doy la pista >", COL(YEL), COL(INK), true)) S.st = SI_PEEK;
        if (menuBtn(6, 210)) scr = SC_MENU;
        break;
    case SI_PEEK: {
        para("Mantené apretado el botón de abajo: arriba aparece la zona del objetivo. Después pensá una pista y decila en voz alta.", 12, 8, 296, 0.46f, COL(INK));
        // el botón grande ocupa (10,70,300,56): cuando se mantiene, se ve el objetivo arriba
        if (holdbtn(10, 70, 300, 56, "MANTENÉ APRETADO PARA VER", COL(YEL), COL(INK))) S.seen = true;
        snprintf(b, sizeof b, "Cambiar escala (%d)", S.sw[S.team]);
        if (btn(10, 138, 300, 34, b, COL(WHITE), COL(INK), S.sw[S.team] > 0)) { S.sw[S.team]--; sinNew(); }
        if (btn(10, 178, 300, 34, "Ya dije la pista > seguir", COL(RED), COL(WHITE), S.seen)) S.st = SI_GUESS;
        if (menuBtn(6, 214)) scr = SC_MENU;
        break;
    }
    case SI_GUESS: {
        // tocar o arrastrar sobre el semicírculo mueve la aguja
        const float cx = 160, cy = 232, r = 150;
        fan(cx, cy, r + 4, 0, 1, COL(INK));
        fan(cx, cy, r, 0, 1, COL(WHITE));
        for (int i = 0; i <= 10; i++) {
            float a = PI_F * (1 - i / 10.0f);
            C2D_DrawLine(cx + (r - 14) * cosf(a), cy - (r - 14) * sinf(a), COL(GREY), cx + r * cosf(a), cy - r * sinf(a), COL(GREY), 2, Z);
        }
        float a = PI_F * (1 - S.guess / 100.0f);
        C2D_DrawLine(cx, cy, COL(INK), cx + r * 0.92f * cosf(a), cy - r * 0.92f * sinf(a), COL(INK), 6, Z);
        C2D_DrawCircleSolid(cx, cy, Z, 12, COL(INK));
        if (theld && ty > 60 && !inside(6, 210, 80, 30)) {
            float ang = atan2f(cy - ty, tx - cx);
            if (ang < 0) ang = tx < cx ? PI_F : 0;
            int g = (int)(100.0f * (1 - ang / PI_F) + 0.5f);
            S.guess = g < 0 ? 0 : g > 100 ? 100 : g;
            S.moved = true;
        }
        if (held & KEY_DLEFT && fc % 2 == 0) { if (S.guess > 0) S.guess--; S.moved = true; }
        if (held & KEY_DRIGHT && fc % 2 == 0) { if (S.guess < 100) S.guess++; S.moved = true; }
        snprintf(b, sizeof b, "Equipo %s: arrastren la aguja", TEAMN[S.team]);
        fit(b, 160, 18, 300, 0.5f, teamCol(S.team));
        if (btn(10, 30, 300, 28, "FIJAR RESPUESTA", COL(RED), COL(WHITE), S.moved)) {
            int d = abs(S.guess - S.target);
            S.pts = d <= 4 ? 4 : d <= 10 ? 3 : d <= 16 ? 2 : 0;
            S.sc[S.team] += S.pts;
            S.st = SI_REVEAL;
        }
        if (menuBtn(6, 210)) scr = SC_MENU;
        break;
    }
    case SI_REVEAL: {
        if (S.pts) { snprintf(b, sizeof b, "+%d puntos", S.pts); } else snprintf(b, sizeof b, "¡Casi!");
        fit(b, 160, 56, 300, 1.3f, S.pts ? COL(GREEN) : COL(INK));
        fit(S.pts == 4 ? "¡Justo en el centro!" : S.pts ? "Muy bien." : "La aguja quedó lejos del objetivo.", 160, 104, 300, 0.5f, COL(INK));
        bool win = S.sc[S.team] >= 10;
        snprintf(b, sizeof b, win ? "Ver resultado final" : "Siguiente: equipo %s >", TEAMN[!S.team]);
        if (btn(20, 140, 280, 48, b, win ? COL(YEL) : COL(BLUE), win ? COL(INK) : COL(WHITE), true)) {
            if (win) S.st = SI_END;
            else { S.team = !S.team; sinNew(); S.st = SI_PASS; }
        }
        break;
    }
    case SI_END:
        if (btn(30, 40, 260, 50, "Jugar de nuevo", COL(RED), COL(WHITE), true)) sinReset();
        if (btn(30, 110, 260, 50, "Volver al menú", COL(WHITE), COL(INK), true)) scr = SC_MENU;
        break;
    }
}

/* ===================================================================== GWW ===================================================================== */

typedef struct { const char *w; int c; bool rev; } Card;   // c: 0 rojo, 1 azul, 2 neutral, 3 asesino
enum { GW_MODE, GW_PLAY, GW_OVER };
static const char *MODEN[3] = {"Palabras", "Personajes", "Artistas"};
static const char *LANGN[3] = {"Español", "English", "Português"};
static const int SECS[5] = {0, 60, 90, 120, 180};
static const char *SECSN[5] = {"Sin reloj", "1:00", "1:30", "2:00", "3:00"};
static struct {
    int st, mode, lang, secsI, turn, start, left[2], sel, score[2], winner;
    Card b[25];
    u64 t0;
    char msg[96], why[64];
} G;

static void gwTurn(int t, const char *m) {
    G.turn = t;
    G.sel = -1;
    G.t0 = osGetTime();
    snprintf(G.msg, sizeof G.msg, "%s", m);
}

static void gwDeal(void) {
    static const char *pool[1024];
    int n = 0;
    if (G.mode == 0) {
        const char **src = G.lang == 0 ? W_ES : G.lang == 1 ? W_EN : W_PT;
        int cnt = G.lang == 0 ? N_W_ES : G.lang == 1 ? N_W_EN : N_W_PT;
        for (int i = 0; i < cnt; i++) pool[n++] = src[i];
    } else if (G.mode == 1) {
        for (int i = 0; i < N_CHARS; i++) pool[n++] = CHARS[i];
    } else {
        // 12 artistas argentinos + 13 del mundo, como en la versión original
        static const char *ar[N_MUSIC_AR], *ww[N_MUSIC_WW];
        memcpy(ar, MUSIC_AR, sizeof ar);
        memcpy(ww, MUSIC_WW, sizeof ww);
        for (int i = N_MUSIC_AR - 1; i > 0; i--) { int j = rnd(i + 1); const char *t = ar[i]; ar[i] = ar[j]; ar[j] = t; }
        for (int i = N_MUSIC_WW - 1; i > 0; i--) { int j = rnd(i + 1); const char *t = ww[i]; ww[i] = ww[j]; ww[j] = t; }
        for (int i = 0; i < 12; i++) pool[n++] = ar[i];
        for (int i = 0; i < 13; i++) pool[n++] = ww[i];
    }
    int m = n < 25 ? n : n;
    if (G.mode != 2) {
        for (int i = m - 1; i > 0; i--) { int j = rnd(i + 1); const char *t = pool[i]; pool[i] = pool[j]; pool[j] = t; }
    }
    G.start = rnd(2);
    G.left[G.start] = 9;
    G.left[!G.start] = 8;
    int col[25], k = 0;
    for (int i = 0; i < 9; i++) col[k++] = G.start;
    for (int i = 0; i < 8; i++) col[k++] = !G.start;
    for (int i = 0; i < 7; i++) col[k++] = 2;
    col[k++] = 3;
    for (int i = 24; i > 0; i--) { int j = rnd(i + 1), t = col[i]; col[i] = col[j]; col[j] = t; }
    int order[25];
    for (int i = 0; i < 25; i++) order[i] = i;
    if (G.mode == 2) for (int i = 24; i > 0; i--) { int j = rnd(i + 1), t = order[i]; order[i] = order[j]; order[j] = t; }
    for (int i = 0; i < 25; i++) { G.b[i].w = pool[order[i]]; G.b[i].c = col[i]; G.b[i].rev = false; }
    G.st = GW_PLAY;
    gwTurn(G.start, "¡Empieza la ronda!");
}

static void gwReveal(int i) {
    Card *c = &G.b[i];
    c->rev = true;
    G.sel = -1;
    if (c->c == 3) {
        G.winner = !G.turn;
        snprintf(G.why, sizeof G.why, "Equipo %s dio vuelta al asesino", TEAMN[G.turn]);
        G.score[G.winner]++;
        G.st = GW_OVER;
        return;
    }
    if (c->c < 2) G.left[c->c]--;
    if (G.left[0] == 0 || G.left[1] == 0) {
        G.winner = G.left[0] == 0 ? 0 : 1;
        snprintf(G.why, sizeof G.why, "Encontraron todas sus palabras");
        G.score[G.winner]++;
        G.st = GW_OVER;
        return;
    }
    char m[96];
    if (c->c == G.turn) { snprintf(m, sizeof m, "¡Correcto! Pueden seguir."); snprintf(G.msg, sizeof G.msg, "%s", m); }
    else if (c->c == 2) gwTurn(!G.turn, "Era neutral: termina el turno");
    else { snprintf(m, sizeof m, "Era del equipo %s: termina el turno", TEAMN[c->c]); gwTurn(!G.turn, m); }
}

static void gwCell(int i, float *x, float *y) { *x = 7 + (i % 5) * 62; *y = 6 + (i / 5) * 37; }

static void gwFrame(void) {
    bool key = false;
    char b[96];
    // ---------- ABAJO (tablero táctil)
    sceneBot();
    if (G.st == GW_MODE) {
        fit("Elegí cómo jugar", 160, 14, 300, 0.55f, COL(INK));
        for (int i = 0; i < 3; i++)
            if (btn(8 + i * 104, 30, 100, 34, MODEN[i], G.mode == i ? COL(RED) : COL(WHITE), G.mode == i ? COL(WHITE) : COL(INK), true)) G.mode = i;
        if (G.mode == 0) {
            for (int i = 0; i < 3; i++)
                if (btn(8 + i * 104, 70, 100, 30, LANGN[i], G.lang == i ? COL(BLUE) : COL(WHITE), G.lang == i ? COL(WHITE) : COL(INK), true)) G.lang = i;
        } else fit(G.mode == 1 ? "Películas, series y videojuegos" : "Bandas y solistas del mundo y Argentina", 160, 85, 300, 0.42f, COL(GREY));
        fit("Tiempo por turno", 160, 116, 300, 0.45f, COL(INK));
        for (int i = 0; i < 5; i++)
            if (btn(6 + i * 62, 130, 58, 28, SECSN[i], G.secsI == i ? COL(YEL) : COL(WHITE), COL(INK), true)) G.secsI = i;
        if (btn(30, 168, 260, 38, "Empezar", COL(RED), COL(WHITE), true)) gwDeal();
        if (menuBtn(6, 210)) scr = SC_MENU;
    } else {
        for (int i = 0; i < 25; i++) {
            float x, y;
            gwCell(i, &x, &y);
            Card *c = &G.b[i];
            u32 fill = COL(WHITE), fg = COL(INK);
            if (c->rev) {
                fill = c->c == 0 ? COL(RED) : c->c == 1 ? COL(BLUE) : c->c == 2 ? COL(NEUT) : COL(INK);
                fg = (c->c == 2) ? COL(INK) : COL(WHITE);
            }
            if (G.st == GW_OVER && !c->rev) {   // al terminar se revela toda la clave
                fill = c->c == 0 ? 0xffd6cf : c->c == 1 ? 0xd3dff5 : c->c == 2 ? COL(NEUT) : 0x888888;
                fill = c->c == 0 ? COL(0xe9b3aa) : c->c == 1 ? COL(0xb4c6ea) : c->c == 2 ? COL(NEUT) : COL(0x6a6a6a);
            }
            bool sel = (i == G.sel);
            if (sel) rrect(x - 3, y - 3, 62, 35, 7, COL(YEL));
            rrect(x, y, 56, 29, 5, COL(INK));
            rrect(x + 2, y + 2, 52, 25, 4, fill);
            fit(c->w, x + 28, y + 14, 48, 0.4f, fg);
            if (G.st == GW_PLAY && !c->rev && tdown && inside(x - 2, y - 2, 60, 33)) {
                if (G.sel == i) gwReveal(i);
                else G.sel = i;
            }
        }
        if (G.st == GW_PLAY) {
            key = holdbtn(6, 196, 100, 38, "Ver clave", COL(YEL), COL(INK));
            if (btn(110, 196, 112, 38, "Terminar turno", COL(WHITE), COL(INK), true)) gwTurn(!G.turn, "Se pasó el turno");
            if (menuBtn(228, 200)) scr = SC_MENU;
        } else {
            if (btn(6, 196, 150, 38, "Nueva ronda", COL(RED), COL(WHITE), true)) { G.st = GW_MODE; }
            if (btn(164, 196, 150, 38, "Menú", COL(WHITE), COL(INK), true)) scr = SC_MENU;
        }
    }
    // ---------- ARRIBA
    sceneTop();
    if (G.st == GW_MODE) {
        tiles(200, 40, "GWW", 3, 66, true);
        fit("Guess the Word with words", 200, 150, 380, 0.6f, COL(INK));
        snprintf(b, sizeof b, "Partidas ganadas   Rojo %d  -  Azul %d", G.score[0], G.score[1]);
        fit(b, 200, 190, 380, 0.5f, COL(GREY));
    } else if (key) {
        // la clave de los espías: cada palabra con el color de su equipo
        for (int i = 0; i < 25; i++) {
            float x = 10 + (i % 5) * 76, y = 8 + (i / 5) * 46;
            Card *c = &G.b[i];
            u32 f = c->c == 0 ? COL(RED) : c->c == 1 ? COL(BLUE) : c->c == 2 ? COL(NEUT) : COL(INK);
            rrect(x, y, 72, 42, 5, f);
            if (c->rev) { rrect(x + 3, y + 3, 66, 36, 3, COL(INK)); }
            fit(c->w, x + 36, y + 21, 64, 0.42f, c->c == 2 ? COL(INK) : COL(WHITE));
        }
    } else {
        int t = G.st == GW_OVER ? G.winner : G.turn;
        rrect(0, 0, 400, 62, 0, teamCol(t));
        if (G.st == GW_OVER) { snprintf(b, sizeof b, "¡GANA %s!", TEAMN[t]); }
        else { snprintf(b, sizeof b, "TURNO · %s", TEAMN[t]); }
        fit(b, 200, 31, 360, 1.0f, COL(WHITE));
        if (G.st == GW_PLAY && SECS[G.secsI] > 0) {
            long rem = SECS[G.secsI] - (long)((osGetTime() - G.t0) / 1000);
            if (rem <= 0) { gwTurn(!G.turn, "Se acabó el tiempo"); rem = SECS[G.secsI]; }
            snprintf(b, sizeof b, "%ld:%02ld", rem / 60, rem % 60);
            card(318, 70, 76, 34, rem <= 10 ? COL(RED) : COL(WHITE), 0);
            fit(b, 356, 87, 66, 0.7f, rem <= 10 ? COL(WHITE) : COL(INK));
        }
        for (int k = 0; k < 2; k++) {
            snprintf(b, sizeof b, "%s faltan %d", TEAMN[k], G.left[k]);
            card(8 + k * 160, 72, 150, 30, teamCol(k), 0);
            fit(b, 83 + k * 160, 87, 136, 0.5f, COL(WHITE));
        }
        if (G.st == GW_OVER) {
            fit(G.why, 200, 140, 380, 0.55f, COL(INK));
            snprintf(b, sizeof b, "Partidas ganadas   Rojo %d  -  Azul %d", G.score[0], G.score[1]);
            fit(b, 200, 190, 380, 0.5f, COL(GREY));
        } else if (G.sel >= 0) {
            card(60, 118, 280, 70, COL(YEL), 4);
            fit(G.b[G.sel].w, 200, 146, 250, 1.0f, COL(INK));
            fit("Tocá otra vez para dar vuelta la palabra", 200, 205, 380, 0.45f, COL(INK));
        } else {
            fit(G.msg, 200, 135, 380, 0.6f, COL(INK));
            fit("Tocá una palabra del tablero (2 veces para confirmar)", 200, 175, 380, 0.4f, COL(GREY));
            fit("Espías: mantengan «Ver clave» (los demás miren para otro lado)", 200, 200, 380, 0.4f, COL(GREY));
        }
    }
}

/* ===================================================================== MENU ===================================================================== */

static void menuFrame(void) {
    sceneTop();
    tiles(200, 34, "TKGA", 4, 62, true);
    fit("JUEGOS PARA JUGAR CON AMIGOS", 200, 150, 380, 0.5f, COL(INK));
    fit("Pasá la consola y jugá", 200, 178, 380, 0.45f, COL(GREY));
    sceneBot();
    // GWW
    bool p1 = theld && inside(12, 14, 296, 92);
    float o1 = p1 ? 4 : 0;
    card(12 + o1, 14 + o1, 296, 92, COL(RED), p1 ? 0 : 5);
    tiles(70 + o1, 30 + o1, "GWW", 3, 26, true);
    txt("GWW", 128 + o1, 26 + o1, 1.0f, COL(WHITE), 0);
    txt("Codenames: pistas y palabras", 128 + o1, 66 + o1, 0.4f, COL(WHITE), 0);
    // Sintonía
    bool p2 = theld && inside(12, 122, 296, 92);
    float o2 = p2 ? 4 : 0;
    card(12 + o2, 122 + o2, 296, 92, COL(BLUE), p2 ? 0 : 5);
    dial(62 + o2, 196 + o2, 42, 0.5f + 0.4f * sinf(fc * 0.05f), -1, true);
    txt("Sintonía", 120 + o2, 134 + o2, 0.95f, COL(WHITE), 0);
    txt("Acertá dónde cae la aguja", 120 + o2, 174 + o2, 0.4f, COL(WHITE), 0);
    fit("START: salir", 160, 226, 200, 0.38f, COL(GREY));
    if (tdown && inside(12, 14, 296, 92)) { scr = SC_GWW; G.st = GW_MODE; }
    if (tdown && inside(12, 122, 296, 92)) { scr = SC_SIN; sinReset(); }
}

int main(void) {
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    TOP = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    BOT = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    tb = C2D_TextBufNew(8192);
    srand((unsigned)time(NULL));
    memset(&G, 0, sizeof G);
    G.sel = -1;
    sinReset();

    while (!quitApp) {
        if (!beginFrame()) break;
        if (down & KEY_START) { endFrame(); break; }
        switch (scr) {
        case SC_MENU: menuFrame(); break;
        case SC_SIN: sinFrame(); break;
        case SC_GWW: gwFrame(); break;
        }
        endFrame();
    }
    C2D_TextBufDelete(tb);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
