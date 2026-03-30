/*
 * ============================================================
 *  CORRIDOR  —  Terminal FPS Raycaster
 *  Pure C++17, zero external libraries.
 *
 *  Windows : compile with MSVC or MinGW
 *    cl /O2 /std:c++17 fps.cpp
 *    g++ -O2 -std=c++17 fps.cpp -o fps
 *
 *  Linux/macOS : uses POSIX termios + ANSI escape codes
 *    g++ -O2 -std=c++17 fps.cpp -o fps && ./fps
 *
 *  Controls:
 *    W / S        – move forward / back
 *    A / D        – rotate left / right
 *    Q / E        – strafe left / right
 *    SPACE        – shoot
 *    ESC          – quit
 * ============================================================
 */

#include <iostream>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <algorithm>
#include <sstream>

/* ─── PLATFORM LAYER ─────────────────────────────────────── */

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #define NOMINMAX
  #include <windows.h>
  #include <conio.h>

  static HANDLE hStdOut;
  static HANDLE hStdIn;

  void platform_init() {
      hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
      hStdIn  = GetStdHandle(STD_INPUT_HANDLE);
      DWORD mode;
      GetConsoleMode(hStdOut, &mode);
      // Try to enable virtual terminal processing (Win10+)
      SetConsoleMode(hStdOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
      // Hide cursor
      CONSOLE_CURSOR_INFO ci = {1, FALSE};
      SetConsoleCursorInfo(hStdOut, &ci);
      // Set console title
      SetConsoleTitleA("CORRIDOR - Terminal FPS");
      // Disable echo / line input
      GetConsoleMode(hStdIn, &mode);
      SetConsoleMode(hStdIn, mode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));
  }

  void platform_restore() {
      CONSOLE_CURSOR_INFO ci = {1, TRUE};
      SetConsoleCursorInfo(hStdOut, &ci);
  }

  // Non-blocking key check: returns char or 0
  char poll_key() {
      if (_kbhit()) return (char)_getch();
      return 0;
  }

  void cursor_home() {
      COORD c = {0, 0};
      SetConsoleCursorPosition(hStdOut, c);
  }

  void get_console_size(int &w, int &h) {
      CONSOLE_SCREEN_BUFFER_INFO csbi;
      GetConsoleScreenBufferInfo(hStdOut, &csbi);
      w = csbi.srWindow.Right  - csbi.srWindow.Left + 1;
      h = csbi.srWindow.Bottom - csbi.srWindow.Top  + 1;
  }

#else
  /* POSIX — Linux / macOS */
  #include <termios.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <sys/ioctl.h>
  #include <signal.h>

  static struct termios orig_termios;

  void platform_init() {
      tcgetattr(STDIN_FILENO, &orig_termios);
      struct termios raw = orig_termios;
      raw.c_lflag &= ~(ECHO | ICANON);
      raw.c_cc[VMIN]  = 0;
      raw.c_cc[VTIME] = 0;
      tcsetattr(STDIN_FILENO, TCSANOW, &raw);
      // Hide cursor
      printf("\033[?25l");
      // Alternate screen buffer
      printf("\033[?1049h");
      fflush(stdout);
  }

  void platform_restore() {
      tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
      printf("\033[?25h");       // show cursor
      printf("\033[?1049l");     // restore screen
      fflush(stdout);
  }

  char poll_key() {
      char c = 0;
      if (read(STDIN_FILENO, &c, 1) == 1) return c;
      return 0;
  }

  void cursor_home() {
      printf("\033[H");
  }

  void get_console_size(int &w, int &h) {
      struct winsize ws;
      ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
      w = ws.ws_col;
      h = ws.ws_row;
  }
#endif

/* ─── ANSI COLOR HELPERS ─────────────────────────────────── */

// 256-color foreground
static void ansi_fg(int code) {
    printf("\033[38;5;%dm", code);
}
// 256-color background
static void ansi_bg(int code) {
    printf("\033[48;5;%dm", code);
}
static void ansi_reset() {
    printf("\033[0m");
}

/* ─── SCREEN BUFFER ──────────────────────────────────────── */

struct Cell {
    char  ch;
    int   fg;   // ANSI 256 color
    int   bg;
};

static int SCR_W = 120;
static int SCR_H = 35;

static std::vector<Cell> framebuf;
static std::vector<Cell> prevbuf;

