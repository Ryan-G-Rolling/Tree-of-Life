/*
 * atmosphere.c  ——  深空背景层（星云 / 星场 / 尘埃 / 暗角）
 *
 * 视觉方向 3.0「Primordial Cosmos」：
 *   底不是一块纯色，而是一片有纵深的宇宙 —— 远处星云、银河带、星点、前景尘埃。
 *   UI 卡片压在上面，只从缝隙与全屏页透出来；全屏页（结算 / Legacy）则是它
 *   真正的主场。
 *
 * 实现要点：
 *   1. 静态部分（星云 + 星场 + 银河带）**只烘焙一次**，按 SS 重烧；
 *      读回图像时加一层 ±3 的抖动噪声，消除 8bit 径向渐变的色带，
 *      同时获得胶片颗粒。
 *   2. 前景层（尘埃 / 暗角）逐帧画，坐标固定、只做极慢位移 ——
 *      4K 下是“呼吸”，不是“粒子雨”。
 *   3. 公开 API 与旧版一致，ui.c / screens.c 不需要知道内部换了什么。
 */
#include <math.h>
#include <stdbool.h>

#include "raylib.h"
#include "atmosphere.h"
#include "ui_internal.h"

#define ATM_TAU (2.0f * PI)

static Texture2D g_static;
static int g_static_ss;
static bool g_ready;
static int g_calls;

static Texture2D g_nebula;      /* assets/tex/nebula.png：生成式星云底图（可选） */
static bool g_nebula_tried;

#define NEBULA_PATH "assets/tex/nebula.png"

int atm_last_call_count(void) { return g_calls; }

static float hash01(unsigned int n)
{
    n = (n ^ 61u) ^ (n >> 16);
    n *= 9u;
    n = n ^ (n >> 4);
    n *= 0x27d4eb2du;
    n = n ^ (n >> 15);
    return (float)(n & 0xFFFFFFu) / 16777215.0f;
}

/* ---------------- 星云 ---------------- */

/* 一团柔光云：3 层径向渐变错开圆心与半径，边缘自然收干净。
 * DrawCircleGradient 是单三角形的顶点插值，色带只能靠外层噪声消。 */
static void nebula(Vector2 c, float r, Color col, float a)
{
    DrawCircleGradient((int)c.x, (int)c.y, r, Fade(col, a), Fade(col, 0.0f));
    DrawCircleGradient((int)(c.x + r * 0.22f), (int)(c.y - r * 0.18f), r * 0.72f,
                       Fade(col, a * 0.70f), Fade(col, 0.0f));
    DrawCircleGradient((int)(c.x - r * 0.26f), (int)(c.y + r * 0.24f), r * 0.55f,
                       Fade(col, a * 0.50f), Fade(col, 0.0f));
}

/* 一颗星：核心 + 柔光；亮星再加十字星芒。 */
static void star(Vector2 p, float r, float a, bool cross)
{
    DrawCircleGradient((int)p.x, (int)p.y, r * 5.0f, Fade(COL_TEXT, a * 0.30f), Fade(COL_TEXT, 0.0f));
    DrawCircleV(p, r, Fade(COL_TEXT, a));
    if (cross) {
        float len = r * 6.5f;
        DrawLineEx((Vector2){ p.x - len, p.y }, (Vector2){ p.x + len, p.y }, 1.0f,
                   Fade(COL_TEXT, a * 0.35f));
        DrawLineEx((Vector2){ p.x, p.y - len }, (Vector2){ p.x, p.y + len }, 1.0f,
                   Fade(COL_TEXT, a * 0.35f));
    }
}

/* 巨星球的边缘：画面右下有一整颗行星探进来，只露一圈冷光。
 * 圆心在画布外，所以玩家只看到“更大世界”的一小段弧。 */
