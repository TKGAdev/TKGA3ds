// TKGA para Nintendo 3DS (homebrew .3dsx): Sintonia y GWW en modo "pasar la consola".
// Arriba: tablero. Abajo: instrucciones y estado.
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "data.h"

static PrintConsole top, bot;
static bool quit = false;

#define RESET "\x1b[0m"
#define RED "\x1b[31;1m"
#define BLUE "\x1b[34;1m"
#define YEL "\x1b[33;1m"
#define MAG "\x1b[35;1m"
#define WHT "\x1b[37;1m"
#define GRY "\x1b[30;1m"

static void at(int r, int c) { printf("\x1b[%d;%dH", r, c); }
static void cls(PrintConsole *p) { consoleSelect(p); printf("\x1b[2J"); }

// Un cuadro: muestra, espera vblank, lee botones. Devuelve false si hay que salir.
static bool frame(void) {
    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();
    if (!aptMainLoop()) { quit = true; return false; }
    hidScanInput();
    return true;
}

// Espera hasta que se apriete alguno de 'mask'. Devuelve la tecla apretada (0 si se sale).
static u32 waitFor(u32 mask) {
    while (frame()) {
        u32 d = hidKeysDown() & mask;
        if (d) return d;
    }
    return 0;
}

static int rnd(int n) { return rand() % n; }

/* ===================================================== SINTONIA ===================================================== */

#define BARW 50

static void bar(int guess, int target, bool showTarget, bool showGuess) {
    consoleSelect(&top);
    at(14, 1);
    for (int i = 0; i < BARW; i++) {
        int v = i * 100 / (BARW - 1);
        char ch = '-';
        const char *col = GRY;
        if (showTarget) {
            int d = abs(v - target);
            if (d <= 4) { ch = '#'; col = YEL; }
            else if (d <= 10) { ch = '+'; col = WHT; }
            else if (d <= 16) { ch = '.'; col = WHT; }
        }
        printf("%s%c" RESET, col, ch);
    }
    at(15, 1);
    for (int i = 0; i < BARW; i++) {
        int v = i * 100 / (BARW - 1), g = guess * (BARW - 1) / 100;
        putchar((showGuess && i == g) ? '^' : ' ');
        (void)v;
    }
}

static void header(int sc0, int sc1, int team) {
    consoleSelect(&top);
    printf("\x1b[2J");
    at(1, 1);
    printf(RED "ROJO %d" RESET "  -  " BLUE "AZUL %d" RESET "      (gana el primero en llegar a 10)", sc0, sc1);
    at(2, 1);
    printf("Turno del equipo: %s", team == 0 ? RED "ROJO" RESET : BLUE "AZUL" RESET);
}