void buf_init(int w, int h) {
    SCR_W = w;
    SCR_H = h;
    framebuf.assign(w * h, {' ', 7, 0});
    prevbuf .assign(w * h, {0,  -1, -1});  // force full redraw first frame
}

inline Cell& buf(int x, int y) {
    return framebuf[y * SCR_W + x];
}

void buf_clear() {
    for (auto &c : framebuf) { c.ch = ' '; c.fg = 7; c.bg = 0; }
}

// Flush only changed cells (delta render)
void buf_flush() {
    cursor_home();
    int cur_fg = -1, cur_bg = -1;
    std::string out;
    out.reserve(SCR_W * SCR_H * 6);
    // Move to (0,0)
    out += "\033[H";
    for (int y = 0; y < SCR_H - 1; y++) {
        for (int x = 0; x < SCR_W; x++) {
            const Cell &c = framebuf[y * SCR_W + x];
            if (c.fg != cur_fg) {
                char tmp[16];
                snprintf(tmp, sizeof(tmp), "\033[38;5;%dm", c.fg);
                out += tmp;
                cur_fg = c.fg;
            }
            if (c.bg != cur_bg) {
                char tmp[16];
                snprintf(tmp, sizeof(tmp), "\033[48;5;%dm", c.bg);
                out += tmp;
                cur_bg = c.bg;
            }
            out += c.ch;
        }
        if (y < SCR_H - 2) out += "\r\n";
    }
    out += "\033[0m";
    fwrite(out.data(), 1, out.size(), stdout);
    fflush(stdout);
}

/* ─── MAP ────────────────────────────────────────────────── */

static const int MAP_W = 20;
static const int MAP_H = 20;

// 0=open, 1=stone, 2=brick, 3=metal, 4=pillar
static const char MAP[MAP_H][MAP_W+1] = {
    "11111111111111111111",
    "10000000010000000001",
    "10000000010000000001",
    "10002000000000200001",
    "10000001110000000001",
    "10000000000000000001",
    "11100000000000000001",
    "10000002000200000001",
    "10000000000000000001",
    "10000111100000000001",
    "10000000000003000001",
    "10000000000000000001",
    "10020000000000000001",
    "10000001000000000001",
    "10000001000002000001",
    "10000000000000000001",
    "10000000011100000001",
    "10002000000000000001",
    "10000000000000000001",
    "11111111111111111111",
};

inline int map_at(int mx, int my) {
    if (mx < 0 || mx >= MAP_W || my < 0 || my >= MAP_H) return 1;
    return MAP[my][mx] - '0';
}

/* ─── PLAYER ─────────────────────────────────────────────── */

struct Player {
    double x     = 3.5;
    double y     = 3.5;
    double angle = 0.0;
    double fov   = M_PI / 3.0;   // 60°
    double speed = 4.0;
    double rot   = 2.0;
    int    hp    = 100;
    int    kills = 0;
    double shoot_timer = 0.0;
    bool   shooting    = false;
};

/* ─── ENEMIES ────────────────────────────────────────────── */

struct Enemy {
    double x, y;
    int    hp;
    bool   alive;
    bool   alert;
    double speed;
    double shoot_cd;   // time until next shot
};

static std::vector<Enemy> enemies = {
    {5.5,  5.5,  3, true, false, 1.2, 0.0},
    {12.5, 4.5,  3, true, false, 1.0, 0.0},
    {8.5,  14.5, 3, true, false, 1.3, 0.0},
    {16.5, 10.5, 3, true, false, 0.9, 0.0},
    {3.5,  15.5, 3, true, false, 1.1, 0.0},
    {15.5, 17.5, 3, true, false, 1.0, 0.0},
};

/* ─── ZBUFFER ────────────────────────────────────────────── */
static std::vector<double> zbuf;

/* ─── WALL SHADE CHARACTERS ──────────────────────────────── */
// Ordered light → dark
static const char* SHADE  = " .:-=+*#%@";
static const int   NSHADE = 10;

// Shade chars for walls — full block variations using unicode block elements
// We'll just use ASCII block chars for maximum compatibility
static const char WALL_CHARS[] = { (char)0xDB, (char)0xB2, (char)0xB1, (char)0xB0, ' ' };
//  On Windows these map to  █ ▓ ▒ ░
//  On Linux we use ANSI bg color blocks instead (space with bg color)