static void planet_limb(void)
{
    Vector2 c = { UI_W + 150.0f, UI_H + 210.0f };
    float R = 520.0f;
    DrawCircleV(c, R, Fade(COL_BG_LIFT, 0.55f));
    DrawCircleGradient((int)c.x, (int)c.y, R,
                       Fade(COL_BG_LIFT, 0.55f), Fade(COL_BG_DEEP, 0.96f));
    DrawRing(c, R - 1.5f, R + 1.5f, 150.0f, 300.0f, 96, Fade(COL_LINE_STRONG, 0.20f));
    DrawRing(c, R - 9.0f, R - 7.5f, 165.0f, 285.0f, 96, Fade(COL_LINE, 0.16f));
}

/* 生成式星云底图：16:9 一张就够。缺文件时整层跳过，纯程序化星云顶上。 */
static bool nebula_ready(void)
{
    if (g_nebula_tried) return g_nebula.id != 0;
    g_nebula_tried = true;
    if (!FileExists(NEBULA_PATH)) return false;
    g_nebula = LoadTexture(NEBULA_PATH);
    if (g_nebula.id == 0 || g_nebula.width <= 0) {
        g_nebula = (Texture2D){ 0 };
        return false;
    }
    SetTextureFilter(g_nebula, TEXTURE_FILTER_BILINEAR);
    TraceLog(LOG_INFO, "ATM: nebula backdrop %dx%d loaded", g_nebula.width, g_nebula.height);
    return true;
}

static void draw_nebula_base(void)
{
    Rectangle dst = { 0.0f, 0.0f, (float)UI_W, (float)UI_H };
    Rectangle src = { 0.0f, 0.0f, (float)g_nebula.width, (float)g_nebula.height };
    /* 略微放大取样，避免顶部/底部出现原图的硬边 */
    src.x += src.width * 0.02f;
    src.y += src.height * 0.02f;
    src.width *= 0.96f;
    src.height *= 0.96f;
    DrawTexturePro(g_nebula, src, dst, (Vector2){ 0.0f, 0.0f }, 0.0f, WHITE);
    /* 统一色调：顶部压一点冷，底部压一点深，让 UI 面板压上去时层次一致 */
    DrawRectangleGradientV(0, 0, UI_W, UI_H / 2,
                           Fade((Color){ 6, 10, 22, 255 }, 0.42f), Fade((Color){ 6, 10, 22, 255 }, 0.0f));
    DrawRectangleGradientV(0, UI_H / 2, UI_W, UI_H / 2,
                           Fade((Color){ 2, 4, 10, 255 }, 0.0f), Fade((Color){ 2, 4, 10, 255 }, 0.55f));
}

/* ---------------- 烘焙 ---------------- */

static void bake_scene(void)
{
    /* 天空渐变：上冷下暖一点，别让整屏是同一个蓝 */
    DrawRectangleGradientV(0, 0, UI_W, UI_H,
                           (Color){ 11, 18, 36, 255 }, (Color){ 4, 7, 15, 255 });

    bool art = nebula_ready();
    if (art) {
        draw_nebula_base();
    } else {
        /* 没有底图：程序化星云顶上（与旧版一致） */
        for (int i = 0; i < 9; ++i) {
            float t = (float)i / 8.0f;
            Vector2 p = { 40.0f + t * 1200.0f, 640.0f - t * 520.0f + sinf(t * 5.0f) * 40.0f };
            nebula(p, 230.0f + 70.0f * hash01((unsigned int)i * 5u + 2u),
                   (i % 2) ? COL_VIOLET : COL_TEAL, 0.042f);
        }
        nebula((Vector2){ 250.0f, 150.0f }, 300.0f, COL_VIOLET, 0.10f);
        nebula((Vector2){ 1070.0f, 610.0f }, 340.0f, COL_TEAL, 0.085f);
        nebula((Vector2){ 720.0f, 40.0f }, 240.0f, COL_ACCENT, 0.055f);
        nebula((Vector2){ 60.0f, 560.0f }, 220.0f, COL_DNA, 0.06f);
    }

    planet_limb();

    /* 星场：260 颗，亮度按幂分布（大部分很暗，少数很亮） */
    for (int i = 0; i < 260; ++i) {
        float x = hash01((unsigned int)i * 3u + 11u) * (float)UI_W;
        float y = hash01((unsigned int)i * 7u + 23u) * (float)UI_H;
        float pick = hash01((unsigned int)i * 11u + 37u);
        float r = 0.6f + 2.2f * pick * pick * pick;
        float a = 0.18f + 0.62f * hash01((unsigned int)i * 13u + 5u);
        bool cross = (i % 37) == 0;
        star((Vector2){ x, y }, r, a, cross);
    }

    /* 极少数“导航星”：带一圈淡淡的光环，给画面一个视觉锚点 */
    for (int i = 0; i < 3; ++i) {
        Vector2 p = { 320.0f + 300.0f * (float)i, 620.0f - 80.0f * (float)i };
        DrawRing(p, 14.0f, 15.0f, 0.0f, 360.0f, 48, Fade(COL_ACCENT, 0.10f));
    }
}

