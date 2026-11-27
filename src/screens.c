/*
 * screens.c  ——  页面结构与绘制（Member C）
 *
 * 每个页面分两步：
 *   build_xxx()  只填矩形表（按钮在哪、是否可点）
 *   draw_xxx()   只画（用同一批矩形 + ui_adapter 提供的只读数据）
 * 这样“命中判定”和“画在哪”永远一致，弹层天然压住底层。
 *
 * 视觉方向 3.0「Primordial Cosmos」：
 *   Cookie Clicker 的“大主体 + 商店列 + 数字暴涨”，Cell to Singularity 的
 *   “深空星云 + 荧光生命 + 宇宙尺度”。所有图形都是程序化矢量（无位图依赖），
 *   在 4K 下保持锐利。
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "raylib.h"
#include "ui_internal.h"
#include "ui_adapter.h"
#include "atmosphere.h"
#include "buffs.h"
#include "codex.h"

/* ================= 通用小工具 ================= */

static UiLayout s_prev;

static UiButton *btn_add(UiLayout *L, UiButtonId id, Rectangle r, int target, int option, bool enabled)
{
    if (L->count >= MAX_UI_BUTTONS) return NULL;
    UiButton *b = &L->items[L->count++];
    float inherit_hover = 0.0f, inherit_press = 0.0f;
    for (int i = 0; i < s_prev.count; ++i) {
        const UiButton *p = &s_prev.items[i];
        if (p->id == id && p->target_id == target && p->option == option &&
            p->rect.x == r.x && p->rect.y == r.y) {
            inherit_hover = p->hover;
            inherit_press = p->press;
            break;
        }
    }
    memset(b, 0, sizeof(*b));
    b->hover = inherit_hover;
    b->press = inherit_press;
    b->id = id;
    b->rect = r;
    b->target_id = target;
    b->option = option;
    b->enabled = enabled;
    return b;
}

static UiButton *btn_find(const UIState *S, UiButtonId id, int target)
{
    for (int i = 0; i < S->layout.count; ++i) {
        const UiButton *b = &S->layout.items[i];
        if (b->id == id && (target < 0 || b->target_id == target)) {
            return (UiButton *)&S->layout.items[i];
        }
    }
    return NULL;
}

static bool is_hot(const UIState *S, const UiButton *b)
{
    return b && S->hot >= 0 && (&S->layout.items[S->hot] == b);
}

static float btn_hover_step(UiButton *b, bool hot)
{
    if (!b) return 0.0f;
    float dt = GetFrameTime();
    if (dt > 0.12f) dt = 0.12f;
    float target = (hot && b->enabled) ? 1.0f : 0.0f;
    b->hover += (target - b->hover) * fminf(1.0f, dt / 0.12f);
    return b->hover;
}

static float hover_lift(float hv)
{
    float u = hv < 0.0f ? 0.0f : (hv > 1.0f ? 1.0f : hv);
    float v = 1.0f - u;
    return -(1.0f - v * v * v) * 1.5f;
}

static float hash01(unsigned int n)
{
    n = (n ^ 61u) ^ (n >> 16);
    n *= 9u;
    n = n ^ (n >> 4);
    n *= 0x27d4eb2du;
    n = n ^ (n >> 15);
    return (float)(n & 0xFFFFFFu) / 16777215.0f;
}

/* ================= 物种身份色 =================
 * 20 个物种各有固定荧光色：只做填充、发光与色轨，永不做正文颜色。
 * 色相分散在整圈，相邻阶段不会撞色。 */
static Color ecology_color(int species_id)
{
    static const Color palette[SPECIES_COUNT] = {
        { 118, 240, 190, 255 },  /*  0 原始细胞   薄荷 */
        { 196, 238, 104, 255 },  /*  1 蓝细菌     柠黄 */
        { 255, 176,  96, 255 },  /*  2 化能细菌   琥珀 */
        {  88, 224, 210, 255 },  /*  3 藻类真核   青 */
        { 255, 124, 120, 255 },  /*  4 原生捕食者 珊瑚 */
        { 172, 150, 255, 255 },  /*  5 多细胞群落 紫 */
        { 240, 124, 206, 255 },  /*  6 极端群落   品红 */
        { 255, 150, 170, 255 },  /*  7 分化组织   玫瑰 */
        { 150, 220, 130, 255 },  /*  8 共生超级   草绿 */
        { 120, 190, 255, 255 },  /*  9 认知生物圈 天蓝 */
        { 210, 180, 120, 255 },  /* 10 真菌网络   沙金 */
        { 255, 190, 140, 255 },  /* 11 珊瑚共生   蜜桃 */
        { 160, 235, 170, 255 },  /* 12 菌根网络   嫩绿 */
        { 110, 220, 235, 255 },  /* 13 礁石都市   水绿 */
        { 255, 214, 140, 255 },  /* 14 智慧生命   暖金 */
        { 180, 200, 255, 255 },  /* 15 技术圈     钢蓝 */
        { 255, 170, 120, 255 },  /* 16 文明       橙 */
        { 140, 220, 255, 255 },  /* 17 太空时代   冰蓝 */
        { 200, 140, 255, 255 },  /* 18 奇点       紫罗兰 */
        { 255, 240, 200, 255 },  /* 19 超越       白金 */
    };
    if (species_id < 0 || species_id >= SPECIES_COUNT) return COL_DIM;
    return palette[species_id];
}

/* ================= 生物形态引擎 =================
 * 形态只由「阶段」决定类别；类内差异由 species_id 的种子决定。
 * 12 类：球菌 / 杆菌 / 真核 / 群体 / 组织 / 共生 / 神经 / 技术圈 / 文明 /
 *        太空 / 奇点 / 超越。同一个物种在任何页面都是同一副样子。
 */
enum {
    MORPH_COCCUS = 0,
    MORPH_ROD,
    MORPH_EUKARYOTE,
    MORPH_COLONY,
    MORPH_TISSUE,
    MORPH_SYMBIO,
    MORPH_NEURAL,
    MORPH_TECHNO,
    MORPH_CIV,
    MORPH_SPACE,
    MORPH_SINGULARITY,
    MORPH_TRANSCEND
};

static int morph_class(int stage)
{
    if (stage <= 0) return MORPH_COCCUS;
    if (stage == 1) return MORPH_ROD;
    if (stage == 2) return MORPH_EUKARYOTE;
    if (stage == 3) return MORPH_COLONY;
    if (stage == 4) return MORPH_TISSUE;
    if (stage == 5) return MORPH_SYMBIO;
    if (stage == 6) return MORPH_NEURAL;
    if (stage == 7) return MORPH_TECHNO;
    if (stage == 8) return MORPH_CIV;
    if (stage == 9) return MORPH_SPACE;
    if (stage == 10) return MORPH_SINGULARITY;
    return MORPH_TRANSCEND;
}

/* 球体：三段渐变（受光/本体/暗面）+ 边缘反弹光 + 两层高光。
 * 单一 DrawCircleGradient 会像塑料球，这里叠一次“从右下压暗”的渐变做出体积。 */
static void sphere(Vector2 c, float r, Color col, float a)
{
    if (r <= 0.5f) return;
    /* 细胞质要比膜暗：细胞器/拟核才有对比度。之前中心用高亮色，大尺寸下就是一坨亮糖，
       里面的结构全被洗掉；改成“中心中调、边缘压暗”，高光只留左上一个小点 */
    Color hi = ui_color_mix(col, WHITE, 0.58f);
    Color mid = ui_color_mix(col, BLACK, 0.16f);
    Color lo = ui_color_mix(col, BLACK, 0.52f);

    ui_glow(c, r * 1.40f, col, 0.15f * a);

    DrawCircleGradient((int)(c.x - r * 0.08f), (int)(c.y - r * 0.10f), r,
                       Fade(mid, 0.96f * a), Fade(lo, 0.96f * a));
    DrawRing(c, r * 0.86f, r * 1.0f, 18.0f, 132.0f, 44, Fade(lo, 0.30f * a));
    DrawRing(c, r * 0.80f, r * 0.98f, 24.0f, 128.0f, 52, Fade(ui_color_mix(col, WHITE, 0.30f), 0.09f * a));
    DrawRing(c, r - 1.1f, r + 0.2f, 0.0f, 360.0f, 84, Fade(hi, 0.30f * a));

    DrawEllipse((int)(c.x - r * 0.36f), (int)(c.y - r * 0.42f),
                r * 0.30f, r * 0.20f, Fade(ui_color_mix(col, WHITE, 0.30f), 0.14f * a));
    DrawEllipse((int)(c.x - r * 0.40f), (int)(c.y - r * 0.46f),
                r * 0.11f, r * 0.07f, Fade(WHITE, 0.26f * a));
}

/* 细胞膜：外亮内暗两条环 + 一段高光弧。任何形态先靠它“有壳”。 */
static void membrane(Vector2 c, float r, Color col, float a)
{
    if (r < 6.0f) return;
    DrawRing(c, r * 0.985f, r * 1.012f, 0.0f, 360.0f, 84,
             Fade(ui_color_mix(col, WHITE, 0.45f), 0.52f * a));
    DrawRing(c, r * 0.935f, r * 0.962f, 0.0f, 360.0f, 84, Fade(col, 0.26f * a));
    DrawRing(c, r * 0.995f, r * 1.025f, 158.0f, 252.0f, 42, Fade(WHITE, 0.34f * a));
}

/* 胶囊体（杆菌 / 线粒体 / 叶绿体共用）：圆头矩形 + 旋转 */
static void capsule(Vector2 p, float len, float th, float ang, Color fill)
{
    Vector2 d = { cosf(ang * DEG2RAD), sinf(ang * DEG2RAD) };
    DrawCircleV(p, th, fill);
    DrawRectanglePro((Rectangle){ p.x, p.y - th, len, th * 2.0f },
                     (Vector2){ 0.0f, th }, ang, fill);
    DrawCircleV((Vector2){ p.x + d.x * len, p.y + d.y * len }, th, fill);
}

/* 行星本体：按列做朗伯明暗 + 云带 + 大陆（带海岸线与内陆高光）+ 极冠 + 海面反光。
 * 目标是“从太空看一颗真的行星”，不是一个蓝球贴几块绿斑。 */
static void planet_body(Vector2 c, float r, Color col, Color land, float a)
{
    /* 星球自己的一套冷色：不跟着物种色走，否则暖色物种会把海面染成橙色 */
    Color ocean = { 26, 72, 134, 255 };
    Color day = { 92, 164, 220, 255 };
    Color night = { 5, 11, 26, 255 };
    Color band = { 214, 236, 250, 255 };
    Color land_dark = ui_color_mix(land, (Color){ 24, 62, 44, 255 }, 0.55f);

    ui_glow(c, r * 1.30f, ui_color_mix(col, (Color){ 80, 170, 220, 255 }, 0.82f), 0.12f * a);

    int cols = (int)(r * 2.0f) + 1;
    if (cols > 110) cols = 110;
    if (cols < 12) cols = 12;
    float step = (r * 2.0f) / (float)cols;

    for (int i = 0; i < cols; ++i) {
        float x = -r + (float)i * step + step * 0.5f;
        float half = sqrtf(fmaxf(0.0f, r * r - x * x));
        float lam = 0.50f - 0.70f * (x / r) + 0.08f * (1.0f - fabsf(x / r));
        if (lam < 0.0f) lam = 0.0f;
        if (lam > 1.0f) lam = 1.0f;
        Color base = ui_color_mix(night, ocean, 0.90f);
        Color cc = ui_color_mix(base, day, lam * 0.92f);
        DrawRectangleV((Vector2){ c.x + x - step * 0.5f - 0.5f, c.y - half },
                       (Vector2){ step + 1.0f, half * 2.0f }, Fade(cc, 0.99f * a));

        /* 两条深色气旋带，让表面不是一块纯色海洋 */
        float by[2] = { -0.36f, 0.28f };
        for (int k = 0; k < 2; ++k) {
            float cy0 = c.y + by[k] * half;
            float bh = half * (0.09f + 0.03f * sinf((float)i * 0.12f + (float)k));
            DrawRectangleV((Vector2){ c.x + x - step * 0.5f - 0.5f, cy0 - bh },
                           (Vector2){ step + 1.0f, bh * 2.0f },
                           Fade(band, (0.05f + 0.04f * lam) * a));
        }
    }

    /* 大陆：四块陆核，每块由 3 个叠在一起的椭圆组成，外圈有海岸线 */
    for (int i = 0; i < 4; ++i) {
        float ang = 0.55f + (float)i * 1.62f + hash01((unsigned int)i * 29u + 11u) * 0.35f;
        float dist = r * (0.20f + 0.28f * hash01((unsigned int)i * 31u + 17u));
        Vector2 cc = { c.x + cosf(ang) * dist, c.y + sinf(ang) * dist * 0.88f };
        float lam = 0.50f - 0.70f * ((cc.x - c.x) / r);
        if (lam < 0.0f) lam = 0.0f;
        if (lam > 1.0f) lam = 1.0f;

        for (int b = 0; b < 3; ++b) {
            float ox = (hash01((unsigned int)(i * 3 + b) * 43u + 5u) - 0.5f) * r * 0.30f;
            float oy = (hash01((unsigned int)(i * 3 + b) * 47u + 7u) - 0.5f) * r * 0.26f;
            float rx = r * (0.13f + 0.09f * hash01((unsigned int)(i * 3 + b) * 53u + 11u));
            float ry = rx * (0.56f + 0.34f * hash01((unsigned int)(i * 3 + b) * 59u + 13u));
            Vector2 bc = { cc.x + ox, cc.y + oy };

            float al = (0.30f + 0.55f * lam) * a;
            /* 海岸线：一圈更深的边 */
            DrawEllipse((int)bc.x, (int)bc.y, rx * 1.10f, ry * 1.12f,
                        Fade(ui_color_mix(land_dark, night, 0.45f), al * 0.65f));
            DrawEllipse((int)bc.x, (int)bc.y, rx, ry,
                        Fade(ui_color_mix(land_dark, day, lam * 0.35f), al));
            /* 内陆高光：一小片更亮的中心，读成高原/植被差异 */
            DrawEllipse((int)(bc.x - rx * 0.18f), (int)(bc.y - ry * 0.18f),
                        rx * 0.44f, ry * 0.38f,
                        Fade(ui_color_mix(land_dark, WHITE, 0.28f), al * 0.30f));
        }
    }

    /* 极冠：南北各一小片冰盖，贴着球体的上下缘 */
    DrawEllipse((int)c.x, (int)(c.y - r * 0.76f), r * 0.20f, r * 0.055f,
                Fade(ui_color_mix(WHITE, day, 0.30f), 0.30f * a));
    DrawEllipse((int)c.x, (int)(c.y + r * 0.78f), r * 0.16f, r * 0.045f,
                Fade(ui_color_mix(WHITE, day, 0.30f), 0.22f * a));

    /* 云带：三条横向细云，用小圆点连成雾带，避免变成一圈圆点 */
    for (int k = 0; k < 3; ++k) {
        float yy = c.y + ((float)k - 1.0f) * r * 0.38f;
        int n = 22;
        for (int i = 0; i < n; ++i) {
            float u = (float)i / (float)(n - 1) * 2.0f - 1.0f;
            float px = c.x + u * r * 0.94f;
            float py = yy + sinf(u * 3.4f + (float)k * 2.0f) * r * 0.045f;
            float dx = (px - c.x) / r;
            float half = sqrtf(fmaxf(0.0f, 1.0f - dx * dx));
            if (half < 0.05f) continue;
            float lam = 0.50f - 0.70f * dx;
            if (lam < 0.0f) lam = 0.0f;
            float al = (0.03f + 0.06f * lam) * a * (0.55f + 0.45f * sinf((float)i * 1.3f + (float)k));
            DrawCircleV((Vector2){ px, py }, r * 0.055f * (0.7f + 0.5f * half), Fade(WHITE, al));
        }
    }

    /* 海面反光：受光侧一枚很收的高光 */
    DrawEllipse((int)(c.x - r * 0.40f), (int)(c.y - r * 0.42f),
                r * 0.20f, r * 0.11f, Fade(WHITE, 0.10f * a));

    /* 大气边缘 + 受光侧轮廓光：右侧暗面留一条细冷光，星球才“圆” */
    DrawRing(c, r * 0.975f, r * 1.02f, 0.0f, 360.0f, 96,
             Fade(ui_color_mix(day, WHITE, 0.55f), 0.26f * a));
    DrawRing(c, r * 1.03f, r * 1.09f, 0.0f, 360.0f, 96, Fade(col, 0.12f * a));
    DrawRing(c, r * 0.975f, r * 1.005f, 96.0f, 258.0f, 64, Fade(WHITE, 0.26f * a));
    DrawRing(c, r * 0.97f, r * 0.995f, -40.0f, 60.0f, 40,
             Fade(ui_color_mix(day, WHITE, 0.30f), 0.16f * a));
}

/* 鞭毛：一条向远端衰减的贝塞尔曲线（3 段折线拼出来会显得硬） */
static void flagella(Vector2 c, float r, float ang, Color col, float t, int count)
{
    for (int f = 0; f < count; ++f) {
        float dir = ang + 2.55f + (float)f * 0.70f;
        float len = r * (1.35f + 0.15f * (float)f);
        Vector2 p0 = { c.x + cosf(dir) * r * 0.84f, c.y + sinf(dir) * r * 0.84f };
        Vector2 p1 = { p0.x + cosf(dir) * len * 0.35f - sinf(dir) * r * 0.18f,
                       p0.y + sinf(dir) * len * 0.35f + cosf(dir) * r * 0.18f };
        Vector2 p2 = { p0.x + cosf(dir) * len * 0.68f + sinf(dir) * r * 0.22f,
                       p0.y + sinf(dir) * len * 0.68f - cosf(dir) * r * 0.22f };
        Vector2 p3 = { p0.x + cosf(dir) * len * 0.52f - sinf(dir) * r * 0.10f,
                       p0.y + sinf(dir) * len * 0.52f + cosf(dir) * r * 0.10f };
        float ph = t * 2.2f + (float)f * 1.4f;
        p3.x += cosf(ph) * r * 0.20f;
        p3.y += sinf(ph) * r * 0.20f;

        Vector2 pts[4] = { p0, p1, p2, p3 };
        DrawSplineBezierCubic(pts, 4, fmaxf(0.8f, r * 0.035f), Fade(col, 0.50f));
        /* 末端更细的一小截，做出“鞭毛尾” */
        Vector2 q1 = { p3.x + cosf(dir) * r * 0.16f, p3.y + sinf(dir) * r * 0.16f };
        DrawLineEx(p3, q1, fmaxf(0.6f, r * 0.018f), Fade(col, 0.30f));
    }
}

/* 球菌：荚膜 → 细胞壁 → 细胞质 → 拟核 + 核糖体 + 内含物 + 菌毛 */
static void morph_coccus(Vector2 c, float r, Color col, float t, float seed, float a, bool detail)
{
    float breathe = 1.0f + 0.035f * sinf(t * 1.5f + seed);

    if (detail && r > 10.0f) {
        DrawRing(c, r * 1.10f, r * 1.16f, 0.0f, 360.0f, 72, Fade(col, 0.09f * a));
    }

    sphere(c, r * breathe, col, a);
    if (!detail) return;

    membrane(c, r * breathe, col, a);

    if (r < 9.0f) return;

    /* 细胞质斑：只做极淡的深浅变化，别压出“瞳孔” */
    for (int i = 0; i < 2; ++i) {
        float ang = seed + (float)i * 3.1f + t * 0.25f;
        Vector2 p = { c.x + cosf(ang) * r * 0.34f, c.y + sinf(ang) * r * 0.30f };
        DrawCircleGradient((int)p.x, (int)p.y, r * 0.52f,
                           Fade(COL_BG_DEEP, 0.14f * a), Fade(COL_BG_DEEP, 0.0f));
    }

    /* 拟核：一团盘绕的 DNA 环，用亮线画（暗块会读成眼球） */
    {
        Vector2 loop[10];
        for (int i = 0; i < 10; ++i) {
            float ang = (float)i * (2.0f * PI / 10.0f) + t * 0.18f + seed;
            float rr = r * (0.26f + 0.10f * sinf((float)i * 2.2f + t * 0.7f));
            loop[i] = (Vector2){ c.x + cosf(ang) * rr, c.y + sinf(ang) * rr * 0.92f };
        }
        Color dna = Fade(ui_color_mix(col, WHITE, 0.52f), 0.34f * a);
        for (int i = 0; i < 10; ++i) {
            DrawLineEx(loop[i], loop[(i + 1) % 10], fmaxf(0.8f, r * 0.030f), dna);
        }
    }

    /* 核糖体：12 颗会轻微闪烁的小点 */
    for (int i = 0; i < 12; ++i) {
        float ang = hash01((unsigned int)i * 17u + 3u) * 2.0f * PI;
        float rr = r * 0.72f * sqrtf(hash01((unsigned int)i * 23u + 7u));
        Vector2 p = { c.x + cosf(ang) * rr, c.y + sinf(ang) * rr * 0.94f };
        float tw = 0.65f + 0.35f * sinf(t * 2.4f + (float)i * 0.9f);
        DrawCircleV(p, fmaxf(0.9f, r * 0.035f), Fade(COL_TEXT, 0.26f * tw * a));
    }

    /* 内含物：3 颗柔和的储藏颗粒 */
    for (int i = 0; i < 3; ++i) {
        float ang = -t * 0.35f + seed + (float)i * 2.09f;
        Vector2 p = { c.x + cosf(ang) * r * 0.52f, c.y + sinf(ang) * r * 0.48f };
        DrawCircleGradient((int)p.x, (int)p.y, r * 0.12f,
                           Fade(ui_color_mix(col, WHITE, 0.55f), 0.42f * a),
                           Fade(ui_color_mix(col, WHITE, 0.20f), 0.0f));
    }

    /* 菌毛：一圈短毛，方向随时间轻摆 */
    for (int i = 0; i < 8; ++i) {
        float ang = (float)i * (2.0f * PI / 8.0f) + seed + sinf(t * 0.6f) * 0.10f;
        Vector2 p0 = { c.x + cosf(ang) * r * 1.00f, c.y + sinf(ang) * r * 1.00f };
        Vector2 p1 = { c.x + cosf(ang) * r * 1.20f, c.y + sinf(ang) * r * 1.20f };
        DrawLineEx(p0, p1, fmaxf(0.6f, r * 0.012f), Fade(col, 0.26f * a));
    }
}

/* 杆菌：胶囊体 + 沿轴渐变 + 拟核螺旋 + 核糖体 + 菌毛 + 双鞭毛 + 分裂沟 */
static void morph_rod(Vector2 c, float r, Color col, float t, float seed, float a, bool detail)
{
    float ang = seed * 70.0f - 35.0f + sinf(t * 0.45f + seed) * 5.0f;
    float rad = ang * DEG2RAD;
    float half = r * 0.92f;
    float thick = r * 0.54f;
    Vector2 d = { cosf(rad), sinf(rad) };
    Vector2 nd = { -d.y, d.x };

    if (detail && r > 10.0f) flagella(c, r, rad, col, t, 2);

    Color hi = ui_color_mix(col, WHITE, 0.34f);
    Color lo = ui_color_mix(col, BLACK, 0.58f);
    Color mid = ui_color_mix(col, BLACK, 0.30f);

    Vector2 e1 = { c.x - d.x * half, c.y - d.y * half };
    Vector2 e2 = { c.x + d.x * half, c.y + d.y * half };

    /* 荚膜：比菌体大一圈的暗环 */
    capsule((Vector2){ e1.x - d.x * 1.5f, e1.y - d.y * 1.5f }, half * 2.0f + 3.0f, thick + 2.0f,
            ang, Fade(ui_color_mix(col, WHITE, 0.10f), 0.09f * a));

    capsule(e1, half * 2.0f, thick, ang, Fade(mid, 0.96f * a));
    /* 受光面：贴着上侧的一条窄高光，别做成第二根菌 */
    Vector2 s1 = { e1.x + nd.x * thick * 0.44f, e1.y + nd.y * thick * 0.44f };
    capsule(s1, half * 2.0f, thick * 0.30f, ang, Fade(hi, 0.20f * a));
    /* 暗面：下侧压一条 */
    Vector2 s2 = { e1.x - nd.x * thick * 0.62f, e1.y - nd.y * thick * 0.62f };
    capsule(s2, half * 2.0f, thick * 0.34f, ang, Fade(lo, 0.40f * a));
    /* 两端各一枚小高光，收住胶囊的圆头 */
    DrawEllipse((int)(e1.x - nd.x * thick * 0.34f), (int)(e1.y - nd.y * thick * 0.34f),
                thick * 0.42f, thick * 0.22f, Fade(WHITE, 0.10f * a));
    DrawEllipse((int)(e2.x - nd.x * thick * 0.34f), (int)(e2.y - nd.y * thick * 0.34f),
                thick * 0.42f, thick * 0.22f, Fade(WHITE, 0.10f * a));

    if (!detail) return;

    /* 拟核：一条沿菌体盘绕的曲线 */
    if (r > 9.0f) {
        Vector2 p[4];
        for (int i = 0; i < 4; ++i) {
            float u = -0.62f + 0.42f * (float)i;
            float wob = sinf(t * 0.9f + (float)i * 1.6f + seed) * thick * 0.40f;
            p[i] = (Vector2){ c.x + d.x * half * u + nd.x * wob,
                              c.y + d.y * half * u + nd.y * wob };
        }
        DrawSplineBezierCubic(p, 4, fmaxf(0.9f, r * 0.055f), Fade(ui_color_mix(col, BLACK, 0.50f), 0.26f * a));
        DrawSplineBezierCubic(p, 4, fmaxf(0.4f, r * 0.022f),
                              Fade(ui_color_mix(col, WHITE, 0.55f), 0.30f * a));
    }

    /* 核糖体 */
    for (int i = 0; i < 10; ++i) {
        float u = -0.75f + 1.5f * hash01((unsigned int)i * 13u + 5u);
        float v = (hash01((unsigned int)i * 19u + 11u) - 0.5f) * 1.1f;
        Vector2 p = { c.x + d.x * half * u + nd.x * thick * v,
                      c.y + d.y * half * u + nd.y * thick * v };
        DrawCircleV(p, fmaxf(0.8f, r * 0.028f), Fade(COL_TEXT, 0.26f * a));
    }

    /* 菌毛：两侧各 4 根 */
    for (int i = 0; i < 4; ++i) {
        float u = -0.55f + 0.36f * (float)i;
        for (int side = -1; side <= 1; side += 2) {
            Vector2 p0 = { c.x + d.x * half * u + nd.x * thick * (float)side,
                           c.y + d.y * half * u + nd.y * thick * (float)side };
            Vector2 p1 = { p0.x + nd.x * thick * 0.55f * (float)side,
                           p0.y + nd.y * thick * 0.55f * (float)side };
            DrawLineEx(p0, p1, fmaxf(0.6f, r * 0.012f), Fade(col, 0.24f * a));
        }
    }

    /* 分裂沟：一道很浅的膜内陷，别用纯黑切线（会在菌体上画出一道“裂纹”） */
    {
        Color furrow = Fade(ui_color_mix(col, BLACK, 0.55f), 0.34f * a);
        DrawLineEx((Vector2){ c.x + nd.x * thick, c.y + nd.y * thick },
                   (Vector2){ c.x - nd.x * thick, c.y - nd.y * thick },
                   fmaxf(0.8f, r * 0.030f), furrow);
        DrawLineEx((Vector2){ c.x + d.x * thick * 0.20f + nd.x * thick, c.y + d.y * thick * 0.20f + nd.y * thick },
                   (Vector2){ c.x + d.x * thick * 0.20f - nd.x * thick, c.y + d.y * thick * 0.20f - nd.y * thick },
                   fmaxf(0.5f, r * 0.014f), Fade(WHITE, 0.10f * a));
    }
}

/* 真核细胞：膜 + 核（核仁/染色质）+ 内质网 + 线粒体 + 高尔基 + 液泡 + 骨架 */
static void morph_eukaryote(Vector2 c, float r, Color col, float t, float seed, float a, bool detail)
{
    float breathe = 1.0f + 0.025f * sinf(t * 1.3f + seed);
    sphere(c, r * breathe, col, a);
    if (!detail) return;

    membrane(c, r * breathe, col, a);
    if (r < 9.0f) return;

    /* 细胞骨架：几条极淡的弦 */
    for (int i = 0; i < 5; ++i) {
        float ang = seed + (float)i * 1.26f + t * 0.10f;
        Vector2 p0 = { c.x + cosf(ang) * r * 0.86f, c.y + sinf(ang) * r * 0.86f };
        Vector2 p1 = { c.x - cosf(ang) * r * 0.80f, c.y - sinf(ang) * r * 0.80f };
        DrawLineEx(p0, p1, fmaxf(0.5f, r * 0.010f), Fade(COL_TEXT, 0.07f * a));
    }

    /* 内质网：绕核的三条粗曲线 */
    for (int i = 0; i < 3; ++i) {
        float base = t * 0.35f + seed + (float)i * 2.09f;
        Vector2 p[4];
        for (int k = 0; k < 4; ++k) {
            float ang = base + (float)k * 0.55f;
            float rr = r * (0.52f + 0.06f * (float)k);
            p[k] = (Vector2){ c.x + cosf(ang) * rr, c.y + sinf(ang) * rr * 0.92f };
        }
        DrawSplineBezierCubic(p, 4, fmaxf(1.0f, r * 0.055f), Fade(ui_color_mix(col, WHITE, 0.40f), 0.30f * a));
    }

    /* 细胞核：双层核膜 + 核仁 + 染色质 */
    Vector2 n = { c.x + cosf(t * 0.35f + seed) * r * 0.14f,
                  c.y + sinf(t * 0.31f + seed) * r * 0.12f };
    float nr = r * 0.30f;
    DrawCircleV(n, nr * 1.06f, Fade(COL_BG_DEEP, 0.28f * a));
    DrawCircleGradient((int)(n.x - nr * 0.2f), (int)(n.y - nr * 0.25f), nr,
                       Fade(ui_color_mix(col, WHITE, 0.55f), 0.62f * a),
                       Fade(ui_color_mix(col, WHITE, 0.10f), 0.36f * a));
    DrawRing(n, nr * 0.96f, nr * 1.04f, 0.0f, 360.0f, 48, Fade(WHITE, 0.30f * a));
    DrawCircleV((Vector2){ n.x + nr * 0.26f, n.y - nr * 0.22f }, nr * 0.28f,
                Fade(ui_color_mix(col, WHITE, 0.70f), 0.42f * a));
    for (int i = 0; i < 4; ++i) {
        float ang = seed + (float)i * 1.6f + t * 0.2f;
        Vector2 p = { n.x + cosf(ang) * nr * 0.55f, n.y + sinf(ang) * nr * 0.50f };
        DrawCircleV(p, fmaxf(0.8f, nr * 0.13f), Fade(COL_TEXT, 0.28f * a));
    }

    /* 线粒体：三枚细长的胶囊，内部一条深色嵴（细长才像线粒体，短粗会读成小方块） */
    for (int i = 0; i < 3; ++i) {
        float ang = seed + (float)i * 2.1f + t * 0.18f;
        Vector2 p = { c.x + cosf(ang) * r * 0.58f, c.y + sinf(ang) * r * 0.54f };
        float mLen = r * 0.30f, mTh = r * 0.070f;
        float rot = ang * RAD2DEG + 62.0f;
        capsule(p, mLen, mTh, rot, Fade(ui_color_mix(ui_color_mix(col, COL_RARE, 0.45f), WHITE, 0.22f), 0.62f * a));
        Vector2 mid = { p.x + cosf(rot * DEG2RAD) * mLen * 0.5f,
                        p.y + sinf(rot * DEG2RAD) * mLen * 0.5f };
        DrawLineEx((Vector2){ mid.x - cosf(rot * DEG2RAD) * mLen * 0.34f,
                              mid.y - sinf(rot * DEG2RAD) * mLen * 0.34f },
                   (Vector2){ mid.x + cosf(rot * DEG2RAD) * mLen * 0.34f,
                              mid.y + sinf(rot * DEG2RAD) * mLen * 0.34f },
                   fmaxf(0.6f, r * 0.016f), Fade(COL_BG_DEEP, 0.50f * a));
    }

    /* 高尔基体：三条叠起来的弧 */
    for (int i = 0; i < 3; ++i) {
        float rr = r * (0.30f + 0.045f * (float)i);
        float rot = t * 8.0f + seed;
        DrawRing((Vector2){ c.x - r * 0.40f, c.y + r * 0.30f }, rr - 1.0f, rr,
                 rot, rot + 92.0f, 24, Fade(ui_color_mix(col, WHITE, 0.30f), 0.24f * a));
    }

    /* 液泡：一枚半透明的大囊，带一圈亮的液泡膜（不能画成第二个“暗核”） */
    Vector2 v = { c.x + r * 0.36f, c.y - r * 0.34f };
    DrawCircleV(v, r * 0.24f, Fade(ui_color_mix(col, COL_BG_DEEP, 0.55f), 0.42f * a));
    DrawRing(v, r * 0.225f, r * 0.250f, 0.0f, 360.0f, 44,
             Fade(ui_color_mix(col, WHITE, 0.55f), 0.34f * a));
    DrawCircleV((Vector2){ v.x - r * 0.07f, v.y - r * 0.08f }, r * 0.05f,
                Fade(WHITE, 0.22f * a));
}