/* ─── COLOR PALETTE ──────────────────────────────────────── */
// ANSI 256-color indices

// Wall colors by type (light face, dark face)
struct WallColor { int light, dark; };
static WallColor WALL_COL[] = {
    {0,  0},   // 0 – unused
    {88, 52},  // 1 – stone  (dark red)
    {94, 58},  // 2 – brick  (brown)
    {59, 23},  // 3 – metal  (teal-grey)
    {54, 18},  // 4 – pillar (purple)
};

// Ceiling gradient (top → horizon): very dark grey shades
static int CEIL_COLS[] = { 232, 233, 234, 235, 236 };
// Floor gradient (horizon → bottom): dark earthy
static int FLOOR_COLS[] = { 236, 235, 234, 233, 232 };

/* ─── RAYCASTER ──────────────────────────────────────────── */

struct RayHit {
    double dist;
    int    side;     // 0=NS wall, 1=EW wall
    int    wall_type;
};

RayHit cast_ray(double px, double py, double angle) {
    double cos_a = cos(angle), sin_a = sin(angle);
    int    mx = (int)px, my = (int)py;
    double delta_x = fabs(1.0 / (cos_a + 1e-12));
    double delta_y = fabs(1.0 / (sin_a + 1e-12));
    int    step_x, step_y;
    double side_x, side_y;

    if (cos_a < 0) { step_x = -1; side_x = (px - mx) * delta_x; }
    else           { step_x =  1; side_x = (mx + 1.0 - px) * delta_x; }
    if (sin_a < 0) { step_y = -1; side_y = (py - my) * delta_y; }
    else           { step_y =  1; side_y = (my + 1.0 - py) * delta_y; }

    int   side = 0;
    double dist = 0.0;
    int   wall  = 0;

    for (int i = 0; i < 64; i++) {
        if (side_x < side_y) { side_x += delta_x; mx += step_x; side = 0; }
        else                  { side_y += delta_y; my += step_y; side = 1; }
        wall = map_at(mx, my);
        if (wall > 0) {
            dist = (side == 0)
                ? (mx - px + (1 - step_x) / 2.0) / cos_a
                : (my - py + (1 - step_y) / 2.0) / sin_a;
            break;
        }
    }
    if (dist < 0.001) dist = 0.001;
    return { dist, side, wall };
}

/* ─── RENDER WALLS ───────────────────────────────────────── */

void render_walls(const Player &p) {
    int cols = SCR_W;
    int rows = SCR_H - 3;  // reserve bottom 3 rows for HUD

    zbuf.assign(cols, 1e9);

    for (int x = 0; x < cols; x++) {
        double ray_angle = p.angle - p.fov / 2.0 + ((double)x / cols) * p.fov;
        RayHit hit = cast_ray(p.x, p.y, ray_angle);
        zbuf[x] = hit.dist;

        // Wall height
        int wall_h = (int)(rows / hit.dist);
        if (wall_h > rows * 3) wall_h = rows * 3;

        int top    = (rows / 2) - (wall_h / 2);
        int bottom = top + wall_h;

        // ── ceiling above wall ──
        for (int y = 0; y < std::min(top, rows); y++) {
            // gradient based on distance from top
            int seg = std::min((int)(((double)y / rows) * 5), 4);
            buf(x, y) = { ' ', 0, CEIL_COLS[seg] };
        }

        // ── wall strip ──
        WallColor wc = WALL_COL[std::min(hit.wall_type, 4)];
        int col = hit.side == 1 ? wc.dark : wc.light;
        // Distance darkening
        double fog = std::min(hit.dist / 12.0, 1.0);
        // Pick wall character based on distance (more distant = lighter char)
        char wch;
        if (fog < 0.15)      wch = '#';
        else if (fog < 0.30) wch = '+';
        else if (fog < 0.50) wch = '=';
        else if (fog < 0.70) wch = '-';
        else                 wch = '.';

        // Darken color by fog
        if (fog > 0.6) col = 236;
        else if (fog > 0.4) col = std::max(col - 2, 232);

        for (int y = std::max(top, 0); y < std::min(bottom, rows); y++) {
            buf(x, y) = { wch, col, col };
        }

        // ── floor below wall ──
        for (int y = std::max(bottom, 0); y < rows; y++) {
            int seg = std::min((int)(((double)(y - rows/2) / (rows/2)) * 5), 4);
            buf(x, y) = { ' ', 0, FLOOR_COLS[seg] };
        }
    }
}