/* 8bit 径向渐变在深色底上会出现同心色带。加一层 ±3 的确定性噪声，
 * 顺带得到胶片颗粒。逐像素一次 LCG，烘焙时只跑一遍。 */
static void image_grain(Image *img, int amount)
{
    if (img->format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) return;
    unsigned char *p = (unsigned char *)img->data;
    int n = img->width * img->height;
    unsigned int s = 0x9E3779B9u;
    for (int i = 0; i < n; ++i) {
        s = s * 1664525u + 1013904223u;
        int d = (int)((s >> 19) & 7u) - 3;
        d = d * amount / 3;
        int r = p[i * 4 + 0] + d;
        int g = p[i * 4 + 1] + d;
        int b = p[i * 4 + 2] + d;
        p[i * 4 + 0] = (unsigned char)(r < 0 ? 0 : (r > 255 ? 255 : r));
        p[i * 4 + 1] = (unsigned char)(g < 0 ? 0 : (g > 255 ? 255 : g));
        p[i * 4 + 2] = (unsigned char)(b < 0 ? 0 : (b > 255 ? 255 : b));
        p[i * 4 + 3] = 255;
    }
}

static void free_static(void)
{
    if (IsTextureValid(g_static)) UnloadTexture(g_static);
    g_static = (Texture2D){ 0 };
    g_ready = false;
}

static void bake_static(int ss)
{
    free_static();

    RenderTexture2D rt = LoadRenderTexture(UI_W * ss, UI_H * ss);
    if (!IsRenderTextureValid(rt)) {
        TraceLog(LOG_WARNING, "ATM: static RT %dx%d failed, background disabled",
                 UI_W * ss, UI_H * ss);
        return;
    }

    BeginTextureMode(rt);
    ClearBackground(COL_BG_DEEP);

    Camera2D cam = { 0 };
    cam.zoom = (float)ss;
    BeginMode2D(cam);
    bake_scene();
    EndMode2D();
    EndTextureMode();

    Image img = LoadImageFromTexture(rt.texture);
    ImageFlipVertical(&img);
    image_grain(&img, 3);

    g_static = LoadTextureFromImage(img);
    SetTextureFilter(g_static, TEXTURE_FILTER_BILINEAR);
    UnloadImage(img);
    UnloadRenderTexture(rt);

    g_static_ss = ss;
    g_ready = IsTextureValid(g_static);

    TraceLog(LOG_INFO, "ATM: cosmic backdrop baked %dx%d (SS=%d)",
             UI_W * ss, UI_H * ss, ss);
}

/* ---------------- 生命周期 ---------------- */

void atm_init(void)
{
    g_static = (Texture2D){ 0 };
    g_static_ss = 0;
    g_ready = false;
}

void atm_ensure(void)
{
    int ss = ui_canvas_ss();
    if (ss < UI_SS_MIN) ss = UI_SS_MIN;
    if (ss > UI_SS_MAX) ss = UI_SS_MAX;
    if (!(g_ready && ss == g_static_ss)) bake_static(ss);
}