/* 群体：黏质层 + 8 枚带膜细胞 + 细胞连接 + 一枚出芽的子细胞 */
static void morph_colony(Vector2 c, float r, Color col, float t, float seed, float a, bool detail)
{
    if (detail && r > 10.0f) {
        /* 黏质层：整团外围的一层淡光 */
        DrawCircleV(c, r * 1.06f, Fade(col, 0.06f * a));
        DrawRing(c, r * 1.03f, r * 1.07f, 0.0f, 360.0f, 72, Fade(col, 0.12f * a));
    }

    float ring = r * 0.55f;
    int cells = 8;
    for (int i = 0; i < cells; ++i) {
        float ang = seed + (float)i * (2.0f * PI / (float)cells) + sinf(t * 0.3f + (float)i) * 0.05f;
        float cr = r * (0.30f + 0.02f * sinf(t * 1.2f + (float)i + seed));
        Vector2 p = { c.x + cosf(ang) * ring, c.y + sinf(ang) * ring };

        if (detail) {
            /* 细胞连接：相邻细胞之间的短桥 */
            float ang2 = seed + (float)((i + 1) % cells) * (2.0f * PI / (float)cells);
            Vector2 q = { c.x + cosf(ang2) * ring, c.y + sinf(ang2) * ring };
            Vector2 mid = { (p.x + q.x) * 0.5f, (p.y + q.y) * 0.5f };
            Vector2 nrm = { mid.x - c.x, mid.y - c.y };
            float nlen = sqrtf(nrm.x * nrm.x + nrm.y * nrm.y);
            if (nlen > 0.1f) {
                nrm.x /= nlen; nrm.y /= nlen;
                DrawLineEx((Vector2){ mid.x - nrm.x * r * 0.09f, mid.y - nrm.y * r * 0.09f },
                           (Vector2){ mid.x + nrm.x * r * 0.09f, mid.y + nrm.y * r * 0.09f },
                           fmaxf(0.8f, r * 0.030f), Fade(ui_color_mix(col, WHITE, 0.3f), 0.30f * a));
            }
            DrawLineEx(c, p, fmaxf(0.5f, r * 0.012f), Fade(col, 0.16f * a));
        }

        sphere(p, cr, ui_color_mix(col, WHITE, (float)(i % 2) * 0.10f), a * 0.92f);
        if (detail && cr > 5.0f) {
            membrane(p, cr, col, a * 0.9f);
            /* 每枚细胞里一颗细胞核级别的暗点 */
            DrawCircleV((Vector2){ p.x + cosf(ang + 1.0f) * cr * 0.28f,
                                   p.y + sinf(ang + 1.0f) * cr * 0.28f },
                        cr * 0.26f, Fade(COL_BG_DEEP, 0.40f * a));
        }
    }

    /* 中央细胞 + 出芽的子细胞（贴着外缘，随时间长大再缩回） */
    sphere(c, r * 0.26f, ui_color_mix(col, WHITE, 0.15f), a * 0.95f);
    if (detail) {
        membrane(c, r * 0.26f, col, a);

        float budPhase = 0.5f + 0.5f * sinf(t * 0.75f + seed);
        float budAng = seed + 0.9f;
        float budR = r * (0.10f + 0.06f * budPhase);
        Vector2 bp = { c.x + cosf(budAng) * (r * 1.02f + budR * 0.4f),
                       c.y + sinf(budAng) * (r * 1.02f + budR * 0.4f) };
        DrawCircleV(bp, budR * 0.55f, Fade(ui_color_mix(col, WHITE, 0.2f), 0.35f * a));
        sphere(bp, budR, ui_color_mix(col, WHITE, 0.18f), a * 0.85f);
    }
}

/* 组织：紧密排列的多边形细胞，带膜、核、胞间层 */
static void morph_tissue(Vector2 c, float r, Color col, float t, float seed, float a, bool detail)
{
    float breathe = 1.0f + 0.02f * sinf(t * 1.1f + seed);
    sphere(c, r * breathe * 0.94f, ui_color_mix(col, BLACK, 0.10f), a * 0.92f);
    if (!detail) return;
    if (r < 9.0f) return;

    /* 外缘：一圈起伏的膜，读成“一片组织”而不是一个球 */
    DrawRing(c, r * 0.90f, r * 0.925f, 0.0f, 360.0f, 72, Fade(COL_TEXT, 0.12f * a));

    struct { float x, y, rad; } cells[13] = {
        { 0.0f, 0.0f, 0.26f },
        { 0.44f, 0.0f, 0.22f }, { -0.44f, 0.0f, 0.22f },
        { 0.22f, 0.40f, 0.22f }, { -0.22f, 0.40f, 0.22f },
        { 0.22f, -0.40f, 0.22f }, { -0.22f, -0.40f, 0.22f },
        { 0.66f, 0.36f, 0.16f }, { -0.66f, 0.36f, 0.16f },
        { 0.66f, -0.36f, 0.16f }, { -0.66f, -0.36f, 0.16f },
        { 0.0f, 0.66f, 0.16f }, { 0.0f, -0.66f, 0.16f },
    };

    for (int i = 0; i < 13; ++i) {
        Vector2 p = { c.x + cells[i].x * r, c.y + cells[i].y * r };
        float cr = cells[i].rad * r;
        float pulse = 1.0f + 0.06f * sinf(t * 1.4f + (float)i * 0.7f + seed);

        DrawCircleV(p, cr * 1.02f * pulse, Fade(ui_color_mix(col, BLACK, 0.45f), 0.65f * a));
        DrawCircleGradient((int)(p.x - cr * 0.15f), (int)(p.y - cr * 0.18f), cr * pulse,
                           Fade(ui_color_mix(col, WHITE, 0.20f), 0.85f * a),
                           Fade(ui_color_mix(col, BLACK, 0.30f), 0.85f * a));
        DrawRing(p, cr * 0.92f, cr * 0.96f, 0.0f, 360.0f, 28, Fade(WHITE, 0.10f * a));
        /* 细胞核 */
        DrawCircleV((Vector2){ p.x + cosf((float)i) * cr * 0.16f,
                               p.y + sinf((float)i) * cr * 0.14f },
                    cr * 0.30f, Fade(ui_color_mix(col, WHITE, 0.35f), 0.42f * a));
        DrawCircleV((Vector2){ p.x + cosf((float)i) * cr * 0.16f,
                               p.y + sinf((float)i) * cr * 0.14f },
                    cr * 0.12f, Fade(COL_TEXT, 0.30f * a));
    }

    /* 中央的内腔：一小片更暗的基质 */
    DrawCircleV(c, r * 0.12f, Fade(COL_BG_DEEP, 0.30f * a));
}

/* 共生：两种生物共享一层膜，中间是交换通道 + 游走的能量粒子 */
static void morph_symbio(Vector2 c, float r, Color col, float t, float seed, float a, bool detail)
{
    float bob = sinf(t * 0.9f + seed) * r * 0.05f;
    Vector2 p1 = { c.x - r * 0.26f, c.y - bob };
    Vector2 p2 = { c.x + r * 0.30f, c.y + bob };

    if (detail) {
        /* 共享膜：把两枚包在一起的一圈淡光 */
        DrawCircleV((Vector2){ (p1.x + p2.x) * 0.5f, c.y }, r * 0.92f, Fade(col, 0.05f * a));
        DrawRing((Vector2){ (p1.x + p2.x) * 0.5f, c.y }, r * 0.90f, r * 0.93f, 0.0f, 360.0f, 72,
                 Fade(ui_color_mix(col, COL_TEAL, 0.5f), 0.16f * a));
    }

    sphere(p1, r * 0.62f, col, a * 0.92f);
    sphere(p2, r * 0.52f, COL_TEAL, a * 0.88f);

    if (!detail) return;
    membrane(p1, r * 0.62f, col, a * 0.9f);
    membrane(p2, r * 0.52f, COL_TEAL, a * 0.9f);

    if (r < 9.0f) return;

    /* 共生界面的叠合区：椭圆高光 */
    Vector2 m = { (p1.x + p2.x) * 0.5f, c.y };
    DrawEllipse((int)m.x, (int)m.y, r * 0.20f, r * 0.42f, Fade(COL_EP, 0.10f * a));
    DrawRing(m, r * 0.15f, r * 0.165f, 0.0f, 360.0f, 40, Fade(COL_EP, 0.42f * a));

    /* 宿主里的叶绿体颗粒（绿色小胶囊） */
    for (int i = 0; i < 4; ++i) {
        float ang = seed + (float)i * 1.57f + t * 0.3f;
        Vector2 q = { p1.x + cosf(ang) * r * 0.34f, p1.y + sinf(ang) * r * 0.30f };
        capsule(q, r * 0.10f, r * 0.045f, ang * RAD2DEG, Fade(COL_BIOMASS, 0.60f * a));
    }
    /* 共生体里的细胞核 + 气泡 */
    DrawCircleV((Vector2){ p2.x - r * 0.06f, p2.y }, r * 0.13f, Fade(COL_BG_DEEP, 0.40f * a));
    DrawCircleV((Vector2){ p2.x + r * 0.14f, p2.y + r * 0.10f }, r * 0.06f, Fade(WHITE, 0.16f * a));

    /* 沿界面来回交换的粒子 */
    for (int i = 0; i < 4; ++i) {
        float ph = t * 1.6f + (float)i * 1.57f;
        float k = 0.5f + 0.5f * sinf(ph);
        Vector2 s = { m.x - r * 0.24f + k * r * 0.48f, m.y + sinf(ph * 0.7f) * r * 0.24f };
        DrawCircleV(s, r * 0.035f, Fade(COL_EP, 0.55f * a));
        ui_glow(s, r * 0.10f, COL_EP, 0.45f * a);
    }
}

/* 神经：胞体 + 树突分叉 + 带髓鞘的轴突 + 沿线奔跑的脉冲 */
static void neuron_branch(Vector2 p, float ang, float len, float w, int depth,
                          Color col, float a, float t, float seed)
{
    if (depth <= 0 || len < 2.5f) return;
    float sway = sinf(t * 0.7f + seed + (float)depth) * 0.08f;
    Vector2 q = { p.x + cosf(ang + sway) * len, p.y + sinf(ang + sway) * len };
    DrawLineEx(p, q, w, Fade(col, 0.55f * a));
    neuron_branch(q, ang + 0.42f, len * 0.62f, w * 0.72f, depth - 1, col, a, t, seed + 1.0f);
    neuron_branch(q, ang - 0.40f, len * 0.60f, w * 0.72f, depth - 1, col, a, t, seed + 2.0f);
}

static void morph_neural(Vector2 c, float r, Color col, float t, float seed, float a, bool detail)
{
    /* 胶质背景：一团极暗的柔软底 */
    DrawCircleV(c, r * 0.98f, Fade(ui_color_mix(col, COL_BG_DEEP, 0.72f), 0.42f * a));
    if (!detail) {
        sphere(c, r * 0.42f, col, a * 0.9f);
        return;
    }

    Vector2 soma = { c.x - r * 0.24f, c.y + r * 0.04f };

    /* 树突：从胞体向 5 个方向分叉，末梢最细 */
    for (int i = 0; i < 5; ++i) {
        float ang = (float)i * 1.257f + 1.2f + seed;
        neuron_branch(soma, ang, r * 0.46f, fmaxf(1.0f, r * 0.05f), 3, col, a, t, seed);
    }

    /* 轴突：一条长曲线伸向右下，外面分段套着髓鞘 */
    Vector2 a0 = { soma.x + r * 0.16f, soma.y };
    Vector2 a1 = { c.x + r * 0.30f, c.y + r * 0.34f };
    Vector2 a2 = { c.x + r * 0.84f, c.y + r * 0.36f };
    Vector2 a3 = { c.x + r * 1.02f, c.y + r * 0.02f };
    Vector2 ax[4] = { a0, a1, a2, a3 };
    DrawSplineBezierCubic(ax, 4, fmaxf(1.2f, r * 0.055f), Fade(ui_color_mix(col, WHITE, 0.25f), 0.55f * a));

    for (int i = 0; i < 5; ++i) {
        float u = 0.12f + 0.17f * (float)i;
        Vector2 p = { a0.x + (a1.x - a0.x) * u + (a2.x - a0.x) * u * u * 0.5f,
                      a0.y + (a1.y - a0.y) * u + (a2.y - a0.y) * u * u * 0.5f };
        float w = r * 0.10f;
        DrawEllipse((int)p.x, (int)p.y, w * 1.3f, w, Fade(ui_color_mix(col, COL_BG_DEEP, 0.5f), 0.30f * a));
    }

    /* 脉冲：沿轴突跑的小光点 */
    for (int i = 0; i < 3; ++i) {
        float u = fmodf(t * 0.55f + (float)i * 0.33f, 1.0f);
        Vector2 p = { a0.x + (a1.x - a0.x) * u + (a2.x - a0.x) * u * u * 0.55f
                      + (a3.x - a2.x) * u * u * u,
                      a0.y + (a1.y - a0.y) * u + (a2.y - a0.y) * u * u * 0.55f
                      + (a3.y - a2.y) * u * u * u };
        DrawCircleV(p, r * 0.045f, Fade(COL_EP, 0.85f * a));
        ui_glow(p, r * 0.16f, COL_EP, 0.55f * a);
    }

    /* 胞体：球 + 核 */
    sphere(soma, r * 0.30f, ui_color_mix(col, COL_BG_DEEP, 0.20f), a * 0.95f);
    DrawCircleV((Vector2){ soma.x + r * 0.03f, soma.y - r * 0.02f }, r * 0.14f,
                Fade(ui_color_mix(col, WHITE, 0.55f), 0.45f * a));
    DrawCircleV((Vector2){ soma.x + r * 0.03f, soma.y - r * 0.02f }, r * 0.055f,
                Fade(COL_TEXT, 0.55f * a));

    /* 远处两个小神经元，让画面有“网络”而不是“单细胞” */
    Vector2 o1 = { c.x + r * 0.52f, c.y - r * 0.52f };
    Vector2 o2 = { c.x - r * 0.62f, c.y - r * 0.46f };
    sphere(o1, r * 0.10f, col, a * 0.8f);
    sphere(o2, r * 0.08f, col, a * 0.7f);
    DrawLineEx(soma, o1, fmaxf(0.6f, r * 0.016f), Fade(col, 0.20f * a));
    DrawLineEx(soma, o2, fmaxf(0.6f, r * 0.016f), Fade(col, 0.16f * a));
}

/* 椭圆轨道的一段弧（用于行星环的“前半圈/后半圈”遮挡） */
static void orbit_arc(Vector2 c, float rx, float ry, float a0, float a1,
                      float thick, Color col, int segs)
{
    if (segs < 2) segs = 2;
    Vector2 prev = { c.x + cosf(a0) * rx, c.y + sinf(a0) * ry };
    for (int i = 1; i <= segs; ++i) {
        float ang = a0 + (a1 - a0) * (float)i / (float)segs;
        Vector2 p = { c.x + cosf(ang) * rx, c.y + sinf(ang) * ry };
        DrawLineEx(prev, p, thick, col);
        prev = p;
    }
}

/* 技术圈：被改造的行星 + 经纬网 + 城市灯网 + 洲际光弧 + 轨道站 */
static void morph_techno(Vector2 c, float r, Color col, float t, float seed, float a, bool detail)
{
    planet_body(c, r * 0.86f, col, ui_color_mix(col, (Color){ 196, 178, 138, 255 }, 0.55f), a);
    if (!detail) return;

    /* 经纬网：只画向阳那半边，暗面不该有网格 */
    for (int i = 1; i <= 3; ++i) {
        float k = (float)i / 4.0f;
        DrawEllipse((int)c.x, (int)c.y, r * 0.84f, r * 0.84f * k, Fade(COL_TEXT, 0.09f * a));
    }
    for (int i = 0; i < 4; ++i) {
        float k = (float)(i - 1.5f) / 2.0f;
        DrawEllipse((int)c.x, (int)c.y, r * 0.84f * fabsf(k), r * 0.84f, Fade(COL_TEXT, 0.07f * a));
    }

    /* 城市灯网：夜里最亮（右侧暗面），白天被大气压掉一些 */
    for (int i = 0; i < 34; ++i) {
        float ang = hash01((unsigned int)i * 13u + 5u) * 2.0f * PI;
        float rr = r * 0.78f * sqrtf(hash01((unsigned int)i * 17u + 9u));
        Vector2 p = { c.x + cosf(ang) * rr, c.y + sinf(ang) * rr * 0.9f };
        float tw = 0.45f + 0.55f * (0.5f + 0.5f * sinf(t * 2.0f + (float)i));
        float lam = 0.50f - 0.68f * ((p.x - c.x) / (r * 0.84f));
        if (lam < 0.0f) lam = 0.0f;
        if (lam > 1.0f) lam = 1.0f;
        DrawCircleV(p, r * 0.026f, Fade(COL_ACCENT, (0.20f + 0.65f * (1.0f - lam)) * tw * a));
        if ((i % 6) == 0) ui_glow(p, r * 0.06f, COL_ACCENT, 0.35f * tw);
    }

    /* 洲际光弧：几条连接城市的细弧，做出“被连起来”的文明感 */
    orbit_arc(c, r * 0.55f, r * 0.22f, -2.6f, -0.7f, fmaxf(0.7f, r * 0.012f),
              Fade(COL_ACCENT, 0.16f * a), 22);
    orbit_arc(c, r * 0.62f, r * 0.26f, 0.4f, 2.4f, fmaxf(0.7f, r * 0.012f),
              Fade(COL_ACCENT, 0.12f * a), 22);

    /* 环轨 + 一颗卫星 */
    DrawEllipse((int)c.x, (int)c.y, r * 1.16f, r * 0.32f, Fade(col, 0.10f * a));
    DrawEllipse((int)c.x, (int)c.y, r * 1.22f, r * 0.36f, Fade(col, 0.06f * a));
    float sa = t * 0.6f + seed;
    Vector2 sp = { c.x + cosf(sa) * r * 1.19f, c.y + sinf(sa) * r * 0.33f };
    DrawCircleV(sp, r * 0.045f, Fade(COL_TEXT, 0.75f * a));
    ui_glow(sp, r * 0.10f, COL_TEAL, 0.40f * a);
}

/* 文明：大陆 + 成簇的城市灯光 + 夜面暖光 + 双卫星 + 空间站 */
static void morph_civ(Vector2 c, float r, Color col, float t, float seed, float a, bool detail)
{
    (void)seed;
    planet_body(c, r * 0.84f, col, (Color){ 88, 196, 146, 255 }, a);
    if (!detail) return;

    /* 灯光簇：在 4 个“城市群”附近随机撒点，而不是均匀铺满 */
    for (int g = 0; g < 4; ++g) {
        float ga = hash01((unsigned int)g * 61u + 3u) * 2.0f * PI;
        float gd = r * (0.24f + 0.42f * hash01((unsigned int)g * 67u + 5u));
        Vector2 gc = { c.x + cosf(ga) * gd, c.y + sinf(ga) * gd * 0.86f };
        for (int i = 0; i < 9; ++i) {
            float ang = hash01((unsigned int)(g * 9 + i) * 19u + 7u) * 2.0f * PI;
            float rr = r * 0.20f * sqrtf(hash01((unsigned int)(g * 9 + i) * 23u + 11u));
            Vector2 p = { gc.x + cosf(ang) * rr, gc.y + sinf(ang) * rr * 0.7f };
            float tw = 0.5f + 0.5f * sinf(t * 1.6f + (float)i * 0.7f + (float)g);
            float lam = 0.50f - 0.68f * ((p.x - c.x) / (r * 0.84f));
            if (lam < 0.0f) lam = 0.0f;
            if (lam > 1.0f) lam = 1.0f;
            DrawCircleV(p, r * 0.022f, Fade(COL_ACCENT, (0.22f + 0.65f * (1.0f - lam)) * (0.6f + 0.4f * tw) * a));
        }
        ui_glow(gc, r * 0.30f, COL_ACCENT, 0.10f * a);
    }

    /* 夜面边缘的城市余晖 */
    DrawRing(c, r * 0.70f, r * 0.82f, 30.0f, 150.0f, 48, Fade(COL_ACCENT, 0.06f * a));

    /* 两圈轨道 + 卫星，前段画在星球前面，形成遮挡 */
    orbit_arc(c, r * 1.08f, r * 0.36f, 0.0f, PI, fmaxf(1.0f, r * 0.018f), Fade(COL_TEAL, 0.30f * a), 30);
    orbit_arc(c, r * 1.08f, r * 0.36f, PI, 2.0f * PI, fmaxf(1.0f, r * 0.018f), Fade(COL_TEAL, 0.16f * a), 30);
    orbit_arc(c, r * 1.24f, r * 0.44f, 0.0f, PI, fmaxf(0.8f, r * 0.012f), Fade(COL_TEAL, 0.18f * a), 30);

    for (int i = 0; i < 2; ++i) {
        float ang = t * (0.55f + 0.3f * (float)i) + (float)i * 2.4f;
        Vector2 p = { c.x + cosf(ang) * r * 1.10f, c.y + sinf(ang) * r * 0.37f };
        float front = sinf(ang) > 0.0f;   /* 下半圈 = 在星球前面 */
        Color cc = front ? COL_TEXT : Fade(COL_TEXT, 0.25f);
        DrawCircleV(p, r * 0.050f, Fade(cc, 0.80f * a));
        ui_glow(p, r * 0.10f, COL_TEAL, 0.25f * a);
    }
}

/* 太空时代：带环行星（环分前后半圈做遮挡）+ 两颗卫星 + 星尘 + 飞船 */
static void morph_space(Vector2 c, float r, Color col, float t, float seed, float a, bool detail)
{
    (void)seed;
    float pr = r * 0.70f;

    if (detail) {
        /* 后半环：上半圈，先画，被星球挡住 */
        orbit_arc(c, r * 1.34f, r * 0.30f, PI, 2.0f * PI, fmaxf(1.2f, r * 0.05f),
                  Fade(ui_color_mix(COL_ACCENT, col, 0.4f), 0.20f * a), 40);
        orbit_arc(c, r * 1.12f, r * 0.24f, PI, 2.0f * PI, fmaxf(0.8f, r * 0.03f),
                  Fade(ui_color_mix(COL_ACCENT, col, 0.4f), 0.14f * a), 36);
    }

    planet_body(c, pr, col, (Color){ 208, 230, 244, 255 }, a);

    if (!detail) return;

    /* 前半环：下半圈，压在星球上；靠左的一段更亮，像是被恒星照亮 */
    orbit_arc(c, r * 1.12f, r * 0.24f, 0.0f, PI, fmaxf(0.8f, r * 0.03f),
              Fade(ui_color_mix(COL_ACCENT, col, 0.4f), 0.32f * a), 36);
    orbit_arc(c, r * 1.34f, r * 0.30f, 0.0f, PI, fmaxf(1.2f, r * 0.05f),
              Fade(ui_color_mix(COL_ACCENT, col, 0.35f), 0.36f * a), 40);
    orbit_arc(c, r * 1.34f, r * 0.30f, 2.5f, 3.5f, fmaxf(1.4f, r * 0.055f),
              Fade(COL_ACCENT, 0.55f * a), 12);

    /* 卫星 */
    float m1 = t * 0.5f;
    Vector2 p1 = { c.x + cosf(m1) * r * 1.42f, c.y + sinf(m1) * r * 0.34f };
    sphere(p1, r * 0.10f, (Color){ 210, 214, 226, 255 }, a * 0.9f);
    float m2 = -t * 0.32f + 1.2f;
    Vector2 p2 = { c.x + cosf(m2) * r * 1.08f, c.y + sinf(m2) * r * 0.62f };
    sphere(p2, r * 0.055f, (Color){ 196, 202, 220, 255 }, a * 0.75f);

    /* 远处的飞船：一个小三角 + 尾迹 */
    float ship = t * 0.9f;
    Vector2 sp = { c.x + cosf(ship) * r * 1.16f, c.y + sinf(ship) * r * 0.44f };
    Vector2 tail = { sp.x - cosf(ship) * r * 0.12f, sp.y - sinf(ship) * r * 0.05f };
    DrawLineEx(tail, sp, fmaxf(0.8f, r * 0.02f), Fade(COL_TEAL, 0.35f * a));
    DrawTriangle(sp, (Vector2){ sp.x - r * 0.03f, sp.y - r * 0.02f },
                 (Vector2){ sp.x - r * 0.03f, sp.y + r * 0.02f }, Fade(COL_TEXT, 0.8f * a));

    /* 星尘 */
    for (int i = 0; i < 8; ++i) {
        float ang = hash01((unsigned int)i * 31u + 7u) * 2.0f * PI;
        float rr = r * (1.0f + 0.5f * hash01((unsigned int)i * 37u + 3u));
        Vector2 p = { c.x + cosf(ang) * rr, c.y + sinf(ang) * rr * 0.7f };
        float tw = 0.3f + 0.7f * (0.5f + 0.5f * sinf(t * 1.8f + (float)i));
        DrawCircleV(p, fmaxf(0.6f, r * 0.012f), Fade(COL_TEXT, 0.45f * tw * a));
    }
}

/* 奇点：吸积盘（前后分层）+ 光子环 + 多普勒热点 + 双极喷流 + 坠落粒子 */
static void morph_singularity(Vector2 c, float r, Color col, float t, float seed, float a, bool detail)
{
    float rx = r * 1.05f, ry = r * 0.32f;
    float er = r * 0.62f;

    if (detail) {
        /* 后半盘：10 层同心带，层距小于线宽 → 叠成一片连续辉光，而不是“铁丝圈” */
        for (int i = 0; i < 10; ++i) {
            float k = 1.0f + (float)i * 0.062f;
            float al = (0.13f - 0.010f * (float)i) * a;
            orbit_arc(c, rx * k, ry * k, PI, 2.0f * PI, fmaxf(2.0f, r * 0.092f),
                      Fade(ui_color_mix(COL_RARE, COL_VIOLET, 0.30f + 0.05f * (float)i), al), 44);
        }
        /* 两极喷流：细长的锥形，越远越淡 */
        for (int dir = -1; dir <= 1; dir += 2) {
            Vector2 b0 = { c.x - r * 0.05f, c.y + (float)dir * er * 0.55f };
            Vector2 b1 = { c.x + r * 0.05f, c.y + (float)dir * er * 0.55f };
            Vector2 tip = { c.x + sinf(t * 0.6f) * r * 0.06f, c.y + (float)dir * r * 1.35f };
            DrawTriangle(b0, b1, tip, Fade(COL_TEAL, 0.10f * a));
            DrawLineEx(b0, tip, fmaxf(0.7f, r * 0.012f), Fade(COL_TEAL, 0.18f * a));
            ui_glow((Vector2){ c.x, c.y + (float)dir * r * 0.9f }, r * 0.30f, COL_TEAL, 0.18f * a);
        }
    }

    /* 事件视界：纯黑球 */
    DrawCircleV(c, er, Fade(BLACK, 1.0f));
    DrawCircleGradient((int)c.x, (int)c.y, er,
                       Fade(BLACK, 1.0f), Fade(COL_BG_DEEP, 0.98f * a));

    /* 光子环：贴着视界的一圈极亮细线 */
    DrawRing(c, er - 1.0f, er + 2.5f, 0.0f, 360.0f, 96, Fade(COL_EP, 0.75f * a));
    DrawRing(c, er + 2.5f, er + 7.0f, 0.0f, 360.0f, 96, Fade(COL_ACCENT, 0.22f * a));
    ui_glow(c, er * 1.35f, COL_ACCENT, 0.25f * a);

    if (!detail) return;

    /* 前半盘：同样叠成连续带，下半圈更亮更暖，左侧多普勒增亮 */
    for (int i = 0; i < 10; ++i) {
        float k = 1.0f + (float)i * 0.062f;
        float al = (0.38f - 0.030f * (float)i) * a;
        orbit_arc(c, rx * k, ry * k, 0.0f, PI, fmaxf(2.0f, r * 0.092f),
                  Fade(ui_color_mix(COL_ACCENT, COL_RARE, (float)i / 10.0f * 0.85f), al), 44);
    }
    /* 内侧炽热的薄盘：贴着光子环，最亮的一圈 */
    orbit_arc(c, rx * 0.94f, ry * 0.94f, 0.0f, PI, fmaxf(1.2f, r * 0.030f), Fade(COL_EP, 0.55f * a), 44);
    orbit_arc(c, rx * 0.94f, ry * 0.94f, PI, 2.0f * PI, fmaxf(1.2f, r * 0.030f), Fade(COL_EP, 0.28f * a), 44);
    ui_glow((Vector2){ c.x - rx * 0.75f, c.y + ry * 0.55f }, r * 0.42f, COL_RARE, 0.30f * a);

    /* 坠落粒子：沿螺旋向内的几个亮点 */
    for (int i = 0; i < 6; ++i) {
        float u = fmodf(t * 0.35f + (float)i * 0.17f, 1.0f);
        float ang = t * 1.2f + (float)i * 1.05f;
        float rr = rx * (0.35f + 0.9f * (1.0f - u));
        Vector2 p = { c.x + cosf(ang) * rr, c.y + sinf(ang) * rr * 0.30f };
        DrawCircleV(p, fmaxf(0.8f, r * 0.022f), Fade(COL_EP, 0.75f * u * a));
    }
}