/* ─── RENDER ENEMIES (SPRITES) ───────────────────────────── */

void render_sprites(const Player &p) {
    int cols = SCR_W;
    int rows = SCR_H - 3;

    // Sort back-to-front
    std::vector<std::pair<double,int>> order;
    for (int i = 0; i < (int)enemies.size(); i++) {
        if (!enemies[i].alive) continue;
        double dx = enemies[i].x - p.x;
        double dy = enemies[i].y - p.y;
        order.push_back({ dx*dx + dy*dy, i });
    }
    std::sort(order.begin(), order.end(),
              [](auto &a, auto &b){ return a.first > b.first; });

    for (auto [dist2, idx] : order) {
        const Enemy &e = enemies[idx];
        double dx = e.x - p.x;
        double dy = e.y - p.y;
        double dist = sqrt(dist2);
        if (dist < 0.3) continue;

        // Angle to enemy relative to player view
        double ea = atan2(dy, dx) - p.angle;
        // Normalise
        while (ea >  M_PI) ea -= 2*M_PI;
        while (ea < -M_PI) ea += 2*M_PI;

        // Off-screen
        if (fabs(ea) > p.fov * 0.75) continue;

        // Project
        double proj_h = (double)rows / dist;
        int    spr_h  = std::max((int)proj_h, 1);
        int    spr_w  = spr_h / 2;  // terminal chars are ~2:1 aspect

        double screen_x = (ea / p.fov + 0.5) * cols;
        int    sx_start = (int)(screen_x - spr_w / 2);
        int    sy_start = (int)(rows / 2 - spr_h / 2);

        // Enemy color by hp
        int body_col = (e.hp >= 3) ? 196 : (e.hp == 2) ? 202 : 240;
        int head_col = (e.hp >= 3) ? 160 : 124;

        for (int sx = sx_start; sx < sx_start + spr_w; sx++) {
            if (sx < 0 || sx >= cols) continue;
            if (dist >= zbuf[sx]) continue;  // behind wall

            double frac = (double)(sx - sx_start) / spr_w;

            for (int sy = sy_start; sy < sy_start + spr_h; sy++) {
                if (sy < 0 || sy >= rows) continue;
                double fy = (double)(sy - sy_start) / spr_h;

                // Simple humanoid shape
                char ch = ' ';
                int  fg = body_col, bg = body_col;

                if (fy < 0.22) {
                    // head - circle shape
                    double cx = frac - 0.5, cy = fy / 0.22 - 0.5;
                    if (cx*cx + cy*cy < 0.25) {
                        ch = 'O'; fg = head_col; bg = head_col;
                    } else continue;
                } else if (fy < 0.65) {
                    // torso
                    if (frac > 0.15 && frac < 0.85) {
                        ch = (frac > 0.35 && frac < 0.65 && fy > 0.35 && fy < 0.55)
                            ? '+' : '#';
                        fg = body_col; bg = body_col;
                    } else continue;
                } else {
                    // legs
                    if ((frac < 0.35) || (frac > 0.65)) {
                        ch = '|'; fg = body_col - 8; bg = body_col - 8;
                    } else continue;
                }
                buf(sx, sy) = { ch, fg, bg };
            }
        }
    }
}

/* ─── RENDER GUN ─────────────────────────────────────────── */

void render_gun(const Player &p, double bob, bool flash) {
    int rows = SCR_H - 3;
    int cx   = SCR_W / 2;
    int gy   = rows - 9 + (int)(sin(bob * 8.0) * 2.0);

    // Muzzle flash
    if (flash) {
        for (int dx = -2; dx <= 2; dx++)
            for (int dy = -2; dy <= 0; dy++) {
                int px2 = cx + dx, py2 = gy - 7 + dy;
                if (px2 >= 0 && px2 < SCR_W && py2 >= 0 && py2 < rows)
                    buf(px2, py2) = { '*', 226, 226 };
            }
    }

    // Gun ASCII art
    // barrel
    static const char* gun[] = {
        "   |   ",
        "   |   ",
        " __|__ ",
        "|     |",
        "|_____|",
        "  |||  ",
    };
    int gun_rows = 6;
    for (int r = 0; r < gun_rows; r++) {
        int py2 = gy + r;
        if (py2 < 0 || py2 >= rows) continue;
        for (int c2 = 0; c2 < (int)strlen(gun[r]); c2++) {
            int px2 = cx - 3 + c2;
            if (px2 < 0 || px2 >= SCR_W) continue;
            char ch = gun[r][c2];
            if (ch != ' ')
                buf(px2, py2) = { ch, 250, 0 };
        }
    }
}