static void sintonia(void) {
    int sc[2] = {0, 0}, sw[2] = {3, 3}, team = rnd(2);
    int order[NPAIRS], pos = NPAIRS;
    for (int i = 0; i < NPAIRS; i++) order[i] = i;

    cls(&top);
    cls(&bot);
    consoleSelect(&top);
    at(3, 1);
    printf(WHT "SINTONIA" RESET "\n\n Un equipo da una pista y ese mismo equipo\n mueve la aguja hasta donde cree que esta\n el objetivo.\n\n"
           " Cada equipo puede cambiar la escala 3 veces\n en toda la partida.\n\n Puntos: centro 4 / medio 3 / borde 2");
    consoleSelect(&bot);
    at(2, 1);
    printf("A: empezar     START: volver al menu");
    u32 k = waitFor(KEY_A | KEY_START);
    if (!k || (k & KEY_START)) return;

    while (!quit) {
        // --- nueva escala
        if (pos >= NPAIRS) {
            for (int i = NPAIRS - 1; i > 0; i--) { int j = rnd(i + 1), t = order[i]; order[i] = order[j]; order[j] = t; }
            pos = 0;
        }
        int idx = order[pos++];
        int flip = rnd(2);
        int target = 10 + rnd(81), guess = 50;

        // --- pasar la consola
        for (;;) {
            header(sc[0], sc[1], team);
            consoleSelect(&top);
            at(5, 1);
            printf("Un jugador de este equipo agarra la consola\npara dar la pista. Los demas miran para otro lado.");
            cls(&bot);
            at(2, 1);
            printf("A: yo doy la pista    START: salir");
            k = waitFor(KEY_A | KEY_START);
            if (!k || (k & KEY_START)) return;
            break;
        }

        // --- mirar el objetivo (mantener A) y pensar la pista
        bool seen = false;
        for (;;) {
            header(sc[0], sc[1], team);
            consoleSelect(&top);
            at(4, 1);
            printf("%s%-24s" RESET "<--->%24s", WHT, PAIRS[idx][flip], PAIRS[idx][!flip]);
            at(12, 1);
            printf("Mantene apretado A para ver el objetivo.");
            cls(&bot);
            at(2, 1);
            printf("MANTENER A: ver objetivo\n");
            printf("Y: cambiar escala (%d restantes)\n", sw[team]);
            printf("R: ya dije la pista, seguir\n");
            bar(guess, target, false, false);
            bool again = false;
            while (frame()) {
                bool held = hidKeysHeld() & KEY_A;
                static bool last = false;
                if (held != last) {
                    last = held;
                    if (held) seen = true;
                    bar(guess, target, held, false);
                    consoleSelect(&top);
                    at(10, 1);
                    printf(held ? YEL "Objetivo: # centro (4)  + medio (3)  . borde (2)" RESET : "                                                  ");
                }
                u32 d = hidKeysDown();
                if (d & KEY_START) return;
                if ((d & KEY_Y) && sw[team] > 0) {
                    sw[team]--;
                    idx = order[pos < NPAIRS ? pos++ : 0];
                    target = 10 + rnd(81);
                    flip = rnd(2);
                    again = true;
                    break;
                }
                if ((d & KEY_R) && seen) break;
            }
            if (quit) return;
            if (!again) break;
        }

        // --- adivinar: izquierda/derecha mueve la aguja
        header(sc[0], sc[1], team);
        consoleSelect(&top);
        at(4, 1);
        printf("%s%-24s" RESET "<--->%24s", WHT, PAIRS[idx][flip], PAIRS[idx][!flip]);
        at(12, 1);
        printf("Equipo %s: pongan la aguja donde creen que cae.", team == 0 ? RED "ROJO" RESET : BLUE "AZUL" RESET);
        cls(&bot);
        at(2, 1);
        printf("IZQ/DER: mover la aguja (L/R: rapido)\nA: fijar respuesta");
        int fr = 0;
        bar(guess, target, false, true);
        while (frame()) {
            u32 d = hidKeysDown(), h = hidKeysHeld();
            int old = guess;
            if (d & KEY_START) return;
            if (d & KEY_A) break;
            if ((d & KEY_DLEFT) || ((h & KEY_DLEFT) && ++fr % 3 == 0)) guess--;
            if ((d & KEY_DRIGHT) || ((h & KEY_DRIGHT) && ++fr % 3 == 0)) guess++;
            if (h & KEY_L) guess -= 2;
            if (h & KEY_R) guess += 2;
            if (guess < 0) guess = 0;
            if (guess > 100) guess = 100;
            if (guess != old) bar(guess, target, false, true);
        }
        if (quit) return;

        // --- resultado
        int dist = abs(guess - target);
        int pts = dist <= 4 ? 4 : dist <= 10 ? 3 : dist <= 16 ? 2 : 0;
        sc[team] += pts;
        header(sc[0], sc[1], team);
        bar(guess, target, true, true);
        consoleSelect(&top);
        at(4, 1);
        printf("%s%-24s" RESET "<--->%24s", WHT, PAIRS[idx][flip], PAIRS[idx][!flip]);
        at(10, 1);
        if (pts) printf(YEL "+%d puntos%s" RESET, pts, pts == 4 ? "  - Justo en el centro!" : "");
        else printf("Casi! La aguja quedo lejos del objetivo.");
        at(1, 1);
        printf(RED "ROJO %d" RESET "  -  " BLUE "AZUL %d" RESET "      ", sc[0], sc[1]);
        cls(&bot);
        at(2, 1);
        if (sc[team] >= 10) {
            printf("GANA el equipo %s!\n\nA: volver al menu", team == 0 ? "ROJO" : "AZUL");
            waitFor(KEY_A | KEY_START);
            return;
        }
        printf("A: siguiente equipo   START: salir");
        k = waitFor(KEY_A | KEY_START);
        if (!k || (k & KEY_START)) return;
        team = !team;
    }
}

/* ======================================================= GWW ======================================================== */

typedef struct { const char *w; int c; bool rev; } Card;   // c: 0 rojo, 1 azul, 2 neutral, 3 asesino

static void gwwDraw(Card *b, int cur, int turn, bool spy, int left[2]) {
    consoleSelect(&top);
    printf("\x1b[2J");
    at(1, 1);
    printf(RED "ROJO faltan %d" RESET "   " BLUE "AZUL faltan %d" RESET "   Turno: %s", left[0], left[1], turn == 0 ? RED "ROJO" RESET : BLUE "AZUL" RESET);
    for (int i = 0; i < 25; i++) {
        int r = 4 + (i / 5) * 5, c = 1 + (i % 5) * 10;
        Card *k = &b[i];
        const char *col = WHT;
        if (k->rev) {
            col = k->c == 0 ? "\x1b[41;37;1m" : k->c == 1 ? "\x1b[44;37;1m" : k->c == 2 ? "\x1b[43;30m" : "\x1b[45;37;1m";
        } else if (spy) {
            col = k->c == 0 ? RED : k->c == 1 ? BLUE : k->c == 2 ? YEL : MAG;
        }
        at(r, c);
        printf("%s%c%-9s" RESET, col, i == cur ? '>' : ' ', k->w);
    }
}