/* 超越：四层曼陀罗（外环/花瓣/反向射线/中心星芒）+ 环绕的光点 */
static void morph_transcend(Vector2 c, float r, Color col, float t, float seed, float a, bool detail)
{
    /* 外层柔光 */
    ui_glow(c, r * 1.35f, col, 0.30f * a);

    for (int i = 0; i < 4; ++i) {
        float rr = r * (0.52f + 0.16f * (float)i) + sinf(t * 0.8f + (float)i) * r * 0.012f;
        DrawRing(c, rr - 1.2f, rr + 1.2f, 0.0f, 360.0f, 88,
                 Fade(ui_color_mix(col, WHITE, 0.15f * (float)i), (0.48f - 0.09f * (float)i) * a));
    }

    if (!detail) {
        DrawCircleV(c, r * 0.16f, Fade(COL_TEXT, 0.85f * a));
        return;
    }

    /* 花瓣：16 片薄椭圆，绕成两圈、反向旋转 */
    for (int ring = 0; ring < 2; ++ring) {
        int n = 16 - ring * 8;
        float dir = ring == 0 ? 1.0f : -1.0f;
        float base = t * 0.10f * dir + seed;
        float rr = r * (0.60f + 0.24f * (float)ring);
        for (int i = 0; i < n; ++i) {
            float ang = base + (float)i * (2.0f * PI / (float)n);
            Vector2 p = { c.x + cosf(ang) * rr, c.y + sinf(ang) * rr };
            DrawEllipse((int)p.x, (int)p.y, r * 0.10f, r * 0.035f,
                        Fade(ui_color_mix(col, COL_EP, 0.35f), 0.18f * a));
        }
    }

    /* 射线：长短交替，末梢带光点 */
    for (int i = 0; i < 16; ++i) {
        float ang = (float)i * (2.0f * PI / 16.0f) + t * 0.08f + seed;
        float inner = r * 0.42f;
        float outer = r * ((i % 4 == 0) ? 0.98f : 0.74f);
        Vector2 p0 = { c.x + cosf(ang) * inner, c.y + sinf(ang) * inner };
        Vector2 p1 = { c.x + cosf(ang) * outer, c.y + sinf(ang) * outer };
        DrawLineEx(p0, p1, (i % 4 == 0) ? 1.6f : 1.0f, Fade(col, (i % 4 == 0 ? 0.42f : 0.24f) * a));
        if (i % 4 == 0) DrawCircleV(p1, r * 0.028f, Fade(COL_TEXT, 0.60f * a));
    }

    /* 中心：六芒星 + 白核 */
    for (int i = 0; i < 3; ++i) {
        float ang = (float)i * (PI / 3.0f) + t * 0.05f;
        Vector2 d = { cosf(ang), sinf(ang) };
        DrawLineEx((Vector2){ c.x - d.x * r * 0.30f, c.y - d.y * r * 0.30f },
                   (Vector2){ c.x + d.x * r * 0.30f, c.y + d.y * r * 0.30f },
                   fmaxf(1.0f, r * 0.02f), Fade(COL_TEXT, 0.35f * a));
    }
    ui_glow(c, r * 0.45f, COL_EP, 0.85f * a);
    DrawCircleV(c, r * 0.14f, Fade(COL_TEXT, 0.95f * a));

    /* 环绕的光点 */
    for (int i = 0; i < 6; ++i) {
        float ang = t * 0.45f + (float)i * 1.05f;
        Vector2 p = { c.x + cosf(ang) * r * 1.12f, c.y + sinf(ang) * r * 0.55f };
        DrawCircleV(p, r * 0.026f, Fade(COL_EP, 0.55f * a));
    }
}

static void draw_organism(Vector2 c, float r, int species_id, int stage, Color accent,
                          float t, bool lit, bool detail, float a)
{
    Color col = lit ? accent : COL_GHOST;
    if (!lit) detail = false;
    if (a <= 0.0f) return;

    float seed = (float)(species_id % 7) * 0.41f + (float)(stage % 3) * 0.7f;

    if (!lit) {
        /* 未发现的物种只留一个空壳剪影：不泄露真实形态，也不让行星/黑洞用自己的固有色“透”出来 */
        DrawCircleV(c, r * 0.90f, Fade(COL_GHOST, 0.05f * a));
        DrawRing(c, r * 0.86f, r * 0.93f, 0.0f, 360.0f, 48, Fade(COL_GHOST, 0.20f * a));
        DrawCircleV(c, r * 0.14f, Fade(COL_GHOST, 0.10f * a));
        return;
    }

    if (lit && a > 0.25f) ui_glow(c, r * 1.55f, col, 0.16f * a);

    switch (morph_class(stage)) {
    case MORPH_COCCUS:      morph_coccus(c, r, col, t, seed, a, detail); break;
    case MORPH_ROD:         morph_rod(c, r, col, t, seed, a, detail); break;
    case MORPH_EUKARYOTE:   morph_eukaryote(c, r, col, t, seed, a, detail); break;
    case MORPH_COLONY:      morph_colony(c, r, col, t, seed, a, detail); break;
    case MORPH_TISSUE:      morph_tissue(c, r, col, t, seed, a, detail); break;
    case MORPH_SYMBIO:      morph_symbio(c, r, col, t, seed, a, detail); break;
    case MORPH_NEURAL:      morph_neural(c, r, col, t, seed, a, detail); break;
    case MORPH_TECHNO:      morph_techno(c, r, col, t, seed, a, detail); break;
    case MORPH_CIV:         morph_civ(c, r, col, t, seed, a, detail); break;
    case MORPH_SPACE:       morph_space(c, r, col, t, seed, a, detail); break;
    case MORPH_SINGULARITY: morph_singularity(c, r, col, t, seed, a, detail); break;
    default:                morph_transcend(c, r, col, t, seed, a, detail); break;
    }
}

/* ================= 图标 =================
 * 一套程序化图标：货币 4 种、升级 3 种、动作 4 种、界面 3 种。
 * 线宽 ≥1，全部走跟随 zoom 的填充原语。 */

enum {
    ICON_ATP = 0,
    ICON_BIOMASS,
    ICON_DNA,
    ICON_EP,
    ICON_META,
    ICON_REPRO,
    ICON_GENE,
    ICON_MUT,
    ICON_BRANCH,
    ICON_END,
    ICON_TREE,
    ICON_CODEX,
    ICON_ENV,
    ICON_POP,
    ICON_STAGE,
    ICON_ACH
};

static void draw_icon(int kind, Vector2 c, float r, Color col, float t)
{
    switch (kind) {
    case ICON_ATP: {
        /* 四芒能量星 */
        for (int i = 0; i < 4; ++i) {
            float ang = (float)i * PI * 0.5f + t * 0.25f;
            Vector2 p = { c.x + cosf(ang) * r * 0.92f, c.y + sinf(ang) * r * 0.92f };
            DrawTriangle((Vector2){ c.x, c.y }, p,
                         (Vector2){ c.x + cosf(ang + 0.35f) * r * 0.34f,
                                    c.y + sinf(ang + 0.35f) * r * 0.34f }, Fade(col, 0.55f));
        }
        DrawCircleV(c, r * 0.30f, Fade(col, 0.95f));
        DrawCircleV(c, r * 0.14f, Fade(COL_TEXT, 0.65f));
        break;
    }
    case ICON_BIOMASS: {
        /* 叶片：水滴形 + 叶脉 */
        DrawCircleV((Vector2){ c.x, c.y + r * 0.20f }, r * 0.52f, Fade(col, 0.85f));
        DrawTriangle((Vector2){ c.x - r * 0.46f, c.y + r * 0.18f },
                     (Vector2){ c.x + r * 0.46f, c.y + r * 0.18f },
                     (Vector2){ c.x + r * 0.04f, c.y - r * 0.80f }, Fade(col, 0.85f));
        DrawLineEx((Vector2){ c.x, c.y - r * 0.62f }, (Vector2){ c.x, c.y + r * 0.55f },
                   1.0f, Fade(COL_TEXT, 0.30f));
        break;
    }
    case ICON_DNA: {
        for (int s = 0; s < 2; ++s) {
            float ph = (float)s * PI;
            for (int i = 0; i < 6; ++i) {
                float y0 = c.y - r * 0.72f + (float)i * r * 0.28f;
                float y1 = y0 + r * 0.28f;
                Vector2 p0 = { c.x + sinf(y0 * 0.55f + t * 1.4f + ph) * r * 0.42f, y0 };
                Vector2 p1 = { c.x + sinf(y1 * 0.55f + t * 1.4f + ph) * r * 0.42f, y1 };
                DrawLineEx(p0, p1, 1.4f, Fade(col, 0.85f));
            }
        }
        for (int i = 0; i < 3; ++i) {
            float y = c.y - r * 0.40f + (float)i * r * 0.40f;
            float dx = sinf(y * 0.55f + t * 1.4f) * r * 0.42f;
            DrawLineEx((Vector2){ c.x - dx, y }, (Vector2){ c.x + dx, y }, 1.0f,
                       Fade(COL_TEXT, 0.24f));
        }
        break;
    }
    case ICON_EP: {
        /* 晶体：菱形 + 内部刻面 */
        Vector2 up = { c.x, c.y - r * 0.86f };
        Vector2 dn = { c.x, c.y + r * 0.86f };
        Vector2 lf = { c.x - r * 0.56f, c.y };
        Vector2 rt = { c.x + r * 0.56f, c.y };
        DrawTriangle(up, lf, dn, Fade(col, 0.80f));
        DrawTriangle(up, dn, rt, Fade(ui_color_mix(col, WHITE, 0.30f), 0.80f));
        DrawLineEx(up, dn, 1.0f, Fade(COL_TEXT, 0.40f));
        DrawLineEx(lf, rt, 1.0f, Fade(COL_TEXT, 0.22f));
        break;
    }
    case ICON_META: {
        DrawCircleV(c, r * 0.30f, Fade(col, 0.9f));
        for (int i = 0; i < 3; ++i) {
            float a = t * 0.9f + (float)i * 2.09f;
            DrawCircleV((Vector2){ c.x + cosf(a) * r * 0.64f, c.y + sinf(a) * r * 0.64f },
                        r * 0.14f, Fade(col, 0.85f));
        }
        break;
    }
    case ICON_REPRO: {
        DrawCircleGradient((int)(c.x - r * 0.34f), (int)c.y, r * 0.44f, Fade(col, 0.9f),
                           Fade(ui_color_mix(col, BLACK, 0.4f), 0.9f));
        DrawCircleGradient((int)(c.x + r * 0.34f), (int)c.y, r * 0.44f, Fade(col, 0.9f),
                           Fade(ui_color_mix(col, BLACK, 0.4f), 0.9f));
        DrawLineEx((Vector2){ c.x, c.y - r * 0.46f }, (Vector2){ c.x, c.y + r * 0.46f },
                   1.0f, Fade(COL_BG_DEEP, 0.5f));
        break;
    }
    case ICON_GENE: {
        DrawEllipse((int)c.x, (int)c.y, r * 0.42f, r * 0.78f, Fade(col, 0.16f));
        for (int s = 0; s < 2; ++s) {
            float ph = (float)s * PI;
            for (int i = 0; i < 5; ++i) {
                float y0 = c.y - r * 0.66f + (float)i * r * 0.33f;
                Vector2 p0 = { c.x + sinf(y0 * 0.7f + t * 1.6f + ph) * r * 0.30f, y0 };
                Vector2 p1 = { c.x + sinf((y0 + r * 0.33f) * 0.7f + t * 1.6f + ph) * r * 0.30f,
                               y0 + r * 0.33f };
                DrawLineEx(p0, p1, 1.6f, Fade(col, 0.9f));
            }
        }
        break;
    }
    case ICON_MUT: {
        DrawCircleV(c, r * 0.22f, Fade(col, 0.9f));
        for (int i = 0; i < 6; ++i) {
            float a = (float)i * (PI / 3.0f) + t * 0.35f;
            Vector2 p0 = { c.x + cosf(a) * r * 0.30f, c.y + sinf(a) * r * 0.30f };
            Vector2 p1 = { c.x + cosf(a) * r * 0.78f, c.y + sinf(a) * r * 0.78f };
            DrawLineEx(p0, p1, 1.2f, Fade(col, 0.70f));
            DrawCircleV(p1, r * 0.10f, Fade(col, 0.90f));
        }
        break;
    }
    case ICON_BRANCH: {
        Vector2 stem = { c.x, c.y + r * 0.70f };
        Vector2 mid = { c.x, c.y - r * 0.10f };
        DrawLineEx(stem, mid, 1.6f, Fade(col, 0.85f));
        for (int s = -1; s <= 1; s += 2) {
            Vector2 tip = { c.x + (float)s * r * 0.52f, c.y - r * 0.62f };
            DrawLineEx(mid, tip, 1.2f, Fade(col, 0.70f));
            DrawCircleV(tip, r * 0.16f, Fade(col, 0.90f));
        }
        break;
    }
    case ICON_END: {
        DrawRing(c, r * 0.62f, r * 0.68f, 0.0f, 360.0f, 64, Fade(col, 0.55f));
        for (int i = 0; i < 6; ++i) {
            float a = (float)i * (PI / 3.0f) + t * 0.2f;
            DrawLineEx((Vector2){ c.x + cosf(a) * r * 0.30f, c.y + sinf(a) * r * 0.30f },
                       (Vector2){ c.x + cosf(a) * r * 1.0f, c.y + sinf(a) * r * 1.0f },
                       (i % 3 == 0) ? 1.6f : 1.0f, Fade(col, (i % 3 == 0) ? 0.75f : 0.35f));
        }
        DrawCircleV(c, r * 0.16f, Fade(COL_TEXT, 0.85f));
        break;
    }
    case ICON_TREE: {
        Vector2 o = { c.x, c.y + r * 0.62f };
        DrawCircleV(o, r * 0.16f, Fade(col, 0.9f));
        Vector2 a = { c.x - r * 0.52f, c.y - r * 0.24f };
        Vector2 b = { c.x + r * 0.52f, c.y - r * 0.24f };
        DrawLineEx(o, a, 1.2f, Fade(col, 0.55f));
        DrawLineEx(o, b, 1.2f, Fade(col, 0.55f));
        DrawCircleV(a, r * 0.12f, Fade(col, 0.9f));
        DrawCircleV(b, r * 0.12f, Fade(col, 0.9f));
        Vector2 c1 = { c.x, c.y - r * 0.70f };
        DrawLineEx(a, c1, 1.0f, Fade(col, 0.40f));
        DrawLineEx(b, c1, 1.0f, Fade(col, 0.40f));
        DrawCircleV(c1, r * 0.14f, Fade(COL_EP, 0.9f));
        break;
    }
    case ICON_CODEX: {
        DrawRectangleRounded((Rectangle){ c.x - r * 0.72f, c.y - r * 0.60f,
                                          r * 1.44f, r * 1.20f }, 0.18f, 6, Fade(col, 0.20f));
        DrawRectangleRoundedLinesEx((Rectangle){ c.x - r * 0.72f, c.y - r * 0.60f,
                                                 r * 1.44f, r * 1.20f }, 0.18f, 6, 1.0f,
                                    Fade(col, 0.55f));
        DrawLineEx((Vector2){ c.x, c.y - r * 0.58f }, (Vector2){ c.x, c.y + r * 0.58f },
                   1.0f, Fade(col, 0.55f));
        for (int i = 0; i < 3; ++i) {
            float y = c.y - r * 0.30f + (float)i * r * 0.28f;
            DrawLineEx((Vector2){ c.x - r * 0.56f, y }, (Vector2){ c.x - r * 0.14f, y },
                       1.0f, Fade(COL_TEXT, 0.25f));
            DrawLineEx((Vector2){ c.x + r * 0.14f, y }, (Vector2){ c.x + r * 0.56f, y },
                       1.0f, Fade(COL_TEXT, 0.25f));
        }
        break;
    }
    case ICON_ENV: {
        for (int i = 0; i < 3; ++i) {
            float y = c.y - r * 0.36f + (float)i * r * 0.36f;
            Vector2 prev = { c.x - r * 0.70f, y };
            for (int k = 1; k <= 4; ++k) {
                float x = c.x - r * 0.70f + (float)k * r * 0.35f;
                Vector2 p = { x, y + sinf((float)k * 1.4f + t * 1.8f + (float)i) * r * 0.12f };
                DrawLineEx(prev, p, 1.2f, Fade(col, 0.55f));
                prev = p;
            }
        }
        break;
    }
    case ICON_POP: {
        for (int i = 0; i < 5; ++i) {
            float a = (float)i * 1.257f + t * 0.2f;
            DrawCircleV((Vector2){ c.x + cosf(a) * r * 0.44f, c.y + sinf(a) * r * 0.44f },
                        r * 0.20f, Fade(col, 0.55f));
        }
        DrawCircleV(c, r * 0.24f, Fade(col, 0.85f));
        break;
    }
    case ICON_STAGE: {
        for (int i = 0; i < 3; ++i) {
            float d = r * (0.22f + (float)i * 0.26f);
            DrawRing(c, d - 0.7f, d + 0.7f, 0.0f, 360.0f, 48,
                     Fade(col, (i == 2) ? 0.55f : 0.18f));
        }
        break;
    }
    default: {
        /* 成就：奖章 */
        DrawRing(c, r * 0.50f, r * 0.62f, 0.0f, 360.0f, 48, Fade(col, 0.55f));
        DrawTriangle((Vector2){ c.x - r * 0.34f, c.y + r * 0.30f },
                     (Vector2){ c.x + r * 0.34f, c.y + r * 0.30f },
                     (Vector2){ c.x, c.y + r * 0.82f }, Fade(col, 0.35f));
        DrawCircleV(c, r * 0.20f, Fade(COL_EP, 0.85f));
        break;
    }
    }
}

/* 图标徽章：圆底 + 图标。所有卡片复用，让界面有统一的“符号语言”。 */
static void icon_badge(Rectangle r, int kind, Color col, float t)
{
    ui_glow((Vector2){ r.x + r.width * 0.5f, r.y + r.height * 0.5f }, r.width * 0.85f, col, 0.30f);
    ui_panel(r, 10.0f, ui_color_mix(COL_PANEL_HI, col, 0.12f), Fade(col, 0.35f));
    Vector2 c = { r.x + r.width * 0.5f, r.y + r.height * 0.5f };
    draw_icon(kind, c, r.height * 0.36f, col, t);
}

/* ================= 卡片 ================= */

/* 标准卡片：深蓝玻璃 + 左侧色轨 + 标题行。 */
static void card_frame(Rectangle r, const char *title, const char *right_text, Color accent)
{
    ui_panel(r, R_CARD, COL_PANEL, Fade(COL_LINE, 0.55f));
    DrawRectangleRounded((Rectangle){ r.x + 6.0f, r.y + 10.0f, 3.0f, r.height - 20.0f },
                         1.0f, 4, Fade(accent, 0.85f));
    if (title && title[0]) ui_text_strong(title, r.x + S_PAD, r.y + 10.0f, F_BODY, COL_DIM);
    if (right_text && right_text[0]) {
        ui_text_right(right_text, r.x + r.width - S_PAD, r.y + 11.0f, F_CAPTION, COL_FAINT);
    }
}

/* ================= 顶部资源条 ================= */

/* 把数值拆成“大数字 + 小单位”：8.42 / 万。顶栏胶囊用这个排版，
 * 数字可以放心放大到 30px 而不会把单位挤成 '?' 或被右对齐的产速压住。 */
static void split_num(double v, char *num, int n, char *suf, int sn)
{
    double a = fabs(v);
    suf[0] = 0;
    if (a >= 1.0e12) { snprintf(num, (size_t)n, "%.2f", v / 1.0e12); snprintf(suf, (size_t)sn, "万亿"); }
    else if (a >= 1.0e8) { snprintf(num, (size_t)n, "%.2f", v / 1.0e8); snprintf(suf, (size_t)sn, "亿"); }
    else if (a >= 1.0e4) { snprintf(num, (size_t)n, "%.2f", v / 1.0e4); snprintf(suf, (size_t)sn, "万"); }
    else if (a >= 10.0) { snprintf(num, (size_t)n, "%.0f", v); }
    else { snprintf(num, (size_t)n, "%.1f", v); }
}

static void draw_resource_chip(Rectangle p, const char *label, double value,
                               double rate, Color col, int kind, bool has_rate)
{
    ui_panel(p, R_CARD, COL_PANEL, Fade(col, 0.22f));
    /* 货币徽章 */
    Rectangle badge = { p.x + 9.0f, p.y + p.height * 0.5f - 18.0f, 36.0f, 36.0f };
    ui_glow((Vector2){ badge.x + 18.0f, badge.y + 18.0f }, 24.0f, col, 0.35f);
    ui_panel(badge, 9.0f, ui_color_mix(COL_PANEL_HI, col, 0.10f), Fade(col, 0.35f));
    draw_icon(kind, (Vector2){ badge.x + 18.0f, badge.y + 18.0f }, 12.0f, col, g_ui.time);

    ui_text(label, p.x + 52.0f, p.y + 5.0f, F_CAPTION, Fade(col, 0.85f));

    if (has_rate) {
        char ratebuf[32];
        ui_fmt_rate(rate, ratebuf, (int)sizeof(ratebuf));
        ui_text_right(ratebuf, p.x + p.width - 9.0f, p.y + 6.0f, F_CAPTION,
                      rate > 0.0 ? COL_GOOD : COL_FAINT);
    } else {
        ui_text_right("永久", p.x + p.width - 9.0f, p.y + 6.0f, F_CAPTION, COL_FAINT);
    }

    char num[24], suf[8];
    split_num(value, num, (int)sizeof(num), suf, (int)sizeof(suf));
    float nw = ui_text_w(num, 30.0f);
    ui_text_strong(num, p.x + 51.0f, p.y + 20.0f, 30.0f, COL_TEXT);
    if (suf[0]) ui_text(suf, p.x + 53.0f + nw, p.y + 33.0f, 14.0f, Fade(col, 0.95f));
}

static void draw_top_button(UiButton *b, int icon, const char *label, bool hot)
{
    float hv = btn_hover_step(b, hot);
    Rectangle r = b->rect;
    r.y -= hv * 1.5f;

    ui_panel(r, R_BTN, ui_color_mix(COL_PANEL_HI, COL_PANEL_HOT, hv),
             ui_color_mix(Fade(COL_ACCENT, 0.24f), COL_ACCENT, hv));
    if (hv > 0.02f) ui_halo(r, R_BTN, COL_ACCENT, 0.10f * hv);

    /* 三个入口要挤进 230px：窄按钮自动降一档字号、图标与文字各自收边，
     * 宽按钮仍是原来的 26/44 内边距，观感不变。 */
    bool compact = r.width < 90.0f;
    float fs = compact ? F_CAPTION : F_LABEL;
    float icon_r = compact ? 11.0f : 14.0f;
    float icon_x = r.x + (compact ? 15.0f : 26.0f);
    float text_x = r.x + (compact ? 30.0f : 44.0f);

    Vector2 ic = { icon_x, r.y + r.height * 0.5f };
    draw_icon(icon, ic, icon_r, COL_ACCENT, g_ui.time);
    ui_text_strong(label, text_x, r.y + (r.height - fs) * 0.5f - 1.0f, fs, COL_TEXT);
}

static void draw_top_bar(const GameState *game)
{
    /* 顶栏：透明玻璃，让星云在上面透出来一层 */
    DrawRectangle(0, 0, UI_W, 76, Fade(COL_CHROME, 0.92f));
    DrawRectangleGradientV(0, 0, UI_W, 76, Fade(COL_BG_LIFT, 0.25f), Fade(COL_BG_DEEP, 0.0f));
    DrawLineEx((Vector2){ 0, 75.5f }, (Vector2){ UI_W, 75.5f }, 1.0f, Fade(COL_ACCENT, 0.18f));

    double rates[3];
    ua_total_rates(game, rates);

    struct {
        const char *label;
        double value;
        double rate;
        Color color;
        int icon;
        bool has_rate;
    } chips[4] = {
        { "ATP",    game->run.atp,     rates[0], COL_ATP,     ICON_ATP,     true  },
        { "生物量", game->run.biomass, rates[1], COL_BIOMASS, ICON_BIOMASS, true  },
        { "DNA",    game->run.dna,     rates[2], COL_DNA,     ICON_DNA,     true  },
        { "EP",     (double)game->legacy.evolution_points, 0, COL_EP, ICON_EP, false },
    };

    for (int i = 0; i < 4; ++i) {
        Rectangle p = { 16.0f + (float)i * 196.0f, 10.0f, 188.0f, 56.0f };
        draw_resource_chip(p, chips[i].label, chips[i].value, chips[i].rate,
                           chips[i].color, chips[i].icon, chips[i].has_rate);
    }

    /* 纪元牌 */
    Rectangle eb = { 804.0f, 10.0f, 216.0f, 56.0f };
    ui_panel(eb, R_CARD, COL_PANEL, Fade(COL_ACCENT, 0.40f));
    ui_halo(eb, R_CARD, COL_ACCENT, 0.06f);
    int main_species = game->run.active_species_count > 0 ? game->run.active_species[0].species_id : 0;
    int stage = ua_species_stage(main_species);
    char line[128];
    snprintf(line, sizeof(line), "第 %d 纪元", game->run.era);
    ui_text_strong(line, eb.x + 12.0f, eb.y + 7.0f, F_LABEL, COL_TEXT);
    snprintf(line, sizeof(line), "%s · 阶段 %d", ua_species_name(main_species), stage);
    ui_text(line, eb.x + 12.0f, eb.y + 30.0f, F_CAPTION, COL_DIM);

    char tbuf[32];
    ui_fmt_time(game->run.elapsed_time, tbuf, (int)sizeof(tbuf));
    ui_text_right(tbuf, eb.x + eb.width - 10.0f, eb.y + 9.0f, F_CAPTION, COL_FAINT);

    /* 纪元牌底部的演化进度：当前主物种走到 12 个形态阶段的第几格 */
    {
        float t = (float)stage / 11.0f;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        ui_bar((Rectangle){ eb.x + 12.0f, eb.y + 47.0f, eb.width - 24.0f, 3.0f }, t,
               COL_ACCENT, Fade(COL_BAR_TRACK, 0.8f));
    }

    UiButton *tree = btn_find(&g_ui, UI_BTN_OPEN_TREE, -1);
    UiButton *muts = btn_find(&g_ui, UI_BTN_OPEN_MUT_LIST, -1);
    UiButton *codex = btn_find(&g_ui, UI_BTN_OPEN_CODEX, -1);
    if (tree) draw_top_button(tree, ICON_TREE, "进化树", is_hot(&g_ui, tree));
    if (muts) draw_top_button(muts, ICON_MUT, "突变", is_hot(&g_ui, muts));
    if (codex) draw_top_button(codex, ICON_CODEX, "图鉴", is_hot(&g_ui, codex));
}

/* ================= 主界面：点击主体 ================= */

static void draw_gain_floats(Vector2 c, float r)
{
    UIState *S = &g_ui;
    for (int g = 0; g < 3; ++g) {
        if (S->gain_t[g] <= 0.0f) continue;
        float k = 1.0f - S->gain_t[g];
        float a = S->gain_t[g] > 0.4f ? 1.0f : S->gain_t[g] / 0.4f;
        char gn[32], txt[56];
        ui_fmt_num(S->gain_val[g], gn, (int)sizeof(gn));
        snprintf(txt, sizeof(txt), "+%s", gn);
        float gx = c.x + (float)(g - 1) * 22.0f;
        float fy = c.y - 30.0f - 30.0f * k;       /* 从皿心上方升起，不压住 ATP 数字 */
        float sz = 22.0f + 6.0f * (1.0f - k);
        float w = ui_text_w(txt, sz);
        ui_text_shadow(txt, gx - w * 0.5f, fy, sz, Fade(COL_ATP, a), Fade(BLACK, 0.55f * a));
    }
}