/* ─── HUD ────────────────────────────────────────────────── */

void render_hud(const Player &p, double fps) {
    int hy = SCR_H - 3;

    // HUD background bar
    for (int x = 0; x < SCR_W; x++) {
        buf(x, hy)   = { ' ', 7, 235 };
        buf(x, hy+1) = { ' ', 7, 234 };
    }

    // HP bar
    char tmp[128];
    snprintf(tmp, sizeof(tmp), " HP:[");
    int tx = 0;
    for (char *c2 = tmp; *c2; c2++, tx++)
        buf(tx, hy) = { *c2, 196, 235 };

    int hp_bars = (p.hp * 15) / 100;
    for (int i = 0; i < 15; i++) {
        char ch = (i < hp_bars) ? '#' : '-';
        int  col = (i < hp_bars)
            ? (p.hp > 50 ? 46 : p.hp > 25 ? 214 : 196)
            : 238;
        buf(tx++, hy) = { ch, col, 235 };
    }
    buf(tx++, hy) = { ']', 196, 235 };

    // HP number
    snprintf(tmp, sizeof(tmp), " %3d%%", p.hp);
    for (char *c2 = tmp; *c2; c2++, tx++)
        buf(tx, hy) = { *c2, 255, 235 };

    // Kills
    snprintf(tmp, sizeof(tmp), "  KILLS: %d", p.kills);
    tx = 35;
    for (char *c2 = tmp; *c2; c2++, tx++)
        buf(tx, hy) = { *c2, 46, 235 };

    // Position
    snprintf(tmp, sizeof(tmp), "  POS: (%.1f, %.1f)  ANG: %.0f",
             p.x, p.y, p.angle * 180.0 / M_PI);
    tx = 55;
    for (char *c2 = tmp; *c2; c2++, tx++)
        buf(tx, hy) = { *c2, 244, 235 };

    // FPS
    snprintf(tmp, sizeof(tmp), "  FPS:%.0f ", fps);
    tx = SCR_W - 10;
    for (char *c2 = tmp; *c2; c2++, tx++)
        buf(tx, hy) = { *c2, 250, 235 };

    // Controls on second line
    const char *ctrl = " [W/S] Move  [A/D] Turn  [Q/E] Strafe  [SPACE] Shoot  [ESC] Quit";
    tx = 0;
    for (const char *c2 = ctrl; *c2 && tx < SCR_W; c2++, tx++)
        buf(tx, hy + 1) = { *c2, 243, 234 };

    // Crosshair
    int mid_x = SCR_W / 2;
    int mid_y = (SCR_H - 3) / 2;
    buf(mid_x,     mid_y) = { '+', 255, 0 };
    buf(mid_x - 1, mid_y) = { '-', 255, 0 };
    buf(mid_x + 1, mid_y) = { '-', 255, 0 };
}

/* ─── COLLISION ──────────────────────────────────────────── */

bool can_move(double nx, double ny) {
    double margin = 0.3;
    return map_at((int)(nx + margin), (int)(ny + margin)) == 0 &&
           map_at((int)(nx - margin), (int)(ny + margin)) == 0 &&
           map_at((int)(nx + margin), (int)(ny - margin)) == 0 &&
           map_at((int)(nx - margin), (int)(ny - margin)) == 0;
}

/* ─── SHOOT ──────────────────────────────────────────────── */

void do_shoot(Player &p) {
    if (p.shoot_timer > 0.0) return;
    p.shooting    = true;
    p.shoot_timer = 0.25;

    // Find nearest enemy in crosshair
    RayHit wall = cast_ray(p.x, p.y, p.angle);

    for (auto &e : enemies) {
        if (!e.alive) continue;
        double dx = e.x - p.x, dy = e.y - p.y;
        double dist = sqrt(dx*dx + dy*dy);
        if (dist > 15.0 || dist >= wall.dist + 0.5) continue;
        double ea = atan2(dy, dx) - p.angle;
        while (ea >  M_PI) ea -= 2*M_PI;
        while (ea < -M_PI) ea += 2*M_PI;
        if (fabs(ea) < 0.10) {
            e.hp--;
            e.alert = true;
            if (e.hp <= 0) { e.alive = false; p.kills++; }
            break;
        }
    }
}