void atm_shutdown(void)
{
    free_static();
    if (IsTextureValid(g_nebula)) UnloadTexture(g_nebula);
    g_nebula = (Texture2D){ 0 };
    g_nebula_tried = false;
    g_static_ss = 0;
}

/* ---------------- 每帧绘制 ---------------- */

void atm_draw_static(void)
{
    g_calls = 0;
    if (!g_ready) return;
    Rectangle src = { 0.0f, 0.0f, (float)g_static.width, (float)g_static.height };
    Rectangle dst = { 0.0f, 0.0f, (float)UI_W, (float)UI_H };
    DrawTexturePro(g_static, src, dst, (Vector2){ 0.0f, 0.0f }, 0.0f, WHITE);
    g_calls++;
}

/* 漂浮尘埃：位置由时间推进并取模回卷，速度极慢（4–11 px/s）。
 * 全部用加法混合的低透明度圆点，读作“空气里悬浮的微粒”。 */
static void draw_dust(int count, float seed, Rectangle area)
{
    float t = g_ui.time;
    BeginBlendMode(BLEND_ADDITIVE);
    for (int i = 0; i < count; ++i) {
        float sp = 4.0f + 7.0f * hash01((unsigned int)i * 17u + (unsigned int)seed);
        float x = area.x + fmodf(hash01((unsigned int)i * 23u + 7u) * area.width
                                 + t * sp * 0.35f, area.width);
        float y = area.y + fmodf(hash01((unsigned int)i * 29u + 13u) * area.height
                                 - t * sp * 0.55f + area.height * 1000.0f, area.height);
        float tw = 0.45f + 0.55f * (0.5f + 0.5f * sinf(t * (0.7f + 0.8f * hash01((unsigned int)i * 31u)) + (float)i));
        float r = 0.8f + 1.4f * hash01((unsigned int)i * 37u + 3u);
        DrawCircleV((Vector2){ x, y }, r, Fade(COL_ACCENT, 0.075f * tw));
    }
    EndBlendMode();
    g_calls += 1;
}

void atm_draw_live(void)
{
    draw_dust(46, 3.0f, (Rectangle){ 0.0f, 0.0f, (float)UI_W, (float)UI_H });

    /* 两级呼吸光团：全屏页上给出“宇宙在缓慢聚散”的感觉 */
    float t = g_ui.time;
    float a = 0.5f + 0.5f * sinf(t * 0.18f);
    DrawCircleGradient(220, 300, 260.0f, Fade(COL_VIOLET, 0.020f + 0.010f * a), Fade(COL_VIOLET, 0.0f));
    DrawCircleGradient(1040, 520, 300.0f, Fade(COL_TEAL, 0.016f + 0.010f * (1.0f - a)), Fade(COL_TEAL, 0.0f));
    g_calls += 2;
}

void atm_draw_live_in(Rectangle area)
{
    draw_dust(14, 11.0f, area);
}

/* 前景暗角：四条渐变带，不压中心。深空题材里这一步是“镜头边缘”的物理感。
 * 上下只压 56–64 px（顶栏 / 底栏是整块 chrome，压太深会把文字吃掉），
 * 左右压 96 px —— 侧边没有内容，可以放心黑。 */
void atm_draw_vignette(void)
{
    float W = (float)UI_W, H = (float)UI_H;
    Color dark = Fade(BLACK, 0.30f);
    Color clear = Fade(BLACK, 0.0f);

    DrawRectangleGradientV(0, 0, UI_W, 56,
                           Fade(BLACK, 0.22f), Fade(BLACK, 0.0f));
    DrawRectangleGradientV(0, UI_H - 64, UI_W, 64,
                           Fade(BLACK, 0.0f), Fade(BLACK, 0.26f));

    Rectangle left = { 0.0f, 0.0f, 96.0f, H };
    DrawRectangleGradientEx(left, dark, dark, clear, clear);
    Rectangle right = { W - 96.0f, 0.0f, 96.0f, H };
    DrawRectangleGradientEx(right, clear, clear, dark, dark);

    g_calls += 3;
}