static void draw_dish(const GameState *game)
{
    UIState *S = &g_ui;
    Rectangle r = { 16.0f, 84.0f, 356.0f, 404.0f };

    /* 面板 + 生物荧光底 */
    ui_panel(r, R_CARD, COL_PANEL, Fade(COL_LINE, 0.55f));
    Color accent = ecology_color(game->run.active_species_count > 0 ? game->run.active_species[0].species_id : 0);

    int sid = game->run.active_species_count > 0 ? game->run.active_species[0].species_id : 0;
    int stage = ua_species_stage(sid);

    ui_text("生命体", r.x + S_PAD, r.y + 10.0f, F_BODY, COL_DIM);
    char lab[32];
    snprintf(lab, sizeof(lab), "样本 %02d", sid + 1);
    ui_text_right(lab, r.x + r.width - S_PAD, r.y + 12.0f, F_CAPTION, COL_FAINT);

    /* ATP 大字 */
    char num[32];
    ui_fmt_num(game->run.atp, num, (int)sizeof(num));
    ui_text_center("ATP", r.x + r.width * 0.5f, r.y + 30.0f, F_CAPTION, Fade(COL_ATP, 0.85f));
    float nw = ui_text_w(num, F_HERO);
    ui_text_shadow(num, r.x + r.width * 0.5f - nw * 0.5f, r.y + 44.0f, F_HERO,
                   COL_TEXT, Fade(BLACK, 0.65f));
    /* 数字后面一层极淡的金色辉光，让它“发亮”而不是浮在暗底上 */
    ui_glow((Vector2){ r.x + r.width * 0.5f, r.y + 66.0f }, 110.0f, COL_ATP, 0.30f);

    double click = ua_click_atp(game);
    char rl[48];
    if (click >= 0.0) {
        char cn[24];
        ui_fmt_num(click, cn, (int)sizeof(cn));
        snprintf(rl, sizeof(rl), "采集 +%s ATP", cn);
    } else {
        snprintf(rl, sizeof(rl), "点击采集");
    }
    ui_text_center(rl, r.x + r.width * 0.5f, r.y + 90.0f, F_CAPTION, COL_ATP);

    /* 培养皿：半径受点击膨胀影响 */
    Vector2 c = { r.x + r.width * 0.5f, r.y + 238.0f };
    float R = 104.0f * (1.0f + 0.055f * S->punch);
    float t = S->time;

    /* 背后环境光（在液体之前画，才不会被不透明皿底盖掉） */
    ui_glow(c, R * 1.44f, ui_color_mix(accent, (Color){ 70, 140, 210, 255 }, 0.72f), 0.12f + 0.08f * S->punch);

    /* 皿底：中心偏亮的液体。染色只留一点点，保住深空的冷调 */
    DrawCircleGradient((int)c.x, (int)c.y, R, Fade(ui_color_mix(COL_BG_DEEP, COL_TEAL, 0.10f), 0.95f),
                       Fade(COL_BG_DEEP, 0.98f));
    DrawCircleV(c, R * 0.985f, Fade(COL_STAGE, 0.22f));
    /* 液体弯月面：贴内壁的一圈冷光，让皿里“有水” */
    DrawRing(c, R * 0.93f, R * 0.955f, 0.0f, 360.0f, 84, Fade(COL_TEAL, 0.10f));

    /* 液体自身的内辉（ADDITIVE，必须画在液体之后） */
    ui_glow(c, R * 0.72f, accent, 0.20f + 0.12f * S->punch);

    /* 皿沿：金色主环 + 细内环 + 72 格刻度 */
    DrawRing(c, R, R + 2.5f, 0.0f, 360.0f, 128, Fade(COL_ACCENT, 0.65f));
    DrawRing(c, R - 9.0f, R - 8.0f, 0.0f, 360.0f, 128, Fade(COL_ACCENT, 0.18f));
    DrawRing(c, R + 7.0f, R + 8.0f, 0.0f, 360.0f, 128, Fade(COL_ACCENT, 0.20f));
    for (int i = 0; i < 72; ++i) {
        float ang = (float)i * (2.0f * PI / 72.0f) - PI * 0.5f;
        bool major = (i % 6) == 0;
        float r0 = R + 8.0f;
        float r1 = R + (major ? 14.0f : 11.0f);
        DrawLineEx((Vector2){ c.x + cosf(ang) * r0, c.y + sinf(ang) * r0 },
                   (Vector2){ c.x + cosf(ang) * r1, c.y + sinf(ang) * r1 },
                   1.0f, Fade(COL_ACCENT, major ? 0.45f : 0.16f));
    }

    /* 呼吸环：一圈极淡的旋转弧，给整个皿“在运转”的生命感 */
    for (int i = 0; i < 2; ++i) {
        float a0 = t * (0.35f + 0.15f * (float)i) + (float)i * 1.8f;
        DrawRing(c, R - 5.5f, R - 4.0f, a0 * 57.3f, a0 * 57.3f + 120.0f, 48,
                 Fade(accent, 0.20f - 0.06f * (float)i));
    }

    /* 内圈微尘 */
    for (int i = 0; i < 10; ++i) {
        float a0 = (float)i * 2.399f + t * 0.05f;
        float rr = R * (0.30f + 0.63f * hash01((unsigned int)i * 7u + 3u));
        Vector2 p = { c.x + cosf(a0) * rr, c.y + sinf(a0) * rr * 0.95f };
        float al = 0.10f + 0.10f * (0.5f + 0.5f * sinf(t * 0.9f + (float)i));
        DrawCircleV(p, 1.4f + 1.0f * hash01((unsigned int)i * 11u + 5u), Fade(accent, al));
    }

    /* 活体 */
    if (game->run.active_species_count > 0) {
        float breathe = 1.0f + 0.02f * sinf(t * 1.5f);
        Vector2 oc = { c.x, c.y + 1.0f };
        draw_organism(oc, 62.0f * breathe * (1.0f + 0.04f * S->punch), sid, stage, accent, t, true, true, 1.0f);
    }

    /* 物种名 / 提示 */
    ui_text_center(ua_species_name(sid), c.x, r.y + 362.0f, F_LABEL, COL_TEXT);
    ui_text_center("点击培养皿或按空格采集", c.x, r.y + 382.0f, F_CAPTION, COL_FAINT);

    /* 采集浮字 + 粒子 */
    draw_gain_floats(c, R);
    ui_fx_draw(c, R);

    /* hover / 按下 / 闪光 */
    UiButton *b = btn_find(S, UI_BTN_CLICK, -1);
    float hv = btn_hover_step(b, is_hot(S, b));
    if (hv > 0.01f) {
        DrawRing(c, R - 2.0f, R + 1.0f, 0.0f, 360.0f, 128, Fade(COL_ACCENT, 0.55f * hv));
        DrawCircleV(c, R, Fade(COL_ACCENT, 0.03f * hv));
    }
    if (S->press_anim > 0.0f) {
        float k = 1.0f - S->press_anim;
        float rr = R + 4.0f + 34.0f * k;
        DrawRing(c, rr - 1.0f, rr + 1.5f, 0.0f, 360.0f, 96,
                 Fade(COL_ACCENT, 0.55f * S->press_anim));
        float rr2 = R + 4.0f + 18.0f * (1.0f - S->press_anim * 0.8f);
        DrawRing(c, rr2 - 0.8f, rr2 + 0.8f, 0.0f, 360.0f, 96,
                 Fade(COL_ATP, 0.30f * S->press_anim));
    }
    if (S->flash > 0.02f) {
        DrawCircleV(c, R, Fade(COL_ATP, 0.14f * S->flash));
    }
}

/* ================= 物种卡（左列下方） ================= */

static void draw_species_card(const GameState *game, int slot)
{
    Rectangle r = { 16.0f, 496.0f, 356.0f, 152.0f };
    if (slot >= game->run.active_species_count) {
        ui_panel(r, R_CARD, COL_SURFACE_OFF, Fade(COL_LINE, 0.4f));
        ui_text_center("空置物种槽", r.x + r.width * 0.5f, r.y + 52.0f, F_LABEL, COL_FAINT);
        ui_text_center(slot == 0 ? "等待主谱系" : "多样性 Lv.2 解锁",
                       r.x + r.width * 0.5f, r.y + 76.0f, F_CAPTION, COL_FAINT);
        return;
    }

    const SpeciesState *sp = &game->run.active_species[slot];
    int id = sp->species_id;
    Color accent = ecology_color(id);
    SpeciesRates rates;
    ua_species_rates(game, slot, &rates);

    ui_panel(r, R_CARD, COL_PANEL, Fade(COL_LINE, 0.55f));
    DrawRectangleRounded((Rectangle){ r.x + 6.0f, r.y + 10.0f, 3.0f, r.height - 20.0f },
                         1.0f, 4, Fade(accent, 0.85f));

    ui_text_strong(ua_species_name(id), r.x + S_PAD, r.y + 8.0f, F_LABEL, COL_TEXT);
    /* 身份 chip */
    {
        const char *tag = sp->is_main_lineage ? "主谱系" : "额外分支";
        float w = ui_text_w(tag, F_CAPTION) + 14.0f;
        Rectangle chip = { r.x + r.width - S_PAD - w, r.y + 8.0f, w, H_CHIP };
        ui_panel(chip, R_CHIP(H_CHIP), Fade(accent, 0.14f), Fade(accent, 0.45f));
        ui_text_center(tag, chip.x + chip.width * 0.5f, chip.y + 4.0f, F_CAPTION, accent);
    }

    char line[160];
    snprintf(line, sizeof(line), "%s · 阶段 %d", ua_ecology_name(id), ua_species_stage(id));
    ui_text(line, r.x + S_PAD, r.y + 28.0f, F_CAPTION, COL_DIM);

    double pop = sp->population;
    if (rates.final_population_cap > 0.0 && pop > rates.final_population_cap) {
        pop = rates.final_population_cap;      /* 防御：种群不可能超过承载上限 */
    }
    char num[32], cap[32];
    ui_fmt_num(pop, num, (int)sizeof(num));
    ui_fmt_num(rates.final_population_cap, cap, (int)sizeof(cap));
    snprintf(line, sizeof(line), "种群 %s / %s", num, cap);
    ui_text(line, r.x + S_PAD, r.y + 44.0f, F_BODY, COL_TEXT);

    float t = rates.final_population_cap > 0.0 ? (float)(pop / rates.final_population_cap) : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    ui_bar((Rectangle){ r.x + S_PAD, r.y + 63.0f, r.width - S_PAD * 2.0f, 5.0f }, t,
           accent, COL_BAR_TRACK);

    /* 三资源产率（带色点，位置固定 ATP → 生物量 → DNA） */
    float y = r.y + 73.0f;
    DrawCircleV((Vector2){ r.x + S_PAD + 2.0f, y + 6.0f }, 3.0f, COL_ATP);
    char a[24], b2[24], c2[24];
    ui_fmt_num(rates.atp_per_second, a, (int)sizeof(a));
    ui_text(a, r.x + S_PAD + 10.0f, y, F_BODY, COL_ATP);
    DrawCircleV((Vector2){ r.x + 112.0f, y + 6.0f }, 3.0f, COL_BIOMASS);
    ui_fmt_num(rates.biomass_per_second, b2, (int)sizeof(b2));
    ui_text(b2, r.x + 120.0f, y, F_BODY, COL_BIOMASS);
    DrawCircleV((Vector2){ r.x + 206.0f, y + 6.0f }, 3.0f, COL_DNA);
    ui_fmt_num(rates.dna_per_second, c2, (int)sizeof(c2));
    ui_text(c2, r.x + 214.0f, y, F_BODY, COL_DNA);

    /* 适应度 */
    char mul[24];
    snprintf(mul, sizeof(mul), "×%.2f", rates.environment_multiplier);
    Color mc = rates.environment_multiplier > 1.001 ? COL_GOOD
             : (rates.environment_multiplier < 0.999 ? COL_BAD : COL_DIM);
    ui_text("环境适应", r.x + S_PAD, r.y + 95.0f, F_CAPTION, COL_FAINT);
    ui_text_strong(mul, r.x + 76.0f, r.y + 92.0f, F_LABEL, mc);
    if (rates.synergy_active) {
        float w = ui_text_w("协同", F_CAPTION) + 12.0f;
        Rectangle chip = { r.x + 156.0f, r.y + 93.0f, w, H_CHIP };
        ui_panel(chip, R_CHIP(H_CHIP), Fade(COL_EP, 0.12f), BLANK);
        ui_text_center("协同", chip.x + chip.width * 0.5f, chip.y + 4.0f, F_CAPTION, COL_EP);
    }

    /* 突变 chips：右端固定一枚"全部 N >"，整行可点 → 打开突变图谱。
     * 这一行原本只放得下 3 枚 chip，"第 4 个以后的突变去哪儿了"没有答案；
     * 现在尾部的计数 chip 就是那个入口。 */
    int total = 0;
    for (int m = 0; m < MUTATION_COUNT; ++m) {
        if (sp->mutations[m]) ++total;
    }

    char allbuf[32];
    if (total > 0) snprintf(allbuf, sizeof(allbuf), "全部 %d >", total);
    else           snprintf(allbuf, sizeof(allbuf), "突变图谱 >");
    float all_w = ui_text_w(allbuf, F_CAPTION) + 18.0f;
    float all_x = r.x + r.width - S_SAFE - all_w;

    UiButton *mb = btn_find(&g_ui, UI_BTN_OPEN_MUT_LIST, 1);
    float mh = btn_hover_step(mb, is_hot(&g_ui, mb));
    Rectangle all = { all_x, r.y + 110.0f, all_w, H_CHIP };
    ui_panel(all, R_CHIP(H_CHIP), Fade(COL_DNA, 0.10f + 0.14f * mh),
             Fade(COL_DNA, 0.26f + 0.45f * mh));
    ui_text_center(allbuf, all.x + all.width * 0.5f, all.y + 4.0f, F_CAPTION, COL_DNA);

    int shown = 0;
    float chip_x = r.x + S_PAD + 36.0f;
    for (int m = 0; m < MUTATION_COUNT; ++m) {
        if (!sp->mutations[m]) continue;
        const char *nm = ua_mutation_name(m);
        float w = ui_text_w(nm, F_CAPTION) + 14.0f;
        if (chip_x + w > all_x - 6.0f) break;
        Rectangle chip = { chip_x, r.y + 110.0f, w, H_CHIP };
        ui_panel(chip, R_CHIP(H_CHIP), Fade(COL_DNA, 0.10f), Fade(COL_DNA, 0.24f));
        ui_text_center(nm, chip.x + chip.width * 0.5f, chip.y + 4.0f, F_CAPTION, COL_DNA);
        chip_x += w + 5.0f;
        shown++;
    }
    if (shown == 0 && total == 0) {
        ui_text("尚无突变", r.x + S_PAD + 36.0f, r.y + 110.0f, F_CAPTION, COL_FAINT);
    }

    char why[192];
    ua_fitness_breakdown(game, slot, why, (int)sizeof(why));
    ui_text(why, r.x + S_PAD, r.y + 131.0f, F_CAPTION, COL_DIM);
}

/* ================= 环境横幅 ================= */

static void draw_countdown_ring(Vector2 c, float radius, float progress, Color col, const char *text)
{
    DrawRing(c, radius - 3.0f, radius, 0.0f, 360.0f, 64, Fade(COL_BG_DEEP, 0.55f));
    if (progress > 0.001f) {
        if (progress > 1.0f) progress = 1.0f;
        DrawRing(c, radius - 3.0f, radius, -90.0f, -90.0f + 360.0f * progress, 64, col);
        DrawRing(c, radius - 3.0f, radius, -90.0f, -90.0f + 360.0f * progress, 64, Fade(col, 0.35f));
    }
    ui_text_center(text, c.x, c.y - 7.0f, F_CAPTION, COL_TEXT);
}

static void draw_env_banner(const GameState *game)
{
    Rectangle r = { 388.0f, 84.0f, 516.0f, 56.0f };
    int env = game->run.current_environment_id;

    double remaining = 0.0, total = 1.0;
    bool in_event = false;
    ua_environment_timer(game, &remaining, &total, &in_event);
    bool crisis = in_event && ua_environment_is_crisis(env);
    Color accent = crisis ? COL_BAD : COL_ACCENT;

    ui_panel(r, R_CARD, crisis ? COL_BAD_SURFACE : COL_PANEL, Fade(accent, 0.40f));
    if (crisis) ui_halo(r, R_CARD, COL_BAD, 0.10f);

    char tbuf[32];
    ui_fmt_time(remaining, tbuf, (int)sizeof(tbuf));
    float prog = total > 0.0 ? 1.0f - (float)(remaining / total) : 0.0f;
    if (prog < 0.0f) prog = 0.0f;
    if (prog > 1.0f) prog = 1.0f;
    draw_countdown_ring((Vector2){ r.x + 32.0f, r.y + 28.0f }, 21.0f, prog,
                        crisis ? COL_BAD : COL_ACCENT, tbuf);

    ui_text_strong(ua_environment_name(env), r.x + 64.0f, r.y + 6.0f, F_LABEL, COL_TEXT);

    const char *chip = in_event ? (crisis ? "危机中" : "丰饶期") : "稳定期";
    float cw = ui_text_w(chip, F_CAPTION) + 14.0f;
    Rectangle chipr = { r.x + r.width - S_PAD - cw, r.y + 6.0f, cw, H_CHIP };
    ui_panel(chipr, R_CHIP(H_CHIP), Fade(accent, 0.16f), Fade(accent, 0.45f));
    ui_text_center(chip, chipr.x + cw * 0.5f, chipr.y + 4.0f, F_CAPTION, accent);

    char desc[128];
    ua_environment_desc(env, desc, (int)sizeof(desc));
    ui_text(desc, r.x + 64.0f, r.y + 28.0f, F_CAPTION, COL_DIM);

    if (game->legacy.levels[LEGACY_ADAPTATION] >= 3 && game->run.next_environment_id != env) {
        char nb[64];
        snprintf(nb, sizeof(nb), "下一事件：%s", ua_environment_name(game->run.next_environment_id));
        float used = ui_text_w(desc, F_CAPTION) + ui_text_w(nb, F_CAPTION) + 28.0f;
        if (r.x + 64.0f + used < r.x + r.width - S_PAD) {
            ui_text_right(nb, r.x + r.width - S_PAD, r.y + 28.0f, F_CAPTION, COL_EP);
        }
    }
}

/* ================= 展台 ================= */

static void draw_pop_arc(Vector2 c, float r, float t, Color col)
{
    if (t <= 0.0f) return;
    if (t > 1.0f) t = 1.0f;
    DrawRing(c, r, r + 3.0f, -90.0f, -90.0f + 360.0f * t, 96, Fade(col, 0.55f));
    DrawRing(c, r + 3.5f, r + 4.0f, -90.0f, -90.0f + 360.0f * t, 96, Fade(COL_TEXT, 0.15f));
}

static void draw_stage(const GameState *game)
{
    UIState *S = &g_ui;
    Rectangle r = { 388.0f, 148.0f, 516.0f, 284.0f };

    ui_panel(r, R_CARD, COL_STAGE, Fade(COL_LINE, 0.45f));
    /* 背景：一片深水 / 星尘中的培养区（内缩 6px 保证渐变不会露出方角） */
    ui_gradient((Rectangle){ r.x + 6.0f, r.y + 6.0f, r.width - 12.0f, r.height - 12.0f },
                Fade(COL_BG_LIFT, 0.55f), Fade(COL_BG_DEEP, 0.75f));
    ui_panel((Rectangle){ r.x + 8.0f, r.y + 8.0f, r.width - 16.0f, r.height - 16.0f },
             R_CARD - 4.0f, Fade(COL_STAGE, 0.86f), BLANK);

    atm_draw_live_in(r);

    if (game->run.active_species_count <= 0) {
        ui_text_center("等待初始化", r.x + r.width * 0.5f, r.y + r.height * 0.5f, F_LABEL, COL_DIM);
        return;
    }

    ui_text("生命体展台", r.x + 14.0f, r.y + 8.0f, F_CAPTION, COL_FAINT);
    char lab[32];
    snprintf(lab, sizeof(lab), "样本 %02d · 40×", game->run.active_species[0].species_id + 1);
    ui_text_right(lab, r.x + r.width - 14.0f, r.y + 8.0f, F_CAPTION, COL_FAINT);

    int nsp = game->run.active_species_count;
    if (nsp > 3) nsp = 3;
    float cy = r.y + 116.0f;

    if (nsp <= 1) {
        float cx = r.x + r.width * 0.5f;
        float radius = 96.0f;
        Color accent = ecology_color(game->run.active_species[0].species_id);
        SpeciesRates rates;
        ua_species_rates(game, 0, &rates);

        ui_glow((Vector2){ cx, cy }, radius * 1.5f, ui_color_mix(accent, (Color){ 70, 140, 210, 255 }, 0.72f), 0.11f);
        draw_organism((Vector2){ cx, cy }, radius, game->run.active_species[0].species_id,
                      ua_species_stage(game->run.active_species[0].species_id), accent,
                      S->time, true, true, 1.0f);
        float pop = rates.final_population_cap > 0.0
                  ? (float)(game->run.active_species[0].population / rates.final_population_cap) : 0.0f;
        draw_pop_arc((Vector2){ cx, cy }, radius + 14.0f, pop, accent);

        ui_text_center_strong(ua_species_name(game->run.active_species[0].species_id), cx,
                              r.y + 228.0f, F_LABEL, COL_TEXT);
        char sub[96], pn[24], cn[24];
        ui_fmt_num(game->run.active_species[0].population, pn, (int)sizeof(pn));
        ui_fmt_num(rates.final_population_cap, cn, (int)sizeof(cn));
        snprintf(sub, sizeof(sub), "主谱系 · 种群 %s / %s", pn, cn);
        ui_text_center(sub, cx, r.y + 250.0f, F_CAPTION, COL_ACCENT);
    } else if (nsp == 2) {
        float cx = r.x + 156.0f;
        float radius = 74.0f;
        Color accent = ecology_color(game->run.active_species[0].species_id);
        SpeciesRates rates;
        ua_species_rates(game, 0, &rates);

        ui_glow((Vector2){ cx, cy }, radius * 1.5f, ui_color_mix(accent, (Color){ 70, 140, 210, 255 }, 0.72f), 0.10f);
        draw_organism((Vector2){ cx, cy }, radius, game->run.active_species[0].species_id,
                      ua_species_stage(game->run.active_species[0].species_id), accent,
                      S->time, true, true, 1.0f);
        float pop = rates.final_population_cap > 0.0
                  ? (float)(game->run.active_species[0].population / rates.final_population_cap) : 0.0f;
        draw_pop_arc((Vector2){ cx, cy }, radius + 13.0f, pop, accent);

        ui_text_center_strong(ua_species_name(game->run.active_species[0].species_id), cx,
                              r.y + 214.0f, F_LABEL, COL_TEXT);
        char sub[96], pn[24], cn[24];
        ui_fmt_num(game->run.active_species[0].population, pn, (int)sizeof(pn));
        ui_fmt_num(rates.final_population_cap, cn, (int)sizeof(cn));
        snprintf(sub, sizeof(sub), "主谱系 · %s / %s", pn, cn);
        ui_text_center(sub, cx, r.y + 236.0f, F_CAPTION, COL_ACCENT);

        float cx2 = r.x + 366.0f;
        Color accent2 = ecology_color(game->run.active_species[1].species_id);
        SpeciesRates r2;
        ua_species_rates(game, 1, &r2);
        ui_glow((Vector2){ cx2, cy }, 54.0f * 1.5f, ui_color_mix(accent2, (Color){ 70, 140, 210, 255 }, 0.72f), 0.09f);
        draw_organism((Vector2){ cx2, cy }, 52.0f, game->run.active_species[1].species_id,
                      ua_species_stage(game->run.active_species[1].species_id), accent2,
                      S->time + 1.7f, true, true, 1.0f);
        float pop2 = r2.final_population_cap > 0.0
                   ? (float)(game->run.active_species[1].population / r2.final_population_cap) : 0.0f;
        draw_pop_arc((Vector2){ cx2, cy }, 66.0f, pop2, accent2);

        ui_text_center_strong(ua_species_name(game->run.active_species[1].species_id), cx2,
                              r.y + 194.0f, F_LABEL, COL_TEXT);
        char sub2[96], pn2[24], cn2[24];
        ui_fmt_num(game->run.active_species[1].population, pn2, (int)sizeof(pn2));
        ui_fmt_num(r2.final_population_cap, cn2, (int)sizeof(cn2));
        snprintf(sub2, sizeof(sub2), "额外分支 · %s / %s", pn2, cn2);
        ui_text_center(sub2, cx2, r.y + 216.0f, F_CAPTION, COL_DIM);

        /* 多样性 Lv.3 协同：两株之间一条能量弧 */
        if (r2.synergy_active || game->legacy.levels[LEGACY_DIVERSIFICATION] >= 3) {
            Vector2 a = { cx + 40.0f, cy - 10.0f };
            Vector2 b = { cx2 - 34.0f, cy - 10.0f };
            for (int i = 0; i < 8; ++i) {
                float u = (float)i / 7.0f;
                Vector2 p = { a.x + (b.x - a.x) * u, a.y + sinf(u * PI) * 18.0f };
                DrawCircleV(p, 1.6f, Fade(COL_EP, 0.30f + 0.25f * sinf(S->time * 2.0f + u * 4.0f)));
            }
        }
    } else {
        /* 三槽：主谱系在左压阵，两个额外分支在右侧上下叠放 */
        {
            float mx = r.x + 132.0f;
            float R0 = 66.0f;
            Color a0 = ecology_color(game->run.active_species[0].species_id);
            SpeciesRates s0;
            ua_species_rates(game, 0, &s0);
            ui_glow((Vector2){ mx, cy }, R0 * 1.5f, ui_color_mix(a0, (Color){ 70, 140, 210, 255 }, 0.72f), 0.11f);
            draw_organism((Vector2){ mx, cy }, R0, game->run.active_species[0].species_id,
                          ua_species_stage(game->run.active_species[0].species_id), a0,
                          S->time, true, true, 1.0f);
            float p0 = s0.final_population_cap > 0.0
                     ? (float)(game->run.active_species[0].population / s0.final_population_cap) : 0.0f;
            draw_pop_arc((Vector2){ mx, cy }, R0 + 12.0f, p0, a0);

            ui_text_center_strong(ua_species_name(game->run.active_species[0].species_id), mx,
                                  r.y + 226.0f, F_LABEL, COL_TEXT);
            char sub0[96], pn0[24], cn0[24];
            ui_fmt_num(game->run.active_species[0].population, pn0, (int)sizeof(pn0));
            ui_fmt_num(s0.final_population_cap, cn0, (int)sizeof(cn0));
            snprintf(sub0, sizeof(sub0), "主谱系 · %s / %s", pn0, cn0);
            ui_text_center(sub0, mx, r.y + 248.0f, F_CAPTION, COL_ACCENT);

            for (int k = 1; k < 3; ++k) {
                float sx = r.x + 352.0f;
                float sy = k == 1 ? cy - 46.0f : cy + 50.0f;
                float R1 = 38.0f;
                Color ak = ecology_color(game->run.active_species[k].species_id);
                SpeciesRates sk;
                ua_species_rates(game, k, &sk);
                ui_glow((Vector2){ sx, sy }, R1 * 1.5f, ui_color_mix(ak, (Color){ 70, 140, 210, 255 }, 0.72f), 0.09f);
                draw_organism((Vector2){ sx, sy }, R1, game->run.active_species[k].species_id,
                              ua_species_stage(game->run.active_species[k].species_id), ak,
                              S->time + (float)k * 1.3f, true, true, 1.0f);
                float pk = sk.final_population_cap > 0.0
                         ? (float)(game->run.active_species[k].population / sk.final_population_cap) : 0.0f;
                draw_pop_arc((Vector2){ sx, sy }, R1 + 10.0f, pk, ak);

                char pn[24], cn[24], sub[72];
                ui_fmt_num(game->run.active_species[k].population, pn, (int)sizeof(pn));
                ui_fmt_num(sk.final_population_cap, cn, (int)sizeof(cn));
                ui_text_strong(ua_species_name(game->run.active_species[k].species_id),
                               sx + 50.0f, sy - 18.0f, F_LABEL, COL_TEXT);
                snprintf(sub, sizeof(sub), "%s / %s", pn, cn);
                ui_text(sub, sx + 50.0f, sy + 2.0f, F_CAPTION, COL_DIM);
            }
        }
    }

    /* 底部一层地平线光，给展台“有纵深”的收尾 */
    DrawRectangleGradientV((int)r.x, (int)(r.y + r.height - 40.0f), (int)r.width, 40,
                           Fade(COL_BG_DEEP, 0.0f), Fade(COL_BG_DEEP, 0.55f));
}

/* ================= 里程碑 ================= */

static void draw_milestone(const GameState *game)
{
    Rectangle r = { 388.0f, 440.0f, 516.0f, 52.0f };
    ui_panel(r, R_CARD, COL_PANEL, Fade(COL_LINE, 0.45f));
    DrawRectangleRounded((Rectangle){ r.x + 6.0f, r.y + 8.0f, 3.0f, r.height - 16.0f },
                         1.0f, 4, Fade(COL_BIOMASS, 0.8f));

    int level = ua_milestone_level(game);
    double cur = game->run.max_biomass;
    int total_levels = ua_milestone_count();
    int top = total_levels > 0 ? total_levels - 1 : 0;
    bool capped = level >= top;
    double next = capped ? 0.0 : ua_milestone_threshold(level + 1);
    double floor_v = ua_milestone_threshold(level);

    float t = 0.0f;
    if (capped) {
        t = 1.0f;                                   /* 满档：进度条填满 */
    } else if (next > floor_v) {
        t = (float)((cur - floor_v) / (next - floor_v));
    }
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    char left[64], right[48], num[32], nn[32];
    ui_fmt_num(cur, num, (int)sizeof(num));
    if (capped) {
        snprintf(right, sizeof(right), "已满档");
    } else {
        ui_fmt_num(next, nn, (int)sizeof(nn));
        snprintf(right, sizeof(right), "下一档 %s", nn);
    }
    snprintf(left, sizeof(left), "生物量里程碑");

    ui_text(left, r.x + S_PAD, r.y + 8.0f, F_CAPTION, COL_DIM);
    ui_text_right(right, r.x + r.width - S_PAD, r.y + 8.0f, F_CAPTION,
                  capped ? COL_BIOMASS : COL_FAINT);

    char b[32];
    snprintf(b, sizeof(b), "B%d", level);
    ui_text_strong(b, r.x + S_PAD, r.y + 24.0f, F_LABEL, COL_BIOMASS);
    ui_text(num, r.x + S_PAD + 30.0f, r.y + 25.0f, F_BODY, COL_TEXT);

    char lv[32];
    snprintf(lv, sizeof(lv), "%d / %d", level, top);
    ui_text_right(lv, r.x + r.width - S_PAD, r.y + 26.0f, F_CAPTION, COL_DIM);

    ui_bar((Rectangle){ r.x + S_PAD, r.y + 42.0f, r.width - S_PAD * 2.0f, 4.0f }, t,
           COL_BIOMASS, COL_BAR_TRACK);
}

/* ================= 情报面板（三页签：下一步 / 产出 / 纪录） ================= */

static void draw_info_tab(const UIState *S, int option, const char *label, Rectangle r, bool selected)
{
    UiButton *b = btn_find(S, UI_BTN_INFO_TAB, option);
    float hv = btn_hover_step(b, is_hot(S, b));

    Color fill = selected ? Fade(COL_ACCENT, 0.16f)
                          : ui_color_mix(Fade(COL_PANEL_HI, 0.85f), COL_PANEL_HOT, hv);
    Color bord = selected ? Fade(COL_ACCENT, 0.9f) : Fade(COL_LINE, 0.55f);

    ui_panel(r, R_CHIP(r.height), fill, bord);
    ui_text_center(label, r.x + r.width * 0.5f, r.y + 4.0f, F_CAPTION,
                   selected ? COL_TEXT : COL_FAINT);
}

/* 一条迷你曲线（无坐标轴，只给趋势）：底轨 + 面积 + 折线 + 末端亮点 */
static void spark_lane(Rectangle r, const float *v, int n, Color col)
{
    ui_panel(r, 6.0f, Fade(COL_BG_DEEP, 0.45f), Fade(COL_LINE, 0.35f));

    float lo = 0.0f, hi = 0.0f;
    for (int i = 0; i < n; ++i) {
        if (i == 0 || v[i] < lo) lo = v[i];
        if (i == 0 || v[i] > hi) hi = v[i];
    }
    float span = hi - lo;
    if (span < 1e-6f) span = 1.0f;

    float x0 = r.x + 6.0f;
    float x1 = r.x + r.width - 8.0f;
    float yb = r.y + r.height - 4.0f;
    float yt = r.y + 4.0f;

    float step = (n > 1) ? (x1 - x0) / (float)(n - 1) : 0.0f;
    Vector2 prev = { 0 };
    for (int i = 0; i < n; ++i) {
        float t = (v[i] - lo) / span;
        Vector2 p = { x0 + step * (float)i, yb - t * (yb - yt) };
        /* 面积：每列一条从曲线垂到底的暗色柱，比多边形填充更省事也更干净 */
        DrawRectangle((int)p.x, (int)p.y, (int)(step + 1.0f) + 1, (int)(yb - p.y),
                      Fade(col, 0.10f));
        if (i > 0) DrawLineEx(prev, p, 1.4f, Fade(col, 0.92f));
        prev = p;
        if (i == n - 1) {
            DrawCircleV(p, 2.2f, col);
            ui_glow(p, 9.0f, col, 0.55f);
        }
    }
}