/* ─── TITLE SCREEN ───────────────────────────────────────── */

void show_title() {
    // Clear screen
    printf("\033[2J\033[H");

    const char* title[] = {
        "",
        "",
        "   ######  #######  ######  ######  ######  ######  #####  ######",
        "  ##      ##     ## ##   ## ##   ## ##   ## ##   ## ##  ## ##   ##",
        "  ##      ##     ## ##   ## ##   ## ##   ## ##   ## ##  ## ##   ##",
        "  ##      ##     ## ######  ######  ###### ##   ## #####  ######",
        "   ######  #######  ##   ## ##   ## ## ##  ##   ## ##  ## ##   ##",
        "",
        "             T E R M I N A L   F I R S T   P E R S O N   S H O O T E R",
        "",
        "  ─────────────────────────────────────────────────────────────────────",
        "   W / S      Move forward / backward",
        "   A / D      Rotate left / right",
        "   Q / E      Strafe left / right",
        "   SPACE       Shoot",
        "   ESC         Quit",
        "  ─────────────────────────────────────────────────────────────────────",
        "",
        "                    Press  ENTER  to begin...",
        "",
    };

    printf("\033[38;5;196m");  // red
    for (auto &line : title) {
        printf("  %s\n", line);
    }
    printf("\033[0m");
    fflush(stdout);

    // Wait for enter
    char c = 0;
    while (c != '\r' && c != '\n' && c != ' ') {
        c = poll_key();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    printf("\033[2J\033[H");
    fflush(stdout);
}

/* ─── MAIN LOOP ──────────────────────────────────────────── */

int main() {
    platform_init();
    show_title();

    // Get terminal size
    int tw = 120, th = 35;
    get_console_size(tw, th);
    tw = std::max(40, std::min(tw, 220));
    th = std::max(20, std::min(th, 60));
    buf_init(tw, th);

    Player player;
    double bob      = 0.0;
    double bob_vel  = 0.0;
    bool   running  = true;

    using clock = std::chrono::high_resolution_clock;
    auto  prev  = clock::now();
    double fps  = 60.0;

    while (running) {
        auto  now = clock::now();
        double dt = std::chrono::duration<double>(now - prev).count();
        prev = now;
        if (dt > 0.05) dt = 0.05;  // cap
        fps = fps * 0.95 + (1.0 / dt) * 0.05;

        /* ── INPUT ── */
        char key = poll_key();
        while (key != 0) {
            switch (key) {
                case 'w': case 'W': {
                    double nx = player.x + cos(player.angle) * player.speed * dt;
                    double ny = player.y + sin(player.angle) * player.speed * dt;
                    if (can_move(nx, player.y)) player.x = nx;
                    if (can_move(player.x, ny)) player.y = ny;
                    bob_vel = 1.0;
                    break;
                }
                case 's': case 'S': {
                    double nx = player.x - cos(player.angle) * player.speed * dt;
                    double ny = player.y - sin(player.angle) * player.speed * dt;
                    if (can_move(nx, player.y)) player.x = nx;
                    if (can_move(player.x, ny)) player.y = ny;
                    bob_vel = 1.0;
                    break;
                }
                case 'a': case 'A':
                    player.angle -= player.rot * dt;
                    break;
                case 'd': case 'D':
                    player.angle += player.rot * dt;
                    break;
                case 'q': case 'Q': {
                    double sa = player.angle - M_PI / 2.0;
                    double nx = player.x + cos(sa) * player.speed * dt;
                    double ny = player.y + sin(sa) * player.speed * dt;
                    if (can_move(nx, player.y)) player.x = nx;
                    if (can_move(player.x, ny)) player.y = ny;
                    bob_vel = 1.0;
                    break;
                }
                case 'e': case 'E': {
                    double sa = player.angle + M_PI / 2.0;
                    double nx = player.x + cos(sa) * player.speed * dt;
                    double ny = player.y + sin(sa) * player.speed * dt;
                    if (can_move(nx, player.y)) player.x = nx;
                    if (can_move(player.x, ny)) player.y = ny;
                    bob_vel = 1.0;
                    break;
                }
                case ' ':
                    do_shoot(player);
                    break;
                case 27:  // ESC
                    running = false;
                    break;
            }
            key = poll_key();
        }

        /* ── PERSISTENT KEY MOVEMENT (held keys via repeated polls) ─ */
        // We poll at 60fps so each frame we read whatever is buffered.
        // For smooth held movement, we also track last action per frame.

        /* ── UPDATE ── */
        player.shoot_timer -= dt;
        if (player.shoot_timer < 0) {
            player.shoot_timer = 0;
            player.shooting = false;
        }

        // Head bob
        if (bob_vel > 0) {
            bob += dt * 6.0;
            bob_vel -= dt * 4.0;
            if (bob_vel < 0) bob_vel = 0;
        }

        // Enemy AI
        for (auto &e : enemies) {
            if (!e.alive) continue;
            double dx = e.x - player.x, dy = e.y - player.y;
            double dist = sqrt(dx*dx + dy*dy);
            if (dist < 8.0) e.alert = true;
            if (!e.alert) continue;

            if (dist > 0.8) {
                double nx = e.x - (dx / dist) * e.speed * dt;
                double ny = e.y - (dy / dist) * e.speed * dt;
                if (can_move(nx, e.y)) e.x = nx;
                if (can_move(e.x, ny)) e.y = ny;
            } else {
                // Enemy in melee range
                e.shoot_cd -= dt;
                if (e.shoot_cd <= 0.0) {
                    player.hp = std::max(0, player.hp - 8);
                    e.shoot_cd = 1.0;
                }
            }

            // Enemy ranged attack
            if (dist > 1.5 && dist < 7.0) {
                e.shoot_cd -= dt;
                if (e.shoot_cd <= 0.0) {
                    // Check line of sight (simple)
                    RayHit lh = cast_ray(player.x, player.y,
                        atan2(e.y - player.y, e.x - player.x));
                    if (lh.dist >= dist - 0.5) {
                        player.hp = std::max(0, player.hp - 5);
                    }
                    e.shoot_cd = 2.0;
                }
            }
        }

        if (player.hp <= 0) {
            // Game over screen
            printf("\033[2J\033[H\033[38;5;196m");
            printf("\n\n\n\n\n\n");
            printf("              ██████╗  █████╗ ███╗   ███╗███████╗ ██████╗ ██╗   ██╗███████╗██████╗\n");
            printf("             ██╔════╝ ██╔══██╗████╗ ████║██╔════╝██╔═══██╗██║   ██║██╔════╝██╔══██╗\n");
            printf("             ██║  ███╗███████║██╔████╔██║█████╗  ██║   ██║██║   ██║█████╗  ██████╔╝\n");
            printf("             ██║   ██║██╔══██║██║╚██╔╝██║██╔══╝  ██║   ██║╚██╗ ██╔╝██╔══╝  ██╔══██╗\n");
            printf("             ╚██████╔╝██║  ██║██║ ╚═╝ ██║███████╗╚██████╔╝ ╚████╔╝ ███████╗██║  ██║\n");
            printf("              ╚═════╝ ╚═╝  ╚═╝╚═╝     ╚═╝╚══════╝ ╚═════╝   ╚═══╝  ╚══════╝╚═╝  ╚═╝\n");
            printf("\n              Kills: %d\n", player.kills);
            printf("\033[0m");
            fflush(stdout);
            std::this_thread::sleep_for(std::chrono::seconds(3));
            break;
        }

        /* ── RENDER ── */
        buf_clear();
        render_walls(player);
        render_sprites(player);
        render_gun(player, bob, player.shooting);
        render_hud(player, fps);
        buf_flush();

        // Frame cap ~60fps
        auto elapsed = std::chrono::duration<double>(clock::now() - now).count();
        double sleep_s = (1.0/60.0) - elapsed;
        if (sleep_s > 0)
            std::this_thread::sleep_for(
                std::chrono::microseconds((long long)(sleep_s * 1e6)));
    }

    platform_restore();
    printf("\nThanks for playing CORRIDOR. Kills: %d\n", player.kills);
    return 0;
}