static void gww(void) {
    Card b[25];
    int pick[NWORDS];
    for (int i = 0; i < NWORDS; i++) pick[i] = i;
    for (int i = NWORDS - 1; i > 0; i--) { int j = rnd(i + 1), t = pick[i]; pick[i] = pick[j]; pick[j] = t; }
    int start = rnd(2), left[2];
    left[start] = 9;
    left[!start] = 8;
    int col[25], n = 0;
    for (int i = 0; i < 9; i++) col[n++] = start;
    for (int i = 0; i < 8; i++) col[n++] = !start;
    for (int i = 0; i < 7; i++) col[n++] = 2;
    col[n++] = 3;
    for (int i = 24; i > 0; i--) { int j = rnd(i + 1), t = col[i]; col[i] = col[j]; col[j] = t; }
    for (int i = 0; i < 25; i++) { b[i].w = WORDS[pick[i]]; b[i].c = col[i]; b[i].rev = false; }

    int cur = 12, turn = start;
    bool spyHeld = false;
    cls(&bot);
    consoleSelect(&bot);
    at(2, 1);
    printf(WHT "GWW" RESET "  (como Codenames)\n\n Empieza el equipo %s (9 palabras).\n\n"
           " Los ESPIAS dan pistas de una palabra + numero.\n Los demas eligen palabras.\n\n"
           " Espias: mantener L = ver la clave\n (que los demas miren para otro lado)\n\n A: empezar    START: menu",
           start == 0 ? "ROJO" : "AZUL");
    u32 k = waitFor(KEY_A | KEY_START);
    if (!k || (k & KEY_START)) return;

    cls(&bot);
    consoleSelect(&bot);
    at(2, 1);
    printf("D-PAD: mover cursor\nA: dar vuelta la palabra\nB: terminar turno\nMANTENER L: clave (solo espias)\nSTART: salir al menu");
    gwwDraw(b, cur, turn, false, left);

    while (frame()) {
        u32 d = hidKeysDown();
        bool spy = hidKeysHeld() & KEY_L;
        bool redraw = false;
        if (d & KEY_START) return;
        if (d & KEY_DLEFT) { cur = cur % 5 ? cur - 1 : cur + 4; redraw = true; }
        if (d & KEY_DRIGHT) { cur = cur % 5 < 4 ? cur + 1 : cur - 4; redraw = true; }
        if (d & KEY_DUP) { cur = cur >= 5 ? cur - 5 : cur + 20; redraw = true; }
        if (d & KEY_DDOWN) { cur = cur < 20 ? cur + 5 : cur - 20; redraw = true; }
        if (spy != spyHeld) { spyHeld = spy; redraw = true; }
        if (d & KEY_B) { turn = !turn; redraw = true; }
        if ((d & KEY_A) && !b[cur].rev) {
            b[cur].rev = true;
            int c = b[cur].c;
            if (c == 3) {
                gwwDraw(b, cur, turn, true, left);
                consoleSelect(&bot);
                cls(&bot);
                at(2, 1);
                printf("ERA EL ASESINO!\nGana el equipo %s.\n\nA: volver al menu", turn == 0 ? "AZUL" : "ROJO");
                waitFor(KEY_A | KEY_START);
                return;
            }
            if (c < 2) left[c]--;
            if (left[0] == 0 || left[1] == 0) {
                int w = left[0] == 0 ? 0 : 1;
                gwwDraw(b, cur, turn, true, left);
                cls(&bot);
                at(2, 1);
                printf("Gana el equipo %s!\n\nA: volver al menu", w == 0 ? "ROJO" : "AZUL");
                waitFor(KEY_A | KEY_START);
                return;
            }
            if (c != turn) turn = !turn;   // palabra neutral o del otro equipo: termina el turno
            redraw = true;
        }
        if (redraw) {
            gwwDraw(b, cur, turn, spyHeld, left);
            consoleSelect(&bot);
        }
    }
}

/* ======================================================= MENU ======================================================= */

int main(void) {
    gfxInitDefault();
    consoleInit(GFX_TOP, &top);
    consoleInit(GFX_BOTTOM, &bot);
    srand((unsigned)time(NULL));

    int sel = 0;
    const char *items[] = {"Sintonia", "GWW (Codenames)"};
    while (!quit) {
        cls(&top);
        cls(&bot);
        consoleSelect(&top);
        at(3, 1);
        printf(WHT "  T K G A" RESET "\n\n");
        for (int i = 0; i < 2; i++) printf("  %s %s\n\n", i == sel ? ">" : " ", items[i]);
        consoleSelect(&bot);
        at(2, 1);
        printf("ARRIBA/ABAJO: elegir\nA: jugar\nSTART: salir a Home\n\nModo: pasar la consola (sin online).");
        for (;;) {
            u32 k = waitFor(KEY_DUP | KEY_DDOWN | KEY_A | KEY_START);
            if (!k) return gfxExit(), 0;
            if (k & KEY_START) { gfxExit(); return 0; }
            if (k & KEY_DUP) sel = 0;
            if (k & KEY_DDOWN) sel = 1;
            if (k & KEY_A) { if (sel == 0) sintonia(); else gww(); break; }
            consoleSelect(&top);
            at(5, 1);
            for (int i = 0; i < 2; i++) printf("  %s %s       \n\n", i == sel ? ">" : " ", items[i]);
        }
    }
    gfxExit();
    return 0;
}