/* 记录格：小标题 + 大数值 */
static void record_cell(Rectangle r, const char *label, const char *value, Color col)
{
    ui_panel(r, 10.0f, Fade(COL_PANEL_HI, 0.55f), Fade(COL_LINE, 0.40f));
    DrawRectangleRounded((Rectangle){ r.x + 1.0f, r.y + 8.0f, 2.0f, r.height - 16.0f },
                         1.0f, 3, Fade(col, 0.7f));
    ui_text(label, r.x + 10.0f, r.y + 5.0f, F_CAPTION, COL_FAINT);
    ui_text_strong(value, r.x + 10.0f, r.y + 21.0f, F_LABEL, COL_TEXT);
}

/* 截图模式没有 ui_update，曲线会是空的：用当前存量补一条确定的上升曲线，
 * 保证导出帧里图是“活的”。只在 hist_n 为空时执行一次。 */
static void seed_hist(const GameState *game)
{
    if (g_ui.hist_n > 0) return;
    double cur[3] = { game->run.atp, game->run.biomass, game->run.dna };
    for (int s = 0; s < 3; ++s) {
        for (int i = 0; i < 72; ++i) {
            float t = (float)i / 71.0f;
            float wob = 1.0f + 0.012f * sinf((float)i * 0.55f + (float)s * 2.1f);
            g_ui.hist[s][i] = (float)(cur[s] * (0.86 + 0.14 * t) * wob);
        }
    }
    g_ui.hist_n = 72;
}

static void info_content_next(const GameState *game, Rectangle r)
{
    float y = r.y + 38.0f;
    float x = r.x + S_PAD;

    int children[SPECIES_COUNT];
    int n = ua_speciation_children(game, children, SPECIES_COUNT);
    int ready = -1;
    for (int i = 0; i < n; ++i) {
        if (ua_speciation_check(game, children[i]) == ACTION_OK) { ready = children[i]; break; }
    }

    DrawCircleV((Vector2){ x + 3.0f, y + 7.0f }, 3.0f, COL_ACCENT);
    if (ready >= 0) {
        char line[200], num[32];
        ui_fmt_num(ua_speciation_cost(game, ready), num, (int)sizeof(num));
        snprintf(line, sizeof(line), "生物量已够，可分化为 %s（%s）", ua_species_name(ready), num);
        ui_text(line, x + 14.0f, y, F_CAPTION, COL_TEXT);
    } else if (n > 0) {
        char req[140], line[220];
        ua_speciation_requirement(children[0], req, (int)sizeof(req));
        snprintf(line, sizeof(line), "下一分支 %s（%s）", ua_species_name(children[0]), req);
        ui_text(line, x + 14.0f, y, F_CAPTION, COL_DIM);
    } else {
        ui_text("已抵达本版最后阶段，可以结算或继续积累", x + 14.0f, y, F_CAPTION, COL_DIM);
    }

    y += 22.0f;
    double erem = 0.0, etot = 1.0;
    bool in_event = false;
    ua_environment_timer(game, &erem, &etot, &in_event);
    (void)etot;
    char tbuf[32];
    ui_fmt_time(erem, tbuf, (int)sizeof(tbuf));
    DrawCircleV((Vector2){ x + 3.0f, y + 7.0f }, 3.0f, in_event ? COL_BAD : COL_DIM);
    if (in_event) {
        char line[200];
        snprintf(line, sizeof(line), "%s 还有 %s 结束，低适应物种会掉种群",
                 ua_environment_name(game->run.current_environment_id), tbuf);
        ui_text(line, x + 14.0f, y, F_CAPTION, COL_DIM);
    } else {
        char line[200];
        snprintf(line, sizeof(line), "常态海洋，%s 后发生下一次扰动", tbuf);
        ui_text(line, x + 14.0f, y, F_CAPTION, COL_DIM);
    }

    y += 22.0f;
    DrawCircleV((Vector2){ x + 3.0f, y + 7.0f }, 3.0f, COL_DNA);
    if (game->run.mutation_offer.active) {
        char line[200], num[32];
        ui_fmt_num(game->run.dna, num, (int)sizeof(num));
        snprintf(line, sizeof(line), "%d 个突变候选待选，DNA 现有 %s",
                 game->run.mutation_offer.choice_count, num);
        ui_text(line, x + 14.0f, y, F_CAPTION, COL_DIM);
    } else if (game->run.dna >= 12.0) {
        ui_text("DNA 足够，可以打开突变面板（M 键）", x + 14.0f, y, F_CAPTION, COL_DIM);
    } else {
        ui_text("积累 DNA 以解锁主动突变（M 键）", x + 14.0f, y, F_CAPTION, COL_DIM);
    }

    y += 22.0f;
    int lit = 0;
    for (int i = 0; i < SPECIES_COUNT; ++i) {
        if (game->legacy.codex_species[i]) ++lit;
    }
    int mut_lit = 0;
    for (int i = 0; i < MUTATION_COUNT; ++i) {
        if (game->legacy.codex_mutations[i]) ++mut_lit;
    }
    DrawCircleV((Vector2){ x + 3.0f, y + 7.0f }, 3.0f, COL_EP);
    {
        char line[200];
        snprintf(line, sizeof(line), "图鉴 %d/%d 物种 · %d/%d 突变，进化树可看谱系",
                 lit, SPECIES_COUNT, mut_lit, MUTATION_COUNT);
        ui_text(line, x + 14.0f, y, F_CAPTION, COL_DIM);
    }

    ui_text("EP = 2E + M/3 + S/2 + min(C,3) + B · 主动结束不会丢失图鉴",
            x, r.y + 126.0f, F_CAPTION, COL_FAINT);
}

static void info_content_output(const GameState *game, Rectangle r)
{
    seed_hist(game);

    double rates[3];
    ua_total_rates(game, rates);

    struct {
        const char *label;
        double value;
        double rate;
        Color col;
    } rows[3] = {
        { "ATP",    game->run.atp,     rates[0], COL_ATP },
        { "生物量", game->run.biomass, rates[1], COL_BIOMASS },
        { "DNA",    game->run.dna,     rates[2], COL_DNA },
    };

    float y = r.y + 38.0f;
    for (int i = 0; i < 3; ++i) {
        float rx = r.x + S_PAD;
        DrawCircleV((Vector2){ rx + 3.0f, y + 5.0f }, 3.0f, rows[i].col);
        ui_text(rows[i].label, rx + 13.0f, y - 2.0f, F_CAPTION, Fade(rows[i].col, 0.95f));

        char num[32], rate[32];
        ui_fmt_num(rows[i].value, num, (int)sizeof(num));
        ui_text_strong(num, rx, y + 12.0f, F_LABEL, COL_TEXT);

        ui_fmt_rate(rows[i].rate, rate, (int)sizeof(rate));
        ui_text_right(rate, rx + 176.0f, y + 14.0f, F_CAPTION,
                      rows[i].rate > 0.0 ? COL_GOOD : COL_FAINT);

        Rectangle lane = { r.x + 216.0f, y + 2.0f, r.width - 216.0f - S_PAD, 22.0f };
        spark_lane(lane, g_ui.hist[i], 72, rows[i].col);

        y += 32.0f;
    }

    ui_text("最近 18 秒的存量变化（每 0.25 秒采样）",
            r.x + 216.0f, r.y + 126.0f, F_CAPTION, COL_FAINT);
}

static void info_content_record(const GameState *game, Rectangle r)
{
    char a[32], b[32], c[32], d[32];

    ui_fmt_num(game->run.max_biomass, a, (int)sizeof(a));

    double pop = 0.0;
    for (int i = 0; i < game->run.active_species_count; ++i) {
        pop += game->run.active_species[i].population;
    }
    ui_fmt_num(pop, b, (int)sizeof(b));

    int lit = 0;
    for (int i = 0; i < SPECIES_COUNT; ++i) {
        if (game->legacy.codex_species[i]) ++lit;
    }
    snprintf(c, sizeof(c), "%d / %d", lit, SPECIES_COUNT);

    EraReward reward;
    ua_era_reward(game, &reward);
    snprintf(d, sizeof(d), "%d EP", reward.total_points);

    float x0 = r.x + S_PAD;
    float x1 = r.x + r.width * 0.5f + 8.0f;
    float cw = r.width * 0.5f - S_PAD - 12.0f;

    record_cell((Rectangle){ x0, r.y + 38.0f, cw, 42.0f }, "本轮峰值生物量", a, COL_BIOMASS);
    record_cell((Rectangle){ x1, r.y + 38.0f, cw, 42.0f }, "当前总种群", b, COL_TEAL);
    record_cell((Rectangle){ x0, r.y + 86.0f, cw, 42.0f }, "已点亮物种", c, COL_DNA);
    record_cell((Rectangle){ x1, r.y + 86.0f, cw, 42.0f }, "预计结算 EP", d, COL_EP);

    /* 成就行：三个小徽章 + 计数，Cookie Clicker 式的“收集完成度” */
    int n = ua_achievement_count();
    if (n > 6) n = 6;
    int got = 0;
    char line[48];
    float bx = x0 + 46.0f;
    for (int i = 0; i < n; ++i) {
        bool un = ua_achievement_unlocked(game, i);
        if (un) ++got;
        Rectangle t = { bx + (float)i * 22.0f, r.y + 132.0f, 16.0f, 16.0f };
        ui_panel(t, 5.0f, un ? Fade(COL_ACCENT, 0.16f) : COL_SURFACE_OFF,
                 un ? Fade(COL_ACCENT, 0.7f) : Fade(COL_LINE, 0.4f));
        DrawCircleV((Vector2){ t.x + 8.0f, t.y + 8.0f }, 2.5f, un ? COL_ACCENT : COL_FAINT);
    }
    snprintf(line, sizeof(line), "成就 %d / %d", got, n);
    ui_text(line, x0, r.y + 134.0f, F_CAPTION, COL_DIM);
}

static void draw_info_panel(const GameState *game)
{
    UIState *S = &g_ui;
    Rectangle r = { 388.0f, 500.0f, 516.0f, 148.0f };
    card_frame(r, "情报", NULL, COL_ACCENT);

    const char *labels[3] = { "下一步", "产出", "纪录" };
    float tab_w = 62.0f, gap = 4.0f;
    float tabs_x = r.x + r.width - S_PAD - (tab_w * 3.0f + gap * 2.0f);
    for (int i = 0; i < 3; ++i) {
        draw_info_tab(S, i, labels[i],
                      (Rectangle){ tabs_x + (tab_w + gap) * (float)i, r.y + 7.0f,
                                   tab_w, H_CHIP }, S->info_tab == i);
    }

    switch (S->info_tab) {
    case 1: info_content_output(game, r); break;
    case 2: info_content_record(game, r); break;
    default: info_content_next(game, r); break;
    }
}

/* ================= 商店 ================= */

static void draw_shop_tab(const UIState *S, int option, const char *label, Rectangle r, bool selected)
{
    UiButton *b = btn_find(S, UI_BTN_STORE_TAB, option);
    float hv = btn_hover_step(b, is_hot(S, b));

    Color fill = selected ? Fade(COL_ACCENT, 0.16f)
                          : ui_color_mix(COL_PANEL_HI, COL_PANEL_HOT, hv);
    Color bord = selected ? COL_ACCENT : Fade(COL_LINE, 0.7f);

    ui_panel(r, R_BTN, fill, bord);
    if (selected) {
        DrawRectangleRounded((Rectangle){ r.x + 20.0f, r.y + r.height - 3.0f,
                                          r.width - 40.0f, 2.0f }, 1.0f, 4, COL_ACCENT);
    }
    ui_text_center_strong(label, r.x + r.width * 0.5f, r.y + 9.0f, F_BODY,
                          selected ? COL_TEXT : COL_DIM);
}

static void draw_shop_upgrade(const GameState *game, int type, Rectangle r)
{
    UIState *S = &g_ui;
    UiButton *b = btn_find(S, UI_BTN_UPGRADE, type);
    float hv = btn_hover_step(b, is_hot(S, b));
    bool usable = b && b->enabled;
    int level = game->run.run_upgrade_levels[type];

    Color icol = (type == UPGRADE_METABOLISM) ? COL_ATP
               : (type == UPGRADE_REPRODUCTION ? COL_BIOMASS : COL_DNA);
    int icon = (type == UPGRADE_METABOLISM) ? ICON_META
             : (type == UPGRADE_REPRODUCTION ? ICON_REPRO : ICON_GENE);

    if (usable && hv > 0.02f) ui_halo(r, R_CARD, icol, 0.10f * hv);
    ui_panel(r, R_CARD, ui_color_mix(COL_PANEL_HI, COL_PANEL_HOT, hv * 0.75f),
             (hv > 0.02f) ? Fade(icol, 0.85f) : Fade(COL_LINE, 0.55f));
    DrawRectangleRounded((Rectangle){ r.x + 6.0f, r.y + 10.0f, 3.0f, r.height - 20.0f },
                         1.0f, 4, Fade(icol, 0.85f));

    Rectangle tile = { r.x + 12.0f, r.y + 12.0f, 56.0f, 56.0f };
    icon_badge(tile, icon, icol, S->time);

    ui_text_strong(ua_run_upgrade_title((RunUpgradeType)type), r.x + 82.0f, r.y + 12.0f,
                   F_LABEL, COL_TEXT);

    char lv[24];
    snprintf(lv, sizeof(lv), "Lv.%d", level);
    float lw = ui_text_w(lv, F_CAPTION) + 14.0f;
    Rectangle chip = { r.x + r.width - 12.0f - lw, r.y + 12.0f, lw, H_CHIP };
    ui_panel(chip, R_CHIP(H_CHIP), Fade(icol, 0.12f), BLANK);
    ui_text_center(lv, chip.x + lw * 0.5f, chip.y + 4.0f, F_CAPTION, icol);

    char eff[96];
    ua_run_upgrade_effect(game, (RunUpgradeType)type, eff, (int)sizeof(eff));
    ui_text(eff, r.x + 82.0f, r.y + 36.0f, F_CAPTION, COL_DIM);

    /* 等级刻条：10 格 */
    {
        int seg = level > 10 ? 10 : level;
        for (int i = 0; i < 10; ++i) {
            Rectangle tick = { r.x + 82.0f + (float)i * 13.0f, r.y + 60.0f, 9.0f, 4.0f };
            ui_panel(tick, 2.0f, i < seg ? Fade(icol, 0.9f) : COL_BAR_TRACK, BLANK);
        }
        if (level > 10) {
            char more[16];
            snprintf(more, sizeof(more), "+%d", level - 10);
            ui_text(more, r.x + 82.0f + 10.0f * 13.0f + 4.0f, r.y + 54.0f, F_CAPTION, COL_FAINT);
        }
    }

    /* 价格条 */
    double price = ua_run_upgrade_cost(game, (RunUpgradeType)type);
    char num[32], cost[56];
    ui_fmt_num(price, num, (int)sizeof(num));
    bool inf = !isfinite(price);
    if (inf) snprintf(cost, sizeof(cost), "已满级");
    else snprintf(cost, sizeof(cost), "%s %s", num, type == UPGRADE_GENOME ? "生物量" : "ATP");

    Rectangle pill = { r.x + 12.0f, r.y + 100.0f, r.width - 24.0f, 30.0f };
    ui_panel(pill, R_BTN, usable ? Fade(icol, 0.13f) : COL_SURFACE_OFF,
             usable ? Fade(icol, 0.55f) : Fade(COL_LINE, 0.7f));
    ui_text(cost, pill.x + 12.0f, pill.y + 6.0f, F_BODY, usable ? icol : COL_FAINT);
    if (!inf) {
        ui_text_right("购买", pill.x + pill.width - 12.0f, pill.y + 7.0f, F_CAPTION,
                      usable ? COL_TEXT : COL_FAINT);
    }

    /* 快捷键角标 */
    char key[16];
    snprintf(key, sizeof(key), "%d", type + 1);
    Rectangle kb = { tile.x + tile.width - 18.0f, tile.y + tile.height - 18.0f, 18.0f, 18.0f };
    ui_panel(kb, 5.0f, Fade(COL_BG_DEEP, 0.85f), Fade(COL_LINE, 1.0f));
    ui_text_center(key, kb.x + 9.0f, kb.y + 3.0f, F_CAPTION, COL_FAINT);
}

static void draw_shop_branch(const GameState *game, int child, Rectangle r)
{
    UIState *S = &g_ui;
    UiButton *b = btn_find(S, UI_BTN_OPEN_SPECIATION, child);
    float hv = btn_hover_step(b, is_hot(S, b));
    ActionCode check = ua_speciation_check(game, child);
    bool ok = check == ACTION_OK;
    Color col = ecology_color(child);
    int stage = ua_species_stage(child);
    bool afford = game->run.biomass >= ua_speciation_cost(game, child);

    if (ok && hv > 0.02f) ui_halo(r, R_CARD, col, 0.10f * hv);
    ui_panel(r, R_CARD, ui_color_mix(COL_PANEL_HI, COL_PANEL_HOT, hv * 0.75f),
             (hv > 0.02f) ? Fade(col, 0.85f) : Fade(COL_LINE, 0.55f));
    DrawRectangleRounded((Rectangle){ r.x + 6.0f, r.y + 10.0f, 3.0f, r.height - 20.0f },
                         1.0f, 4, Fade(col, 0.85f));

    Rectangle tile = { r.x + 12.0f, r.y + 12.0f, 56.0f, 56.0f };
    ui_panel(tile, 10.0f, ui_color_mix(COL_PANEL_HI, col, 0.10f), Fade(col, 0.30f));
    draw_organism((Vector2){ tile.x + 28.0f, tile.y + 30.0f }, 21.0f, child, stage, col,
                  S->time + (float)child * 0.3f, ok, ok, ok ? 1.0f : 0.65f);

    ui_text_strong(ua_species_name(child), r.x + 82.0f, r.y + 12.0f, F_LABEL, COL_TEXT);

    char meta[96];
    snprintf(meta, sizeof(meta), "阶段 %d · %s", stage, ua_ecology_name(child));
    ui_text(meta, r.x + 82.0f, r.y + 36.0f, F_CAPTION, COL_DIM);

    char req[128];
    ua_speciation_requirement(child, req, (int)sizeof(req));
    ui_text(req, r.x + 82.0f, r.y + 56.0f, F_CAPTION, ok ? COL_GOOD : COL_FAINT);

    char num[32], cost[56];
    ui_fmt_num(ua_speciation_cost(game, child), num, (int)sizeof(num));
    snprintf(cost, sizeof(cost), "%s 生物量", num);
    Rectangle pill = { r.x + 12.0f, r.y + 100.0f, r.width - 24.0f, 30.0f };
    ui_panel(pill, R_BTN, afford ? Fade(COL_BIOMASS, 0.13f) : COL_SURFACE_OFF,
             afford ? Fade(COL_BIOMASS, 0.55f) : Fade(COL_LINE, 0.7f));
    ui_text(cost, pill.x + 12.0f, pill.y + 6.0f, F_BODY, afford ? COL_BIOMASS : COL_FAINT);
    ui_text_right(ok ? (afford ? "分化" : "资源不足") : "前置不足",
                  pill.x + pill.width - 12.0f, pill.y + 7.0f, F_CAPTION,
                  ok && afford ? COL_TEXT : COL_FAINT);
}

static void draw_shop(const GameState *game)
{
    UIState *S = &g_ui;
    Rectangle r = { 912.0f, 84.0f, 352.0f, 564.0f };
    ui_panel(r, R_CARD, COL_PANEL, Fade(COL_LINE, 0.6f));
    DrawRectangleRounded((Rectangle){ r.x + 6.0f, r.y + 12.0f, 3.0f, r.height - 24.0f },
                         1.0f, 4, Fade(COL_ACCENT, 0.75f));

    ui_text_strong("进化商店", r.x + S_PAD, r.y + 8.0f, F_TITLE, COL_TEXT);
    char money[96], a[24], b[24];
    ui_fmt_num(game->run.atp, a, (int)sizeof(a));
    ui_fmt_num(game->run.biomass, b, (int)sizeof(b));
    snprintf(money, sizeof(money), "%s ATP · %s 生物量", a, b);
    ui_text_right(money, r.x + r.width - S_PAD, r.y + 16.0f, F_CAPTION, COL_FAINT);

    draw_shop_tab(S, 0, "机能", (Rectangle){ r.x + S_PAD, r.y + 42.0f, 153.0f, 34.0f },
                  S->store_tab == 0);
    draw_shop_tab(S, 1, "分化", (Rectangle){ r.x + S_PAD + 161.0f, r.y + 42.0f, 153.0f, 34.0f },
                  S->store_tab == 1);

    int children[SPECIES_COUNT];
    int n = ua_speciation_children(game, children, SPECIES_COUNT);
    for (int i = 0; i < 3; ++i) {
        Rectangle row = { r.x + S_PAD, r.y + 88.0f + (float)i * 148.0f, 320.0f, 140.0f };
        if (S->store_tab == 0) {
            draw_shop_upgrade(game, i, row);
        } else if (i < n) {
            draw_shop_branch(game, children[i], row);
        } else if (i == 0) {
            ui_panel(row, R_CARD, COL_PANEL, Fade(COL_LINE, 0.5f));
            ui_text_center("本版已到最后阶段", row.x + row.width * 0.5f, row.y + 50.0f,
                           F_BODY, COL_DIM);
            ui_text_center("可以结束本轮，把积累换成 EP", row.x + row.width * 0.5f, row.y + 72.0f,
                           F_CAPTION, COL_FAINT);
        } else {
            /* 锁定占位：让空档读成“待解锁的分支”，而不是漏画的空白 */
            ui_panel(row, R_CARD, Fade(COL_SURFACE_OFF, 0.55f), Fade(COL_LINE, 0.28f));
            Rectangle tile = { row.x + 12.0f, row.y + 12.0f, 56.0f, 56.0f };
            ui_panel(tile, 10.0f, Fade(COL_SURFACE_OFF, 0.7f), Fade(COL_LINE, 0.30f));
            ui_text_center("?", tile.x + 28.0f, tile.y + 16.0f, F_TITLE, COL_FAINT);
            ui_text("未解锁的分支", row.x + 82.0f, row.y + 14.0f, F_LABEL, Fade(COL_DIM, 0.75f));
            ui_text("继续积累生物量、推进阶段后开放", row.x + 82.0f, row.y + 38.0f,
                    F_CAPTION, COL_FAINT);
        }
    }

    /* 洞见购买按钮由 draw_main 使用同一布局矩形绘制。 */
}

/* ================= 底部动作条 ================= */

static void draw_action_button(UiButton *b, int icon, const char *title, const char *sub,
                               bool hot, Color accent)
{
    float hv = btn_hover_step(b, hot);
    Rectangle r = b->rect;
    r.y -= hv * 1.5f;

    if (hot && b->enabled) ui_halo(r, R_BTN, accent, 0.12f);
    ui_panel(r, R_BTN, ui_color_mix(COL_PANEL_HI, COL_PANEL_HOT, hv),
             ui_color_mix(Fade(accent, 0.30f), accent, hv));

    Rectangle tile = { r.x + 8.0f, r.y + 8.0f, 32.0f, 32.0f };
    ui_panel(tile, 8.0f, Fade(accent, 0.12f), Fade(accent, 0.35f));
    draw_icon(icon, (Vector2){ tile.x + 16.0f, tile.y + 16.0f }, 11.0f,
              b->enabled ? accent : COL_FAINT, g_ui.time);

    ui_text_strong(title, r.x + 48.0f, r.y + 8.0f, F_LABEL, b->enabled ? COL_TEXT : COL_FAINT);
    if (sub && sub[0]) {
        ui_text(sub, r.x + 48.0f, r.y + 28.0f, F_CAPTION, b->enabled ? COL_DIM : COL_FAINT);
    }
}

/* 底栏右侧的“新闻条”：Cookie Clicker 那种一句一句轮播的世界旁白。
 * 文案全部由当前状态推导（不写死、不编数据），每 9 秒换一条；
 * 有提示条时整块让位（提示条由 ui.c 画在同一区域）。 */
static void draw_ticker(const GameState *game)
{
    if (g_ui.toast_t > 0.0f) return;

    char msg[220];
    int main_species = game->run.active_species_count > 0 ? game->run.active_species[0].species_id : 0;
    int stage = ua_species_stage(main_species);
    int idx = (int)(g_ui.time / 9.0f) % 7;

    switch (idx) {
    case 0:
        snprintf(msg, sizeof(msg), "第 %d 纪元 · %s 正处在阶段 %d，%s",
                 game->run.era, ua_species_name(main_species), stage,
                 game->run.era >= 5 ? "宇宙的尺度开始向你展开" : "演化还在缓慢积累");
        break;
    case 1: {
        char num[32], next[32];
        ui_fmt_num(game->run.biomass, num, (int)sizeof(num));
        int lvl = ua_milestone_level(game);
        int top = ua_milestone_count() - 1;
        if (lvl >= top) {
            snprintf(msg, sizeof(msg), "生物量 %s · 里程碑已满档 B%d", num, lvl);
        } else {
            ui_fmt_num(ua_milestone_threshold(lvl + 1), next, (int)sizeof(next));
            snprintf(msg, sizeof(msg), "生物量 %s · 距 B%d（%s）还有一段路", num, lvl + 1, next);
        }
        break;
    }
    case 2: {
        double rem = 0.0, tot = 1.0;
        bool in_event = false;
        char t[32];
        ua_environment_timer(game, &rem, &tot, &in_event);
        ui_fmt_time(rem, t, (int)sizeof(t));
        if (in_event) {
            snprintf(msg, sizeof(msg), "环境警报：%s，剩余 %s，耐性低的物种正在掉种群",
                     ua_environment_name(game->run.current_environment_id), t);
        } else {
            snprintf(msg, sizeof(msg), "海水平静，%s 后下一次扰动会抵达", t);
        }
        break;
    }
    case 3: {
        double rates[3];
        char a[32], b[32];
        ua_total_rates(game, rates);
        ui_fmt_rate(rates[0], a, (int)sizeof(a));
        ui_fmt_num(ua_click_atp(game), b, (int)sizeof(b));
        snprintf(msg, sizeof(msg), "每秒 %s ATP · 每次点击培养皿 %s ATP", a, b);
        break;
    }
    case 4: {
        int muts = 0;
        if (game->run.active_species_count > 0) {
            for (int i = 0; i < MAX_MUTATIONS; ++i)
                if (game->run.active_species[0].mutations[i]) ++muts;
        }
        snprintf(msg, sizeof(msg), "已获得 %d 项突变，DNA 是唯一能改写形态的资源", muts);
        break;
    }
    case 5: {
        int lit = 0;
        for (int i = 0; i < SPECIES_COUNT; ++i) if (game->legacy.codex_species[i]) ++lit;
        snprintf(msg, sizeof(msg), "图鉴已点亮 %d / %d 个物种，进化树上 %d 环正在发光",
                 lit, SPECIES_COUNT, stage + 1);
        break;
    }
    default:
        snprintf(msg, sizeof(msg), "从一枚细胞到宇宙尽头 —— 这是第 %d 次演化",
                 game->run.era);
        break;
    }

    /* 换条时淡入，读起来不会突然跳 */
    float ft = fmodf(g_ui.time, 9.0f) / 9.0f;
    float a = ft < 0.06f ? ft / 0.06f : 1.0f;
    if (a < 0.0f) a = 0.0f;

    Vector2 dm = { 688.0f, 670.0f };
    ui_glow(dm, 12.0f, COL_ACCENT, 0.45f * a);
    DrawPoly(dm, 4, 3.6f, 45.0f, Fade(COL_ACCENT, a));
    ui_text(msg, 702.0f, 663.0f, F_BODY, Fade(COL_DIM, a));

    ui_text("空格 采集 · 1/2/3 升级 · 4 容量 · 5 储备 · 6 科技 · M 突变 · E 结算 · C 图鉴 · T 星图",
            702.0f, 688.0f, F_CAPTION, COL_FAINT);
}

static void draw_bottom_bar(const GameState *game)
{
    UIState *S = &g_ui;
    DrawRectangle(0, 648, UI_W, 72, Fade(COL_CHROME, 0.94f));
    DrawRectangleGradientV(0, 648, UI_W, 72, Fade(COL_BG_DEEP, 0.0f), Fade(COL_BG_DEEP, 0.55f));
    DrawLineEx((Vector2){ 0, 648.5f }, (Vector2){ UI_W, 648.5f }, 1.0f, Fade(COL_ACCENT, 0.20f));

    UiButton *mut = btn_find(S, UI_BTN_OPEN_MUTATION, -1);
    UiButton *spe = btn_find(S, UI_BTN_OPEN_SPECIATION, -1);
    UiButton *end = btn_find(S, UI_BTN_OPEN_END_ERA, -1);

    if (mut) {
        char sub[48];
        if (game->run.mutation_offer.active) snprintf(sub, sizeof(sub), "%d 个候选",
                                                     game->run.mutation_offer.choice_count);
        else snprintf(sub, sizeof(sub), "消耗 DNA");
        draw_action_button(mut, ICON_MUT, "主动突变", sub, is_hot(S, mut), COL_DNA);
    }
    if (spe) draw_action_button(spe, ICON_BRANCH, "物种分化", "消耗生物量", is_hot(S, spe), COL_BIOMASS);
    if (end) draw_action_button(end, ICON_END, "结束本轮演化", "预览 Evolution Points",
                               is_hot(S, end), COL_ACCENT);

    if (g_ui.toast_t <= 0.0f) {
        draw_ticker(game);
    }
}

/* ================= 主界面布局 ================= */

static void build_main(UiLayout *L, const GameState *game, const UIState *S)
{
    int children[SPECIES_COUNT];
    int child_n = ua_speciation_children(game, children, SPECIES_COUNT);
    bool can_speciate = child_n > 0;

    /* 底栏动作先入表：btn_find(id, -1) 取第一个同 id 按钮 */
    btn_add(L, UI_BTN_OPEN_MUTATION, (Rectangle){ 16.0f, 660.0f, 200.0f, 48.0f }, 0, 0, true);
    btn_add(L, UI_BTN_OPEN_SPECIATION, (Rectangle){ 228.0f, 660.0f, 200.0f, 48.0f }, 0, 0, can_speciate);
    btn_add(L, UI_BTN_OPEN_END_ERA, (Rectangle){ 440.0f, 660.0f, 220.0f, 48.0f }, 0, 0, true);

    /* 培养皿命中区（外接方）：与 draw_dish 的皿心/半径保持一致 */
    btn_add(L, UI_BTN_CLICK, (Rectangle){ 84.0f, 212.0f, 220.0f, 220.0f }, 0, 0, true);

    /* 商店页签 */
    btn_add(L, UI_BTN_STORE_TAB, (Rectangle){ 928.0f, 126.0f, 153.0f, 34.0f }, 0, 0, true);
    btn_add(L, UI_BTN_STORE_TAB, (Rectangle){ 1089.0f, 126.0f, 153.0f, 34.0f }, 1, 1, true);

    /* 情报面板页签：位置必须与 draw_info_panel 里的绘制矩形逐像素一致 */
    {
        float tab_w = 62.0f, gap = 4.0f;
        float tabs_x = 388.0f + 516.0f - S_PAD - (tab_w * 3.0f + gap * 2.0f);
        for (int i = 0; i < 3; ++i) {
            btn_add(L, UI_BTN_INFO_TAB,
                    (Rectangle){ tabs_x + (tab_w + gap) * (float)i, 507.0f, tab_w, H_CHIP },
                    i, 0, true);
        }
    }

    if (S->store_tab == 1) {
        int ch[SPECIES_COUNT];
        int n = ua_speciation_children(game, ch, SPECIES_COUNT);
        for (int i = 0; i < n && i < 3; ++i) {
            btn_add(L, UI_BTN_OPEN_SPECIATION,
                    (Rectangle){ 928.0f, 172.0f + (float)i * 148.0f, 320.0f, 140.0f },
                    ch[i], 0, true);
        }
    } else {
        for (int i = 0; i < 3; ++i) {
            double price = ua_run_upgrade_cost(game, (RunUpgradeType)i);
            bool affordable = isfinite(price) &&
                ((i == UPGRADE_GENOME) ? (game->run.biomass >= price) : (game->run.atp >= price));
            btn_add(L, UI_BTN_UPGRADE,
                    (Rectangle){ 928.0f, 172.0f + (float)i * 148.0f, 320.0f, 140.0f },
                    i, 0, affordable);
        }
    }

    double insight_cost = ua_insight_cost(game);
    btn_add(L, UI_BTN_BUY_INSIGHT, (Rectangle){ 928.0f, 612.0f, 320.0f, 28.0f },
            0, 0, isfinite(insight_cost) && game->run.knowledge >= insight_cost);
    if (game->run.buff_visible)
        btn_add(L, UI_BTN_COLLECT_BUFF, (Rectangle){ 24.0f, 446.0f, 340.0f, 40.0f }, 0, 0, true);

    /* 顶栏三入口：进化树 / 突变 / 图鉴。总宽 230 = 74×3 + 4×2，右缘对齐 1258。
     * 74 宽放不下 16px 的三字标签，draw_top_button 会自动降到 F_CAPTION。 */
    btn_add(L, UI_BTN_OPEN_TREE,     (Rectangle){ 1028.0f, 14.0f, 74.0f, 48.0f }, 0, 0, true);
    btn_add(L, UI_BTN_OPEN_MUT_LIST, (Rectangle){ 1106.0f, 14.0f, 74.0f, 48.0f }, 0, 0, true);
    btn_add(L, UI_BTN_OPEN_CODEX,    (Rectangle){ 1184.0f, 14.0f, 74.0f, 48.0f }, 0, 0, true);

    /* 物种卡的突变 chips 整行可点：点开“突变图谱”看本轮全部突变 */
    btn_add(L, UI_BTN_OPEN_MUT_LIST,
            (Rectangle){ 16.0f + S_PAD + 30.0f, 496.0f + 106.0f, 356.0f - S_PAD * 2.0f - 30.0f, 22.0f },
            1, 1, true);
}

static void draw_main(const GameState *game, const UIState *state, const ActionResult *last)
{
    (void)last;

    draw_top_bar(game);
    draw_dish(game);
    draw_species_card(game, 0);
    draw_env_banner(game);
    draw_stage(game);
    draw_milestone(game);
    draw_info_panel(game);
    draw_shop(game);
    draw_bottom_bar(game);
    UiButton *insight = btn_find(state, UI_BTN_BUY_INSIGHT, -1);
    if (insight) {
        char cost[32], label[96];
        ui_fmt_num(ua_insight_cost(game), cost, sizeof cost);
        snprintf(label, sizeof label, "洞见 Lv.%d · %s 理念 [7]", game->run.insight_levels, cost);
        ui_btn_draw(insight, label, NULL, is_hot(state, insight));
    }
    UiButton *reward = btn_find(state, UI_BTN_COLLECT_BUFF, -1);
    if (reward) {
        char label[96];
        snprintf(label, sizeof label, "领取奖励：%s [B]", buffs_type_name(game->run.buff_visible_type));
        ui_btn_draw(reward, label, NULL, is_hot(state, reward));
    } else if (game->run.buff_active_time_left > 0.0) {
        char label[96];
        snprintf(label, sizeof label, "%s · 剩余 %.0f 秒", buffs_type_name(game->run.buff_active_type),
                 game->run.buff_active_time_left);
        ui_text_center(label, 194.0f, 458.0f, F_BODY, COL_ACCENT);
    }
}

/* ================= End of Era ================= */

static void build_end_era(UiLayout *L, const GameState *game, const UIState *S)
{
    (void)S;
    btn_add(L, UI_BTN_CANCEL_END_ERA, (Rectangle){ 290.0f, 556.0f, 340.0f, H_PRIMARY }, 0, 0, true);
    btn_add(L, UI_BTN_CONFIRM_END_ERA, (Rectangle){ 650.0f, 556.0f, 340.0f, H_PRIMARY }, 0, 0,
            !game->run.reward_claimed);
}

static void draw_primary_btn(UiButton *b, const char *title, const char *sub, bool hot)
{
    Rectangle r = b->rect;
    bool en = b->enabled;
    if (en) {
        ui_glow((Vector2){ r.x + r.width * 0.5f, r.y + r.height * 0.5f },
                r.height * 1.5f, COL_ACCENT, 0.10f + 0.08f * (hot ? 1.0f : 0.0f));
    }
    Rectangle rr = r;
    if (en && hot) rr.y -= 1.0f;

    DrawRectangleRounded(rr, 0.30f, 10, en ? ui_color_mix(COL_ACCENT, COL_PANEL_HOT, 0.08f)
                                            : COL_SURFACE_OFF);
    if (en) {
        DrawRectangleRounded((Rectangle){ rr.x + 1.0f, rr.y + 1.0f, rr.width - 2.0f, rr.height * 0.46f },
                             0.62f, 10, Fade(WHITE, 0.17f));
        DrawRectangleRoundedLinesEx(rr, 0.30f, 10, 1.0f, Fade(ui_color_mix(COL_ACCENT, WHITE, 0.25f), 0.9f));
    } else {
        DrawRectangleRoundedLinesEx(rr, 0.30f, 10, 1.0f, Fade(COL_LINE, 0.6f));
    }

    Color tc = en ? (Color){ 30, 19, 5, 255 } : COL_FAINT;
    float tw = ui_text_w(title, F_LABEL);
    ui_text(title, rr.x + rr.width * 0.5f - tw * 0.5f, rr.y + 7.0f, F_LABEL, tc);
    if (sub) {
        float sw = ui_text_w(sub, F_CAPTION);
        ui_text(sub, rr.x + rr.width * 0.5f - sw * 0.5f, rr.y + 25.0f, F_CAPTION,
                en ? Fade(tc, 0.70f) : COL_FAINT);
    }
}

static void draw_end_era(const GameState *game, const UIState *S)
{
    DrawRectangle(0, 0, UI_W, UI_H, Fade(COL_BG_DEEP, 0.55f));
    atm_draw_live();

    Rectangle panel = { 250.0f, 60.0f, 780.0f, 600.0f };
    ui_halo(panel, R_PANEL, COL_ACCENT, 0.10f);
    ui_panel(panel, R_PANEL, COL_PANEL, Fade(COL_ACCENT, 0.35f));

    ui_text_strong("结束本轮演化", panel.x + 36.0f, panel.y + 24.0f, F_DISPLAY, COL_TEXT);

    char sub[180], tbuf[32];
    ui_fmt_time(game->run.elapsed_time, tbuf, (int)sizeof(tbuf));
    snprintf(sub, sizeof(sub), "本轮持续 %s · 第 %d 纪元 · 确认后本轮冻结，Evolution Points 只入账一次",
             tbuf, game->run.era);
    ui_text(sub, panel.x + 36.0f, panel.y + 66.0f, F_BODY, COL_DIM);

    ui_divider(panel.x + 36.0f, panel.x + panel.width - 36.0f, panel.y + 96.0f, Fade(COL_LINE, 0.7f));

    int c[5];
    ua_era_breakdown(game, c);
    EraReward reward;
    ua_era_reward(game, &reward);

    struct {
        const char *label;
        const char *var;
        const char *rule;
        int count;
        int points;
        Color color;
        int icon;
    } rows[5] = {
        { "主要进化阶段", "E", "每阶段 2 EP",         c[0], reward.era_points,      COL_ACCENT,  ICON_STAGE },
        { "解锁突变",     "M", "每 3 个 1 EP",        c[1], reward.mutation_points, COL_DNA,     ICON_MUT },
        { "发现物种",     "S", "每 2 个 1 EP",        c[2], reward.species_points,  COL_BIOMASS, ICON_POP },
        { "度过危机",     "C", "每个 1 EP，最多 3",   c[3], reward.crisis_points,   COL_RARE,    ICON_ENV },
        { "生物量里程碑", "B", "每档 1 EP",           c[4], reward.biomass_points,  COL_EP,      ICON_BIOMASS },
    };

    for (int i = 0; i < 5; ++i) {
        Rectangle row = { panel.x + 36.0f, panel.y + 112.0f + (float)i * 58.0f,
                          panel.width - 72.0f, 50.0f };
        ui_panel(row, R_BTN, COL_PANEL_HI, Fade(COL_LINE, 0.6f));
        Rectangle badge = { row.x + 10.0f, row.y + 10.0f, 30.0f, 30.0f };
        ui_panel(badge, 8.0f, Fade(rows[i].color, 0.12f), Fade(rows[i].color, 0.30f));
        draw_icon(rows[i].icon, (Vector2){ badge.x + 15.0f, badge.y + 15.0f }, 10.0f,
                  rows[i].color, g_ui.time);

        ui_text(rows[i].label, row.x + 50.0f, row.y + 15.0f, F_LABEL, COL_TEXT);

        /* 变量 = 计数（避免把 “2 × E = 8” 误读成算式） */
        char mid[64];
        snprintf(mid, sizeof(mid), "%s = %d", rows[i].var, rows[i].count);
        float mw = ui_text_w(mid, F_LABEL) + 20.0f;
        Rectangle chip = { row.x + 206.0f, row.y + 13.0f, mw, 24.0f };
        ui_panel(chip, 6.0f, Fade(rows[i].color, 0.12f), Fade(rows[i].color, 0.32f));
        ui_text_center(mid, chip.x + chip.width * 0.5f, chip.y + 5.0f, F_LABEL, rows[i].color);

        ui_text(rows[i].rule, chip.x + chip.width + 14.0f, row.y + 16.0f, F_CAPTION, COL_FAINT);

        char pts[32];
        snprintf(pts, sizeof(pts), "+%d", rows[i].points);
        ui_text_right(pts, row.x + row.width - 18.0f, row.y + 12.0f, F_TITLE, rows[i].color);
    }

    Rectangle total = { panel.x + 36.0f, panel.y + 410.0f, panel.width - 72.0f, 80.0f };
    ui_panel(total, R_CARD, COL_EP_SURFACE, Fade(COL_EP, 0.45f));
    ui_text("本轮 Evolution Points", total.x + 22.0f, total.y + 14.0f, F_LABEL, COL_TEXT);
    char note[96];
    snprintf(note, sizeof(note), "现有 %d EP · 未使用部分永久保留", game->legacy.evolution_points);
    ui_text(note, total.x + 22.0f, total.y + 40.0f, F_CAPTION, COL_DIM);

    char tot[32];
    snprintf(tot, sizeof(tot), "%d", reward.total_points);
    ui_text_right(tot, total.x + total.width - 25.0f, total.y + 12.0f, F_HERO, COL_EP);

    if (game->run.reward_claimed) {
        ui_text_center("本轮奖励已领取，只能开始新轮", panel.x + panel.width * 0.5f,
                       panel.y + 552.0f, F_BODY, COL_BAD);
    }

    UiButton *cancel = btn_find(S, UI_BTN_CANCEL_END_ERA, -1);
    UiButton *confirm = btn_find(S, UI_BTN_CONFIRM_END_ERA, -1);
    if (cancel) ui_btn_draw(cancel, "返回本轮", "继续成长", is_hot(S, cancel));
    if (confirm) draw_primary_btn(confirm, "确认结束 · 结算 EP", "本轮冻结，进入 Legacy", is_hot(S, confirm));
}

/* ================= Legacy ================= */

static void build_legacy(UiLayout *L, const GameState *game, const UIState *S)
{
    (void)S;
    for (int i = 0; i < 4; ++i) {
        Rectangle r = { 60.0f + (float)(i % 2) * 590.0f, 122.0f + (float)(i / 2) * 218.0f,
                        570.0f, 206.0f };
        bool maxed = game->legacy.levels[i] >= ua_legacy_max_level();
        bool afford = game->legacy.evolution_points >= ua_legacy_cost(i, game->legacy.levels[i]);
        btn_add(L, UI_BTN_BUY_LEGACY, r, i, 0, !maxed && afford);
    }
    btn_add(L, UI_BTN_START_NEW_RUN, (Rectangle){ 480.0f, 566.0f, 320.0f, H_PRIMARY }, 0, 0, true);
}

static void draw_legacy(const GameState *game, const UIState *S)
{
    DrawRectangle(0, 0, UI_W, UI_H, Fade(COL_BG_DEEP, 0.55f));
    atm_draw_live();

    ui_text_strong("Evolution Legacy", 60.0f, 28.0f, F_DISPLAY, COL_TEXT);
    ui_text("把上一轮的成果投进永久强化，新轮从原始生命重新演化", 60.0f, 66.0f, F_BODY, COL_DIM);

    Rectangle bal = { 960.0f, 26.0f, 260.0f, 64.0f };
    ui_panel(bal, R_CARD, COL_EP_SURFACE, Fade(COL_EP, 0.50f));
    ui_halo(bal, R_CARD, COL_EP, 0.08f);
    ui_text("Evolution Points", bal.x + S_PAD, bal.y + 8.0f, F_CAPTION, COL_DIM);
    char ep[32];
    snprintf(ep, sizeof(ep), "%d", game->legacy.evolution_points);
    ui_text_right(ep, bal.x + bal.width - S_PAD, bal.y + 20.0f, F_DISPLAY, COL_EP);

    static const int branch_icon[4] = { ICON_META, ICON_MUT, ICON_ENV, ICON_BRANCH };
    Color branch_col[4] = { COL_ATP, COL_DNA, COL_TEAL, COL_BIOMASS };

    for (int i = 0; i < 4; ++i) {
        Rectangle r = { 60.0f + (float)(i % 2) * 590.0f, 122.0f + (float)(i / 2) * 218.0f,
                        570.0f, 206.0f };
        UiButton *b = btn_find(S, UI_BTN_BUY_LEGACY, i);
        bool hot = is_hot(S, b);
        int level = game->legacy.levels[i];
        int max_level = ua_legacy_max_level();
        bool maxed = level >= max_level;
        int cost = maxed ? 0 : ua_legacy_cost(i, level);
        float hv = btn_hover_step(b, hot);
        bool usable = b && b->enabled;

        Rectangle rr = r;
        rr.y += hover_lift(hv);
        if (usable && hv > 0.02f) ui_halo(rr, R_CARD, COL_EP, 0.12f * hv);
        ui_panel(rr, R_CARD, maxed ? COL_SURFACE_OFF : ui_color_mix(COL_PANEL, COL_PANEL_HOT, hv),
                 (usable && hv > 0.02f) ? COL_EP : Fade(COL_LINE, 0.6f));
        DrawRectangleRounded((Rectangle){ rr.x + 6.0f, rr.y + 10.0f, 3.0f, rr.height - 20.0f },
                             1.0f, 4, Fade(branch_col[i], 0.85f));

        Rectangle tile = { rr.x + 16.0f, rr.y + 14.0f, 44.0f, 44.0f };
        icon_badge(tile, branch_icon[i], branch_col[i], S->time);

        ui_text_strong(ua_legacy_name(i), rr.x + 72.0f, rr.y + 16.0f, F_TITLE, COL_TEXT);

        char lv[32];
        snprintf(lv, sizeof(lv), "Lv.%d / %d", level, max_level);
        ui_text_right(lv, rr.x + rr.width - 20.0f, rr.y + 18.0f, F_LABEL, COL_DIM);

        for (int k = 0; k < max_level; ++k) {
            Rectangle pip = { rr.x + 72.0f + (float)k * 15.0f, rr.y + 44.0f, 10.0f, 5.0f };
            DrawRectangleRounded(pip, 1.0f, 4, k < level ? branch_col[i] : COL_BAR_TRACK);
        }

        char cur[160], next[160];
        ua_legacy_effect(game, i, level, cur, (int)sizeof(cur));
        ua_legacy_effect(game, i, maxed ? max_level : level + 1, next, (int)sizeof(next));

        ui_text("当前", rr.x + 16.0f, rr.y + 72.0f, F_CAPTION, COL_FAINT);
        ui_text(level > 0 ? cur : "尚未强化", rr.x + 58.0f, rr.y + 71.0f, F_BODY,
                level > 0 ? COL_DIM : COL_FAINT);

        ui_text("下一级", rr.x + 16.0f, rr.y + 98.0f, F_CAPTION, COL_FAINT);
        ui_text(maxed ? "已满级" : next, rr.x + 58.0f, rr.y + 97.0f, F_BODY, COL_TEXT);

        /* 大号幽灵等级数字，填补卡片下半部并制造纵深 */
        char ghost[16];
        snprintf(ghost, sizeof(ghost), "%02d", level > 0 ? (level % 100) : 0);
        float gw = ui_text_w(ghost, F_HERO) * 1.4f;
        ui_text(ghost, rr.x + rr.width - 26.0f - gw, rr.y + 116.0f, F_HERO * 1.4f,
                Fade(branch_col[i], maxed ? 0.16f : 0.10f));

        if (maxed) {
            ui_text_right("MAX", rr.x + rr.width - 20.0f, rr.y + 140.0f, F_LABEL, COL_EP);
        } else {
            char price[48];
            snprintf(price, sizeof(price), "%d EP", cost);
            Rectangle pill = { rr.x + 16.0f, rr.y + 134.0f, 130.0f, H_INLINE };
            ui_panel(pill, R_CHIP(H_INLINE), usable ? Fade(COL_EP, 0.14f) : COL_SURFACE_OFF,
                     usable ? Fade(COL_EP, 0.55f) : Fade(COL_LINE, 0.7f));
            ui_text(price, pill.x + 12.0f, pill.y + 5.0f, F_BODY, usable ? COL_EP : COL_FAINT);
            char step[64];
            snprintf(step, sizeof(step), "%s", usable ? "点击升级" : "EP 不足");
            ui_text(step, pill.x + pill.width + 12.0f, pill.y + 6.0f, F_CAPTION,
                    usable ? COL_DIM : COL_FAINT);
        }
    }

    UiButton *start = btn_find(S, UI_BTN_START_NEW_RUN, -1);
    if (start) draw_primary_btn(start, "开始新轮演化", "保留全部 Legacy 强化", is_hot(S, start));

    /* 生涯档案：填掉按钮右侧的空白，顺手把跨轮数据摆出来 */
    {
        int sp = 0, mu = 0;
        for (int i = 0; i < SPECIES_COUNT; ++i) if (game->legacy.codex_species[i]) sp++;
        for (int i = 0; i < MUTATION_COUNT; ++i) if (game->legacy.codex_mutations[i]) mu++;

        Rectangle box = { 866.0f, 554.0f, 354.0f, 88.0f };
        ui_panel(box, R_CARD, Fade(COL_PANEL, 0.9f), Fade(COL_LINE, 0.5f));
        ui_text("生涯档案", box.x + 14.0f, box.y + 7.0f, F_CAPTION, COL_FAINT);
        ui_divider(box.x + 14.0f, box.x + box.width - 14.0f, box.y + 27.0f, Fade(COL_LINE, 0.55f));

        char b1[64], b2[64], b3[64], b4[64], n1[24];
        snprintf(b1, sizeof(b1), "第 %d 纪元", game->legacy.best_era > 0 ? game->legacy.best_era : game->run.era);
        snprintf(b2, sizeof(b2), "%d EP", game->legacy.best_evolution_points);
        ui_fmt_num(game->legacy.best_biomass, n1, (int)sizeof(n1));
        snprintf(b3, sizeof(b3), "%s", n1);
        snprintf(b4, sizeof(b4), "%d/%d · %d/%d", sp, SPECIES_COUNT, mu, MUTATION_COUNT);

        const char *lab[4] = { "最高纪元", "历史最佳 EP", "最高生物量", "图鉴收录" };
        const char *val[4] = { b1, b2, b3, b4 };
        for (int i = 0; i < 4; ++i) {
            float y = box.y + 31.0f + (float)i * 15.0f;
            ui_text(lab[i], box.x + 14.0f, y, F_CAPTION, COL_FAINT);
            ui_text_right(val[i], box.x + box.width - 14.0f, y, F_CAPTION, COL_DIM);
        }
    }

    ui_text_center("未使用的 EP 会保留到以后", 640.0f, 626.0f, F_CAPTION, COL_FAINT);
}

/* ================= 弹层 ================= */

/* 前向声明：build_overlay 在绘制函数之前定义，但它要用树/突变图谱的几何。 */
static Rectangle tree_panel_rect(void);
static Rectangle mut_list_panel_rect(void);
static void tree_layout(Vector2 node[SPECIES_COUNT], int parent_of[SPECIES_COUNT],
                        int sib[SPECIES_COUNT]);
static float tree_node_r(void);

static Rectangle mutation_panel_rect(const GameState *game)
{
    int n = game->run.mutation_offer.active ? game->run.mutation_offer.choice_count : 0;
    if (n < 1) n = 1;
    float h = 96.0f + n * 110.0f + 60.0f;
    if (h < 300.0f) h = 300.0f;
    return (Rectangle){ 320.0f, (UI_H - h) * 0.5f, 640.0f, h };
}

/* Four rows per page keep every expanded route on the logical canvas. */
static Rectangle speciation_panel_rect(const GameState *game)
{
    (void)game;
    return (Rectangle){ 300.0f, 52.0f, 680.0f, 616.0f };
}

static int speciation_page_start(const UIState *state, int total)
{
    int last = total > 0 ? (total - 1) / UI_SPECIATION_PAGE_SIZE : 0;
    int page = state->speciation_page;
    if (page < 0) page = 0;
    if (page > last) page = last;
    return page * UI_SPECIATION_PAGE_SIZE;
}

static Rectangle codex_panel_rect(int tab, int achievement_count)
{
    (void)tab;
    (void)achievement_count;
    return (Rectangle){ 60.0f, 58.0f, 1160.0f, 604.0f };
}

static Rectangle codex_detail_rect(void)
{
    return (Rectangle){ 400.0f, 70.0f, 480.0f, 580.0f };
}

static int codex_page_first(const UIState *state)
{
    int count = codex_count((CodexKind)state->codex_tab);
    int last = count > 0 ? (count - 1) / UI_CODEX_PAGE_SIZE : 0;
    int page = state->codex_page;
    if (page < 0) page = 0;
    if (page > last) page = last;
    return page * UI_CODEX_PAGE_SIZE;
}

static bool codex_found(const GameState *game, int kind, int id)
{
    if (!codex_entry((CodexKind)kind, id)) return false;
    return kind == CODEX_SPECIES
         ? (game->legacy.codex_species[id] || game->run.discovered_species[id])
         : (game->legacy.codex_mutations[id] || game->run.discovered_mutations[id]);
}

static Rectangle codex_cell_rect(Rectangle panel, int slot)
{
    return (Rectangle){ panel.x + 24.0f + (float)(slot % 4) * 281.0f,
                        panel.y + 112.0f + (float)(slot / 4) * 140.0f, 269.0f, 128.0f };
}

static void build_overlay(UiLayout *L, const GameState *game, const UIState *S)
{
    btn_add(L, UI_BTN_BACKDROP, (Rectangle){ 0, 0, UI_W, UI_H }, 0, 0, true);

    switch (S->overlay) {
    case UI_OVL_MUTATION: {
        Rectangle panel = mutation_panel_rect(game);
        for (int i = 0; i < game->run.mutation_offer.choice_count && i < MAX_MUTATION_CHOICES; ++i) {
            bool afford = game->run.dna >= game->run.mutation_offer.dna_costs[i];
            btn_add(L, UI_BTN_CHOOSE_MUTATION,
                    (Rectangle){ panel.x + 24.0f, panel.y + 84.0f + (float)i * 110.0f,
                                 panel.width - 48.0f, 98.0f },
                    game->run.mutation_offer.mutation_ids[i], i, afford);
        }
        btn_add(L, UI_BTN_CLOSE_OVERLAY,
                (Rectangle){ panel.x + (panel.width - 180.0f) * 0.5f,
                             panel.y + panel.height - 52.0f, 180.0f, H_SECONDARY }, 0, 0, true);
        if (!game->run.mutation_offer.active) {
            btn_add(L, UI_BTN_OPEN_MUTATION,
                    (Rectangle){ panel.x + (panel.width - 220.0f) * 0.5f,
                                 panel.y + panel.height - 104.0f, 220.0f, H_PRIMARY }, 0, 0, true);
        }
        break;
    }
    case UI_OVL_SPECIATION: {
        Rectangle panel = speciation_panel_rect(game);
        int children[SPECIES_COUNT];
        int n = ua_speciation_children(game, children, SPECIES_COUNT);
        int first = speciation_page_start(S, n);
        for (int i = first; i < n && i < first + UI_SPECIATION_PAGE_SIZE; ++i) {
            bool ok = ua_speciation_check(game, children[i]) == ACTION_OK
                   && game->run.biomass >= ua_speciation_cost(game, children[i]);
            btn_add(L, UI_BTN_SPECIATE,
                    (Rectangle){ panel.x + 24.0f, panel.y + 84.0f + (float)(i - first) * 102.0f,
                                 panel.width - 48.0f, 94.0f },
                    children[i], S->keep_parent, ok);
        }
        btn_add(L, UI_BTN_SPECIATION_PAGE,
                (Rectangle){ panel.x + 24.0f, panel.y + panel.height - 104.0f, 110.0f, 32.0f },
                0, first / UI_SPECIATION_PAGE_SIZE - 1, first > 0);
        btn_add(L, UI_BTN_SPECIATION_PAGE,
                (Rectangle){ panel.x + panel.width - 134.0f, panel.y + panel.height - 104.0f, 110.0f, 32.0f },
                1, first / UI_SPECIATION_PAGE_SIZE + 1, first + UI_SPECIATION_PAGE_SIZE < n);
        btn_add(L, UI_BTN_TOGGLE_KEEP_PARENT,
                (Rectangle){ panel.x + 24.0f, panel.y + panel.height - 56.0f, 340.0f, 40.0f },
                0, 0, true);
        btn_add(L, UI_BTN_CLOSE_OVERLAY,
                (Rectangle){ panel.x + panel.width - 150.0f, panel.y + panel.height - 52.0f,
                             126.0f, H_SECONDARY }, 0, 0, true);
        break;
    }
    case UI_OVL_CODEX: {
        Rectangle panel = codex_panel_rect(S->codex_tab, ua_achievement_count());
        btn_add(L, UI_BTN_NONE, panel, 0, 0, true);
        for (int i = 0; i < 3; ++i) {
            btn_add(L, UI_BTN_CODEX_TAB,
                    (Rectangle){ panel.x + 24.0f + (float)i * 110.0f, panel.y + 62.0f,
                                 102.0f, 32.0f }, i, i, true);
        }
        int count = codex_count((CodexKind)S->codex_tab);
        int first = codex_page_first(S);
        for (int i = first; i < count && i < first + UI_CODEX_PAGE_SIZE; ++i) {
            btn_add(L, UI_BTN_CODEX_ENTRY, codex_cell_rect(panel, i - first), i, S->codex_tab,
                    codex_found(game, S->codex_tab, i));
        }
        if (count > 0) {
            btn_add(L, UI_BTN_CODEX_PAGE,
                    (Rectangle){ panel.x + 24.0f, panel.y + 552.0f, 110.0f, 32.0f },
                    0, first / UI_CODEX_PAGE_SIZE - 1, first > 0);
            btn_add(L, UI_BTN_CODEX_PAGE,
                    (Rectangle){ panel.x + panel.width - 134.0f, panel.y + 552.0f, 110.0f, 32.0f },
                    1, first / UI_CODEX_PAGE_SIZE + 1, first + UI_CODEX_PAGE_SIZE < count);
        }
        btn_add(L, UI_BTN_CLOSE_OVERLAY,
                (Rectangle){ panel.x + panel.width - 140.0f, panel.y + 16.0f, 116.0f, 36.0f },
                0, 0, true);
        break;
    }
    case UI_OVL_CODEX_DETAIL: {
        Rectangle panel = codex_detail_rect();
        btn_add(L, UI_BTN_NONE, panel, 0, 0, true);
        btn_add(L, UI_BTN_CODEX_BACK,
                (Rectangle){ panel.x + 140.0f, panel.y + panel.height - 58.0f, 200.0f, 36.0f },
                0, 0, true);
        break;
    }
    case UI_OVL_TREE: {
        Rectangle panel = tree_panel_rect();
        /* 每个节点一个按钮：命中框比徽记大 6px（4K 下手指也不用狙击）。
         * 几何与绘制同源 —— 都从 tree_layout() 取坐标。 */
        Vector2 node[SPECIES_COUNT];
        int parent_of[SPECIES_COUNT];
        tree_layout(node, parent_of, NULL);
        float nr = tree_node_r();
        float hit = nr + 6.0f;
        for (int i = 0; i < SPECIES_COUNT; ++i) {
            btn_add(L, UI_BTN_TREE_NODE,
                    (Rectangle){ node[i].x - hit, node[i].y - hit, hit * 2.0f, hit * 2.0f },
                    i, 0, true);
        }
        btn_add(L, UI_BTN_CLOSE_OVERLAY,
                (Rectangle){ panel.x + panel.width - 140.0f, panel.y + 16.0f, 116.0f, 36.0f },
                0, 0, true);
        break;
    }
    case UI_OVL_MUT_LIST: {
        Rectangle panel = mut_list_panel_rect();
        btn_add(L, UI_BTN_CLOSE_OVERLAY,
                (Rectangle){ panel.x + panel.width - 140.0f, panel.y + 16.0f, 116.0f, 36.0f },
                0, 0, true);
        break;
    }
    default:
        break;
    }
}

static void draw_overlay_backdrop(float alpha)
{
    DrawRectangle(0, 0, UI_W, UI_H, Fade(COL_BG_DEEP, alpha));
}

/* 突变纹章：每个突变一个“基因花”。
   瓣数 / 环数 / 核形 / 色相 / 生长方向都由 mutation_id 派生，保证连排时不重样。 */
static void draw_mutation_sigil_scaled(Vector2 c, int mutation_id, Color col, bool rare, float t, float scale)
{
    int arms = 3 + (mutation_id % 7);
    int rings = 1 + ((mutation_id / 2) % 3);
    int arch = mutation_id % 4;
    float rr = (rare ? 25.0f : 20.0f) * scale;
    float base = rare ? 1.0f : 0.82f;
    float rot = ((float)((mutation_id * 37) % 360)) * 0.01745f;

    Color tint = col;
    switch (mutation_id % 4) {
    case 1: tint = ui_color_mix(col, COL_TEAL, 0.40f); break;
    case 2: tint = ui_color_mix(col, COL_EP, 0.34f); break;
    case 3: tint = ui_color_mix(col, COL_BIOMASS, 0.30f); break;
    default: break;
    }

    ui_glow(c, rr * 1.7f, tint, base * 0.95f);
    tint = ui_color_mix(tint, WHITE, 0.12f);

    /* 同心环：1~3 圈 */
    for (int i = 0; i < rings; ++i) {
        float r = rr * (0.42f + 0.26f * (float)i);
        DrawRing(c, r - 1.8f, r, 0.0f, 360.0f, 56, Fade(tint, base * (0.66f - 0.12f * (float)i)));
    }

    /* 核：多边形，边数随 id 变化 */
    int sides = 3 + (mutation_id % 5);
    DrawPoly(c, sides, rr * 0.30f, (rot + t * 0.20f) * 57.3f, Fade(tint, base * 0.95f));

    for (int i = 0; i < arms; ++i) {
        float a = (float)i * (2.0f * PI / (float)arms) + rot + t * 0.22f;
        float along[4] = { 0.40f, 0.60f, 0.80f, 1.00f };
        if (arch == 0) {
            /* 尖刺 + 端点球 */
            Vector2 p0 = { c.x + cosf(a) * rr * 0.26f, c.y + sinf(a) * rr * 0.26f };
            Vector2 p1 = { c.x + cosf(a) * rr, c.y + sinf(a) * rr };
            DrawLineEx(p0, p1, 1.2f, Fade(tint, base * 0.70f));
            DrawCircleV(p1, rr * (rare ? 0.13f : 0.10f), Fade(tint, base));
        } else if (arch == 1) {
            /* 瓣：沿该方向的一串递减圆点 */
            for (int k = 0; k < 4; ++k) {
                float d = rr * along[k];
                float s = rr * (0.16f - 0.022f * (float)k);
                DrawCircleV((Vector2){ c.x + cosf(a) * d, c.y + sinf(a) * d },
                            s, Fade(tint, base * (0.9f - 0.14f * (float)k)));
            }
        } else if (arch == 2) {
            /* 折线臂：外段有 12° 偏移，像旋转的菌丝 */
            Vector2 p0 = { c.x + cosf(a) * rr * 0.30f, c.y + sinf(a) * rr * 0.30f };
            Vector2 pm = { c.x + cosf(a + 0.22f) * rr * 0.68f, c.y + sinf(a + 0.22f) * rr * 0.68f };
            Vector2 p1 = { c.x + cosf(a) * rr, c.y + sinf(a) * rr };
            DrawLineEx(p0, pm, 1.2f, Fade(tint, base * 0.62f));
            DrawLineEx(pm, p1, 1.0f, Fade(tint, base * 0.80f));
            DrawCircleV(p1, rr * 0.07f, Fade(tint, base * 0.9f));
        } else {
            /* 三角叶：臂根窄、尖端宽 */
            Vector2 p0 = { c.x + cosf(a) * rr * 0.34f, c.y + sinf(a) * rr * 0.34f };
            Vector2 p1 = { c.x + cosf(a - 0.16f) * rr * 0.98f, c.y + sinf(a - 0.16f) * rr * 0.98f };
            Vector2 p2 = { c.x + cosf(a + 0.16f) * rr * 0.98f, c.y + sinf(a + 0.16f) * rr * 0.98f };
            DrawTriangle(p0, p1, p2, Fade(tint, base * 0.45f));
            DrawLineEx(p0, (Vector2){ c.x + cosf(a) * rr, c.y + sinf(a) * rr }, 1.0f,
                       Fade(tint, base * 0.85f));
        }
    }

    if (rare) {
        /* 稀有：外环一圈虚线 + 更亮的核 */
        for (int i = 0; i < 12; ++i) {
            float a0 = (float)i * (2.0f * PI / 12.0f) + t * 0.30f;
            Vector2 p = { c.x + cosf(a0) * rr * 1.18f, c.y + sinf(a0) * rr * 1.18f };
            DrawCircleV(p, 1.2f, Fade(COL_EP, base * 0.75f));
        }
    }
}

static void draw_mutation_sigil(Vector2 c, int mutation_id, Color col, bool rare, float t)
{
    draw_mutation_sigil_scaled(c, mutation_id, col, rare, t, 1.0f);
}

static void draw_mutation_overlay(const GameState *game, const UIState *S)
{
    Rectangle panel = mutation_panel_rect(game);
    ui_halo(panel, R_PANEL, COL_DNA, 0.10f);
    ui_panel(panel, R_PANEL, COL_PANEL, Fade(COL_DNA, 0.30f));

    ui_text_strong("主动突变", panel.x + 24.0f, panel.y + 18.0f, F_TITLE, COL_TEXT);

    char dna[64], num[32];
    ui_fmt_num(game->run.dna, num, (int)sizeof(num));
    snprintf(dna, sizeof(dna), "DNA %s", num);
    ui_text_right(dna, panel.x + panel.width - 24.0f, panel.y + 24.0f, F_LABEL, COL_DNA);

    const char *hint = game->run.mutation_offer.active
        ? "候选已锁定：换页或读档都不会刷新，选定后清除"
        : "当前没有候选，生成一次后固定保存";
    ui_text(hint, panel.x + 24.0f, panel.y + 50.0f, F_CAPTION, COL_FAINT);

    if (!game->run.mutation_offer.active) {
        UiButton *gen = btn_find(S, UI_BTN_OPEN_MUTATION, -1);
        if (gen) ui_btn_draw(gen, "生成候选", "每个物种独立计算", is_hot(S, gen));
        UiButton *close = btn_find(S, UI_BTN_CLOSE_OVERLAY, -1);
        if (close) ui_btn_draw(close, "关闭", NULL, is_hot(S, close));
        return;
    }

    for (int i = 0; i < game->run.mutation_offer.choice_count; ++i) {
        Rectangle r = { panel.x + 24.0f, panel.y + 84.0f + (float)i * 110.0f,
                        panel.width - 48.0f, 98.0f };
        UiButton *b = btn_find(S, UI_BTN_CHOOSE_MUTATION, game->run.mutation_offer.mutation_ids[i]);
        bool hot = is_hot(S, b);
        int mid = game->run.mutation_offer.mutation_ids[i];
        bool rare = ua_mutation_is_rare(mid);
        float hv = btn_hover_step(b, hot);
        bool usable = b && b->enabled;

        if (usable && hv > 0.02f) ui_halo(r, R_CARD, rare ? COL_RARE : COL_DNA, 0.10f * hv);
        ui_panel(r, R_CARD, usable ? ui_color_mix(COL_PANEL_HI, COL_PANEL_HOT, hv) : COL_PANEL_HI,
                 rare ? Fade(COL_RARE, 0.60f) : Fade(COL_LINE, 0.6f));

        Rectangle tile = { r.x + 14.0f, r.y + 22.0f, 54.0f, 54.0f };
        ui_panel(tile, 10.0f, Fade(COL_BG_DEEP, 0.6f), Fade(rare ? COL_RARE : COL_DNA, 0.30f));
        draw_mutation_sigil((Vector2){ tile.x + 27.0f, tile.y + 27.0f }, mid,
                            rare ? COL_RARE : COL_DNA, rare, g_ui.time);

        ui_text_strong(ua_mutation_name(mid), r.x + 82.0f, r.y + 12.0f, F_TITLE, COL_TEXT);
        if (rare) {
            float nw = ui_text_w(ua_mutation_name(mid), F_TITLE);
            Rectangle tag = { r.x + 82.0f + nw + 10.0f, r.y + 16.0f,
                              ui_text_w("稀有", F_CAPTION) + 14.0f, H_CHIP };
            ui_panel(tag, R_CHIP(tag.height), Fade(COL_RARE, 0.16f), Fade(COL_RARE, 0.45f));
            ui_text_center("稀有", tag.x + tag.width * 0.5f, tag.y + 4.0f, F_CAPTION, COL_RARE);
        }

        char desc[160];
        ua_mutation_desc(mid, desc, (int)sizeof(desc));
        ui_text(desc, r.x + 82.0f, r.y + 40.0f, F_BODY, COL_DIM);

        char cost[48];
        ui_fmt_num(game->run.mutation_offer.dna_costs[i], num, (int)sizeof(num));
        snprintf(cost, sizeof(cost), "%s DNA", num);
        ui_text(cost, r.x + 82.0f, r.y + 68.0f, F_BODY, usable ? COL_DNA : COL_BAD);
        ui_text_right("点击选择", r.x + r.width - 16.0f, r.y + 70.0f, F_CAPTION, COL_FAINT);
    }

    UiButton *close = btn_find(S, UI_BTN_CLOSE_OVERLAY, -1);
    if (close) ui_btn_draw(close, "关闭", NULL, is_hot(S, close));
}

static void draw_speciation_overlay(const GameState *game, const UIState *S)
{
    Rectangle panel = speciation_panel_rect(game);
    ui_halo(panel, R_PANEL, COL_BIOMASS, 0.10f);
    ui_panel(panel, R_PANEL, COL_PANEL, Fade(COL_BIOMASS, 0.30f));

    ui_text_strong("物种分化", panel.x + 24.0f, panel.y + 18.0f, F_TITLE, COL_TEXT);

    char bio[64], num[32];
    ui_fmt_num(game->run.biomass, num, (int)sizeof(num));
    snprintf(bio, sizeof(bio), "生物量 %s", num);
    ui_text_right(bio, panel.x + panel.width - 24.0f, panel.y + 24.0f, F_LABEL, COL_BIOMASS);

    int children[SPECIES_COUNT];
    int n = ua_speciation_children(game, children, SPECIES_COUNT);

    if (n <= 0) {
        ui_text_center("当前阶段没有可用的分化方向", panel.x + panel.width * 0.5f,
                       panel.y + 150.0f, F_LABEL, COL_DIM);
        ui_text_center("先通过突变满足前置条件", panel.x + panel.width * 0.5f,
                       panel.y + 176.0f, F_CAPTION, COL_FAINT);
    }

    int first = speciation_page_start(S, n);
    for (int i = first; i < n && i < first + UI_SPECIATION_PAGE_SIZE; ++i) {
        Rectangle r = { panel.x + 24.0f, panel.y + 84.0f + (float)(i - first) * 102.0f,
                        panel.width - 48.0f, 94.0f };
        UiButton *b = btn_find(S, UI_BTN_SPECIATE, children[i]);
        bool hot = is_hot(S, b);
        int sid = children[i];
        ActionCode check = ua_speciation_check(game, sid);
        double cost = ua_speciation_cost(game, sid);
        bool afford = game->run.biomass >= cost;
        float hv = btn_hover_step(b, hot);
        bool usable = b && b->enabled;

        ui_panel(r, R_CARD, usable ? ui_color_mix(COL_PANEL_HI, COL_PANEL_HOT, hv) : COL_PANEL_HI,
                 usable ? Fade(COL_ACCENT, 0.55f) : Fade(COL_LINE, 0.6f));

        Rectangle tile = { r.x + 14.0f, r.y + 18.0f, 58.0f, 58.0f };
        ui_panel(tile, 10.0f, Fade(COL_BG_DEEP, 0.6f), Fade(ecology_color(sid), 0.30f));
        draw_organism((Vector2){ tile.x + 29.0f, tile.y + 31.0f }, 22.0f, sid,
                      ua_species_stage(sid), ecology_color(sid), g_ui.time + (float)i, true, true, 1.0f);

        ui_text_strong(ua_species_name(sid), r.x + 84.0f, r.y + 12.0f, F_LABEL, COL_TEXT);
        char meta[96];
        snprintf(meta, sizeof(meta), "%s · 阶段 %d", ua_ecology_name(sid), ua_species_stage(sid));
        ui_text(meta, r.x + 84.0f, r.y + 34.0f, F_CAPTION, COL_DIM);

        char req[160];
        ua_speciation_requirement(sid, req, (int)sizeof(req));
        ui_text(req, r.x + 84.0f, r.y + 56.0f, F_CAPTION,
               check == ACTION_OK ? COL_FAINT : COL_BAD);

        char price[64];
        ui_fmt_num(cost, num, (int)sizeof(num));
        snprintf(price, sizeof(price), "%s 生物量", num);
        ui_text_right(price, r.x + r.width - 18.0f, r.y + 16.0f, F_LABEL,
                      afford ? COL_BIOMASS : COL_BAD);

        if (check != ACTION_OK) {
            ui_text_right("前置不足", r.x + r.width - 18.0f, r.y + 42.0f, F_CAPTION, COL_BAD);
        } else if (!afford) {
            ui_text_right("生物量不足", r.x + r.width - 18.0f, r.y + 42.0f, F_CAPTION, COL_BAD);
        } else {
            ui_text_right("可立即分化", r.x + r.width - 18.0f, r.y + 42.0f, F_CAPTION, COL_GOOD);
        }
    }

    UiButton *prev = btn_find(S, UI_BTN_SPECIATION_PAGE, 0);
    UiButton *next = btn_find(S, UI_BTN_SPECIATION_PAGE, 1);
    if (prev) ui_btn_draw(prev, "上一页", NULL, is_hot(S, prev));
    if (next) ui_btn_draw(next, "下一页", NULL, is_hot(S, next));
    char page[64];
    snprintf(page, sizeof(page), "%d / %d 页 · %d 条路线", first / UI_SPECIATION_PAGE_SIZE + 1,
             n > 0 ? (n + UI_SPECIATION_PAGE_SIZE - 1) / UI_SPECIATION_PAGE_SIZE : 1, n);
    ui_text_center(page, panel.x + panel.width * 0.5f, panel.y + panel.height - 96.0f,
                   F_BODY, COL_DIM);

    UiButton *keep = btn_find(S, UI_BTN_TOGGLE_KEEP_PARENT, -1);
    if (keep) {
        char label[96];
        snprintf(label, sizeof(label), "保留亲本为额外分支：%s", S->keep_parent ? "是" : "否");
        ui_btn_draw(keep, label, S->keep_parent ? "需要空的第二物种槽" : "亲本离开活跃列表",
                    is_hot(S, keep));
    }

    UiButton *close = btn_find(S, UI_BTN_CLOSE_OVERLAY, -1);
    if (close) ui_btn_draw(close, "关闭", NULL, is_hot(S, close));
}

static void draw_codex_overlay(const GameState *game, const UIState *S)
{
    Rectangle panel = codex_panel_rect(S->codex_tab, ua_achievement_count());
    ui_halo(panel, R_PANEL, COL_ACCENT, 0.08f);
    ui_panel(panel, R_PANEL, COL_PANEL, Fade(COL_ACCENT, 0.28f));

    ui_text_strong("生命图鉴", panel.x + 24.0f, panel.y + 16.0f, F_TITLE, COL_TEXT);

    UiButton *close = btn_find(S, UI_BTN_CLOSE_OVERLAY, -1);
    if (close) ui_btn_draw(close, "关闭", NULL, is_hot(S, close));

    const char *tabs[3] = { "物种", "突变", "记录" };
    for (int i = 0; i < 3; ++i) {
        UiButton *b = btn_find(S, UI_BTN_CODEX_TAB, i);
        if (!b) continue;
        bool active = (S->codex_tab == i);
        ui_panel(b->rect, R_BTN, active ? Fade(COL_ACCENT, 0.16f) : COL_PANEL_HI,
                 active ? COL_ACCENT : Fade(COL_LINE, 0.7f));
        ui_text_center_strong(tabs[i], b->rect.x + b->rect.width * 0.5f, b->rect.y + 7.0f,
                              F_BODY, active ? COL_TEXT : COL_DIM);
    }

    ui_divider(panel.x + 24.0f, panel.x + panel.width - 24.0f, panel.y + 104.0f,
               Fade(COL_LINE, 0.7f));

    if (S->codex_tab == 0 || S->codex_tab == 1) {
        int count = codex_count((CodexKind)S->codex_tab);
        int first = codex_page_first(S);
        int discovered = 0;
        for (int i = 0; i < count; ++i) discovered += codex_found(game, S->codex_tab, i);
        for (int i = first; i < count && i < first + UI_CODEX_PAGE_SIZE; ++i) {
            Rectangle r = codex_cell_rect(panel, i - first);
            bool found = codex_found(game, S->codex_tab, i);
            UiButton *button = btn_find(S, UI_BTN_CODEX_ENTRY, i);
            float hover = btn_hover_step(button, is_hot(S, button));
            Color accent = S->codex_tab == CODEX_SPECIES ? ecology_color(i) : COL_DNA;
            ui_panel(r, R_CARD, found ? ui_color_mix(COL_PANEL_HI, COL_PANEL_HOT, hover) : COL_SURFACE_OFF,
                     found ? Fade(accent, 0.35f + hover * 0.35f) : Fade(COL_LINE, 0.5f));
            Vector2 center = { r.x + 42.0f, r.y + 54.0f };
            if (S->codex_tab == CODEX_SPECIES) {
                draw_organism(center, 25.0f, i, ua_species_stage(i), found ? accent : COL_GHOST,
                              S->time + (float)i * 0.7f, found, found, 1.0f);
            } else if (found) {
                draw_mutation_sigil(center, i, accent, ua_mutation_is_rare(i), S->time);
            } else {
                ui_text_center("?", center.x, center.y - 10.0f, F_TITLE, COL_FAINT);
            }
            const CodexEntry *entry = codex_entry((CodexKind)S->codex_tab, i);
            ui_text_strong(found ? entry->name : "未发现", r.x + 80.0f, r.y + 24.0f, F_LABEL,
                           found ? COL_TEXT : COL_FAINT);
            char meta[80];
            if (S->codex_tab == CODEX_SPECIES && found)
                snprintf(meta, sizeof meta, "%s · 阶段 %d", ua_ecology_name(i), ua_species_stage(i));
            else if (found)
                snprintf(meta, sizeof meta, "%s · 第 %d 号", ua_mutation_is_rare(i) ? "稀有突变" : "突变", i + 1);
            else snprintf(meta, sizeof meta, "探索后收录");
            ui_text(meta, r.x + 80.0f, r.y + 52.0f, F_CAPTION, COL_DIM);
            ui_text(found ? "点击查看卡片" : "尚未解锁", r.x + 20.0f, r.y + 100.0f,
                    F_CAPTION, found ? accent : COL_FAINT);
        }
        char count_text[64], page_text[80];
        snprintf(count_text, sizeof count_text, "已发现 %d / %d", discovered, count);
        ui_text_right(count_text, panel.x + panel.width - 24.0f, panel.y + 70.0f, F_BODY, COL_DIM);
        UiButton *prev = btn_find(S, UI_BTN_CODEX_PAGE, 0);
        UiButton *next = btn_find(S, UI_BTN_CODEX_PAGE, 1);
        if (prev) ui_btn_draw(prev, "上一页", NULL, is_hot(S, prev));
        if (next) ui_btn_draw(next, "下一页", NULL, is_hot(S, next));
        snprintf(page_text, sizeof page_text, "%d / %d 页 · 点击已收录条目查看故事",
                 first / UI_CODEX_PAGE_SIZE + 1, (count + UI_CODEX_PAGE_SIZE - 1) / UI_CODEX_PAGE_SIZE);
        ui_text_center(page_text, panel.x + panel.width * 0.5f, panel.y + 560.0f, F_BODY, COL_DIM);
    } else {
        const char *labels[3] = { "最高纪元", "最高 EP", "最高生物量" };
        double values[3] = { (double)game->legacy.best_era,
                             (double)game->legacy.best_evolution_points,
                             game->legacy.best_biomass };
        Color colors[3] = { COL_ACCENT, COL_EP, COL_BIOMASS };
        int icons[3] = { ICON_STAGE, ICON_EP, ICON_BIOMASS };
        for (int i = 0; i < 3; ++i) {
            Rectangle r = { panel.x + 24.0f + (float)i * 368.0f, panel.y + 116.0f, 350.0f, 88.0f };
            ui_panel(r, R_CARD, COL_PANEL_HI, Fade(COL_LINE, 0.6f));
            draw_icon(icons[i], (Vector2){ r.x + 34.0f, r.y + 44.0f }, 16.0f, colors[i], g_ui.time);
            ui_text(labels[i], r.x + 62.0f, r.y + 14.0f, F_CAPTION, COL_DIM);
            char v[32];
            if (i < 2) snprintf(v, sizeof(v), "%.0f", values[i]);
            else ui_fmt_num(values[i], v, (int)sizeof(v));
            ui_text_strong(v, r.x + 62.0f, r.y + 36.0f, F_DISPLAY, colors[i]);
        }

        ui_text("成就", panel.x + 24.0f, panel.y + 220.0f, F_LABEL, COL_TEXT);
        int n = ua_achievement_count();
        int arows = n > 0 ? (n + 2) / 3 : 1;
        for (int i = 0; i < n && i < 9; ++i) {
            Rectangle r = { panel.x + 24.0f + (float)(i % 3) * 370.0f,
                            panel.y + 244.0f + (float)(i / 3) * 70.0f, 356.0f, 60.0f };
            bool got = ua_achievement_unlocked(game, i);
            ui_panel(r, R_BTN, got ? COL_PANEL_HI : COL_SURFACE_OFF,
                     got ? Fade(COL_ACCENT, 0.45f) : Fade(COL_LINE, 0.5f));
            draw_icon(ICON_ACH, (Vector2){ r.x + 30.0f, r.y + 30.0f }, 14.0f,
                      got ? COL_EP : COL_FAINT, g_ui.time);
            ui_text(ua_achievement_name(i), r.x + 54.0f, r.y + 10.0f, F_BODY,
                    got ? COL_TEXT : COL_FAINT);
            ui_text(ua_achievement_desc(i), r.x + 54.0f, r.y + 32.0f, F_CAPTION,
                    got ? COL_DIM : COL_FAINT);
            if (got) ui_text_right("已解锁", r.x + r.width - 12.0f, r.y + 12.0f,
                                   F_CAPTION, COL_EP);
        }

        /* 生涯摘要：把“记录”页下半部的空白填成一张可读的数据表 */
        {
            int sp = 0, mu = 0, cr = 0, ac = 0, lv = 0, ml = ua_legacy_max_level();
            int env_total = 0;
            for (int i = 0; i < MAX_ENVIRONMENTS; ++i) {
                if (!ua_environment_name(i)) break;
                if (!ua_environment_is_crisis(i)) continue;
                env_total++;
                if (game->run.survived_crises[i]) cr++;
            }
            for (int i = 0; i < SPECIES_COUNT; ++i) if (game->legacy.codex_species[i]) sp++;
            for (int i = 0; i < MUTATION_COUNT; ++i) if (game->legacy.codex_mutations[i]) mu++;
            for (int i = 0; i < 4; ++i) lv += game->legacy.levels[i];
            for (int i = 0; i < ua_achievement_count(); ++i) if (ua_achievement_unlocked(game, i)) ac++;

            Rectangle sm = { panel.x + 24.0f, panel.y + 244.0f + (float)arows * 70.0f + 16.0f,
                             panel.width - 48.0f, 96.0f };
            ui_panel(sm, R_CARD, Fade(COL_PANEL, 0.9f), Fade(COL_LINE, 0.55f));
            ui_text("生涯统计", sm.x + 16.0f, sm.y + 10.0f, F_CAPTION, COL_FAINT);
            ui_divider(sm.x + 16.0f, sm.x + sm.width - 16.0f, sm.y + 30.0f, Fade(COL_LINE, 0.6f));

            char a1[48], a2[48], a3[48], a4[48], a5[48], a6[48];
            snprintf(a1, sizeof(a1), "%d / %d", sp, SPECIES_COUNT);
            snprintf(a2, sizeof(a2), "%d / %d", mu, MUTATION_COUNT);
            snprintf(a3, sizeof(a3), "%d / %d", cr, env_total);
            snprintf(a4, sizeof(a4), "Lv.%d / %d", lv, ml * 4);
            int sec = (int)game->run.elapsed_time;
            snprintf(a5, sizeof(a5), "%02d:%02d", sec / 60, sec % 60);
            snprintf(a6, sizeof(a6), "%d / %d", ac, ua_achievement_count());

            const char *lb[6] = { "已点亮物种", "已收录突变", "渡过危机",
                                  "Legacy 强化", "本轮时长", "成就达成" };
            const char *vl[6] = { a1, a2, a3, a4, a5, a6 };
            for (int i = 0; i < 6; ++i) {
                float x = sm.x + 16.0f + (float)(i / 3) * (sm.width * 0.5f);
                float y = sm.y + 40.0f + (float)(i % 3) * 21.0f;
                ui_text(lb[i], x, y, F_CAPTION, COL_DIM);
                ui_text_right(vl[i], x + sm.width * 0.5f - 34.0f, y, F_CAPTION, COL_TEXT);
            }
        }
    }
}

/* Wrap at UTF-8 boundaries and measured width; never truncate an approved quote. */
static void draw_codex_quote(const char *quote, Rectangle area)
{
    char line[192] = { 0 };
    int used = 0, row = 0;
    for (const char *p = quote; *p;) {
        int bytes = 1;
        while (p[bytes] && ((unsigned char)p[bytes] & 0xC0u) == 0x80u) ++bytes;
        char trial[192];
        memcpy(trial, line, (size_t)used);
        memcpy(trial + used, p, (size_t)bytes);
        trial[used + bytes] = '\0';
        if (used > 0 && ui_text_w(trial, F_TITLE) > area.width) {
            ui_text_center_strong(line, area.x + area.width * 0.5f, area.y + row * 34.0f,
                                  F_TITLE, COL_TEXT);
            ++row;
            used = 0;
        }
        memcpy(line + used, p, (size_t)bytes);
        used += bytes;
        line[used] = '\0';
        p += bytes;
    }
    if (used) ui_text_center_strong(line, area.x + area.width * 0.5f, area.y + row * 34.0f,
                                   F_TITLE, COL_TEXT);
}

static void draw_codex_detail(const GameState *game, const UIState *S)
{
    Rectangle panel = codex_detail_rect();
    const CodexEntry *entry = codex_entry((CodexKind)S->codex_detail_kind, S->codex_detail_id);
    bool found = entry && codex_found(game, S->codex_detail_kind, S->codex_detail_id);
    bool is_species = S->codex_detail_kind == CODEX_SPECIES;
    Color accent = is_species ? COL_TEAL : COL_DNA;
    ui_halo(panel, R_PANEL, accent, 0.12f);
    ui_panel(panel, R_PANEL, COL_PANEL, Fade(accent, 0.6f));
    ui_text_strong(found ? entry->name : "未发现", panel.x + 24.0f, panel.y + 20.0f,
                   F_TITLE, COL_TEXT);
    Rectangle art = { panel.x + 20.0f, panel.y + 62.0f, panel.width - 40.0f, 254.0f };
    ui_panel(art, R_CARD, COL_BG_DEEP, Fade(accent, 0.25f));
    Vector2 center = { art.x + art.width * 0.5f, art.y + art.height * 0.5f };
    if (found) {
        ui_glow(center, 100.0f, accent, 0.25f);
        if (is_species)
            draw_organism(center, 66.0f, S->codex_detail_id, ua_species_stage(S->codex_detail_id),
                          ecology_color(S->codex_detail_id), S->time, true, true, 1.0f);
        else
            draw_mutation_sigil_scaled(center, S->codex_detail_id, accent,
                                       ua_mutation_is_rare(S->codex_detail_id), S->time, 3.0f);
    } else ui_text_center("?", center.x, center.y - 25.0f, F_HERO, COL_FAINT);
    ui_text_center(is_species ? "物种图鉴" : "突变图鉴", panel.x + panel.width * 0.5f,
                   panel.y + 340.0f, F_CAPTION, accent);
    draw_codex_quote(found ? entry->tagline : "继续探索，故事还在前面。",
                     (Rectangle){ panel.x + 40.0f, panel.y + 376.0f, panel.width - 80.0f, 112.0f });
    UiButton *back = btn_find(S, UI_BTN_CODEX_BACK, -1);
    if (back) ui_btn_draw(back, "返回图鉴", NULL, is_hot(S, back));
    ui_text_center("Esc 返回 · 保留当前页", panel.x + panel.width * 0.5f,
                   panel.y + panel.height - 14.0f, F_CAPTION, COL_FAINT);
}

/* ---------------- 进化树（星图） ----------------
 *
 * 极坐标星图：中心是原始细胞，一圈一个阶段，越外越晚；同一阶段的不同分支
 * 在圈上按角度分开，连线是向中心微微收拢的贝塞尔 —— 读起来像星图上的航线。
 *
 * 这一版只做了一件事：**把间距拉开**。做法三条：
 *   1. 圆改成接近正圆的椭圆（rx = 1.30·ry）。旧版 rx 是 ry 的 2.3 倍，
 *      左右两端被拉得很开、上下两端挤成一团，同圈的兄弟节点自然叠在一起。
 *   2. 角度按“叶子数量”等分（每个叶支 2π/叶数），而不是按子树权重随意分，
 *      每个节点至少拿到一份完整的角度配额，兄弟之间不会再共享同一条半径。
 *   3. 圈距 17px > 2×节点半径，相邻两圈的节点即使同角度也不会贴在一起。
 * 顺带保留：点节点选中，它到根的整条路线高亮；再点一次取消。
 */

#define TREE_STAGE_R(s)  (58.0f + 14.5f * (float)(s))
/* 横向 1.75 倍：面板是 16:9，正圆会在左右各空出 200px；拉成椭圆既不挤，
 * 又把画框用满（最外圈半宽 378 < 552，上下 216 < 220）。 */
#define TREE_STAGE_RX(s) (TREE_STAGE_R(s) * 1.75f)
#define TREE_STAGE_RY(s) (TREE_STAGE_R(s))

static Rectangle tree_panel_rect(void)
{
    return (Rectangle){ 60.0f, 52.0f, 1160.0f, 612.0f };
}

static Rectangle tree_sky_rect(void)
{
    Rectangle p = tree_panel_rect();
    return (Rectangle){ p.x + 14.0f, p.y + 66.0f, p.width - 28.0f, p.height - 104.0f };
}

/* 星图的圆心：竖向要给底部详情条让出 68px，所以圆心略偏上 */
static Vector2 tree_center(void)
{
    Rectangle sky = tree_sky_rect();
    return (Vector2){ sky.x + sky.width * 0.5f, sky.y + (sky.height - 68.0f) * 0.5f + 6.0f };
}

static int   s_kid[SPECIES_COUNT][SPECIES_COUNT];
static int   s_kidn[SPECIES_COUNT];
static int   s_leaves[SPECIES_COUNT];
static float s_ang;

static int tree_count_leaves(int id, int guard)
{
    if (guard > SPECIES_COUNT || s_kidn[id] <= 0) { s_leaves[id] = 1; return 1; }
    int w = 0;
    for (int k = 0; k < s_kidn[id]; ++k) w += tree_count_leaves(s_kid[id][k], guard + 1);
    s_leaves[id] = w;
    return w;
}

static void tree_place(int id, float slot, Vector2 c, Vector2 *node, int guard)
{
    int st = ua_species_stage(id);
    if (st < 0) st = 0;
    if (st > 11) st = 11;

    float span = (float)s_leaves[id] * slot;
    float a = s_ang + span * 0.5f;
    /* 每往外一圈整体拧 0.085 rad：父子不再落在同一条半径上，连线因此是螺旋；
     * 更重要的是“标签不再沿半径叠罗汉”——旧版父子同角度，名字会竖着串成一条。 */
    a += (float)st * 0.085f;

    node[id] = (Vector2){ c.x + cosf(a) * TREE_STAGE_RX(st),
                          c.y + sinf(a) * TREE_STAGE_RY(st) };
    if (id == 0) node[id] = c;                 /* 中心留给原始细胞自己 */

    s_ang += span;
    if (guard > SPECIES_COUNT) return;
    for (int k = 0; k < s_kidn[id]; ++k) tree_place(s_kid[id][k], slot, c, node, guard + 1);
}

/* 星图布局。build_overlay（放节点按钮）与 draw_tree_overlay（画）共用同一份几何，
   避免“看得到的位置”和“点得到的位置”对不上。
   parent_of[] 输出主亲本（-1 表示根）；sib[] 仅层状布局用，这里保留参数占位。 */
static void tree_layout(Vector2 node[SPECIES_COUNT], int parent_of[SPECIES_COUNT],
                        int sib[SPECIES_COUNT])
{
    for (int i = 0; i < SPECIES_COUNT; ++i) {
        s_kidn[i] = 0;
        s_leaves[i] = 1;
        parent_of[i] = -1;
        if (sib) sib[i] = 0;
    }

    /* 主亲本：优先“阶段更早的祖先”里最晚的那个；没有就取同阶段里 id 更小的
     *（侧支的规则亲本常常是同阶段的主干物种，例如 蜂群政体 ← 智慧生命）。 */
    for (int i = 1; i < SPECIES_COUNT; ++i) {
        int ps[4];
        int n = ua_species_parents(i, ps, 4);
        int own = ua_species_stage(i);
        int best = -1, best_st = -1, same = -1;

        for (int k = 0; k < n; ++k) {
            int p = ps[k];
            if (p < 0 || p >= SPECIES_COUNT || p == i) continue;
            int st = ua_species_stage(p);
            if (st < own) {
                if (st > best_st) { best = p; best_st = st; }
            } else if (st == own && p < i) {
                if (same < 0 || p < same) same = p;
            }
        }
        if (best < 0) best = (same >= 0) ? same : 0;
        parent_of[i] = best;
        if (best >= 0 && s_kidn[best] < SPECIES_COUNT) s_kid[best][s_kidn[best]++] = i;
    }

    int leaves = tree_count_leaves(0, 0);
    if (leaves < 1) leaves = 1;

    s_ang = -PI * 0.5f;                       /* 从正上方开始铺 */
    tree_place(0, 2.0f * PI / (float)leaves, tree_center(), node, 0);

    /* 兜底：走不到的节点（理论上不存在）沿最外圈均分，绝不留原点上的孤儿 */
    {
        float extra = 2.0f * PI / (float)SPECIES_COUNT;
        Vector2 c = tree_center();
        for (int i = 0; i < SPECIES_COUNT; ++i) {
            if (node[i].x == 0.0f && node[i].y == 0.0f) {
                int st = ua_species_stage(i);
                node[i] = (Vector2){ c.x + cosf((float)i * extra) * TREE_STAGE_RX(st),
                                     c.y + sinf((float)i * extra) * TREE_STAGE_RY(st) };
            }
        }
    }
}

static float tree_node_r(void) { return 9.5f; }

static void draw_nebula(Vector2 c, float rad, Color col, float a){
    for (int i = 0; i < 30; ++i) {
        float k = 1.0f - (float)i / 30.0f;
        DrawCircleV(c, rad * k, Fade(col, a * 0.012f * k * k));
    }
}

/* 亲缘线：控制点向圆心收 14%（“被引力拉弯”），起点/终点都落在节点边缘 */
static void tree_edge(Vector2 a, Vector2 b, Color c, float w, float offset)
{
    (void)offset;
    Vector2 g = tree_center();
    Vector2 m = { (a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f };
    m.x += (g.x - m.x) * 0.14f;
    m.y += (g.y - m.y) * 0.14f;

    Vector2 prev = a;
    for (int i = 1; i <= 12; ++i) {
        float t = (float)i / 12.0f;
        float u = 1.0f - t;
        Vector2 p = { u * u * a.x + 2.0f * u * t * m.x + t * t * b.x,
                      u * u * a.y + 2.0f * u * t * m.y + t * t * b.y };
        DrawLineEx(prev, p, w, c);
        prev = p;
    }
}

static void draw_tree_overlay(const GameState *game, const UIState *S)
{
    Rectangle panel = tree_panel_rect();
    ui_halo(panel, R_PANEL, COL_ACCENT, 0.10f);
    ui_panel(panel, R_PANEL, COL_PANEL, Fade(COL_ACCENT, 0.30f));

    ui_text_strong("进化树 · 星图", panel.x + 24.0f, panel.y + 14.0f, F_TITLE, COL_TEXT);
    ui_text("一圈一个阶段，越外越晚；亮线是走过的亲缘，暗线是尚未探索的分支",
            panel.x + 24.0f, panel.y + 44.0f, F_CAPTION, COL_DIM);

    int discovered = 0;
    for (int i = 0; i < SPECIES_COUNT; ++i) {
        if (game->legacy.codex_species[i] || game->run.discovered_species[i]) ++discovered;
    }

    UiButton *close = btn_find(S, UI_BTN_CLOSE_OVERLAY, -1);
    if (close) ui_btn_draw(close, "关闭", NULL, is_hot(S, close));

    Rectangle sky = tree_sky_rect();
    ui_panel(sky, R_CARD, COL_BG_DEEP, Fade(COL_LINE, 0.7f));
    ui_gradient((Rectangle){ sky.x + 8, sky.y + 8, sky.width - 16, sky.height - 16 },
                Fade(COL_BG_LIFT, 0.32f), Fade(COL_BG_DEEP, 0.88f));

    draw_nebula((Vector2){ sky.x + 250.0f, sky.y + 130.0f }, 180.0f, COL_DNA, 0.9f);
    draw_nebula((Vector2){ sky.x + 860.0f, sky.y + 350.0f }, 200.0f, COL_ACCENT, 0.8f);
    draw_nebula((Vector2){ sky.x + 520.0f, sky.y + 430.0f }, 150.0f, COL_TEAL, 0.7f);

    for (int i = 0; i < 160; ++i) {
        float x = sky.x + 6.0f + hash01((unsigned int)i * 3u + 1u) * (sky.width - 12.0f);
        float y = sky.y + 6.0f + hash01((unsigned int)i * 3u + 2u) * (sky.height - 12.0f);
        float a = 0.04f + 0.20f * hash01((unsigned int)i * 3u + 3u);
        float tw = 0.62f + 0.38f * sinf(g_ui.time * (0.6f + 0.9f * hash01((unsigned int)i * 5u)) + (float)i);
        DrawCircleV((Vector2){ x, y }, 0.7f + 1.2f * hash01((unsigned int)i * 7u + 9u),
                    Fade(COL_TEXT, a * tw));
    }

    Vector2 center = tree_center();

    /* ---------- 同心圈（阶段刻度） ---------- */
    for (int s = 1; s <= 11; ++s) {
        float rx = TREE_STAGE_RX(s), ry = TREE_STAGE_RY(s);
        Vector2 prev = { center.x + rx, center.y };
        for (int k = 1; k <= 64; ++k) {
            float a = (float)k * (2.0f * PI / 64.0f);
            Vector2 p = { center.x + cosf(a) * rx, center.y + sinf(a) * ry };
            DrawLineEx(prev, p, 1.0f, Fade(COL_LINE_STRONG, s % 2 ? 0.10f : 0.16f));
            prev = p;
        }
        /* 圈号不再写字：11 个"阶段 N"会在轴上叠成一串，还和标签抢位置。
         * 只留一个刻度点，阶段数在底部详情条里跟着选中物种显示。 */
        DrawCircleV((Vector2){ center.x, center.y - ry }, 1.6f, Fade(COL_ACCENT, 0.35f));
    }
    for (int k = 0; k < 12; ++k) {
        float a = (float)k * (2.0f * PI / 12.0f);
        DrawLineEx(center, (Vector2){ center.x + cosf(a) * TREE_STAGE_RX(11),
                                      center.y + sinf(a) * TREE_STAGE_RY(11) },
                   1.0f, Fade(COL_LINE, 0.08f));
    }

    Vector2 node[SPECIES_COUNT];
    int parent_of[SPECIES_COUNT];
    tree_layout(node, parent_of, NULL);
    float nr = tree_node_r();

    int main_id = game->run.active_species_count > 0 ? game->run.active_species[0].species_id : 0;
    int sel = (S->tree_sel >= 0 && S->tree_sel < SPECIES_COUNT) ? S->tree_sel : main_id;

    bool on_route[SPECIES_COUNT];
    for (int i = 0; i < SPECIES_COUNT; ++i) on_route[i] = false;
    for (int v = sel, guard = 0; v >= 0 && v < SPECIES_COUNT && guard < SPECIES_COUNT; ++guard) {
        on_route[v] = true;
        int p = parent_of[v];
        if (p == v || p < 0) break;
        v = p;
    }

    /* ---------- 亲缘连线 ---------- */
    for (int child = 0; child < SPECIES_COUNT; ++child) {
        int ps[4];
        int n = ua_species_parents(child, ps, 4);
        int own = ua_species_stage(child);
        bool child_found = game->legacy.codex_species[child] || game->run.discovered_species[child];
        for (int k = 0; k < n; ++k) {
            int p = ps[k];
            if (p < 0 || p >= SPECIES_COUNT || p == child) continue;
            if (ua_species_stage(p) >= own) continue;    /* 只画“早 → 晚”的方向 */

            bool parent_found = game->legacy.codex_species[p] || game->run.discovered_species[p];
            bool lit = parent_found && child_found;
            bool hi = on_route[child] && on_route[p];

            if (hi) {
                float pulse = 0.5f + 0.5f * sinf(g_ui.time * 2.0f + (float)child * 0.4f);
                tree_edge(node[p], node[child], Fade(COL_ACCENT, 0.10f + 0.08f * pulse), 4.5f, 0.0f);
                tree_edge(node[p], node[child], Fade(COL_ACCENT, 0.55f + 0.30f * pulse), 2.0f, 0.0f);
            } else {
                Color c = lit ? Fade(ecology_color(child), 0.42f) : Fade(COL_LINE_STRONG, 0.20f);
                tree_edge(node[p], node[child], c, lit ? 1.3f : 1.0f, 0.0f);
            }
        }
    }

    /* 圆心 */
    ui_glow(center, 46.0f, COL_ACCENT, 0.30f);

    /* ---------- 节点 ---------- */
    for (int i = 0; i < SPECIES_COUNT; ++i) {
        Vector2 p = node[i];
        bool found = game->legacy.codex_species[i] || game->run.discovered_species[i];
        bool cur = (i == main_id);
        bool pick = (i == sel);
        Color col = ecology_color(i);

        if (pick) {
            float pulse = 0.5f + 0.5f * sinf(g_ui.time * 2.4f);
            ui_glow(p, nr + 15.0f + 3.0f * pulse, COL_ACCENT, 0.5f);
        } else if (found) {
            ui_glow(p, nr + 7.0f, col, 0.28f);
        }

        DrawCircleV(p, nr, found ? COL_PANEL_HI : COL_SURFACE_OFF);
        DrawRing(p, nr - 1.0f, nr, 0.0f, 360.0f, 40,
                 found ? Fade(col, 0.90f) : Fade(COL_LINE, 0.9f));
        draw_organism(p, nr * 0.70f, i, ua_species_stage(i), col,
                      g_ui.time + (float)i * 0.7f, found, found, found ? 1.0f : 0.85f);

        if (cur) {
            DrawRing(p, nr + 2.0f, nr + 3.0f, 0.0f, 360.0f, 40, Fade(COL_ACCENT, 0.55f));
        }
    }

    /* ---------- 标签 ----------
     * 贴着节点沿半径向外放；八个候选方向 × 两档距离，撞到已放标签、别的节点、
     * 圈号或底部条就换下一个。当前/选中的名字永远画（宁可压一点点，也不能丢）。
     * 挤不下的普通名字直接不画 —— 名字少一个好过糊成一团。 */
    {
        Rectangle placed[SPECIES_COUNT];
        int placed_n = 0;

        for (int pass = 0; pass < 2; ++pass) {
            for (int i = 0; i < SPECIES_COUNT; ++i) {
                bool found = game->legacy.codex_species[i] || game->run.discovered_species[i];
                bool cur = (i == main_id);
                bool pick = (i == sel);
                bool must = cur || pick;
                if (!(found || must)) continue;
                if ((pass == 0) != must) continue;

                const char *nm = ua_species_name(i);
                float w = ui_text_w(nm, F_CAPTION);
                Vector2 p = node[i];
                float base = atan2f(p.y - center.y, p.x - center.x);

                const float offs[8] = { 0.0f, 0.42f, -0.42f, 0.84f, -0.84f, 1.35f, -1.35f, PI };
                Rectangle best = { 0, 0, 0, 0 };

                for (int c = 0; c < 8 && best.width <= 0.0f; ++c) {
                    for (int ring = 0; ring < 2 && best.width <= 0.0f; ++ring) {
                        float d = nr + 12.0f + 14.0f * (float)ring;
                        float a = base + offs[c];
                        float dx = cosf(a), dy = sinf(a);
                        Rectangle lr;
                        lr.width = w;
                        lr.height = 14.0f;
                        lr.y = p.y + dy * d - 7.0f;
                        if (dx > 0.30f)       lr.x = p.x + dx * d;
                        else if (dx < -0.30f) lr.x = p.x + dx * d - w;
                        else                  lr.x = p.x - w * 0.5f;

                        bool clash = false;
                        Rectangle pad = { lr.x - 3.0f, lr.y - 3.0f, lr.width + 6.0f, lr.height + 6.0f };
                        for (int k = 0; k < placed_n && !clash; ++k) {
                            if (CheckCollisionRecs(pad, placed[k])) clash = true;
                        }
                        for (int j = 0; j < SPECIES_COUNT && !clash; ++j) {
                            if (j == i) continue;
                            Rectangle q = { node[j].x - nr - 5.0f, node[j].y - nr - 5.0f,
                                            (nr + 5.0f) * 2.0f, (nr + 5.0f) * 2.0f };
                            if (CheckCollisionRecs(pad, q)) clash = true;
                        }
                        if (lr.x < sky.x + 4.0f || lr.x + lr.width > sky.x + sky.width - 4.0f) clash = true;
                        if (lr.y < sky.y + 4.0f || lr.y + lr.height > sky.y + sky.height - 62.0f) clash = true;
                        if (!clash) best = lr;
                    }
                }
                if (i == 0 && best.width > 0.0f) {
                    /* 圆心那枚节点的名字固定放正上方：它周围一圈全是第一代分支 */
                    best.x = p.x - w * 0.5f;
                    best.y = p.y - nr - 26.0f;
                }
                if (best.width <= 0.0f) {
                    if (!must) continue;
                    best.width = w;
                    best.height = 14.0f;
                    best.x = p.x - w * 0.5f;
                    best.y = p.y + nr + 6.0f;
                    if (best.x < sky.x + 4.0f) best.x = sky.x + 4.0f;
                    if (best.x + best.width > sky.x + sky.width - 4.0f)
                        best.x = sky.x + sky.width - 4.0f - best.width;
                    if (best.y + best.height > sky.y + sky.height - 62.0f)
                        best.y = sky.y + sky.height - 62.0f - best.height;
                }

                ui_text(nm, best.x + 1.0f, best.y + 1.0f, F_CAPTION, Fade(COL_BG_DEEP, 0.85f));
                ui_text(nm, best.x, best.y, F_CAPTION, pick ? COL_ACCENT : (found ? COL_DIM : COL_FAINT));
                placed[placed_n++] = best;
            }
        }
    }

    /* ---------- 底部详情条 ---------- */
    Rectangle bar = { sky.x + 12.0f, sky.y + sky.height - 56.0f, sky.width - 24.0f, 44.0f };
    ui_panel(bar, R_CARD, COL_PANEL, Fade(COL_ACCENT, 0.30f));

    {
        int sid = sel;
        int st = ua_species_stage(sid);
        draw_organism((Vector2){ bar.x + 24.0f, bar.y + 22.0f }, 12.0f, sid, st,
                      ecology_color(sid), S->time, true, true, 1.0f);
        ui_text_strong(ua_species_name(sid), bar.x + 44.0f, bar.y + 5.0f, F_LABEL, COL_TEXT);

        char line[192];
        int pp[4];
        int pn = ua_species_parents(sid, pp, 4);
        if (pn > 0) {
            snprintf(line, sizeof(line), "%s · 阶段 %d · 分化自 %s",
                     ua_ecology_name(sid), st, ua_species_name(pp[0]));
        } else {
            snprintf(line, sizeof(line), "%s · 阶段 %d · 万物起点", ua_ecology_name(sid), st);
        }
        ui_text(line, bar.x + 44.0f, bar.y + 24.0f, F_CAPTION, COL_DIM);

        double pop = -1.0, cap = 0.0;
        for (int s = 0; s < game->run.active_species_count; ++s) {
            if (game->run.active_species[s].species_id != sid) continue;
            SpeciesRates rates;
            ua_species_rates(game, s, &rates);
            pop = game->run.active_species[s].population;
            cap = rates.final_population_cap;
            break;
        }
        if (pop >= 0.0) {
            char pbuf[24], cbuf[24], tail[96];
            ui_fmt_num(pop, pbuf, (int)sizeof(pbuf));
            ui_fmt_num(cap, cbuf, (int)sizeof(cbuf));
            snprintf(tail, sizeof(tail), "本轮在场 · 种群 %s / %s", pbuf, cbuf);
            ui_text_right(tail, bar.x + bar.width - 14.0f, bar.y + 6.0f, F_CAPTION, COL_BIOMASS);
        }

        char cnt[64];
        snprintf(cnt, sizeof(cnt), "已点亮 %d / %d", discovered, SPECIES_COUNT);
        ui_text_right(cnt, bar.x + bar.width - 14.0f, bar.y + 24.0f, F_CAPTION, COL_EP);
    }

    {
        const char *lg[3] = { "选中路线", "已点亮", "未探索" };
        Color lc[3] = { COL_ACCENT, COL_BIOMASS, COL_LINE_STRONG };
        float lx = panel.x + 24.0f;
        for (int i = 0; i < 3; ++i) {
            DrawCircleV((Vector2){ lx + 3.0f, panel.y + panel.height - 18.0f }, 3.0f, lc[i]);
            ui_text(lg[i], lx + 12.0f, panel.y + panel.height - 25.0f, F_CAPTION, COL_FAINT);
            lx += 12.0f + ui_text_w(lg[i], F_CAPTION) + 18.0f;
        }
        ui_text_right("点节点高亮它的整条路线，再点一次取消",
                      panel.x + panel.width - 24.0f, panel.y + panel.height - 25.0f,
                      F_CAPTION, COL_FAINT);
    }
}

/* ---------------- 突变图谱 ---------------- */

static Rectangle mut_list_panel_rect(void)
{
    return (Rectangle){ 60.0f, 52.0f, 1160.0f, 612.0f };
}

static void draw_mut_list_overlay(const GameState *game, const UIState *S)
{
    Rectangle panel = mut_list_panel_rect();
    ui_halo(panel, R_PANEL, COL_DNA, 0.10f);
    ui_panel(panel, R_PANEL, COL_PANEL, Fade(COL_DNA, 0.30f));

    ui_text_strong("突变图谱", panel.x + 24.0f, panel.y + 14.0f, F_TITLE, COL_TEXT);
    ui_text("本轮已经获得的全部突变，按物种分栏；底部是本轮点亮的共振",
            panel.x + 24.0f, panel.y + 44.0f, F_CAPTION, COL_DIM);

    UiButton *close = btn_find(S, UI_BTN_CLOSE_OVERLAY, -1);
    if (close) ui_btn_draw(close, "关闭", NULL, is_hot(S, close));

    int slots = game->run.active_species_count;
    int cols = slots <= 2 ? (slots < 1 ? 1 : slots) : 2;

    Rectangle area = { panel.x + 20.0f, panel.y + 78.0f, panel.width - 40.0f, 366.0f };
    float gap = 12.0f;
    float cw = (area.width - gap * (float)(cols - 1)) / (float)cols;
    float ch = (slots > 2) ? (area.height - gap) * 0.5f : area.height;

    int total_owned = 0;
    for (int s = 0; s < slots; ++s) {
        for (int m = 0; m < MUTATION_COUNT; ++m) {
            if (game->run.active_species[s].mutations[m]) ++total_owned;
        }
    }

    int boxes = cols * ((slots + cols - 1) / cols);
    if (boxes < 1) boxes = 1;

    for (int si = 0; si < boxes; ++si) {
        Rectangle box = { area.x + (float)(si % cols) * (cw + gap),
                          area.y + (float)(si / cols) * (ch + gap), cw, ch };
        bool exists = si < slots;
        Color accent = exists ? ecology_color(game->run.active_species[si].species_id) : COL_LINE;

        ui_panel(box, R_CARD, exists ? Fade(COL_BG_LIFT, 0.35f) : COL_SURFACE_OFF,
                 exists ? Fade(accent, 0.35f) : Fade(COL_LINE, 0.45f));

        if (!exists) {
            ui_text_center(si == 0 ? "等待主谱系" : "空置物种槽",
                           box.x + box.width * 0.5f, box.y + box.height * 0.5f - 9.0f,
                           F_LABEL, COL_FAINT);
            ui_text_center(si == 0 ? "开局后这里显示主谱系" : "多样性 Lv.2 解锁",
                           box.x + box.width * 0.5f, box.y + box.height * 0.5f + 12.0f,
                           F_CAPTION, COL_FAINT);
            continue;
        }

        const SpeciesState *sp = &game->run.active_species[si];
        int sid = sp->species_id;
        DrawRectangleRounded((Rectangle){ box.x + 6.0f, box.y + 8.0f, 3.0f, box.height - 16.0f },
                             1.0f, 4, Fade(accent, 0.8f));
        draw_organism((Vector2){ box.x + 30.0f, box.y + 24.0f }, 11.0f, sid,
                      ua_species_stage(sid), accent, S->time, true, true, 1.0f);
        ui_text_strong(ua_species_name(sid), box.x + 48.0f, box.y + 10.0f, F_LABEL, COL_TEXT);

        int owned = 0;
        for (int m = 0; m < MUTATION_COUNT; ++m) {
            if (sp->mutations[m]) ++owned;
        }
        char line[160];
        snprintf(line, sizeof(line), "%s · 阶段 %d · %d 项突变",
                 ua_ecology_name(sid), ua_species_stage(sid), owned);
        ui_text(line, box.x + 48.0f, box.y + 32.0f, F_CAPTION, COL_DIM);

        /* 格子 3 列 × N 行：一屏装不下的按剩余数量标一句，不静默丢 */
        float gx = box.x + 14.0f;
        float gy = box.y + 52.0f;
        float gw = (box.width - 28.0f - 12.0f) / 3.0f;
        float gh = 26.0f;
        int max_rows = (int)((box.height - 56.0f) / gh);
        if (max_rows < 1) max_rows = 1;
        int cap = 3 * max_rows;
        int drawn = 0;

        for (int m = 0; m < MUTATION_COUNT; ++m) {
            if (!sp->mutations[m]) continue;
            if (drawn >= cap) break;
            int rr = drawn / 3, cc = drawn % 3;
            Rectangle cell = { gx + (float)cc * (gw + 6.0f), gy + (float)rr * gh, gw, gh - 5.0f };
            Color sig = ua_mutation_is_rare(m) ? COL_EP : COL_DNA;
            ui_panel(cell, 6.0f, Fade(sig, 0.07f), Fade(sig, 0.18f));
            /* 小尺寸纹章：半径 20 的 draw_mutation_sigil 放不进去，这里用
             * “环 + 核 + 两颗卫星”的简化形，颜色仍然区分稀有度。 */
            Vector2 sc = { cell.x + 12.0f, cell.y + cell.height * 0.5f };
            DrawRing(sc, 5.0f, 6.0f, 0.0f, 360.0f, 20, Fade(sig, 0.9f));
            DrawCircleV(sc, 2.0f, Fade(sig, 0.85f));
            DrawCircleV((Vector2){ sc.x + 6.5f, sc.y - 3.0f }, 1.4f, Fade(sig, 0.55f));
            DrawCircleV((Vector2){ sc.x - 6.0f, sc.y + 3.5f }, 1.1f, Fade(sig, 0.45f));
            ui_text(ua_mutation_name(m), cell.x + 22.0f, cell.y + 4.0f, F_CAPTION, COL_TEXT);
            ++drawn;
        }

        if (owned == 0) {
            ui_text("还没有获得任何突变 · 按 M 打开突变面板",
                    gx, gy + 6.0f, F_CAPTION, COL_FAINT);
        } else if (owned > drawn) {
            char more[32];
            snprintf(more, sizeof(more), "还有 %d 项", owned - drawn);
            int rr = drawn / 3;
            ui_text(more, gx, gy + (float)rr * gh + 6.0f, F_CAPTION, COL_FAINT);
        }
    }

    /* ---------- 本轮共振 ---------- */
    Rectangle syn = { panel.x + 20.0f, area.y + area.height + 12.0f,
                      panel.width - 40.0f, panel.height - (area.y - panel.y) - area.height - 46.0f };
    ui_panel(syn, R_CARD, Fade(COL_EP, 0.06f), Fade(COL_EP, 0.28f));
    ui_text("本轮共振", syn.x + 14.0f, syn.y + 8.0f, F_CAPTION, COL_EP);

    int syn_n = ua_synergy_count();
    int active_n = 0;
    float cx = syn.x + 14.0f;
    float cy = syn.y + 26.0f;
    for (int i = 0; i < syn_n; ++i) {
        bool on = false;
        for (int s = 0; s < slots && !on; ++s) on = ua_synergy_active(game, s, i);
        if (!on) continue;
        ++active_n;

        int pair[2];
        ua_synergy_pair(i, pair);
        const char *nm = ua_synergy_name(i);

        float w = ui_text_w(nm, F_CAPTION) + 18.0f;
        if (cx + w > syn.x + syn.width - 14.0f) {
            cx = syn.x + 14.0f;
            cy += 24.0f;
        }
        if (cy > syn.y + syn.height - 22.0f) break;

        Rectangle chip = { cx, cy, w, 20.0f };
        ui_panel(chip, 10.0f, Fade(COL_EP, 0.16f), Fade(COL_EP, 0.45f));
        ui_text_center(nm, chip.x + chip.width * 0.5f, chip.y + 4.0f, F_CAPTION, COL_EP);
        cx += w + 6.0f;

        /* 组名后面跟“哪两个突变凑出来的”，一行放下就写，放不下就不写 */
        char pairbuf[128];
        snprintf(pairbuf, sizeof(pairbuf), "%s + %s",
                 ua_mutation_name(pair[0]), ua_mutation_name(pair[1]));
        float pw = ui_text_w(pairbuf, F_CAPTION) + 10.0f;
        if (cx + pw < syn.x + syn.width - 14.0f && cy <= syn.y + syn.height - 22.0f) {
            ui_text(pairbuf, cx, cy + 5.0f, F_CAPTION, COL_DIM);
            cx += pw;
        }
    }

    if (active_n == 0) {
        char hint[192];
        snprintf(hint, sizeof(hint),
                 "同一个物种凑齐两条前置突变即触发共振 · 共 %d 条可解锁", syn_n);
        ui_text(hint, syn.x + 14.0f, syn.y + 30.0f, F_CAPTION, COL_FAINT);
    }

    /* ---------- 页脚 ---------- */
    {
        char foot[160];
        int codex_mut = 0;
        for (int m = 0; m < MUTATION_COUNT; ++m) {
            if (game->legacy.codex_mutations[m]) ++codex_mut;
        }
        snprintf(foot, sizeof(foot), "本轮 %d 项突变 · 图鉴收录 %d / %d · 共振 %d / %d",
                 total_owned, codex_mut, MUTATION_COUNT, active_n, syn_n);
        ui_text(foot, panel.x + 24.0f, panel.y + panel.height - 24.0f, F_CAPTION, COL_DIM);
        ui_text_right("按 V 随时打开 · 进化树里能看到这些突变把你带到了哪一支",
                      panel.x + panel.width - 24.0f, panel.y + panel.height - 24.0f,
                      F_CAPTION, COL_FAINT);
    }
}

static void draw_overlay(const GameState *game, const UIState *S)
{
    if (S->overlay == UI_OVL_NONE) return;
    draw_overlay_backdrop(0.72f * S->ovl_t);
    switch (S->overlay) {
    case UI_OVL_MUTATION:   draw_mutation_overlay(game, S); break;
    case UI_OVL_SPECIATION: draw_speciation_overlay(game, S); break;
    case UI_OVL_CODEX:      draw_codex_overlay(game, S); break;
    case UI_OVL_CODEX_DETAIL: draw_codex_detail(game, S); break;
    case UI_OVL_TREE:       draw_tree_overlay(game, S); break;
    case UI_OVL_MUT_LIST:   draw_mut_list_overlay(game, S); break;
    default: break;
    }
}

/* ================= 对外入口 ================= */

void screens_build(UiLayout *out, const GameState *game, const UIState *state)
{
    out->count = 0;
    UiScreen screen = state->screen;

    if (screen == UI_SCREEN_MAIN)         build_main(out, game, state);
    else if (screen == UI_SCREEN_END_ERA) build_end_era(out, game, state);
    else                                  build_legacy(out, game, state);

    if (state->overlay != UI_OVL_NONE) build_overlay(out, game, state);

    s_prev = *out;
}

void screens_draw(const GameState *game, const UIState *state, const ActionResult *last_result)
{
    (void)last_result;
    switch (state->screen) {
    case UI_SCREEN_MAIN:     draw_main(game, state, last_result); break;
    case UI_SCREEN_END_ERA:  draw_end_era(game, state); break;
    case UI_SCREEN_LEGACY:   draw_legacy(game, state); break;
    default: break;
    }
    draw_overlay(game, state);
}

