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
#include "tex.h"
#include "balance.h"
#include "atmosphere.h"
#include "buffs.h"
#include "codex.h"
#include "habitat.h"

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
 * 20 个主线物种各有固定荧光色：只做填充、发光与色轨，永不做正文颜色。
 * 色相分散在整圈，相邻阶段不会撞色。
 * v14 把物种扩到 264 之后，后 244 个物种按「阶段基色 + id 抖动」派生：
 * 旧写法只填了 20 项，数组其余元素是 alpha=0 的透明黑，图鉴里一半以上的
 * 生物会直接消失。派生色保持阶段内的色域，又让同阶段的不同物种能分开。 */
static Color ecology_color(int species_id)
{
    static const Color palette[20] = {
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
    /* 12 个阶段的色域锚点：派生色只在同阶段的相邻两色之间取，读起来仍成体系 */
    static const Color stage_hue[12] = {
        { 118, 240, 190, 255 }, { 196, 238, 104, 255 }, {  88, 224, 210, 255 },
        { 172, 150, 255, 255 }, { 150, 220, 130, 255 }, { 120, 190, 255, 255 },
        { 210, 180, 120, 255 }, { 160, 235, 170, 255 }, { 255, 214, 140, 255 },
        { 180, 200, 255, 255 }, { 140, 220, 255, 255 }, { 255, 240, 200, 255 },
    };
    if (species_id < 0 || species_id >= SPECIES_COUNT) return COL_DIM;
    if (species_id < 20) return palette[species_id];

    int st = ua_species_stage(species_id);
    if (st < 0) st = 0;
    if (st > 11) st = 11;
    Color a = stage_hue[st];
    Color b = stage_hue[(st + 3) % 12];
    float u = hash01((unsigned int)species_id * 2654435761u + 17u);
    Color c = ui_color_mix(a, b, 0.12f + 0.38f * u);
    float k = 0.90f + 0.24f * hash01((unsigned int)species_id * 40503u + 7u);
    int r = (int)((float)c.r * k), g = (int)((float)c.g * k), bl = (int)((float)c.b * k);
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (bl > 255) bl = 255;
    return (Color){ (unsigned char)r, (unsigned char)g, (unsigned char)bl, 255 };
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

/* 形态只由「阶段」决定类别；类内差异由 species_id 的种子决定。 */
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

/* 形态"本体"的外接半径（相对本体半径 r）。
 * 各形态都会在 r 之外再伸出一圈实体：杆菌的荚膜、群体的出芽细胞、行星的环、
 * 奇点的吸积盘……只按 r 排布会顶出画框（培养皿/展台/图鉴卡）。
 * thin=true 时把鞭毛、星尘、喷流这类细线也算进来：培养皿是有玻璃壁的硬边界，
 * 细线伸出去就等于画到皿外；展台/图鉴是矩形画框，细线到边被裁掉即可。 */
static float morph_extent(int stage, bool thin)
{
    switch (morph_class(stage)) {
    case MORPH_COCCUS:      return thin ? 1.30f : 1.18f;   /* 荚膜环 1.16 */
    case MORPH_ROD:         return thin ? 2.28f : 1.48f;   /* 胶囊半长 1.46 + 鞭毛 */
    case MORPH_EUKARYOTE:   return 1.05f;
    case MORPH_COLONY:      return thin ? 1.30f : 1.24f;   /* 出芽细胞贴外缘 1.18 */
    case MORPH_TISSUE:      return 1.00f;
    case MORPH_SYMBIO:      return 1.00f;
    case MORPH_NEURAL:      return thin ? 1.34f : 1.22f;   /* 轴突末梢的光点 */
    case MORPH_TECHNO:      return thin ? 1.34f : 1.28f;
    case MORPH_CIV:         return thin ? 1.38f : 1.32f;
    case MORPH_SPACE:       return thin ? 1.62f : 1.55f;   /* 卫星轨道 1.42 + 半径 0.10 */
    case MORPH_SINGULARITY: return thin ? 1.74f : 1.66f;   /* 吸积盘 1.05 × 1.56 + 喷流 */
    default:                return thin ? 1.40f : 1.32f;
    }
}

/* 把本体半径收缩到"整只生物刚好放进 limit 半径"。
 * limit 是该容器允许的最大视觉半径（培养皿内圈 / 展台留白 / 图鉴画框）。 */
static float organism_fit_ex(int stage, float r, float limit, bool thin)
{
    if (r <= 0.0f || limit <= 0.0f) return r;
    float extent = morph_extent(stage, thin);
    if (extent < 0.01f) extent = 1.0f;
    float cap = limit / extent;
    return r < cap ? r : cap;
}

static float organism_fit(int stage, float r, float limit)
{
    return organism_fit_ex(stage, r, limit, false);
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
/* 用扫描线填一个以 pts[0] 为中心的星形多边形。
 * 一开始用 DrawTriangleFan / DrawTriangle 逐面拼，结果是"什么都不出现"——
 * 同一条链路里 DrawCircle / DrawRectangle 都是正常的，只有三角面被整片丢掉
 * （3 倍离屏缓冲 + 裁剪时最明显）。改成逐行求交、再用 DrawRectangleV 填，
 * 这条路径在本工程的 UI 渲染里 100% 可靠。 */
static void fill_blob(const Vector2 *pts, int n, Color col)
{
    if (n < 4) return;

    float minY = pts[1].y, maxY = pts[1].y;
    for (int i = 2; i < n; ++i) {
        if (pts[i].y < minY) minY = pts[i].y;
        if (pts[i].y > maxY) maxY = pts[i].y;
    }

    int y0 = (int)floorf(minY);
    int y1 = (int)ceilf(maxY);
    for (int y = y0; y <= y1; ++y) {
        float cy = (float)y + 0.5f;
        float lo = 1e30f, hi = -1e30f;
        int hit = 0;
        for (int i = 1; i < n; ++i) {
            Vector2 a = pts[i];
            Vector2 b = pts[(i + 1 < n) ? i + 1 : 1];
            if ((a.y <= cy && b.y > cy) || (b.y <= cy && a.y > cy)) {
                float t = (cy - a.y) / (b.y - a.y);
                float x = a.x + (b.x - a.x) * t;
                if (x < lo) lo = x;
                if (x > hi) hi = x;
                ++hit;
            }
        }
        if (hit < 2) continue;
        DrawRectangleV((Vector2){ lo, (float)y }, (Vector2){ hi - lo, 1.0f }, col);
    }
}

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

    /* 大陆：第 18 版改成"噪声海岸线的多边形团块"。
     * 原来是每块大陆用 3 个叠在一起的椭圆，边缘太平滑，缩到展台上就是几颗
     * 绿药丸。现在按角度给半径加 hash 抖动，再套一圈更暗的陆架，海岸线才有
     * 真实的锯齿和浅滩过渡。 */
    for (int i = 0; i < 4; ++i) {
        float ang = 0.55f + (float)i * 1.62f + hash01((unsigned int)i * 29u + 11u) * 0.35f;
        float dist = r * (0.20f + 0.30f * hash01((unsigned int)i * 31u + 17u));
        Vector2 cc = { c.x + cosf(ang) * dist, c.y + sinf(ang) * dist * 0.86f };
        float lam = 0.50f - 0.70f * ((cc.x - c.x) / r);
        if (lam < 0.0f) lam = 0.0f;
        if (lam > 1.0f) lam = 1.0f;

        float baseR = r * (0.20f + 0.13f * hash01((unsigned int)i * 53u + 21u));

        float seedc = hash01((unsigned int)i * 71u + 5u) * 6.28f;
        /* 每块大陆的植被底色略不同：雨林偏绿、旱地偏赭、浅海偏青。
         * 亮度整体压到海面之下，否则在蓝色球体上只剩一层"绿雾"，
         * 缩到展台就看不见大陆轮廓。 */
        Color veg[3] = {
            (Color){ 62, 126, 88, 255 },
            (Color){ 142, 118, 74, 255 },
            (Color){ 56, 108, 118, 255 }
        };
        Color lc = ui_color_mix((Color){ 58, 96, 74, 255 }, veg[i % 3], 0.62f);

        const int n = 24;
        Vector2 shelf[n + 1];
        Vector2 shore[n + 1];
        shelf[0] = cc;
        shore[0] = cc;
        for (int k = 0; k < n; ++k) {
            float aa = (float)k / (float)n * 2.0f * PI;
            float j = 0.86f + 0.30f * hash01((unsigned int)(i * 97 + k) * 131u + 3u);
            /* 两个不同频率的正弦叠上去，轮廓就不会是"均匀毛刺" */
            float lobe = 1.0f + 0.10f * sinf(aa * 3.0f + seedc) + 0.05f * sinf(aa * 5.0f - seedc * 1.7f);
            float rr = baseR * j * lobe;
            shore[k + 1] = (Vector2){ cc.x + cosf(aa) * rr * 1.04f, cc.y + sinf(aa) * rr * 0.80f };
            shelf[k + 1] = (Vector2){ cc.x + cosf(aa) * rr * 1.22f, cc.y + sinf(aa) * rr * 0.95f };
        }

        float al = (0.52f + 0.42f * lam) * a;
        /* 陆架（浅海）：比陆地更暗更蓝的一圈，宽度不均匀，读成近岸浅滩 */
        fill_blob(shelf, n + 1, Fade(ui_color_mix(ocean, land_dark, 0.12f), al * 0.22f));
        /* 海岸暗边 */
        fill_blob(shore, n + 1, Fade(ui_color_mix(day, WHITE, 0.55f), a * 0.30f));

        /* 陆地主体：把 shore 收 8% 再画一层，留出细的海岸线 */
        Vector2 body[n + 1];
        body[0] = cc;
        for (int k = 0; k < n; ++k) {
            body[k + 1].x = cc.x + (shore[k + 1].x - cc.x) * 0.94f;
            body[k + 1].y = cc.y + (shore[k + 1].y - cc.y) * 0.94f;
        }
        fill_blob(body, n + 1, Fade(ui_color_mix(lc, day, 0.06f + 0.12f * lam), a * 0.96f));

        /* 植被/沙漠碎斑：几颗更亮更暗的小团，压掉"一块纯色"
         * 位置用 hash 取在半径 0.45 以内，基本不会越出海岸线 */
        for (int b = 0; b < 4; ++b) {
            float ba = hash01((unsigned int)(i * 13 + b) * 89u + 7u) * 6.28f;
            float bd = baseR * (0.12f + 0.34f * hash01((unsigned int)(i * 17 + b) * 97u + 9u));
            float brx = baseR * (0.08f + 0.12f * hash01((unsigned int)(i * 19 + b) * 103u + 11u));
            float bry = brx * (0.55f + 0.40f * hash01((unsigned int)(i * 23 + b) * 107u + 13u));
            Color bc = (b % 2) ? ui_color_mix(lc, (Color){ 58, 92, 60, 255 }, 0.35f) : ui_color_mix(lc, (Color){ 30, 56, 44, 255 }, 0.45f);
            DrawEllipse((int)(cc.x + cosf(ba) * bd * 1.1f), (int)(cc.y + sinf(ba) * bd * 0.8f),
                        brx, bry, Fade(bc, al * 0.34f));
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
        for (int i = 0; i < 5; ++i) {
            float ang = hash01((unsigned int)(g * 9 + i) * 19u + 7u) * 2.0f * PI;
            float rr = r * 0.13f * sqrtf(hash01((unsigned int)(g * 9 + i) * 23u + 11u));
            Vector2 p = { gc.x + cosf(ang) * rr, gc.y + sinf(ang) * rr * 0.7f };
            float tw = 0.5f + 0.5f * sinf(t * 1.6f + (float)i * 0.7f + (float)g);
            float lam = 0.50f - 0.68f * ((p.x - c.x) / (r * 0.84f));
            if (lam < 0.0f) lam = 0.0f;
            if (lam > 1.0f) lam = 1.0f;
            DrawCircleV(p, r * 0.011f, Fade(COL_ACCENT, (0.10f + 0.32f * (1.0f - lam)) * (0.6f + 0.4f * tw) * a));
        }
        ui_glow(gc, r * 0.24f, COL_ACCENT, 0.06f * a);
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

/* 第 17 版：形态族 → AI 插画（assets/tex/morph）。
 * 先按物种名做“属级”归类（硅藻 / 水母 / 菊石 / 蕨 / 珊瑚 / 真菌 / 鱼 …），
 * 命中不了再退到阶段形态。返回 -1 表示这一类仍走程序化矢量（行星、黑洞、
 * 城市、曼陀罗这些用矢量反而更稳、更衬 UI 配色）。 */
static int sprite_morph(int species_id, int stage)
{
    const char *n = ua_species_name(species_id);
    if (n) {
        /* 顺序有讲究：先判“水母”，再判“鱼”，否则 Jellyfish 会被 Fish 抢走 */
        if (strstr(n, "水母")) return TEXM_JELLY;
        if (strstr(n, "硅藻")) return TEXM_DIATOM;
        if (strstr(n, "纤毛") || strstr(n, "草履")) return TEXM_CILIATE;
        if (strstr(n, "变形虫")) return TEXM_AMOEBA;
        if (strstr(n, "菊石") || strstr(n, "鹦鹉螺") || strstr(n, "螺")) return TEXM_SPIRAL;
        if (strstr(n, "蕨")) return TEXM_FERN;
        if (strstr(n, "珊瑚") || strstr(n, "礁")) return TEXM_CORAL;
        if (strstr(n, "真菌") || strstr(n, "菌丝") || strstr(n, "菌根") || strstr(n, "霉菌"))
            return TEXM_FUNGUS;
        if (strstr(n, "鱼")) return TEXM_FISH;
        if (strstr(n, "蜂") || strstr(n, "蚁") || strstr(n, "巢")) return TEXM_HIVE;
        if (strstr(n, "丝状") || strstr(n, "丝藻") || strstr(n, "藻丝")) return TEXM_FILAMENT;
    }

    switch (morph_class(stage)) {
    case MORPH_COCCUS:    return TEXM_COCCUS;
    case MORPH_ROD:       return TEXM_ROD;
    case MORPH_EUKARYOTE: return TEXM_EUKARYOTE;
    case MORPH_COLONY:    return TEXM_COLONY;
    case MORPH_TISSUE:    return TEXM_TISSUE;
    case MORPH_SYMBIO:    return TEXM_SYMBIO;
    case MORPH_NEURAL:    return TEXM_NEURAL;
    case MORPH_TECHNO:    return TEXM_TECHNO;
    default:              return -1;
    }
}

/* 第 18 版：族级微调系数。
 * 插画已经统一归一化到"主体占画布 86% 且居中"（tools/normsprites.c），
 * 这里只给几类在视觉上偏轻/偏重的形态补 2–6%，不做大幅缩放，
 * 免得再顶到展台里的名字。 */
static float sprite_scale(int sp)
{
    switch (sp) {
    case TEXM_FISH:   return 1.06f;
    case TEXM_FERN:   return 1.05f;
    case TEXM_NEURAL: return 1.04f;
    case TEXM_JELLY:  return 1.02f;
    case TEXM_HIVE:   return 0.98f;
    case TEXM_COCCUS: return 1.02f;
    default: return 1.0f;
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

    if (lit && a > 0.25f) ui_glow(c, r * 1.55f, col, 0.12f * a);

    /* 有 AI 插画就用插画：真实标本质感（第 17 版的核心改动）。
     * 呼吸只是 ±1.2% 的缩放，够“活”，又不会让图鉴缩略图抖。 */
    int sp = sprite_morph(species_id, stage);
    if (sp >= 0 && tex_morph_ready(sp)) {
        float size = r * 2.15f * sprite_scale(sp) * (1.0f + 0.012f * sinf(t * 1.7f + seed));
        float bob  = r * 0.015f * sinf(t * 1.1f + seed * 2.0f);
        float cy = c.y + bob;
        float al = a > 1.0f ? 1.0f : a;

        /* 显微背射光：标本背后垫一层极淡的冷白圆池。
         * 这些插画是"标本摄影"的中间调，直接贴在深藏青展台上会像剪贴画浮着；
         * 加一层背光后边缘有透光过渡，暗部主体也不会陷进背景里。 */
        for (int i = 3; i >= 1; --i) {
            float rr = r * (0.62f + 0.20f * (float)i);
            float k  = 0.030f * (float)(4 - i) / 3.0f * al;
            Color col2 = { 0, 0, 0, 0 };
            DrawCircleGradient((int)c.x, (int)cy, rr,
                               Fade((Color){ 176, 205, 232, 255 }, k * 3.0f), col2);
        }

        Rectangle dst = { c.x - size * 0.5f, c.y - size * 0.5f + bob, size, size };

        /* 生态色只做 12% 的轻染：既保住物种之间"这个偏暖、那个偏冷"的辨识度，
         * 又不至于把标本照片染回卡通色块。 */
        Color base = ui_color_mix(accent, WHITE, 0.55f);
        Color tint = ui_color_mix(base, WHITE, 0.88f);
        tint.a = (unsigned char)(255.0f * al);
        if (tex_morph_draw(sp, dst, tint)) return;
    }

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

/* 画框内可用的最大本体半径：取圆心到四条边里最近的一条，再留 4px 余量。
 * limit_out 回传该上限，给种群环留一圈"刚好套住生物"的半径用。 */
static float organism_fit_in(Rectangle box, Vector2 c, int stage, float r, float *limit_out)
{
    float hx = fminf(c.x - box.x, box.x + box.width - c.x);
    float hy = fminf(c.y - box.y, box.y + box.height - c.y);
    float limit = fminf(hx, hy) - 4.0f;
    if (limit_out) *limit_out = limit;
    return organism_fit(stage, r, limit);
}

static void draw_organism_in(Rectangle box, Vector2 c, float r, int sid, int stage,
                             Color accent, float t, bool lit, bool detail, float a)
{
    /* 先按形态外接半径收缩，再用 scissor 硬裁兜底：展台、图鉴大图、网格图标都走这里，
     * 杆状的鞭毛、行星环、奇点喷流就不会画到标题或相邻面板上。 */
    float used = organism_fit_in(box, c, stage, r, NULL);
    ui_clip_begin(box);
    draw_organism(c, used, sid, stage, accent, t, lit, detail, a);
    ui_clip_end();
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
    ICON_ACH,
    ICON_INSIGHT   /* 理念 / 洞见：环 + 核，与洞见卡徽章同源 */
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
    case ICON_INSIGHT: {
        /* 理念：一圈环 + 中心核 + 三颗沿环巡行的小点 */
        DrawRing(c, r * 0.56f, r * 0.70f, 0.0f, 360.0f, 40, Fade(col, 0.85f));
        DrawCircleV(c, r * 0.24f, Fade(col, 0.90f));
        for (int i = 0; i < 3; ++i) {
            float a = t * 0.9f + (float)i * 2.094f;
            DrawCircleV((Vector2){ c.x + cosf(a) * r * 0.86f, c.y + sinf(a) * r * 0.86f },
                        r * 0.10f, Fade(col, 0.70f));
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
    } chips[5] = {
        { "ATP",    game->run.atp,          rates[0], COL_ATP,     ICON_ATP,     true  },
        { "生物量", game->run.biomass,      rates[1], COL_BIOMASS, ICON_BIOMASS, true  },
        { "DNA",    game->run.dna,          rates[2], COL_DNA,     ICON_DNA,     true  },
        /* 理念是洞见唯一的花费项，之前顶栏根本不显示它 —— 玩家看不见余额，
         * 洞见卡上写“2000 理念”就等于一句空话。 */
        { "理念",   game->run.knowledge,    ua_knowledge_rate(game), COL_VIOLET, ICON_INSIGHT, true },
        { "EP",     (double)game->legacy.evolution_points, 0, COL_EP, ICON_EP,  false },
    };

    /* 五个 chip 平分 16..782：宽 150、步距 154，正好给右边的纪元牌留出 22px */
    for (int i = 0; i < 5; ++i) {
        Rectangle p = { 16.0f + (float)i * 154.0f, 10.0f, 150.0f, 56.0f };
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

    /* 本纪元封顶：B 的 v8 P0-17 给每个纪元压了深度上限（阶段 8 / 10 / 13），
     * 到顶后分化会被规则层直接拒掉，而界面上此前只说“前置条件未满足”。
     * 纪元牌只有 216px 宽，写全“本纪上限 阶段 N”会和物种名撞上，
     * 所以这里只在到顶时补两字，具体数字交给分化弹层与结算页。 */
    int era_cap = ua_era_stage_cap(game);
    bool era_capped = era_cap > 0 && stage >= era_cap;
    if (era_capped) {
        ui_text_right("封顶", eb.x + eb.width - 10.0f, eb.y + 30.0f, F_CAPTION, COL_BAD);
    }

    char tbuf[32];
    ui_fmt_time(game->run.elapsed_time, tbuf, (int)sizeof(tbuf));
    ui_text_right(tbuf, eb.x + eb.width - 10.0f, eb.y + 9.0f, F_CAPTION, COL_FAINT);

    /* 纪元牌底部的演化进度：当前主物种走到 12 个形态阶段的第几格 */
    {
        /* 分母换成本纪元上限：这条读作“这一纪元还能走多深”，
         * 到顶时正好铺满，和右边的「封顶」对得上。
         * 用整体 12 阶做分母的话，纪元 1 到顶时条子只到 8/11，
         * 看着像“还没走完”，跟规则正好相反。 */
        float t = era_cap > 0 ? (float)stage / (float)era_cap : 0.0f;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        ui_bar((Rectangle){ eb.x + 12.0f, eb.y + 47.0f, eb.width - 24.0f, 3.0f }, t,
               era_capped ? COL_BAD : COL_ACCENT, Fade(COL_BAR_TRACK, 0.8f));
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

/* ================= 随机增益因子（buffs） =================
 *
 * B 的规则层每 45–90 s 刷一个“页面上可点击的因子”，但规则层不知道页面长什么样，
 * 所以落点由 UI 决定：固定在培养皿内的四个方位之一（由 icon 决定，稳定不跳动），
 * 半径 0.78R —— 正好落在活体（62）与皿内壁（95）之间，不压住细胞也不出圈。
 *
 * 四种图标不是装饰：B 每次刷新都会随机一个 BUFF_ICON_*，玩家能从外形预判类型
 * 倾向（光球=常规、光链=点击、气泡=补给、星点=强力）。
 * build_main（放命中框）与 draw_dish（画）都调 screens_buff_factor_pos，
 * 保证“看到哪就点到哪”。 */

#define DISH_CX 194.0f
#define DISH_CY 322.0f
#define DISH_R  104.0f

Vector2 screens_buff_factor_pos(const GameState *game, const UIState *state)
{
    int icon = game ? game->run.buff_visible_icon : 0;
    float punch = state ? state->punch : 0.0f;
    /* 四个方位避开正上方（ATP 大字）与正下方（物种名） */
    static const float k_ang[4] = { -1.15f, -0.25f, 1.95f, 2.90f };
    float a = k_ang[(icon >= 0 && icon < 4) ? icon : 0];
    float R = DISH_R * (1.0f + 0.055f * punch);
    /* 0.74R：内侧离活体（62）还有 15px，外侧离皿内壁（95）留 9px，
     * 光晕不至于压到刻度环上。 */
    return (Vector2){ DISH_CX + cosf(a) * R * 0.74f, DISH_CY + sinf(a) * R * 0.74f };
}

/* 因子类型 → 语义色。稀有强增益用 COL_RARE，其余沿用资源色系，读起来有谱。 */
static Color buff_color(int type)
{
    switch (type) {
    case BUFF_PRODUCTION_STRONG: return COL_RARE;
    case BUFF_CLICK:             return COL_ATP;
    case BUFF_INSTANT:           return COL_BIOMASS;
    default:                     return COL_TEAL;
    }
}

/* 四种外观。半径 r 约 13–15，全部用填充原语，4K 下依然锐利。 */
static void draw_buff_sigil(int icon, Vector2 p, float r, Color col, float t)
{
    switch (icon) {
    case BUFF_ICON_CELL: {
        /* 光球：亮核 + 三颗绕行小点，像一颗被捕获的细胞 */
        DrawCircleV(p, r * 0.30f, Fade(COL_TEXT, 0.95f));
        DrawCircleV(p, r * 0.56f, Fade(col, 0.30f));
        for (int i = 0; i < 3; ++i) {
            float a = t * 1.7f + (float)i * 2.094f;
            DrawCircleV((Vector2){ p.x + cosf(a) * r * 0.80f, p.y + sinf(a) * r * 0.80f },
                        r * 0.15f, Fade(col, 0.95f));
        }
        break;
    }
    case BUFF_ICON_DNA: {
        /* 光链：两条反相正弦珠链 */
        for (int strand = 0; strand < 2; ++strand) {
            float phase = (float)strand * PI;
            Vector2 prev = { 0.0f, 0.0f };
            for (int i = 0; i <= 10; ++i) {
                float k = (float)i / 10.0f;
                Vector2 cur = {
                    p.x + sinf(k * PI * 2.2f + phase + t * 1.1f) * r * 0.50f,
                    p.y - r * 0.80f + k * r * 1.60f
                };
                if (i > 0) DrawLineEx(prev, cur, 1.5f, Fade(col, 0.85f));
                if (i > 0 && i < 10) DrawCircleV(cur, r * 0.11f, Fade(COL_TEXT, 0.7f));
                prev = cur;
            }
        }
        break;
    }
    case BUFF_ICON_VESICLE: {
        /* 气泡：外环 + 内囊 + 高光点 */
        DrawRing(p, r * 0.60f, r * 0.78f, 0.0f, 360.0f, 32, Fade(col, 0.90f));
        DrawCircleV((Vector2){ p.x + r * 0.16f, p.y + r * 0.18f }, r * 0.20f, Fade(col, 0.55f));
        DrawCircleV((Vector2){ p.x - r * 0.22f, p.y - r * 0.24f }, r * 0.09f, Fade(COL_TEXT, 0.9f));
        break;
    }
    default: {
        /* 星点：四芒 + 核 */
        for (int i = 0; i < 4; ++i) {
            float a = (float)i * PI * 0.5f + t * 0.45f;
            DrawLineEx(p, (Vector2){ p.x + cosf(a) * r * 0.90f, p.y + sinf(a) * r * 0.90f },
                       1.8f, Fade(col, 0.85f));
        }
        DrawCircleV(p, r * 0.18f, Fade(COL_TEXT, 0.95f));
        break;
    }
    }
}

static void draw_buff_factor(const GameState *game, const UIState *S)
{
    if (!buffs_factor_visible(game)) return;

    int icon = game->run.buff_visible_icon;
    int type = game->run.buff_visible_type;
    Color col = buff_color(type);
    bool rare = (type == BUFF_PRODUCTION_STRONG);
    Vector2 p = screens_buff_factor_pos(game, S);

    UiButton *b = btn_find(S, UI_BTN_COLLECT_BUFF, -1);
    float hv = btn_hover_step(b, is_hot(S, b));
    float pulse = 0.5f + 0.5f * sinf(S->time * (rare ? 4.0f : 2.6f));
    float r = 14.0f * (1.0f + 0.06f * pulse + 0.12f * hv);

    /* 三层光：底晕 + 呼吸环 + 两圈外扩涟漪，远看就知道“这里有东西可以点” */
    ui_glow(p, r * 3.6f, col, 0.40f + 0.14f * pulse);
    float ring = r + 4.0f + 5.0f * pulse;
    DrawRing(p, ring, ring + 1.5f, 0.0f, 360.0f, 48, Fade(col, 0.55f - 0.25f * pulse));
    for (int i = 0; i < 2; ++i) {
        float k = fmodf(S->time * 0.8f + (float)i * 0.5f, 1.0f);
        float rr = r + 3.0f + 24.0f * k;
        DrawRing(p, rr, rr + 1.2f, 0.0f, 360.0f, 56, Fade(col, 0.30f * (1.0f - k)));
    }

    /* 本体：深底 + 亮边，保证图标在任何活体颜色上都读得出来 */
    DrawCircleV(p, r, Fade(COL_BG_DEEP, 0.90f));
    DrawRing(p, r - 1.7f, r, 0.0f, 360.0f, 48, col);
    draw_buff_sigil(icon, p, r * 0.88f, col, S->time);

    if (hv > 0.02f) {
        char tip[96];
        snprintf(tip, sizeof(tip), "%s · %s", buffs_icon_name(icon), buffs_type_name(type));
        float w = ui_text_w(tip, F_CAPTION) + 16.0f;
        Rectangle tb = { p.x - w * 0.5f, p.y - r - 30.0f, w, 22.0f };
        if (tb.x < 24.0f) tb.x = 24.0f;
        if (tb.x + tb.width > 360.0f) tb.x = 360.0f - tb.width;
        if (tb.y < 88.0f) tb.y = p.y + r + 8.0f;
        ui_panel(tb, R_CHIP(22.0f), Fade(COL_BG_DEEP, 0.94f), Fade(col, 0.65f));
        ui_text_center(tip, tb.x + tb.width * 0.5f, tb.y + 5.0f, F_CAPTION, col);
    }
}

/* 增益生效时沿皿内壁画一段收短的弧：剩余比例一眼可见，不额外占版面。
 * 进度弧画在呼吸环之外、皿沿之内，和刻度、活体都不打架。 */
static void draw_buff_timer(Vector2 c, float R, const GameState *game)
{
    if (game->run.buff_active_time_left <= 0.0) return;
    int type = game->run.buff_active_type;
    double dur = ua_buff_duration(type);
    if (dur <= 0.0) return;

    float frac = (float)(game->run.buff_active_time_left / dur);
    if (frac > 1.0f) frac = 1.0f;
    if (frac < 0.0f) frac = 0.0f;
    Color col = buff_color(type);

    float r0 = R - 8.0f, r1 = R - 5.0f;
    DrawRing(c, r0, r1, -90.0f, -90.0f + 360.0f * frac, 96, Fade(col, 0.72f));
    /* 快结束时再补一层呼吸，提醒“要没了” */
    if (frac < 0.25f) {
        float blink = 0.5f + 0.5f * sinf(g_ui.time * 7.0f);
        DrawRing(c, r0 - 1.5f, r1 + 1.5f, -90.0f, -90.0f + 360.0f * frac, 96,
                 Fade(col, 0.45f * blink));
    }
    /* 起点刻度：让弧的“满格”位置有个参照 */
    DrawCircleV((Vector2){ c.x, c.y - r0 - 1.0f }, 1.8f, Fade(col, 0.8f));
}

/* 皿下那一行文字：因子 > 生效中 > 常规提示。三种状态互斥，永远只占一行。 */
static void draw_dish_hint(const GameState *game, Vector2 c, float y)
{
    if (buffs_factor_visible(game)) {
        int type = game->run.buff_visible_type;
        char line[96];
        snprintf(line, sizeof(line), "因子 · %s · 点它或按 B 领取", buffs_type_name(type));
        ui_text_center(line, c.x, y, F_CAPTION, buff_color(type));
        return;
    }
    if (game->run.buff_active_time_left > 0.0) {
        int type = game->run.buff_active_type;
        char line[96];
        snprintf(line, sizeof(line), "%s · 剩余 %.0f 秒",
                 buffs_type_name(type), game->run.buff_active_time_left);
        ui_text_center(line, c.x, y, F_CAPTION, buff_color(type));
        return;
    }

    char line[96];
    if (game->run.buff_spawn_timer > 0.0) {
        snprintf(line, sizeof(line), "点击培养皿采集 · 下一个因子约 %.0f 秒",
                 game->run.buff_spawn_timer);
    } else {
        snprintf(line, sizeof(line), "点击培养皿或按空格采集");
    }
    ui_text_center(line, c.x, y, F_CAPTION, COL_FAINT);
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
    /* 皿里的"样品"来自哪片生境：液体与沉积跟着变，左右两块不会打架 */
    int dhab = habitat_for_species(sid, stage);
    Color hab_col = habitat_accent(dhab);

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

    /* 产率增益的倍率：贴在 ATP 数字右侧，别让玩家只能靠“数字变快了”去猜 */
    {
        double pm = buffs_production_multiplier(game);
        if (pm > 1.0) {
            char mb[24];
            snprintf(mb, sizeof(mb), "×%.1f", pm);
            float mwid = ui_text_w(mb, F_LABEL) + 12.0f;
            float mx = r.x + r.width * 0.5f + nw * 0.5f + 8.0f;
            if (mx + mwid > r.x + r.width - 10.0f) mx = r.x + r.width - 10.0f - mwid;
            Rectangle mc = { mx, r.y + 52.0f, mwid, 20.0f };
            Color mcol = buff_color(game->run.buff_active_type);
            ui_panel(mc, R_CHIP(20.0f), Fade(mcol, 0.16f), Fade(mcol, 0.55f));
            ui_text_center(mb, mc.x + mc.width * 0.5f, mc.y + 3.0f, F_LABEL, mcol);
        }
    }

    double click = ua_click_atp(game);
    char rl[64];
    if (click >= 0.0) {
        char cn[24];
        ui_fmt_num(click, cn, (int)sizeof(cn));
        /* v10：点击收益 = 3 秒产率，把口径写在玩家眼前 */
        if (buffs_click_multiplier(game) > 1.0) {
            snprintf(rl, sizeof(rl), "采集 +%s ATP（3 秒产出）×2", cn);
        } else {
            snprintf(rl, sizeof(rl), "采集 +%s ATP（3 秒产出）", cn);
        }
    } else {
        snprintf(rl, sizeof(rl), "点击采集");
    }
    ui_text_center(rl, r.x + r.width * 0.5f, r.y + 90.0f, F_CAPTION, COL_ATP);

    /* 培养皿：半径受点击膨胀影响。圆心/半径用与 screens_buff_factor_pos 同一组
     * 常量，因子落点才不会和皿心算错。 */
    Vector2 c = { DISH_CX, DISH_CY };
    float R = DISH_R * (1.0f + 0.055f * S->punch);
    float t = S->time;

    /* 背后环境光（在液体之前画，才不会被不透明皿底盖掉） */
    ui_glow(c, R * 1.44f, ui_color_mix(accent, (Color){ 70, 140, 210, 255 }, 0.72f), 0.12f + 0.08f * S->punch);

    /* 皿底：中心偏亮的液体。染色只留一点点，保住深空的冷调；
     * 色相取自当前生境，陆地阶段的样品皿不再是海水蓝。 */
    DrawCircleGradient((int)c.x, (int)c.y, R,
                       Fade(ui_color_mix(COL_BG_DEEP, ui_color_mix(COL_TEAL, hab_col, 0.55f), 0.16f), 0.95f),
                       Fade(COL_BG_DEEP, 0.98f));
    DrawCircleV(c, R * 0.985f, Fade(COL_STAGE, 0.22f));
    /* 液体弯月面：贴内壁的一圈冷光，让皿里“有水” */
    DrawRing(c, R * 0.93f, R * 0.955f, 0.0f, 360.0f, 84, Fade(COL_TEAL, 0.10f));
    /* 沉积带：贴底部的一段弧。水下的皿是细沙，陆生样品是土壤，
     * 轨道阶段几乎不留——同一个皿，读起来都像"取自某个地方的样本"。 */
    {
        float dep = dhab == HABITAT_ORBIT || dhab == HABITAT_SINGULARITY ? 0.18f : 0.42f;
        DrawRing(c, R * 0.60f, R * 0.80f, 32.0f, 148.0f, 56, Fade(COL_BG_DEEP, dep));
        for (int i = 0; i < 7; ++i) {
            unsigned int s = (unsigned int)i * 37u + 11u;
            float ang = (40.0f + 100.0f * hash01(s)) * DEG2RAD;
            float rr = R * (0.62f + 0.16f * hash01(s + 7u));
            Vector2 p = { c.x + cosf(ang) * rr, c.y + sinf(ang) * rr };
            DrawCircleV(p, 1.0f + 1.2f * hash01(s + 13u), Fade(hab_col, 0.20f));
        }
    }

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

    /* 限时增益的剩余时间：沿皿内壁画弧（在呼吸环之外、皿沿之内） */
    draw_buff_timer(c, R, game);

    /* 内圈微尘 */
    for (int i = 0; i < 10; ++i) {
        float a0 = (float)i * 2.399f + t * 0.05f;
        float rr = R * (0.30f + 0.63f * hash01((unsigned int)i * 7u + 3u));
        Vector2 p = { c.x + cosf(a0) * rr, c.y + sinf(a0) * rr * 0.95f };
        float al = 0.10f + 0.10f * (0.5f + 0.5f * sinf(t * 0.9f + (float)i));
        DrawCircleV(p, 1.4f + 1.0f * hash01((unsigned int)i * 11u + 5u), Fade(accent, al));
    }

    /* 活体：先按形态的外接半径收进皿内圈，再裁到皿的方框里兜底，
     * 杆状/奇点这类"越界"的形态不会再把鞭毛、吸积盘画到皿外的面板上。 */
    if (game->run.active_species_count > 0) {
        float breathe = 1.0f + 0.02f * sinf(t * 1.5f);
        Vector2 oc = { c.x, c.y + 1.0f };
        float org_r = organism_fit_ex(stage, 62.0f * breathe * (1.0f + 0.04f * S->punch),
                                      R * 0.92f, true);
        ui_clip_begin((Rectangle){ c.x - R * 0.96f, c.y - R * 0.96f, R * 1.92f, R * 1.92f });
        draw_organism(oc, org_r, sid, stage, accent, t, true, true, 1.0f);
        ui_clip_end();
    }

    /* 物种名 / 提示 */
    ui_text_center(ua_species_name(sid), c.x, r.y + 362.0f, F_LABEL, COL_TEXT);
    draw_dish_hint(game, c, r.y + 382.0f);

    /* 随机增益因子：画在活体之上，才不会被细胞盖住 */
    draw_buff_factor(game, S);

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

/* 当前大环境：只看主谱系。展台、环境横幅、全屏色晕都从这里取，
 * 避免三处各算一遍、出现"展台是森林、横幅还写着海洋"的错位。 */
static int current_habitat(const GameState *game)
{
    if (game->run.active_species_count <= 0) return HABITAT_HYDROTHERMAL;
    int sid = game->run.active_species[0].species_id;
    return habitat_for_species(sid, ua_species_stage(sid));
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

    /* 常态期不再一律叫“常态海洋”：上岸之后横幅继续写海洋会直接推翻展台的画面。
     * 名称只写「常态环境」，生境名交给展台的 chip —— 同屏出现两次“城市与技术圈”太啰嗦。
     * 危机 / 丰饶事件仍用规则层的名字与说明。 */
    int hab = current_habitat(game);
    const char *ename = ua_environment_name(env);
    const char *edesc = NULL;
    if (!in_event && env == ENV_NORMAL_OCEAN) {
        ename = "常态环境";
        edesc = habitat_note(hab);
    }
    ui_text_strong(ename, r.x + 64.0f, r.y + 6.0f, F_LABEL, COL_TEXT);

    const char *chip = in_event ? (crisis ? "危机中" : "丰饶期") : "稳定期";
    float cw = ui_text_w(chip, F_CAPTION) + 14.0f;
    Rectangle chipr = { r.x + r.width - S_PAD - cw, r.y + 6.0f, cw, H_CHIP };
    ui_panel(chipr, R_CHIP(H_CHIP), Fade(accent, 0.16f), Fade(accent, 0.45f));
    ui_text_center(chip, chipr.x + cw * 0.5f, chipr.y + 4.0f, F_CAPTION, accent);

    char desc[128];
    if (edesc) snprintf(desc, sizeof(desc), "%s", edesc);
    else ua_environment_desc(env, desc, (int)sizeof(desc));
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

    /* 生境：由主谱系的阶段 + 中文名线索决定；没有物种时退回原始海洋。
     * 大环境跟着时代走，玩家一眼能看出自己已经离开水里多久了。 */
    int hab = current_habitat(game);

    ui_panel(r, R_CARD, COL_STAGE, Fade(COL_LINE, 0.45f));
    /* 展台内是一块"取样窗口"：先铺满不透明底色保住圆角，再把生境画在里面。
     * 旧代码在这里调 atm_draw_live_in()，会把全屏星云重绘一遍并整块盖住背景；
     * 现在微尘由生境层自己出，水下是气泡、陆地是孢子、城市是灯点。 */
    Rectangle win = { r.x + 6.0f, r.y + 6.0f, r.width - 12.0f, r.height - 12.0f };
    ui_panel(win, R_CARD - 4.0f, COL_STAGE, BLANK);
    ui_clip_begin(win);
    habitat_draw(win, hab, S->time, 1.0f);
    ui_clip_end();

    if (game->run.active_species_count <= 0) {
        ui_text_center("等待初始化", r.x + r.width * 0.5f, r.y + r.height * 0.5f, F_LABEL, COL_DIM);
        return;
    }

    ui_text("生命体展台", r.x + 14.0f, r.y + 8.0f, F_CAPTION, COL_FAINT);
    /* 生境 chip：环境名直接写在标题旁边，不用玩家猜这片背景是什么 */
    {
        const char *hn = habitat_name(hab);
        Color hc = habitat_accent(hab);
        float tw = ui_text_w(hn, F_CAPTION);
        Rectangle chip = { r.x + 84.0f, r.y + 6.0f, tw + 22.0f, 18.0f };
        ui_panel(chip, 9.0f, Fade(hc, 0.12f), Fade(hc, 0.32f));
        DrawCircleV((Vector2){ chip.x + 8.0f, chip.y + 9.0f }, 2.2f, Fade(hc, 0.85f));
        ui_text(hn, chip.x + 15.0f, chip.y + 2.0f, F_CAPTION, hc);
    }
    char lab[32];
    snprintf(lab, sizeof(lab), "样本 %02d · 40×", game->run.active_species[0].species_id + 1);
    ui_text_right(lab, r.x + r.width - 14.0f, r.y + 8.0f, F_CAPTION, COL_FAINT);

    int nsp = game->run.active_species_count;
    if (nsp > 3) nsp = 3;
    float cy = r.y + 116.0f;

    /* 生物画框：上让开"生命体展台"标题、下让开物种名与种群行。
     * 所有展台里的生物都画在这个框里，形态再张扬也不会盖到标题或压到下方的面板。 */
    Rectangle art = { r.x + 8.0f, r.y + 32.0f, r.width - 16.0f, 194.0f };

    if (nsp <= 1) {
        float cx = r.x + r.width * 0.5f;
        float radius = 96.0f;
        Color accent = ecology_color(game->run.active_species[0].species_id);
        SpeciesRates rates;
        ua_species_rates(game, 0, &rates);

        ui_glow((Vector2){ cx, cy }, radius * 1.5f, ui_color_mix(accent, (Color){ 70, 140, 210, 255 }, 0.72f), 0.11f);
        int st0 = ua_species_stage(game->run.active_species[0].species_id);
        float limit0 = 0.0f;
        float used = organism_fit_in(art, (Vector2){ cx, cy }, st0, radius, &limit0);
        float pop = rates.final_population_cap > 0.0
                  ? (float)(game->run.active_species[0].population / rates.final_population_cap) : 0.0f;
        /* 环套在生物外面：外接半径 +10，最多到画框留白，避免切线穿过本体 */
        draw_pop_arc((Vector2){ cx, cy },
                     fminf(used * morph_extent(st0, false) + 10.0f, limit0 + 4.0f), pop, accent);
        draw_organism_in(art, (Vector2){ cx, cy }, radius,
                         game->run.active_species[0].species_id, st0, accent,
                         S->time, true, true, 1.0f);

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
        int st0 = ua_species_stage(game->run.active_species[0].species_id);
        float limit0 = 0.0f;
        float used = organism_fit_in(art, (Vector2){ cx, cy }, st0, radius, &limit0);
        float pop = rates.final_population_cap > 0.0
                  ? (float)(game->run.active_species[0].population / rates.final_population_cap) : 0.0f;
        draw_pop_arc((Vector2){ cx, cy },
                     fminf(used * morph_extent(st0, false) + 10.0f, limit0 + 4.0f), pop, accent);
        draw_organism_in(art, (Vector2){ cx, cy }, radius,
                         game->run.active_species[0].species_id, st0, accent,
                         S->time, true, true, 1.0f);

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
        int st1 = ua_species_stage(game->run.active_species[1].species_id);
        float limit1 = 0.0f;
        float used2 = organism_fit_in(art, (Vector2){ cx2, cy }, st1, 52.0f, &limit1);
        float pop2 = r2.final_population_cap > 0.0
                   ? (float)(game->run.active_species[1].population / r2.final_population_cap) : 0.0f;
        draw_pop_arc((Vector2){ cx2, cy },
                     fminf(used2 * morph_extent(st1, false) + 10.0f, limit1 + 4.0f), pop2, accent2);
        draw_organism_in(art, (Vector2){ cx2, cy }, 52.0f,
                         game->run.active_species[1].species_id, st1, accent2,
                         S->time + 1.7f, true, true, 1.0f);

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
            int st0 = ua_species_stage(game->run.active_species[0].species_id);
            float limit0 = 0.0f;
            float used0 = organism_fit_in(art, (Vector2){ mx, cy }, st0, R0, &limit0);
            float p0 = s0.final_population_cap > 0.0
                     ? (float)(game->run.active_species[0].population / s0.final_population_cap) : 0.0f;
            draw_pop_arc((Vector2){ mx, cy },
                         fminf(used0 * morph_extent(st0, false) + 10.0f, limit0 + 4.0f), p0, a0);
            draw_organism_in(art, (Vector2){ mx, cy }, R0,
                             game->run.active_species[0].species_id, st0, a0,
                             S->time, true, true, 1.0f);

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
                int stk = ua_species_stage(game->run.active_species[k].species_id);
                float limitk = 0.0f;
                float usedk = organism_fit_in(art, (Vector2){ sx, sy }, stk, R1, &limitk);
                float pk = sk.final_population_cap > 0.0
                         ? (float)(game->run.active_species[k].population / sk.final_population_cap) : 0.0f;
                draw_pop_arc((Vector2){ sx, sy },
                             fminf(usedk * morph_extent(stk, false) + 10.0f, limitk + 4.0f), pk, ak);
                draw_organism_in(art, (Vector2){ sx, sy }, R1,
                                 game->run.active_species[k].species_id, stk, ak,
                                 S->time + (float)k * 1.3f, true, true, 1.0f);

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

    /* 底部一层地平线光，给展台“有纵深”的收尾。
     * 只压 32px 且很淡：再深一点就会把生境里的城市楼群 / 海床吃掉。 */
    DrawRectangleGradientV((int)r.x, (int)(r.y + r.height - 32.0f), (int)r.width, 32,
                           Fade(COL_BG_DEEP, 0.0f), Fade(COL_BG_DEEP, 0.32f));
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

    /* 卡高 118 → 96：底部让出两条「扩张」（容量 / 储备），
     * 它们是 ATP 的两个出口，和机能卡同一列读起来才顺。 */
    Rectangle tile = { r.x + 12.0f, r.y + 8.0f, 40.0f, 40.0f };
    icon_badge(tile, icon, icol, S->time);

    ui_text_strong(ua_run_upgrade_title((RunUpgradeType)type), r.x + 62.0f, r.y + 6.0f,
                   F_LABEL, COL_TEXT);

    char lv[24];
    snprintf(lv, sizeof(lv), "Lv.%d", level);
    float lw = ui_text_w(lv, F_CAPTION) + 14.0f;
    Rectangle chip = { r.x + r.width - 12.0f - lw, r.y + 6.0f, lw, H_CHIP };
    ui_panel(chip, R_CHIP(H_CHIP), Fade(icol, 0.12f), BLANK);
    ui_text_center(lv, chip.x + lw * 0.5f, chip.y + 4.0f, F_CAPTION, icol);

    char eff[96];
    ua_run_upgrade_effect(game, (RunUpgradeType)type, eff, (int)sizeof(eff));
    ui_text(eff, r.x + 62.0f, r.y + 26.0f, F_CAPTION, COL_DIM);

    /* 等级刻条：10 格 */
    {
        int seg = level > 10 ? 10 : level;
        for (int i = 0; i < 10; ++i) {
            Rectangle tick = { r.x + 62.0f + (float)i * 12.0f, r.y + 42.0f, 8.0f, 4.0f };
            ui_panel(tick, 2.0f, i < seg ? Fade(icol, 0.9f) : COL_BAR_TRACK, BLANK);
        }
        if (level > 10) {
            char more[16];
            snprintf(more, sizeof(more), "+%d", level - 10);
            ui_text(more, r.x + 62.0f + 10.0f * 12.0f + 4.0f, r.y + 36.0f, F_CAPTION, COL_FAINT);
        }
    }

    /* 价格条 */
    double price = ua_run_upgrade_cost(game, (RunUpgradeType)type);
    char num[32], cost[56];
    ui_fmt_num(price, num, (int)sizeof(num));
    bool inf = !isfinite(price);
    if (inf) snprintf(cost, sizeof(cost), "已满级");
    else snprintf(cost, sizeof(cost), "%s %s", num, type == UPGRADE_GENOME ? "生物量" : "ATP");

    Rectangle pill = { r.x + 12.0f, r.y + 58.0f, r.width - 24.0f, 28.0f };
    ui_panel(pill, R_BTN, usable ? Fade(icol, 0.13f) : COL_SURFACE_OFF,
             usable ? Fade(icol, 0.55f) : Fade(COL_LINE, 0.7f));
    ui_text(cost, pill.x + 12.0f, pill.y + 5.0f, F_BODY, usable ? icol : COL_FAINT);
    if (!inf) {
        ui_text_right("购买", pill.x + pill.width - 12.0f, pill.y + 6.0f, F_CAPTION,
                      usable ? COL_TEXT : COL_FAINT);
    }

    /* 快捷键角标 */
    char key[16];
    snprintf(key, sizeof(key), "%d", type + 1);
    Rectangle kb = { tile.x + tile.width - 16.0f, tile.y + tile.height - 16.0f, 16.0f, 16.0f };
    ui_panel(kb, 5.0f, Fade(COL_BG_DEEP, 0.85f), Fade(COL_LINE, 1.0f));
    ui_text_center(key, kb.x + 8.0f, kb.y + 2.0f, F_CAPTION, COL_FAINT);
}

static void draw_shop_branch(const GameState *game, int child, Rectangle r)
{
    UIState *S = &g_ui;
    UiButton *b = btn_find(S, UI_BTN_OPEN_SPECIATION, child);
    float hv = btn_hover_step(b, is_hot(S, b));
    ActionCode check = ua_speciation_check(game, child);
    bool ok = check == ACTION_OK;
    /* 同样的“封顶优先”判断：商店这三张卡是最先被玩家看到的分化入口，
     * 这里写着「前置不足」会把人误导去堆突变。 */
    bool capped = ua_speciation_era_capped(game, child);
    Color col = ecology_color(child);
    int stage = ua_species_stage(child);
    bool afford = game->run.biomass >= ua_speciation_cost(game, child);

    if (ok && hv > 0.02f) ui_halo(r, R_CARD, col, 0.10f * hv);
    ui_panel(r, R_CARD, ui_color_mix(COL_PANEL_HI, COL_PANEL_HOT, hv * 0.75f),
             (hv > 0.02f) ? Fade(col, 0.85f) : Fade(COL_LINE, 0.55f));
    DrawRectangleRounded((Rectangle){ r.x + 6.0f, r.y + 10.0f, 3.0f, r.height - 20.0f },
                         1.0f, 4, Fade(col, 0.85f));

    Rectangle tile = { r.x + 12.0f, r.y + 8.0f, 40.0f, 40.0f };
    ui_panel(tile, 10.0f, ui_color_mix(COL_PANEL_HI, col, 0.10f), Fade(col, 0.30f));
    draw_organism_in((Rectangle){ tile.x + 4.0f, tile.y + 4.0f, 32.0f, 32.0f },
                     (Vector2){ tile.x + 20.0f, tile.y + 21.0f }, 15.0f, child, stage, col,
                     S->time + (float)child * 0.3f, ok, ok, ok ? 1.0f : 0.65f);

    ui_text_strong(ua_species_name(child), r.x + 62.0f, r.y + 6.0f, F_LABEL, COL_TEXT);

    char meta[96];
    snprintf(meta, sizeof(meta), "阶段 %d · %s", stage, ua_ecology_name(child));
    ui_text(meta, r.x + 62.0f, r.y + 26.0f, F_CAPTION, COL_DIM);

    char req[128];
    if (capped) {
        snprintf(req, sizeof(req), "超出本纪元上限 %d", ua_era_stage_cap(game));
    } else {
        ua_speciation_requirement(child, req, (int)sizeof(req));
    }
    ui_text(req, r.x + 62.0f, r.y + 42.0f, F_CAPTION, capped ? COL_BAD : (ok ? COL_GOOD : COL_FAINT));

    char num[32], cost[56];
    ui_fmt_num(ua_speciation_cost(game, child), num, (int)sizeof(num));
    snprintf(cost, sizeof(cost), "%s 生物量", num);
    Rectangle pill = { r.x + 12.0f, r.y + 60.0f, r.width - 24.0f, 26.0f };
    ui_panel(pill, R_BTN, afford ? Fade(COL_BIOMASS, 0.13f) : COL_SURFACE_OFF,
             afford ? Fade(COL_BIOMASS, 0.55f) : Fade(COL_LINE, 0.7f));
    ui_text(cost, pill.x + 12.0f, pill.y + 5.0f, F_BODY, afford ? COL_BIOMASS : COL_FAINT);
    ui_text_right(capped ? "本纪元封顶" : (ok ? (afford ? "分化" : "资源不足") : "前置不足"),
                  pill.x + pill.width - 12.0f, pill.y + 6.0f, F_CAPTION,
                  capped ? COL_BAD : (ok && afford ? COL_TEXT : COL_FAINT));
}

/* ---------------- 扩张：容量 / 储备 ----------------
 * 快捷键 4/5 一直是可用的，但界面里原先没有按钮，玩家只能靠底栏一行小字猜。
 * 这两张小卡把"花什么、换来什么、每级加多少"直接写出来：
 *   容量  提高种群承载上限（每级 +5%）
 *   储备  削减危机造成的伤害（每级 ×0.94，最低留 20%）
 * 价格与效果都取自适配层，界面不碰规则公式。 */
static void draw_expand_card(const GameState *game, const UIState *S,
                             bool reserve, Rectangle r)
{
    UiButton *b = btn_find(S, reserve ? UI_BTN_BUY_RESERVE : UI_BTN_BUY_CAPACITY, -1);
    float hv = btn_hover_step(b, is_hot(S, b));
    bool usable = b && b->enabled;
    Color col = reserve ? COL_TEAL : COL_BIOMASS;
    int lv = reserve ? ua_reserve_level(game) : ua_capacity_level(game);
    double cost = reserve ? ua_reserve_atp_cost(game)
                          : ua_capacity_atp_cost(game) + ua_capacity_biomass_cost(game);

    if (usable && hv > 0.02f) ui_halo(r, R_BTN, col, 0.11f * hv);
    ui_panel(r, R_BTN, ui_color_mix(COL_PANEL_HI, COL_PANEL_HOT, hv * 0.75f),
             (hv > 0.02f) ? Fade(col, 0.85f) : Fade(COL_LINE, 0.55f));
    DrawRectangleRounded((Rectangle){ r.x + 5.0f, r.y + 8.0f, 3.0f, r.height - 16.0f },
                         1.0f, 4, Fade(col, 0.85f));

    char title[32];
    snprintf(title, sizeof(title), "%s %s Lv.%d", reserve ? "5 储备" : "4 容量",
             reserve ? "减伤" : "上限", lv);
    ui_text_strong(title, r.x + 12.0f, r.y + 3.0f, F_LABEL, COL_TEXT);

    char eff[56];
    if (reserve) {
        snprintf(eff, sizeof(eff), "-%d%% · 每级 -%d%%",
                 (int)(ua_reserve_reduction(game) * 100.0 + 0.5),
                 (int)(ua_reserve_step() * 100.0 + 0.5));
    } else {
        snprintf(eff, sizeof(eff), "+%d%% · 每级 +%d%%",
                 (int)(ua_capacity_cap_bonus(game) * 100.0 + 0.5),
                 (int)(ua_capacity_step() * 100.0 + 0.5));
    }
    ui_text(eff, r.x + 12.0f, r.y + 22.0f, F_CAPTION, COL_DIM);

    char num[24], costline[56];
    bool inf = !isfinite(cost);
    if (inf) {
        snprintf(costline, sizeof(costline), "已满级");
    } else {
        ui_fmt_num(cost, num, (int)sizeof(num));
        snprintf(costline, sizeof(costline), "%s ATP", num);
    }
    ui_text(costline, r.x + 12.0f, r.y + 39.0f, F_CAPTION, inf ? COL_FAINT : (usable ? col : COL_FAINT));
    if (!inf) {
        ui_text_right(usable ? "购买" : "资源不足", r.x + r.width - 12.0f, r.y + 39.0f,
                      F_CAPTION, usable ? COL_TEXT : COL_FAINT);
    }
}

/* ---------------- 洞见（Insight）：理念的长线出口 ----------------
 *
 * 规则层：每级 +2% 全物种产率，成本 2000 × 1.9^级，无上限。
 * 它和子升级是**相乘**关系，所以这一条要在数值上说得清楚：
 * 现在的总倍率、买下一级之后的倍率、以及“理念从哪来”（阶段 6 起才产）。
 * 之前只有一条 28px 高的按钮，玩家既看不出收益也看不出进度，等于没有。 */

static void draw_insight_card(const GameState *game, const UIState *S)
{
    UiButton *b = btn_find(S, UI_BTN_BUY_INSIGHT, -1);
    Rectangle r = b ? b->rect : (Rectangle){ 928.0f, 552.0f, 320.0f, 80.0f };
    float hv = btn_hover_step(b, is_hot(S, b));
    bool usable = b && b->enabled;
    int level = game->run.insight_levels;
    double cost = ua_insight_cost(game);
    double now = ua_insight_multiplier(game);
    double next = ua_insight_next_multiplier(game);
    bool inf = !isfinite(cost);

    if (usable && hv > 0.02f) ui_halo(r, R_CARD, COL_VIOLET, 0.12f * hv);
    ui_panel(r, R_CARD, ui_color_mix(COL_PANEL_HI, COL_PANEL_HOT, hv * 0.75f),
             (hv > 0.02f) ? Fade(COL_VIOLET, 0.85f) : Fade(COL_LINE, 0.55f));
    DrawRectangleRounded((Rectangle){ r.x + 6.0f, r.y + 8.0f, 3.0f, r.height - 16.0f },
                         1.0f, 4, Fade(COL_VIOLET, 0.85f));

    /* 徽章：用“理念”本体的环状符号，不用货币图标 —— 它花的是知识，不是 ATP */
    Rectangle badge = { r.x + 12.0f, r.y + 14.0f, 52.0f, 52.0f };
    ui_glow((Vector2){ badge.x + 26.0f, badge.y + 26.0f }, 38.0f, COL_VIOLET, 0.45f);
    ui_panel(badge, 10.0f, ui_color_mix(COL_PANEL_HI, COL_VIOLET, 0.26f),
             Fade(COL_VIOLET, 0.80f));
    {
        Vector2 bc = { badge.x + 26.0f, badge.y + 26.0f };
        /* 环要够厚：12px 直径上 2px 的线在 4K 缩到 1080 时几乎看不见 */
        DrawRing(bc, 10.0f, 13.5f, 0.0f, 360.0f, 40, Fade(COL_VIOLET, 0.95f));
        DrawCircleV(bc, 5.0f, Fade(COL_TEXT, 0.92f));
        for (int i = 0; i < 3; ++i) {
            float a = S->time * 0.9f + (float)i * 2.094f;
            DrawCircleV((Vector2){ bc.x + cosf(a) * 19.0f, bc.y + sinf(a) * 19.0f },
                        2.2f, Fade(COL_VIOLET, 0.90f));
        }
    }

    ui_text_strong("洞见", r.x + 74.0f, r.y + 6.0f, F_LABEL, COL_TEXT);

    char lv[32];
    snprintf(lv, sizeof(lv), "Lv.%d", level);
    float lw = ui_text_w(lv, F_CAPTION) + 14.0f;
    Rectangle chip = { r.x + r.width - 12.0f - lw, r.y + 6.0f, lw, H_CHIP };
    ui_panel(chip, R_CHIP(H_CHIP), Fade(COL_VIOLET, 0.12f), BLANK);
    ui_text_center(lv, chip.x + lw * 0.5f, chip.y + 4.0f, F_CAPTION, COL_VIOLET);

    /* 倍率：当前 → 下一级。数字直接写，不用“+2%”让玩家自己算。 */
    char mult[96];
    if (inf) {
        snprintf(mult, sizeof(mult), "全产率 ×%.2f · 已达上限", now);
    } else {
        snprintf(mult, sizeof(mult), "全产率 ×%.2f → ×%.2f", now, next);
    }
    ui_text(mult, r.x + 74.0f, r.y + 27.0f, F_CAPTION, COL_DIM);

    /* 价格行只写价格：余额顶栏已经有第四个 chip（理念），卡上再写一遍
     * 会和右下角胶囊挤在同一行，反而读不清。 */
    char num[32], costline[96];
    ui_fmt_num(cost, num, (int)sizeof(num));
    bool has_source = ua_knowledge_rate(game) > 0.0 || game->run.knowledge > 0.0;

    if (inf) {
        snprintf(costline, sizeof(costline), "已达上限");
    } else if (!has_source) {
        /* 前期理念产量为 0：把“还买不到”的原因写在价格位上，比灰按钮诚实。
         * 文案要短 —— 右缘还有一枚"理念不足"胶囊，长句会顶上去。 */
        snprintf(costline, sizeof(costline), "阶段 6 起才有理念产出");
    } else {
        snprintf(costline, sizeof(costline), "%s 理念", num);
    }
    ui_text(costline, r.x + 74.0f, r.y + 47.0f, F_CAPTION,
            usable ? COL_VIOLET : (has_source ? COL_FAINT : Fade(COL_FAINT, 0.9f)));

    /* 行动胶囊与价格同一行，右缘对齐；整张卡本身就是按钮 */
    Rectangle act = { r.x + r.width - 78.0f, r.y + 44.0f, 66.0f, 20.0f };
    ui_panel(act, R_CHIP(20.0f), usable ? Fade(COL_VIOLET, 0.18f) : COL_SURFACE_OFF,
             usable ? Fade(COL_VIOLET, 0.55f) : Fade(COL_LINE, 0.6f));
    ui_text_center(usable ? "领悟 [7]" : (inf ? "满级" : "理念不足"),
                   act.x + act.width * 0.5f, act.y + 3.0f, F_CAPTION,
                   usable ? COL_VIOLET : COL_FAINT);
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
    /* 三张机能/分化卡高 96、间距 102；再往下并列两张扩张卡（容量 / 储备），
     * 最底部留给洞见。四条长线一屏看全，玩家不必靠快捷键说明猜作用。 */
    for (int i = 0; i < 3; ++i) {
        Rectangle row = { r.x + S_PAD, r.y + 88.0f + (float)i * 102.0f, 320.0f, 96.0f };
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
            ui_text("未解锁的分支", row.x + 72.0f, row.y + 12.0f, F_LABEL, Fade(COL_DIM, 0.75f));
            ui_text("继续积累生物量、推进阶段后开放", row.x + 72.0f, row.y + 34.0f,
                    F_CAPTION, COL_FAINT);
        }
    }

    /* 容量 / 储备：两个页签下都在。它们不是阶段性目标，而是随时可买的扩张。 */
    draw_expand_card(game, S, false, (Rectangle){ r.x + S_PAD, r.y + 392.0f, 156.0f, 60.0f });
    draw_expand_card(game, S, true,  (Rectangle){ r.x + S_PAD + 164.0f, r.y + 392.0f, 156.0f, 60.0f });

    /* 洞见：两个页签下都在，因为它是“理念”的出口，与机能/分化并列的第三条长线 */
    draw_insight_card(game, S);
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
    case 0: {
        /* 到顶时这条旁白要换说法：继续写“演化还在缓慢积累”会让玩家
         * 以为还能往上走，实际上分化已经被本纪元上限挡住了。 */
        int cap = ua_era_stage_cap(game);
        if (cap > 0 && stage >= cap) {
            snprintf(msg, sizeof(msg),
                     "第 %d 纪元 · 已封顶在阶段 %d，结算后进入下一纪元才能继续上行",
                     game->run.era, cap);
        } else {
            snprintf(msg, sizeof(msg), "第 %d 纪元 · %s 正处在阶段 %d，%s",
                     game->run.era, ua_species_name(main_species), stage,
                     game->run.era >= 5 ? "宇宙的尺度开始向你展开" : "演化还在缓慢积累");
        }
        break;
    }
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
    if (spe) {
        /* 到顶后这个按钮点开只会看到一页封顶卡，先把原因写在副标题上 */
        int cap = ua_era_stage_cap(game);
        bool capped = cap > 0 && ua_main_stage(game) >= cap;
        draw_action_button(spe, ICON_BRANCH, "物种分化",
                           capped ? "本纪元已封顶" : "消耗生物量", is_hot(S, spe), COL_BIOMASS);
    }
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
                    (Rectangle){ 928.0f, 172.0f + (float)i * 102.0f, 320.0f, 96.0f },
                    ch[i], 0, true);
        }
    } else {
        for (int i = 0; i < 3; ++i) {
            double price = ua_run_upgrade_cost(game, (RunUpgradeType)i);
            bool affordable = isfinite(price) &&
                ((i == UPGRADE_GENOME) ? (game->run.biomass >= price) : (game->run.atp >= price));
            btn_add(L, UI_BTN_UPGRADE,
                    (Rectangle){ 928.0f, 172.0f + (float)i * 102.0f, 320.0f, 96.0f },
                    i, 0, affordable);
        }
    }

    /* 扩张：容量 / 储备。两者都作用在"当前在场物种"上，没有活体时不可买。 */
    {
        bool alive = game->run.active_species_count > 0;
        btn_add(L, UI_BTN_BUY_CAPACITY, (Rectangle){ 928.0f, 476.0f, 156.0f, 60.0f }, 0, 0,
                alive && game->run.atp >= ua_capacity_atp_cost(game)
                      && game->run.biomass >= ua_capacity_biomass_cost(game));
        btn_add(L, UI_BTN_BUY_RESERVE, (Rectangle){ 1092.0f, 476.0f, 156.0f, 60.0f }, 0, 0,
                alive && game->run.atp >= ua_reserve_atp_cost(game));
    }

    /* 洞见：与机能/分化同列，占底部 80px */
    double insight_cost = ua_insight_cost(game);
    btn_add(L, UI_BTN_BUY_INSIGHT, (Rectangle){ 928.0f, 552.0f, 320.0f, 80.0f },
            0, 0, isfinite(insight_cost) && game->run.knowledge >= insight_cost);
    /* 因子命中框：跟着因子走，而不是固定在皿下一条 —— 点哪就是哪。
     * 必须在 UI_BTN_CLICK 之后入表，命中判定从后往前扫，因子才抢得过采集。 */
    if (game->run.buff_visible) {
        Vector2 fp = screens_buff_factor_pos(game, S);
        float hit = 22.0f;
        btn_add(L, UI_BTN_COLLECT_BUFF,
                (Rectangle){ fp.x - hit, fp.y - hit, hit * 2.0f, hit * 2.0f }, 0, 0, true);
    }

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

/* ================= 危机演出 =================
 * 进入危机时由 ui.c 的 crisis_watch() 置位，这里只负责画。
 * 强度包络 a: 1 → 0（约 1.9 s），抖屏在 ui.c 的最终合成里按同一包络平移整帧。 */

static Color crisis_tint(int env)
{
    switch (env) {
    case ENV_METEOR_IMPACT:   return (Color){ 255, 150, 74, 255 };
    case ENV_SOLAR_FLARE:     return (Color){ 255, 236, 190, 255 };
    case ENV_TOXIC_BLOOM:     return (Color){ 138, 214, 96, 255 };
    case ENV_GLOBAL_WARMING:  return (Color){ 255, 122, 78, 255 };
    case ENV_GLOBAL_COOLING:  return (Color){ 150, 205, 255, 255 };
    case ENV_OXYGEN_RISE:     return COL_TEAL;
    case ENV_LOW_LIGHT_PERIOD:return (Color){ 120, 130, 210, 255 };
    /* 第 16 版补齐：雾霾是灰褐、长夏是金色、寒流是冰蓝、孢子云是黄绿，
       否则四种危机共用一条“默认红”，画面读起来全一样 */
    case ENV_INDUSTRIAL_SMOG: return (Color){ 156, 138, 112, 255 };
    case ENV_LONG_SUMMER:     return (Color){ 255, 196, 120, 255 };
    case ENV_COLD_CURRENT:    return (Color){ 150, 205, 255, 255 };
    case ENV_SPORE_CLOUD:     return (Color){ 176, 202, 118, 255 };
    case ENV_OXYGEN_PULSE:    return COL_TEAL;
    default:                  return COL_BAD;
    }
}

/* ============ 危机氛围层（第 16 版）============
 * 上一版只有事件触发后 1.9 s 的爆发演出，之后十几秒画面完全静止，
 * 危机读起来像“一次性红闪”。这一版给每种危机配一层**持续天气**，各按自己的性格：
 *   全球变暖 = 热浪横纹 + 上升火星 + 暖色边光
 *   全球变冷 = 全屏飘雪 + 面板结霜 + 冷蓝边光
 *   微光期   = 光圈收拢 + 蓝紫尘埃（光合谱系被掐住）
 *   毒素爆发 = 上升气泡 + 底栖绿雾
 *   陨石撞击 = 尘幕 + 偶发火痕 + 坑口余温
 *   工业雾霾 = 横向灰褐霾带 + 煤烟颗粒
 * 爆发演出仍由 draw_crisis_fx 负责，两层叠加。 */

static void weather_edge_light(Color tint, float a)
{
    DrawRectangleGradientV(0, 0, UI_W, 92, Fade(tint, 0.10f * a), BLANK);
    DrawRectangleGradientV(0, UI_H - 92, UI_W, 92, BLANK, Fade(tint, 0.13f * a));
    DrawRectangleGradientH(0, 0, 84, UI_H, Fade(tint, 0.06f * a), BLANK);
    DrawRectangleGradientH(UI_W - 84, 0, 84, UI_H, BLANK, Fade(tint, 0.06f * a));
}

/* 培养皿上的危机光：只在皿内染色，边缘一圈同色环，读作“环境压力正压进皿里” */
static void weather_dish_tint(Vector2 dish, Color tint, float a)
{
    DrawCircleV(dish, 104.0f, Fade(tint, 0.055f * a));
    DrawRing(dish, 100.0f, 105.5f, 0.0f, 360.0f, 96, Fade(tint, 0.26f * a));
    DrawRing(dish, 96.0f, 98.0f, 0.0f, 360.0f, 96, Fade(tint, 0.10f * a));
}

static void env_weather(const GameState *game, const UIState *S)
{
    int env = game->run.current_environment_id;
    double rem = 0.0, tot = 1.0;
    bool in_event = false;
    ua_environment_timer(game, &rem, &tot, &in_event);
    if (!in_event || !ua_environment_is_crisis(env)) return;

    float t = S->time;
    Color tint = crisis_tint(env);
    float s = 0.86f + 0.14f * sinf(t * 0.85f);
    Rectangle st = { 388.0f, 148.0f, 516.0f, 284.0f };
    Vector2 dish = { 194.0f, 322.0f };

    weather_edge_light(tint, s);
    weather_dish_tint(dish, tint, s);

    if (env == ENV_GLOBAL_WARMING || env == ENV_LONG_SUMMER) {
        /* 热浪：一层暖色压底 + 横向光纹自下而上爬 + 上升火星 */
        DrawRectangle(0, 0, UI_W, UI_H, Fade(tint, 0.05f * s));
        DrawRectangleGradientV((int)st.x, (int)st.y, (int)st.width, (int)st.height,
                               BLANK, Fade(tint, 0.16f * s));
        for (int i = 0; i < 7; ++i) {
            float u = (float)i / 7.0f;
            float y = st.y + st.height * (1.0f - u) - fmodf(t * 16.0f, st.height * 0.5f);
            if (y < st.y - 6.0f || y > st.y + st.height) continue;
            for (int k = 0; k < 26; ++k) {
                float x0 = st.x + (float)k * 20.0f;
                float dy = sinf(t * 1.4f + x0 * 0.02f + (float)i) * 3.0f;
                DrawLineEx((Vector2){ x0, y + dy }, (Vector2){ x0 + 22.0f, y + dy },
                           fmaxf(1.0f, 1.6f + 1.2f * sinf(t + (float)i)),
                           Fade(tint, 0.09f * s));
            }
        }
        for (int i = 0; i < 40; ++i) {
            unsigned int seed = (unsigned int)i * 29u + 5u;
            float u = fmodf(hash01(seed) + t * (0.16f + 0.18f * hash01(seed + 3u)), 1.0f);
            float y = st.y + st.height * (1.0f - u);
            float x = st.x + hash01(seed + 71u) * st.width + sinf(t * 1.2f + (float)i) * 7.0f;
            float r = 1.2f + 1.8f * hash01(seed + 17u);
            DrawCircleV((Vector2){ x, y }, r, Fade((Color){ 255, 214, 150, 255 }, (0.55f - 0.34f * u) * s));
            DrawCircleV((Vector2){ x, y }, r * 2.6f, Fade(tint, 0.06f * s));
        }
    } else if (env == ENV_GLOBAL_COOLING || env == ENV_COLD_CURRENT) {
        /* 变冷：冷色压底 + 全屏细雪 + 面板边缘结霜 + 皿沿冰棱 */
        DrawRectangle(0, 0, UI_W, UI_H, Fade(tint, 0.05f * s));
        for (int i = 0; i < 70; ++i) {
            unsigned int seed = (unsigned int)i * 37u + 9u;
            float x = fmodf(hash01(seed) * (float)UI_W + t * 14.0f, (float)UI_W);
            float y = fmodf(hash01(seed + 43u) * (float)UI_H + t * (26.0f + 18.0f * hash01(seed + 7u)),
                            (float)UI_H);
            float r = 0.9f + 1.7f * hash01(seed + 61u);
            DrawCircleV((Vector2){ x, y }, r, Fade(WHITE, 0.34f * s));
        }
        /* 霜：四角斜向的冰晶枝 */
        Vector2 corners[4] = { { st.x + 10.0f, st.y + 10.0f }, { st.x + st.width - 10.0f, st.y + 8.0f },
                               { st.x + 8.0f, st.y + st.height - 10.0f },
                               { st.x + st.width - 12.0f, st.y + st.height - 8.0f } };
        for (int i = 0; i < 4; ++i) {
            float grow = 14.0f + 12.0f * hash01((unsigned int)i * 13u + 5u) +
                         2.5f * sinf(t * 1.1f + (float)i);
            for (int k = 0; k < 5; ++k) {
                float ang = (float)k * (PI * 0.5f) + 0.4f + (float)i * 0.8f;
                Vector2 p1 = { corners[i].x + cosf(ang) * grow, corners[i].y + sinf(ang) * grow };
                DrawLineEx(corners[i], p1, 1.0f, Fade((Color){ 210, 238, 255, 255 }, 0.30f * s));
                Vector2 mid = { corners[i].x + cosf(ang) * grow * 0.55f,
                                corners[i].y + sinf(ang) * grow * 0.55f };
                DrawLineEx(mid, (Vector2){ mid.x + cosf(ang + 0.9f) * grow * 0.35f,
                                           mid.y + sinf(ang + 0.9f) * grow * 0.35f },
                           0.8f, Fade((Color){ 210, 238, 255, 255 }, 0.20f * s));
            }
        }
        /* 皿沿冰棱：外圈叠几段短弧 */
        for (int i = 0; i < 10; ++i) {
            float ang = (float)i * 36.0f + t * 2.0f;
            float r0 = 105.0f;
            Vector2 p0 = { dish.x + cosf(ang * DEG2RAD) * r0, dish.y + sinf(ang * DEG2RAD) * r0 };
            Vector2 p1 = { dish.x + cosf((ang + 6.0f) * DEG2RAD) * (r0 + 4.0f),
                           dish.y + sinf((ang + 6.0f) * DEG2RAD) * (r0 + 4.0f) };
            DrawLineEx(p0, p1, 1.2f, Fade(WHITE, 0.30f * s));
        }
    } else if (env == ENV_LOW_LIGHT_PERIOD) {
        /* 微光：整屏压暗一圈、培养皿只剩中心一小块可见光，蓝紫尘埃只在光锥里飘 */
        DrawRectangle(0, 0, UI_W, UI_H, Fade(BLACK, 0.10f * s));
        DrawRectangleGradientV((int)st.x, (int)st.y, (int)st.width, (int)st.height,
                               Fade((Color){ 5, 8, 20, 255 }, 0.50f * s),
                               Fade((Color){ 5, 8, 20, 255 }, 0.28f * s));
        /* 光锥：从皿心向上散开的一束微光，让“光在衰减”有可见的边界 */
        for (int i = 0; i < 5; ++i) {
            float k = (float)i / 4.0f;
            float w = 40.0f + 60.0f * k;
            DrawRectangleGradientV((int)(dish.x - w * 0.5f), (int)(st.y + 60.0f), (int)w,
                                   (int)(st.height - 90.0f),
                                   Fade((Color){ 150, 170, 255, 255 }, 0.035f * s), BLANK);
        }
        DrawCircleGradient((int)dish.x, (int)dish.y, 120.0f,
                           Fade((Color){ 10, 14, 34, 255 }, 0.30f * s), Fade(BLACK, 0.42f * s));
        for (int i = 0; i < 40; ++i) {
            unsigned int seed = (unsigned int)i * 41u + 3u;
            float ang = hash01(seed) * 2.0f * PI + t * 0.10f;
            float rad = 20.0f + hash01(seed + 19u) * 74.0f;
            float y = st.y + 70.0f + hash01(seed + 71u) * (st.height - 110.0f);
            Vector2 p = { dish.x + cosf(ang) * rad * 1.4f, y };
            float r = 1.2f + 1.6f * hash01(seed + 31u);
            DrawCircleV(p, r * 2.6f, Fade((Color){ 130, 150, 255, 255 }, 0.10f * s));
            DrawCircleV(p, r, Fade((Color){ 190, 205, 255, 255 }, 0.42f * s));
        }
    } else if (env == ENV_TOXIC_BLOOM || env == ENV_SPORE_CLOUD) {
        /* 毒潮：底栖绿雾 + 上浮气泡（气泡带一圈亮边，才不像绿色圆点） */
        DrawRectangleGradientV((int)st.x, (int)st.y, (int)st.width, (int)st.height,
                               BLANK, Fade(tint, 0.22f * s));
        for (int i = 0; i < 26; ++i) {
            unsigned int seed = (unsigned int)i * 23u + 11u;
            float u = fmodf(hash01(seed) + t * (0.10f + 0.12f * hash01(seed + 5u)), 1.0f);
            float x = st.x + 10.0f + hash01(seed + 47u) * (st.width - 20.0f) +
                      sinf(t * 1.1f + (float)i) * 6.0f;
            float y = st.y + st.height * (1.0f - u);
            float r = 2.0f + 4.2f * hash01(seed + 83u);
            DrawCircleV((Vector2){ x, y }, r, Fade(ui_color_mix(tint, BLACK, 0.30f), 0.20f * s));
            DrawRing((Vector2){ x, y }, r * 0.86f, r * 1.02f, 0.0f, 360.0f, 22,
                     Fade(ui_color_mix(tint, WHITE, 0.35f), 0.34f * s));
            DrawCircleV((Vector2){ x - r * 0.3f, y - r * 0.35f }, r * 0.28f, Fade(WHITE, 0.20f * s));
        }
    } else if (env == ENV_METEOR_IMPACT) {
        /* 陨石：持续尘幕（细颗粒向下压）+ 每 2.5 s 一道远空火痕 */
        for (int i = 0; i < 70; ++i) {
            unsigned int seed = (unsigned int)i * 31u + 13u;
            float x = hash01(seed) * (float)UI_W + sinf(t * 0.6f + (float)i) * 10.0f;
            float y = fmodf(hash01(seed + 29u) * (float)UI_H + t * 10.0f, (float)UI_H);
            DrawCircleV((Vector2){ x, y }, 0.9f + 1.2f * hash01(seed + 67u),
                        Fade((Color){ 210, 190, 170, 255 }, 0.22f * s));
        }
        float cyc = fmodf(t, 2.6f);
        if (cyc < 0.55f) {
            float k = cyc / 0.55f;
            Vector2 a0 = { 60.0f + 180.0f * sinf(t * 3.0f), -30.0f };
            Vector2 a1 = { a0.x + 240.0f, a0.y + 300.0f };
            Vector2 head = { a0.x + (a1.x - a0.x) * k, a0.y + (a1.y - a0.y) * k };
            DrawLineEx(a0, head, 2.2f, Fade((Color){ 255, 210, 150, 255 }, 0.45f * s * (1.0f - k)));
            DrawCircleV(head, 2.4f, Fade((Color){ 255, 240, 205, 255 }, 0.70f * s * (1.0f - k)));
        }
        /* 坑口余温：皿底一圈暖光 */
        DrawCircleGradient((int)dish.x, (int)(dish.y + 60.0f), 110.0f, Fade(tint, 0.12f * s), BLANK);
    } else if (env == ENV_INDUSTRIAL_SMOG) {
        /* 雾霾：横向灰褐霾团缓慢右移 + 煤烟颗粒，整体压灰压暗。
           （旧写法用整宽矩形，会横穿左侧培养皿与右侧商店，面板边界被切出硬边） */
        DrawRectangle(0, 0, UI_W, UI_H, Fade((Color){ 96, 86, 74, 255 }, 0.14f * s));
        DrawRectangleGradientV((int)st.x, (int)st.y, (int)st.width, (int)st.height,
                               Fade((Color){ 120, 106, 88, 255 }, 0.10f * s), BLANK);
        for (int i = 0; i < 5; ++i) {
            /* 中心与半径都收在展台内：霾团不越界，左侧培养皿和右侧商店就不会被糊一层椭圆 */
            float cx = st.x + 104.0f + fmodf(t * 9.0f + (float)i * 118.0f, st.width - 208.0f);
            float cy = st.y + 58.0f + (float)i * 44.0f + sinf(t * 0.45f + (float)i) * 5.0f;
            DrawEllipse((int)cx, (int)cy, 88.0f, 26.0f, Fade((Color){ 170, 150, 126, 255 }, 0.13f * s));
            DrawEllipse((int)cx, (int)(cy + 7.0f), 52.0f, 15.0f,
                        Fade((Color){ 150, 132, 110, 255 }, 0.12f * s));
        }
        for (int i = 0; i < 56; ++i) {
            unsigned int seed = (unsigned int)i * 19u + 7u;
            float x = fmodf(hash01(seed) * (float)UI_W + t * 22.0f, (float)UI_W);
            float y = hash01(seed + 37u) * (float)UI_H + sinf(t * 0.7f + (float)i) * 4.0f;
            float r = 1.0f + 1.8f * hash01(seed + 59u);
            DrawCircleV((Vector2){ x, y }, r, Fade((Color){ 120, 108, 96, 255 }, 0.34f * s));
            DrawCircleV((Vector2){ x, y }, r * 2.2f, Fade((Color){ 120, 108, 96, 255 }, 0.08f * s));
        }
    } else {
        /* 富氧 / 太阳耀斑等：气泡式微光，不做强天气，避免所有事件长得一样 */
        for (int i = 0; i < 22; ++i) {
            unsigned int seed = (unsigned int)i * 43u + 17u;
            float u = fmodf(hash01(seed) + t * 0.12f, 1.0f);
            Vector2 p = { st.x + hash01(seed + 23u) * st.width,
                          st.y + st.height * (1.0f - u) };
            DrawRing(p, 2.0f, 3.0f, 0.0f, 360.0f, 16, Fade(tint, 0.22f * s));
        }
    }
}

static void draw_crisis_fx(const GameState *game, const UIState *S)
{
    (void)game;
    if (S->crisis_t <= 0.0f || S->crisis_env < 0) return;

    int env = S->crisis_env;
    Color tint = crisis_tint(env);
    float k = S->crisis_t / 2.2f;      /* 1 → 0 */
    float a = k * k;                   /* 平方衰减，前 0.3 s 最猛 */
    float t = S->time;

    /* 全屏色调 + 冲击闪光 */
    DrawRectangle(0, 0, UI_W, UI_H, Fade(tint, 0.14f * a));
    if (S->crisis_flash > 0.0f) {
        DrawRectangle(0, 0, UI_W, UI_H, Fade(tint, 0.26f * S->crisis_flash));
    }

    if (env == ENV_METEOR_IMPACT) {
        /* 陨石：从画面上方斜砸向培养皿的火痕 + 一圈冲击波 */
        Vector2 c = { 194.0f, 322.0f };
        for (int i = 0; i < 64; ++i) {
            unsigned int seed = (unsigned int)i * 17u + 3u;
            float h1 = hash01(seed), h2 = hash01(seed + 91u), h3 = hash01(seed + 173u);
            Vector2 start = { h1 * (float)UI_W, -60.0f - h2 * 260.0f };
            Vector2 dir = { c.x - start.x, c.y - start.y };
            float len = sqrtf(dir.x * dir.x + dir.y * dir.y) + 0.001f;
            dir.x /= len; dir.y /= len;
            /* 起始就推进 35%：否则刚触发的那几帧火痕还全在画面外，玩家看不到 */
            float travel = (0.35f + 0.65f * (1.0f - a)) * len;
            Vector2 head = { start.x + dir.x * travel, start.y + dir.y * travel };
            float sl = 60.0f + 110.0f * h3;
            Vector2 tail = { head.x - dir.x * sl, head.y - dir.y * sl };
            DrawLineEx(tail, head, 1.0f + 2.4f * h3, Fade((Color){ 255, 196, 120, 255 }, 0.62f * a));
            DrawCircleV(head, 1.4f + 1.6f * h3, Fade((Color){ 255, 240, 200, 255 }, 0.75f * a));
        }
        float rr = 26.0f + (1.0f - a) * 300.0f;
        DrawRing(c, rr, rr + 3.0f, 0.0f, 360.0f, 72, Fade(tint, 0.55f * a));
        DrawRing(c, rr * 0.62f, rr * 0.62f + 2.0f, 0.0f, 360.0f, 64, Fade(tint, 0.35f * a));
        ui_glow(c, 180.0f * (0.6f + 0.4f * a), tint, 0.30f * a);
    } else if (env == ENV_SOLAR_FLARE) {
        /* 太阳耀斑：右上角放射光条 */
        Vector2 c = { UI_W - 120.0f, -40.0f };
        for (int i = 0; i < 18; ++i) {
            float ang = PI * 0.5f + (float)i * (PI * 0.5f / 18.0f);
            float r0 = 90.0f, r1 = 520.0f * (0.5f + 0.5f * a);
            Vector2 p0 = { c.x + cosf(ang) * r0, c.y + sinf(ang) * r0 };
            Vector2 p1 = { c.x + cosf(ang) * r1, c.y + sinf(ang) * r1 };
            DrawLineEx(p0, p1, 2.0f, Fade(tint, 0.13f * a));
        }
        ui_glow(c, 300.0f, tint, 0.30f * a);
    } else if (env == ENV_TOXIC_BLOOM) {
        /* 毒潮：慢慢爬上来的绿雾，从画面下沿往中间蔓延 */
        for (int i = 0; i < 5; ++i) {
            float y = (float)UI_H - 40.0f - (float)i * 46.0f - (1.0f - a) * 60.0f;
            float wob = sinf(t * 1.6f + (float)i) * 8.0f;
            DrawRectangle(0, (int)y, UI_W, 54, Fade(tint, 0.05f * a));
            DrawCircleV((Vector2){ 180.0f + (float)i * 260.0f + wob, y }, 120.0f,
                        Fade(tint, 0.06f * a));
        }
    } else if (env == ENV_GLOBAL_COOLING) {
        /* 骤冷：斜向飘落的冰晶 */
        for (int i = 0; i < 60; ++i) {
            unsigned int seed = (unsigned int)i * 23u + 7u;
            float x = hash01(seed) * (float)UI_W + t * 26.0f;
            float y = fmodf(hash01(seed + 41u) * (float)UI_H + (1.0f - a) * 260.0f, (float)UI_H);
            float r = 1.0f + 1.6f * hash01(seed + 83u);
            DrawCircleV((Vector2){ fmodf(x, (float)UI_W), y }, r, Fade(tint, 0.5f * a));
        }
    } else {
        /* 其它危机（变暖 / 富氧 / 低光 / 其它）：轻微热浪脉冲 */
        for (int i = 0; i < 4; ++i) {
            float rr = 260.0f + (float)i * 90.0f + (1.0f - a) * 160.0f;
            DrawRing((Vector2){ 194.0f, 322.0f }, rr, rr + 2.0f, 0.0f, 360.0f, 72,
                     Fade(tint, 0.10f * a));
        }
    }

    /* 危机标题：落在环境横幅下方，不遮倒计时环；最后 0.4 s 淡出 */
    {
        float fade = k > 0.55f ? 1.0f : k / 0.55f;
        float off = (1.0f - (k > 0.8f ? 1.0f : k / 0.8f)) * -18.0f;
        Rectangle plate = { 430.0f, 170.0f + off, 420.0f, 56.0f };
        /* 横幅固定红（危机=危险），标题板跟着危机自己的色走，
           这样“热浪/寒潮/毒潮/雾霾”在演出瞬间就能一眼分开 */
        Color plate_bg = ui_color_mix(COL_BAD_SURFACE, tint, 0.30f);
        ui_panel(plate, R_CARD, Fade(plate_bg, 0.92f * fade), Fade(tint, 0.80f * fade));
        ui_text_center("环境危机", plate.x + plate.width * 0.5f, plate.y + 5.0f,
                       F_CAPTION, Fade(tint, fade));
        const char *nm = ua_environment_name(env);
        ui_text_center_strong(nm, plate.x + plate.width * 0.5f, plate.y + 23.0f,
                              F_TITLE, Fade(COL_TEXT, fade));
    }
}


static void draw_main(const GameState *game, const UIState *state, const ActionResult *last)
{
    (void)last;

    /* 全屏极淡的环境色晕：让整屏氛围跟着时代走（热泉偏青、陆地偏暖、轨道偏冷），
     * 只压上下边缘，中心的星云与卡片不受影响。 */
    habitat_bleed(current_habitat(game), state->time, 1.0f);

    draw_top_bar(game);
    draw_dish(game);
    draw_species_card(game, 0);
    draw_env_banner(game);
    draw_stage(game);
    draw_milestone(game);
    draw_info_panel(game);
    draw_shop(game);
    draw_bottom_bar(game);
    /* 危机氛围 + 进入瞬间的爆发演出，压在所有面板之上 */
    env_weather(game, state);
    draw_crisis_fx(game, state);
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

    /* 结算前把“这一纪元还能走多深”写在标题右侧：
     * 玩家在这里才真正权衡“继续爬”还是“换纪元”，不给这个数字就没得比。 */
    {
        int cap = ua_era_stage_cap(game);
        int stage = ua_main_stage(game);
        bool capped = cap > 0 && stage >= cap;
        char capnote[96];
        if (capped) {
            snprintf(capnote, sizeof(capnote), "本纪元上限 阶段 %d · 已封顶", cap);
        } else {
            snprintf(capnote, sizeof(capnote), "本纪元上限 阶段 %d · 还能再走 %d 阶", cap, cap - stage);
        }
        ui_text_right(capnote, panel.x + panel.width - 36.0f, panel.y + 32.0f, F_LABEL,
                      capped ? COL_BAD : COL_FAINT);
    }

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
static Rectangle tree_sky_rect(void);
static void tree_visibility(const GameState *game, bool found[SPECIES_COUNT],
                            bool near[SPECIES_COUNT]);
static Vector2 tree_view(Vector2 p);
static Rectangle mut_list_panel_rect(void);
static void tree_layout(Vector2 node[SPECIES_COUNT], int parent_of[SPECIES_COUNT],
                        int sib[SPECIES_COUNT]);
static float tree_node_r(void);

static Rectangle mutation_panel_rect(const GameState *game)
{
    int n = game->run.mutation_offer.active ? game->run.mutation_offer.choice_count : 0;
    if (n < 1) n = 1;
    /* 每条候选卡多一行"这条路线后续通向哪里"，卡高 98→112、间距 110→124 */
    float h = 96.0f + n * 124.0f + 60.0f;
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

static bool codex_found(const GameState *game, int kind, int id);

/* 图鉴的可见条目列表。
 *
 * 物种页只列「已发现 + 相邻」：186 个物种全列要翻 16 页，第 2 页起全是剪影，
 * 玩家反而找不到自己认识的那几个。相邻 = 已发现物种的父与子，
 * 让玩家看到"下一步能往哪走"，而不是一片问号。
 * 顺序：已发现的在前（按阶段→编号），相邻的在后 —— 一打开就是自己认识的。
 *
 * 突变页只列已收录：未收录的条目连名字都没有，摆一屏"未发现"同样没信息量。 */
static int codex_visible_list(const GameState *game, int kind, int *out, int max_out)
{
    int count = codex_count((CodexKind)kind);
    if (!out || max_out <= 0) return 0;

    if (kind != CODEX_SPECIES) {
        int n = 0;
        for (int i = 0; i < count && n < max_out; ++i) {
            if (codex_found(game, kind, i)) out[n++] = i;
        }
        return n;
    }

    bool found[SPECIES_COUNT];
    bool inc[SPECIES_COUNT];
    for (int i = 0; i < SPECIES_COUNT; ++i) {
        found[i] = game->legacy.codex_species[i] || game->run.discovered_species[i];
        inc[i] = found[i];
    }
    inc[0] = true;      /* 原始细胞永远是已知的起点 */

    for (int i = 0; i < SPECIES_COUNT; ++i) {
        if (!found[i]) continue;
        int ps[8];
        int pn = ua_species_parents(i, ps, 8);
        for (int k = 0; k < pn; ++k) {
            if (ps[k] >= 0 && ps[k] < SPECIES_COUNT) inc[ps[k]] = true;
        }
        int cs[SPECIES_COUNT];
        int cn = ua_species_child_list(i, cs, SPECIES_COUNT);
        for (int k = 0; k < cn; ++k) {
            if (cs[k] >= 0 && cs[k] < SPECIES_COUNT) inc[cs[k]] = true;
        }
    }

    /* 两趟：先已发现、后相邻；每趟内部按 阶段→编号 插入排序 */
    int n = 0;
    for (int pass = 0; pass < 2; ++pass) {
        int seg = n;
        for (int i = 0; i < SPECIES_COUNT && n < max_out; ++i) {
            if (!inc[i]) continue;
            if ((pass == 0) != found[i]) continue;
            out[n++] = i;
        }
        for (int i = seg + 1; i < n; ++i) {
            int v = out[i];
            int sv = ua_species_stage(v);
            int j = i - 1;
            while (j >= seg) {
                int sj = ua_species_stage(out[j]);
                if (sj < sv || (sj == sv && out[j] < v)) break;
                out[j + 1] = out[j];
                --j;
            }
            out[j + 1] = v;
        }
    }
    return n;
}

static int codex_page_count(const GameState *game, int tab)
{
    int list[SPECIES_COUNT];
    int total = codex_visible_list(game, tab, list, SPECIES_COUNT);
    return total > 0 ? (total + UI_CODEX_PAGE_SIZE - 1) / UI_CODEX_PAGE_SIZE : 1;
}

static int codex_page_first(const GameState *game, const UIState *state)
{
    int pages = codex_page_count(game, state->codex_tab);
    int page = state->codex_page;
    if (page < 0) page = 0;
    if (page >= pages) page = pages - 1;
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
            /* 突变是双币：DNA 主价 + ATP 门票。只判 DNA 会让卡面显示“可点”，
             * 点下去却只弹一句「ATP 不足」，玩家不知道还差多少 —— 所以两边一起判。 */
            int mid = game->run.mutation_offer.mutation_ids[i];
            bool afford = game->run.dna >= game->run.mutation_offer.dna_costs[i] &&
                          game->run.atp >= ua_mutation_atp_cost(game, mid);
            btn_add(L, UI_BTN_CHOOSE_MUTATION,
                    (Rectangle){ panel.x + 24.0f, panel.y + 84.0f + (float)i * 124.0f,
                                 panel.width - 48.0f, 112.0f },
                    game->run.mutation_offer.mutation_ids[i], i, afford);
        }        btn_add(L, UI_BTN_CLOSE_OVERLAY,
                (Rectangle){ panel.x + (panel.width - 180.0f) * 0.5f,
                             panel.y + panel.height - 52.0f, 180.0f, H_SECONDARY }, 0, 0, true);
        if (game->run.mutation_offer.active) {
            /* v14 E3：换一批候选。每批 ≤3 次，价格随刷新次数 ×1.7。 */
            double refresh = ua_mutation_refresh_cost(game);
            bool can = game->run.mutation_refresh_count < MUTATION_REFRESH_LIMIT
                    && game->run.dna >= refresh;
            btn_add(L, UI_BTN_REFRESH_MUTATION,
                    (Rectangle){ panel.x + 24.0f, panel.y + panel.height - 52.0f,
                                 200.0f, H_SECONDARY },
                    0, 0, can);
        }
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
                    (Rectangle){ panel.x + 24.0f, panel.y + 84.0f + (float)(i - first) * 108.0f,
                                 panel.width - 48.0f, 100.0f },
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
        int list[SPECIES_COUNT];
        int count = codex_visible_list(game, S->codex_tab, list, SPECIES_COUNT);
        int first = codex_page_first(game, S);
        for (int k = first; k < count && k < first + UI_CODEX_PAGE_SIZE; ++k) {
            int id = list[k];
            btn_add(L, UI_BTN_CODEX_ENTRY, codex_cell_rect(panel, k - first), id, S->codex_tab,
                    codex_found(game, S->codex_tab, id));
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
        Rectangle sky = tree_sky_rect();

        /* 星图空白区先入表：点空白是"按住拖动"，不是"关闭弹层"。
         * 节点按钮在它之后入表，命中优先级更高。 */
        btn_add(L, UI_BTN_TREE_SKY, sky, 0, 0, true);

        /* 每个节点一个按钮：命中框比徽记大 6px（4K 下手指也不用狙击）。
         * 几何与绘制同源 —— 都从 tree_layout() 取坐标，再套同一个 pan/zoom。 */
        Vector2 node[SPECIES_COUNT];
        int parent_of[SPECIES_COUNT];
        tree_layout(node, parent_of, NULL);
        float nr = tree_node_r();
        float hit = nr + 6.0f;
        bool v_found[SPECIES_COUNT], v_near[SPECIES_COUNT];
        tree_visibility(game, v_found, v_near);
        for (int i = 0; i < SPECIES_COUNT; ++i) {
            if (!v_found[i] && !v_near[i]) continue;
            Vector2 p = tree_view(node[i]);
            /* 画框外的节点不给按钮：既点不到，也不会挡住底部图例 */
            if (p.x < sky.x || p.x > sky.x + sky.width) continue;
            if (p.y < sky.y || p.y > sky.y + sky.height) continue;
            btn_add(L, UI_BTN_TREE_NODE,
                    (Rectangle){ p.x - hit, p.y - hit, hit * 2.0f, hit * 2.0f },
                    i, 0, true);
        }
        btn_add(L, UI_BTN_TREE_RESET,
                (Rectangle){ panel.x + panel.width - 216.0f, panel.y + 16.0f, 76.0f, 36.0f },
                0, 0, true);
        btn_add(L, UI_BTN_CLOSE_OVERLAY,
                (Rectangle){ panel.x + panel.width - 132.0f, panel.y + 16.0f, 108.0f, 36.0f },
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

/* 行尾遗留的 "·" 与空格：折行落在分隔符上的话，下一行以内容开头会更干净 */
static void trim_row_tail(char *s)
{
    int n = (int)strlen(s);
    for (;;) {
        if (n > 0 && s[n - 1] == ' ') { s[--n] = '\0'; continue; }
        if (n >= 2 && (unsigned char)s[n - 2] == 0xC2u && (unsigned char)s[n - 1] == 0xB7u) {
            n -= 2;
            s[n] = '\0';
            continue;
        }
        break;
    }
}

/* 按实测像素宽度折行（中文逐字宽、数字/百分号更窄，不能按字数估）。
 * 行数用尽时在末行补省略号，必要时从尾部退到 UTF-8 边界再补 —— 卡片是硬边框，
 * 宁可少读两个效果项，也不能让字冲出框外。返回实际行数。 */
static int wrap_text_limited(const char *text, float width, float size, int max_rows,
                             char rows[][192])
{
    if (max_rows < 1) max_rows = 1;
    for (int i = 0; i < max_rows; ++i) rows[i][0] = '\0';

    int row = 0, used = 0;
    const char *p = text;
    while (*p) {
        int bytes = 1;
        while (p[bytes] && ((unsigned char)p[bytes] & 0xC0u) == 0x80u) ++bytes;
        char trial[192];
        memcpy(trial, rows[row], (size_t)used);
        memcpy(trial + used, p, (size_t)bytes);
        trial[used + bytes] = '\0';
        if (used > 0 && ui_text_w(trial, size) > width) {
            if (row + 1 >= max_rows) break;   /* 行数用尽：剩下的不要了 */
            trim_row_tail(rows[row]);
            ++row;
            used = 0;
        }
        memcpy(rows[row] + used, p, (size_t)bytes);
        used += bytes;
        rows[row][used] = '\0';
        p += bytes;
    }

    if (*p) {
        char *last = rows[row];
        int n = (int)strlen(last);
        float ell = ui_text_w("…", size);
        while (n > 0 && ui_text_w(last, size) + ell > width) {
            --n;
            while (n > 0 && ((unsigned char)last[n] & 0xC0u) == 0x80u) --n;
            last[n] = '\0';
        }
        trim_row_tail(last);
        strncat(last, "…", sizeof(rows[0]) - strlen(last) - 1);
    } else {
        trim_row_tail(rows[row]);
    }
    return row + 1;
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
        Rectangle r = { panel.x + 24.0f, panel.y + 84.0f + (float)i * 124.0f,
                        panel.width - 48.0f, 112.0f };
        UiButton *b = btn_find(S, UI_BTN_CHOOSE_MUTATION, game->run.mutation_offer.mutation_ids[i]);
        bool hot = is_hot(S, b);
        int mid = game->run.mutation_offer.mutation_ids[i];
        bool rare = ua_mutation_is_rare(mid);
        float hv = btn_hover_step(b, hot);
        bool usable = b && b->enabled;

        if (usable && hv > 0.02f) ui_halo(r, R_CARD, rare ? COL_RARE : COL_DNA, 0.10f * hv);
        ui_panel(r, R_CARD, usable ? ui_color_mix(COL_PANEL_HI, COL_PANEL_HOT, hv) : COL_PANEL_HI,
                 rare ? Fade(COL_RARE, 0.60f) : Fade(COL_LINE, 0.6f));

        Rectangle tile = { r.x + 14.0f, r.y + 28.0f, 54.0f, 54.0f };
        ui_panel(tile, 10.0f, Fade(COL_BG_DEEP, 0.6f), Fade(rare ? COL_RARE : COL_DNA, 0.30f));
        draw_mutation_sigil((Vector2){ tile.x + 27.0f, tile.y + 27.0f }, mid,
                            rare ? COL_RARE : COL_DNA, rare, g_ui.time);

        ui_text_strong(ua_mutation_name(mid), r.x + 82.0f, r.y + 10.0f, F_TITLE, COL_TEXT);
        if (rare) {
            float nw = ui_text_w(ua_mutation_name(mid), F_TITLE);
            Rectangle tag = { r.x + 82.0f + nw + 10.0f, r.y + 14.0f,
                              ui_text_w("稀有", F_CAPTION) + 14.0f, H_CHIP };
            ui_panel(tag, R_CHIP(tag.height), Fade(COL_RARE, 0.16f), Fade(COL_RARE, 0.45f));
            ui_text_center("稀有", tag.x + tag.width * 0.5f, tag.y + 4.0f, F_CAPTION, COL_RARE);
        }

        /* 效果摘要最多 9 项（5 项百分比 + 5 项抗性 + 理念），一行必然冲出卡片右边界。
         * 按实测字宽折两行，仍放不下就末行省略号收口。 */
        char desc[224];
        ua_mutation_desc(mid, desc, (int)sizeof(desc));
        char drow[3][192];
        float text_w = r.width - 82.0f - 16.0f;
        int drows = wrap_text_limited(desc, text_w, F_BODY, 2, drow);
        for (int k = 0; k < drows; ++k) {
            ui_text(drow[k], r.x + 82.0f, r.y + 31.0f + (float)k * 15.0f, F_BODY, COL_DIM);
        }
        float below = r.y + 31.0f + (float)drows * 15.0f;

        /* 路线预告：选之前就知道这条路后面会解锁哪些物种。
         * 名字与走向由适配层从"前置突变 → 物种 → 子代"现查，UI 不复制规则表。
         * 三跳物种名同样会超宽，按剩余宽度折一行 + 省略号。 */
        char route[192];
        ua_mutation_route(mid, route, (int)sizeof(route));
        float route_y = below + 2.0f;
        ui_text("后续", r.x + 82.0f, route_y, F_CAPTION, COL_ACCENT);
        float route_w = text_w - ui_text_w("后续 ", F_CAPTION);
        char rrow[2][192];
        wrap_text_limited(route, route_w, F_CAPTION, 1, rrow);
        ui_text(rrow[0], r.x + 82.0f + ui_text_w("后续 ", F_CAPTION), route_y,
                F_CAPTION, COL_DIM);

        /* 双币报价：DNA 主价（紫）+ ATP 门票（黄）。ATP 门票随已购张数上涨，
         * 不写出来玩家不会知道“为什么 DNA 够了还是买不了”。 */
        char cost[64];
        float cx = r.x + 82.0f;
        ui_fmt_num(game->run.mutation_offer.dna_costs[i], num, (int)sizeof(num));
        snprintf(cost, sizeof(cost), "%s DNA", num);
        ui_text(cost, cx, r.y + 83.0f, F_BODY, usable ? COL_DNA : COL_BAD);
        cx += ui_text_w(cost, F_BODY);

        double atp_price = ua_mutation_atp_cost(game, mid);
        if (atp_price > 0.0) {
            char an[32];
            ui_fmt_num(atp_price, an, (int)sizeof(an));
            snprintf(cost, sizeof(cost), "  ·  %s ATP", an);
            ui_text(cost, cx, r.y + 83.0f, F_BODY, usable ? COL_ATP : COL_BAD);
        }

        ui_text_right(usable ? "点击选择" : "资源不足", r.x + r.width - 16.0f, r.y + 85.0f,
                      F_CAPTION, usable ? COL_FAINT : COL_BAD);
    }

    UiButton *refresh = btn_find(S, UI_BTN_REFRESH_MUTATION, -1);
    if (refresh) {
        char price[64], sub[48];
        if (game->run.mutation_refresh_count >= MUTATION_REFRESH_LIMIT) {
            snprintf(sub, sizeof(sub), "本批已刷新 3 次");
        } else {
            ui_fmt_num(ua_mutation_refresh_cost(game), num, (int)sizeof(num));
            snprintf(sub, sizeof(sub), "%s DNA · %d/3", num,
                     game->run.mutation_refresh_count);
        }
        (void)price;
        ui_btn_draw(refresh, "换一批", sub, is_hot(S, refresh));
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

    /* 本纪元的阶段上限（v8 P0-17）。这条不写出来时，玩家到顶后只会看到
     * 每张卡都「前置不足」，而实际原因是一条跟资源、跟突变都无关的规则。 */
    {
        int cap = ua_era_stage_cap(game);
        int stage = ua_main_stage(game);
        bool capped = cap > 0 && stage >= cap;
        char note[192];
        if (capped) {
            snprintf(note, sizeof(note),
                     "第 %d 纪元上限：阶段 %d · 已到顶，本轮不会再出现可分化方向，"
                     "结算进入下一纪元才能继续上行", game->run.era, cap);
        } else {
            snprintf(note, sizeof(note), "第 %d 纪元上限：阶段 %d · 主线当前阶段 %d，还能再走 %d 阶",
                     game->run.era, cap, stage, cap - stage);
        }
        ui_text(note, panel.x + 24.0f, panel.y + 54.0f, F_CAPTION, capped ? COL_BAD : COL_FAINT);
    }

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
        Rectangle r = { panel.x + 24.0f, panel.y + 84.0f + (float)(i - first) * 108.0f,
                        panel.width - 48.0f, 100.0f };
        UiButton *b = btn_find(S, UI_BTN_SPECIATE, children[i]);
        bool hot = is_hot(S, b);
        int sid = children[i];
        ActionCode check = ua_speciation_check(game, sid);
        bool capped = ua_speciation_era_capped(game, sid);
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
        if (capped) {
            /* 封顶时把“需要某项突变”换成真实原因：阶段数越界。
             * 卡的上一行已经写了目标阶段，这里不再重复“阶段 N”。 */
            snprintf(req, sizeof(req), "超出本纪元上限 %d · 结算进入下一纪元",
                     ua_era_stage_cap(game));
        } else {
            ua_speciation_requirement(sid, req, (int)sizeof(req));
        }
        ui_text(req, r.x + 84.0f, r.y + 56.0f, F_CAPTION,
               (capped || check != ACTION_OK) ? COL_BAD : COL_FAINT);

        /* 与突变候选同一套提示：选这个分支之后还能走向哪里。
         * 三跳物种名在 264 物种里可能很长，按剩余宽度折一行 + 省略号兜底。 */
        char tail[128];
        ua_species_route(sid, tail, (int)sizeof(tail));
        ui_text("后续", r.x + 84.0f, r.y + 77.0f, F_CAPTION, COL_ACCENT);
        float tail_w = r.width - 84.0f - 18.0f - ui_text_w("后续 ", F_CAPTION);
        char trow[2][192];
        wrap_text_limited(tail, tail_w, F_CAPTION, 1, trow);
        ui_text(trow[0], r.x + 84.0f + ui_text_w("后续 ", F_CAPTION), r.y + 77.0f,
                F_CAPTION, COL_DIM);

        char price[64];
        ui_fmt_num(cost, num, (int)sizeof(num));
        snprintf(price, sizeof(price), "%s 生物量", num);
        ui_text_right(price, r.x + r.width - 18.0f, r.y + 16.0f, F_LABEL,
                      afford ? COL_BIOMASS : COL_BAD);

        if (capped) {
            ui_text_right("本纪元封顶", r.x + r.width - 18.0f, r.y + 42.0f, F_CAPTION, COL_BAD);
        } else if (check != ACTION_OK) {
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
        int list[SPECIES_COUNT];
        int count = codex_visible_list(game, S->codex_tab, list, SPECIES_COUNT);
        int first = codex_page_first(game, S);
        int discovered = 0;
        for (int k = 0; k < count; ++k) discovered += codex_found(game, S->codex_tab, list[k]);
        for (int k = first; k < count && k < first + UI_CODEX_PAGE_SIZE; ++k) {
            int i = list[k];
            Rectangle r = codex_cell_rect(panel, k - first);
            bool found = codex_found(game, S->codex_tab, i);
            UiButton *button = btn_find(S, UI_BTN_CODEX_ENTRY, i);
            float hover = btn_hover_step(button, is_hot(S, button));
            Color accent = S->codex_tab == CODEX_SPECIES ? ecology_color(i) : COL_DNA;
            ui_panel(r, R_CARD, found ? ui_color_mix(COL_PANEL_HI, COL_PANEL_HOT, hover) : COL_SURFACE_OFF,
                     found ? Fade(accent, 0.35f + hover * 0.35f) : Fade(COL_LINE, 0.5f));
            Vector2 center = { r.x + 42.0f, r.y + 54.0f };
            if (S->codex_tab == CODEX_SPECIES) {
                /* 图标区 72×72：卡片左侧留给名字/阶段，杆状与行星形态不能压到文字上 */
                Rectangle icon = { r.x + 6.0f, r.y + 18.0f, 72.0f, 72.0f };
                if (found) {
                    draw_organism_in(icon, center, 25.0f, i, ua_species_stage(i), accent,
                                     S->time + (float)i * 0.7f, true, true, 1.0f);
                } else {
                    draw_organism(center, 25.0f, i, ua_species_stage(i), COL_GHOST,
                                  S->time + (float)i * 0.7f, false, false, 1.0f);
                }
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
        char count_text[96], page_text[96];
        if (S->codex_tab == CODEX_SPECIES) {
            /* 物种页只列"已发现 + 相邻"，所以分母是全量 186，不是列表长度 */
            snprintf(count_text, sizeof count_text, "已发现 %d / %d · 相邻 %d",
                     discovered, SPECIES_COUNT, count - discovered);
        } else {
            snprintf(count_text, sizeof count_text, "已收录 %d / %d", discovered,
                     codex_count(CODEX_MUTATION));
        }
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
    int dhab = HABITAT_HYDROTHERMAL;
    if (is_species) {
        dhab = habitat_for_species(S->codex_detail_id, ua_species_stage(S->codex_detail_id));
    }
    if (found) {
        /* 已发现的物种：大图里也给一片它自己的生境，和展台是同一套 */
        if (is_species) {
            ui_clip_begin(art);
            habitat_draw(art, dhab, S->time, 0.72f);
            ui_clip_end();
        }
        ui_glow(center, 100.0f, accent, 0.25f);
        if (is_species)
            draw_organism_in(art, center, 66.0f, S->codex_detail_id,
                             ua_species_stage(S->codex_detail_id),
                             ecology_color(S->codex_detail_id), S->time, true, true, 1.0f);
        else
            draw_mutation_sigil_scaled(center, S->codex_detail_id, accent,
                                       ua_mutation_is_rare(S->codex_detail_id), S->time, 3.0f);
    } else ui_text_center("?", center.x, center.y - 25.0f, F_HERO, COL_FAINT);
    {
        char tag[64];
        if (is_species && found) {
            snprintf(tag, sizeof(tag), "物种图鉴 · %s", habitat_name(dhab));
        } else {
            snprintf(tag, sizeof(tag), "%s", is_species ? "物种图鉴" : "突变图鉴");
        }
        ui_text_center(tag, panel.x + panel.width * 0.5f, panel.y + 340.0f, F_CAPTION, accent);
    }
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
 * v9 三处改动（内容是 186 物种 / 156 叶子，旧参数已经画不开）：
 *   1. 环数不再写死 11：按内容最大阶段动态生成。旧版把阶段钳到 11，
 *      阶段 12/13 的物种全被压到第 11 圈上重叠。
 *   2. 画框放到接近全屏，横向半径比 2.2：最外圈弧长 ≈ 22px > 节点直径 19px，
 *      同圈节点不再必然重叠。
 *   3. 支持拖拽平移 + 滚轮缩放：默认 1.0 看全貌，滚轮放大到 2.8 看细节。
 * 另外未探索的深层分支不再全画（186 个节点的全连线是一张糊住的网），
 * 只保留"已点亮"和"从已点亮往外一格"的前沿连线。
 */

#define TREE_MAX_RY 246.0f
/* v16 F8：最内环下限与画框留白。pow(1.25) 在第 1 环只给 0.0545R ≈ 13px 的增量，
 * 同环比内圈密，11 个节点会重新挤成一坨；给个 56px 的地板，最外环再按画框留 24px。 */
#define TREE_MIN_RY 56.0f
#define TREE_FRAME_MARGIN 24.0f
/* v14 W12：同屏亲缘边上限。264 节点下“全连线”会糊成网，超出按离圆心升序保留。 */
#define TREE_EDGE_CAP 120

static int s_maxst = 0;

static int tree_max_stage(void)
{
    if (s_maxst <= 0) {
        s_maxst = ua_max_species_stage();
        if (s_maxst < 1) s_maxst = 1;
    }
    return s_maxst;
}

/* 环半径：最外圈固定 246，内圈按阶段等比铺开，阶段数变化时自动重算 */
static float tree_ring_ry(int s)
{
    float m = (float)tree_max_stage();
    if (s < 0) s = 0;
    if ((float)s > m) s = (int)m;
    /* v14：环比不等距——内圈密、外圈疏。264 个物种下同圈 26–27 个，
     * 线性等距会让最外两圈挤在一起；pow 1.25 把外圈推开、内圈收紧。 */
    float r = 48.0f + (TREE_MAX_RY - 48.0f) * powf((float)s / m, 1.25f);
    /* v16 F8：内环地板 + 外环画框留白，两头都不许贴边 */
    if (r < TREE_MIN_RY) r = TREE_MIN_RY;
    float frame = TREE_MAX_RY;
    Rectangle panel = tree_panel_rect();
    float limit = panel.height * 0.5f - TREE_FRAME_MARGIN;
    if (limit < frame) frame = limit;
    if (r > frame) r = frame;
    return r;
}

/* 横向 2.2 倍：面板是 16:9，正圆会在左右各空出 200px；拉成椭圆既不挤，
 * 又把画框用满（最外圈半宽 541 < 606，上下 246 < 250）。 */
static float tree_ring_rx(int s) { return tree_ring_ry(s) * 2.2f; }

static Rectangle tree_panel_rect(void)
{
    return (Rectangle){ 20.0f, 16.0f, 1240.0f, 676.0f };
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

Vector2 screens_tree_center(void) { return tree_center(); }

float screens_tree_pan_limit(const GameState *game)
{
    (void)game;
    float z = g_ui.tree_zoom > 0.05f ? g_ui.tree_zoom : 1.0f;
    return 540.0f * z + 140.0f;
}

/* 地图坐标 → 屏幕坐标：以圆心为原点缩放，再叠加平移 */
static Vector2 tree_view(Vector2 p)
{
    Vector2 c = tree_center();
    float z = g_ui.tree_zoom > 0.05f ? g_ui.tree_zoom : 1.0f;
    return (Vector2){ c.x + (p.x - c.x) * z + g_ui.tree_pan_x,
                      c.y + (p.y - c.y) * z + g_ui.tree_pan_y };
}

static int   s_kid[SPECIES_COUNT][SPECIES_COUNT];
static int   s_kidn[SPECIES_COUNT];
static int   s_order[SPECIES_COUNT];   /* 先序遍历序号：同圈内按谱系顺序排，兄弟相邻 */

/* 先序 DFS 给每个节点一个"谱系顺序"。圈内排序靠它，
 * 这样同一圈的物种彼此靠近的也确实是亲戚，而不是按 id 乱跳。 */
static void tree_order_dfs(int id, int *next, int guard)
{
    if (guard > SPECIES_COUNT) return;
    if (s_order[id] >= 0) return;
    s_order[id] = (*next)++;
    for (int k = 0; k < s_kidn[id]; ++k) tree_order_dfs(s_kid[id][k], next, guard + 1);
}

/* 星图布局。build_overlay（放节点按钮）与 draw_tree_overlay（画）共用同一份几何，
   避免“看得到的位置”和“点得到的位置”对不上。
   parent_of[] 输出主亲本（-1 表示根）；sib[] 仅层状布局用，这里保留参数占位。

   角度分配不再"按叶子数等分"：156 个叶子均分 2π 只有 2.3°/个，内圈半径小的时候
   弧长不到 5px，必然叠在一起（旧版截图里左边那一串就是）。改成**每一圈按该阶段
   的物种数均分**——同圈节点间距 = 2πR/k，半径和数量都确定，几何上保证不重叠。 */
static void tree_layout(Vector2 node[SPECIES_COUNT], int parent_of[SPECIES_COUNT],
                        int sib[SPECIES_COUNT])
{
    for (int i = 0; i < SPECIES_COUNT; ++i) {
        s_kidn[i] = 0;
        s_order[i] = -1;
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

    int next = 0;
    tree_order_dfs(0, &next, 0);
    for (int i = 0; i < SPECIES_COUNT; ++i) {
        if (s_order[i] < 0) s_order[i] = next++;   /* 走不到的孤儿排到最后 */
    }

    int maxst = tree_max_stage();
    Vector2 c = tree_center();
    for (int st = 0; st <= maxst; ++st) {
        int lst[SPECIES_COUNT];
        int n = 0;
        for (int i = 0; i < SPECIES_COUNT; ++i) {
            if (ua_species_stage(i) == st) lst[n++] = i;
        }
        if (n <= 0) continue;

        /* 按先序插入排序（n ≤ 17，比 qsort 更省事） */
        for (int a = 1; a < n; ++a) {
            int v = lst[a], b = a - 1;
            while (b >= 0 && s_order[lst[b]] > s_order[v]) { lst[b + 1] = lst[b]; --b; }
            lst[b + 1] = v;
        }

        float rx = tree_ring_rx(st), ry = tree_ring_ry(st);
        for (int k = 0; k < n; ++k) {
            /* 从正上方开始，每圈整体拧 0.085 rad：父子不落在同一条半径上，连线成螺旋 */
            float a = -PI * 0.5f + (float)k * (2.0f * PI / (float)n)
                    + 0.085f * (float)st;
            node[lst[k]] = (Vector2){ c.x + cosf(a) * rx, c.y + sinf(a) * ry };
        }
    }

    node[0] = c;   /* 中心留给原始细胞自己 */
}

static float tree_node_r(void) { return 8.0f; }

static void draw_nebula(Vector2 c, float rad, Color col, float a){
    for (int i = 0; i < 30; ++i) {
        float k = 1.0f - (float)i / 30.0f;
        DrawCircleV(c, rad * k, Fade(col, a * 0.012f * k * k));
    }
}

/* 亲缘线：控制点向圆心收 14%（“被引力拉弯”），起点/终点都落在节点边缘 */
static void tree_edge(Vector2 a, Vector2 b, Vector2 g, Color c, float w)
{
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

/* 同阶段亲缘线：两端落在同一圈上，直线弦会从星图中心横穿过去
 *（264 个物种里有 35 对是这种“同一时代里分出去的一支”，
 * 例如 藻类真核生物 → 光合菌席、简单多细胞群落 → 几丁质礁）。
 * 这里沿这一圈的椭圆弧走：弧线贴着圈，不会糊住别的节点，
 * 形状本身也说明了“这不是往下一阶段的推进，而是横向的一支”。 */
static void tree_arc(Vector2 a, Vector2 b, Vector2 g, float rx, float ry, Color c, float w)
{
    if (rx <= 0.0f || ry <= 0.0f) { tree_edge(a, b, g, c, w); return; }
    float a0 = atan2f((a.y - g.y) / ry, (a.x - g.x) / rx);
    float a1 = atan2f((b.y - g.y) / ry, (b.x - g.x) / rx);
    float d = a1 - a0;
    while (d > PI) d -= 2.0f * PI;
    while (d < -PI) d += 2.0f * PI;
    Vector2 prev = a;
    /* 段数随弧长走：短弧 8 段就够，跨半圈的弧要更密才不显折角。
     * 上限 40 段，避免两条隔了半圈的节点之间画出几百条小线段。 */
    float span = fabsf(d);
    int steps = (int)(span / (PI / 20.0f)) + 8;
    if (steps > 40) steps = 40;
    for (int i = 1; i <= steps; ++i) {
        float ang = a0 + d * ((float)i / (float)steps);
        Vector2 p = { g.x + cosf(ang) * rx, g.y + sinf(ang) * ry };
        DrawLineEx(prev, p, w, c);
        prev = p;
    }
}

/* 两种亲缘线共用入口：同圈走弧，跨圈走“被引力拉弯”的弦 */
static void tree_link(Vector2 a, Vector2 b, Vector2 g, float rx, float ry, bool same_ring,
                      Color c, float w)
{
    if (same_ring) tree_arc(a, b, g, rx, ry, c, w);
    else tree_edge(a, b, g, c, w);
}

/* v14 W12：264 节点下只画“已发现 + 与已发现相邻”的两圈。
 * 全量暗点会在最外 4 圈糊成一片，而且没有信息量。 */
static void tree_visibility(const GameState *game, bool found[SPECIES_COUNT],
                            bool near[SPECIES_COUNT])
{
    for (int i = 0; i < SPECIES_COUNT; ++i) {
        found[i] = game->legacy.codex_species[i] || game->run.discovered_species[i];
        near[i] = false;
    }
    for (int child = 1; child < SPECIES_COUNT; ++child) {
        if (!found[child]) continue;
        int ps[4];
        int n = ua_species_parents(child, ps, 4);
        for (int k = 0; k < n; ++k) {
            int p = ps[k];
            if (p >= 0 && p < SPECIES_COUNT && p != child) near[p] = true;
        }
    }
    for (int i = 0; i < SPECIES_COUNT; ++i) {
        if (found[i]) near[i] = false;
    }
}

static void draw_tree_overlay(const GameState *game, const UIState *S)
{
    Rectangle panel = tree_panel_rect();
    Rectangle sky = tree_sky_rect();
    ui_halo(panel, R_PANEL, COL_ACCENT, 0.10f);
    ui_panel(panel, R_PANEL, COL_PANEL, Fade(COL_ACCENT, 0.30f));

    ui_text_strong("进化树 · 星图", panel.x + 24.0f, panel.y + 14.0f, F_TITLE, COL_TEXT);
    ui_text("一圈一个阶段，越外越晚 · 同圈弧线 = 同阶段旁支 · 拖拽平移 · 滚轮缩放 · 点节点看它从哪来",
            panel.x + 24.0f, panel.y + 44.0f, F_CAPTION, COL_DIM);

    /* 发现表只算一次，节点降噪、前沿判定、标签排序共用 */
    bool found_flag[SPECIES_COUNT];
    int discovered = 0;
    int found_stage_max = 0;
    for (int i = 0; i < SPECIES_COUNT; ++i) {
        found_flag[i] = game->legacy.codex_species[i] || game->run.discovered_species[i];
        if (found_flag[i]) {
            ++discovered;
            int st = ua_species_stage(i);
            if (st > found_stage_max) found_stage_max = st;
        }
    }

    UiButton *reset = btn_find(S, UI_BTN_TREE_RESET, -1);
    if (reset) ui_btn_draw(reset, "复位", NULL, is_hot(S, reset));

    UiButton *close = btn_find(S, UI_BTN_CLOSE_OVERLAY, -1);
    if (close) ui_btn_draw(close, "关闭", NULL, is_hot(S, close));

    ui_panel(sky, R_CARD, COL_BG_DEEP, Fade(COL_LINE, 0.7f));
    ui_gradient((Rectangle){ sky.x + 8, sky.y + 8, sky.width - 16, sky.height - 16 },
                Fade(COL_BG_LIFT, 0.32f), Fade(COL_BG_DEEP, 0.88f));

    draw_nebula((Vector2){ sky.x + 250.0f, sky.y + 130.0f }, 180.0f, COL_DNA, 0.9f);
    draw_nebula((Vector2){ sky.x + 900.0f, sky.y + 380.0f }, 200.0f, COL_ACCENT, 0.8f);
    draw_nebula((Vector2){ sky.x + 520.0f, sky.y + 450.0f }, 150.0f, COL_TEAL, 0.7f);

    for (int i = 0; i < 160; ++i) {
        float x = sky.x + 6.0f + hash01((unsigned int)i * 3u + 1u) * (sky.width - 12.0f);
        float y = sky.y + 6.0f + hash01((unsigned int)i * 3u + 2u) * (sky.height - 12.0f);
        float a = 0.04f + 0.20f * hash01((unsigned int)i * 3u + 3u);
        float tw = 0.62f + 0.38f * sinf(g_ui.time * (0.6f + 0.9f * hash01((unsigned int)i * 5u)) + (float)i);
        DrawCircleV((Vector2){ x, y }, 0.7f + 1.2f * hash01((unsigned int)i * 7u + 9u),
                    Fade(COL_TEXT, a * tw));
    }

    float zoom = g_ui.tree_zoom > 0.05f ? g_ui.tree_zoom : 1.0f;
    Vector2 center = tree_center();
    Vector2 c2 = { center.x + g_ui.tree_pan_x, center.y + g_ui.tree_pan_y };
    int maxst = tree_max_stage();

    /* ---------- 同心圈（阶段刻度） ---------- */
    for (int s = 1; s <= maxst; ++s) {
        float rx = tree_ring_rx(s) * zoom, ry = tree_ring_ry(s) * zoom;
        Vector2 prev = { c2.x + rx, c2.y };
        for (int k = 1; k <= 64; ++k) {
            float a = (float)k * (2.0f * PI / 64.0f);
            Vector2 p = { c2.x + cosf(a) * rx, c2.y + sinf(a) * ry };
            DrawLineEx(prev, p, 1.0f, Fade(COL_LINE_STRONG, s % 2 ? 0.06f : 0.10f));
            prev = p;
        }
        /* 圈号不写字：14 个"阶段 N"会在轴上叠成一串，还和标签抢位置。
         * 只留一个刻度点，阶段数在底部详情条里跟着选中物种显示。 */
        DrawCircleV((Vector2){ c2.x, c2.y - ry }, 1.6f, Fade(COL_ACCENT, 0.30f));
    }
    /* 旧版 12 条辐射线把星图压成一个"靶心"，和 186 个节点叠在一起更乱。
     * 环已经足够表达阶段，放射线整组删掉。 */

    Vector2 node[SPECIES_COUNT];
    Vector2 vp[SPECIES_COUNT];
    int parent_of[SPECIES_COUNT];
    tree_layout(node, parent_of, NULL);
    float nr = tree_node_r();

    for (int i = 0; i < SPECIES_COUNT; ++i) vp[i] = tree_view(node[i]);

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

    /* v14 W12：可见性在连线与节点之前算一次，两处共用。 */
    bool t_found[SPECIES_COUNT], t_near[SPECIES_COUNT];
    tree_visibility(game, t_found, t_near);

    /* ---------- 亲缘连线 ----------
     * 只画三类：选中路线（亮）、两端都已点亮（生态色）、从已点亮往外一格的前沿（暗）。
     * 186 个物种的"全连线"是一张糊住的网，深层未探索的支线对玩家没有信息量。
     *
     * 放大后连线会伸出画框（贝塞尔曲线弯出去），所以整段用 scissor 兜住 sky：
     * 曲线可以到边界为止，但不会画到标题文字上。scissor 用渲染目标像素，
     * 逻辑坐标要乘超采样倍数。 */
    int sss = ui_canvas_ss();
    BeginScissorMode((int)(sky.x * (float)sss), (int)(sky.y * (float)sss),
                     (int)(sky.width * (float)sss), (int)(sky.height * (float)sss));
    {
        Rectangle cull = { sky.x - 80.0f, sky.y - 80.0f, sky.width + 160.0f, sky.height + 160.0f };
        bool drawn_pair[SPECIES_COUNT][SPECIES_COUNT] = { { false } };
        int edge_budget = TREE_EDGE_CAP;
        /* v14：按“离圆心近的优先”消耗 120 条边的预算。父节点阶段越小越靠内，
         * 玩家当前所处的阶段附近先画满，深层未探索的连线让位。 */
        for (int band = 0; band <= maxst && edge_budget > 0; ++band) {
            for (int child = 0; child < SPECIES_COUNT && edge_budget > 0; ++child) {
                int ps[4];
                int n = ua_species_parents(child, ps, 4);
                int own = ua_species_stage(child);
                bool child_found = t_found[child];
                for (int k = 0; k < n && edge_budget > 0; ++k) {
                    int p = ps[k];
                    if (p < 0 || p >= SPECIES_COUNT || p == child) continue;
                    int pst = ua_species_stage(p);
                    /* 早期版本这里是 `pst >= own` 就跳过，等于把整类同阶段旁支
                     *（264 个物种里 35 对）从星图上抹掉了：节点画在同一圈，
                     * 却看不到它们的亲缘关系。现在只挡“晚 → 早”的反向边。 */
                    if (pst > own) continue;
                    if (pst != band) continue;
                    if (drawn_pair[p][child]) continue;
                    drawn_pair[p][child] = true;

                    bool parent_found = t_found[p];
                    if (!parent_found && !child_found) continue;
                    bool lit = parent_found && child_found;
                    bool hi = on_route[child] && on_route[p];
                    bool frontier = parent_found != child_found;
                    if (!hi && !lit && !frontier) continue;
                    if (!CheckCollisionPointRec(vp[p], cull) && !CheckCollisionPointRec(vp[child], cull))
                        continue;

                    bool same_ring = (pst == own);
                    /* 弧线用的是屏幕坐标：半径必须同样乘上 zoom，
                     * 否则放大后弧会缩在圈的里面、末端接不到目标节点。 */
                    float rr_x = tree_ring_rx(band) * zoom, rr_y = tree_ring_ry(band) * zoom;

                    --edge_budget;
                    if (hi) {
                        float pulse = 0.5f + 0.5f * sinf(g_ui.time * 2.0f + (float)child * 0.4f);
                        tree_link(vp[p], vp[child], c2, rr_x, rr_y, same_ring,
                                  Fade(COL_ACCENT, 0.10f + 0.08f * pulse), 4.5f);
                        tree_link(vp[p], vp[child], c2, rr_x, rr_y, same_ring,
                                  Fade(COL_ACCENT, 0.55f + 0.30f * pulse), 2.0f);
                    } else if (lit) {
                        tree_link(vp[p], vp[child], c2, rr_x, rr_y, same_ring,
                                  Fade(ecology_color(child), 0.26f), 1.1f);
                    } else {
                        tree_link(vp[p], vp[child], c2, rr_x, rr_y, same_ring,
                                  Fade(COL_LINE_STRONG, 0.10f), 1.0f);
                    }
                }
            }
        }
    }

    /* 圆心 */
    ui_glow(c2, 46.0f * zoom, COL_ACCENT, 0.30f);

    /* ---------- 节点 ---------- */
    for (int i = 0; i < SPECIES_COUNT; ++i) {
        bool found = t_found[i];
        /* v14：不画“再往外一圈”的深层节点，264 个暗点会糊住已点亮的部分。 */
        if (!found && !t_near[i]) continue;
        Vector2 p = vp[i];
        if (p.x < sky.x - nr || p.x > sky.x + sky.width + nr) continue;
        if (p.y < sky.y - nr || p.y > sky.y + sky.height + nr) continue;

        bool cur = (i == main_id);
        bool pick = (i == sel);
        Color col = ecology_color(i);

        /* 未探索节点：只留一颗"星尘"。旧版把 169 个未发现物种也画成
         * 8px 圆底 + 描边 + 暗色生物，同心圈之间又只有十几像素，远看就是一张
         * 密集点阵，把真正点亮的 17 个节点完全盖住。这里改成两种点：
         *   前沿（亲本已点亮）= 4.2px 生态色 + 细环，提示"下一步能往这走"；
         *   其余未探索          = 2.4px 暗点，只保留"后面还有很多"的轮廓。
         * 选中节点即使在暗区也要完整显示，否则点不到自己在看什么。 */
        if (!found && !pick) {
            bool frontier = false;
            int ps[4];
            int pn = ua_species_parents(i, ps, 4);
            for (int q = 0; q < pn && !frontier; ++q) {
                if (ps[q] >= 0 && ps[q] < SPECIES_COUNT && found_flag[ps[q]]) frontier = true;
            }
            /* 越过已探索前沿一圈以上的点再淡一档，画面重心留在玩家能走的区域 */
            bool far = ua_species_stage(i) > found_stage_max + 1;
            float dr = frontier ? 4.2f : (far ? 2.0f : 2.6f);
            DrawCircleV(p, dr, frontier ? Fade(col, 0.62f)
                                        : Fade(COL_LINE_STRONG, far ? 0.22f : 0.38f));
            if (frontier) {
                DrawRing(p, dr + 1.6f, dr + 2.4f, 0.0f, 360.0f, 24, Fade(col, 0.32f));
            }
            continue;
        }

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

        /* 三轮放置：当前/选中 → 选中路线 → 其余已点亮。
         * 原来只有"必画/普通"两轮，17 个已点亮节点按 id 抢位置，
         * 常把玩家正在看的整条路线名字挤掉、却留下一堆无关支线的名字。 */
        for (int pass = 0; pass < 3; ++pass) {
            for (int i = 0; i < SPECIES_COUNT; ++i) {
                bool found = t_found[i];
                bool cur = (i == main_id);
                bool pick = (i == sel);
                bool must = cur || pick;
                bool onpath = on_route[i];
                /* v14：缩到 0.6 以下只留点，不让 264 个名字互相压。 */
                if (!must && zoom < 0.6f) continue;
                if (!(found || must)) continue;
                if (pass == 0 && !must) continue;
                if (pass == 1 && (!onpath || must)) continue;
                if (pass == 2 && (onpath || must)) continue;

                Vector2 p = vp[i];
                if (p.x < sky.x - 20.0f || p.x > sky.x + sky.width + 20.0f) continue;
                if (p.y < sky.y - 20.0f || p.y > sky.y + sky.height + 20.0f) continue;

                const char *nm = ua_species_name(i);
                float w = ui_text_w(nm, F_CAPTION);
                float base = atan2f(p.y - c2.y, p.x - c2.x);

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
                            /* 未探索的暗点不参与避让：它们只有 8px 且很淡，让标签绕开
                             * 186 个点等于一律放不下。只避让已点亮/当前/选中的节点。 */
                            bool j_found = game->legacy.codex_species[j] || game->run.discovered_species[j];
                            if (!j_found && j != sel) continue;
                            if (vp[j].x < sky.x - nr || vp[j].x > sky.x + sky.width + nr) continue;
                            if (vp[j].y < sky.y - nr || vp[j].y > sky.y + sky.height + nr) continue;
                            Rectangle q = { vp[j].x - nr - 5.0f, vp[j].y - nr - 5.0f,
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

                /* 半透明底托：星图里连线纵横，只靠 1px 阴影压不住，
                 * 名字会被亮线穿过读不清。底色不盖住节点本身，只垫字。 */
                DrawRectangleRounded((Rectangle){ best.x - 3.0f, best.y - 2.0f,
                                                  best.width + 6.0f, best.height + 4.0f },
                                     3.0f, 4, Fade(COL_BG_DEEP, 0.78f));
                ui_text(nm, best.x, best.y, F_CAPTION, pick ? COL_ACCENT : (found ? COL_DIM : COL_FAINT));
                placed[placed_n++] = best;
            }
        }
    }
    EndScissorMode();

    /* ---------- 底部详情条 ---------- */
    Rectangle bar = { sky.x + 12.0f, sky.y + sky.height - 56.0f, sky.width - 24.0f, 44.0f };
    ui_panel(bar, R_CARD, COL_PANEL, Fade(COL_ACCENT, 0.30f));

    {
        int sid = sel;
        int st = ua_species_stage(sid);
        bool sel_found = t_found[sid];
        draw_organism((Vector2){ bar.x + 24.0f, bar.y + 22.0f }, 12.0f, sid, st,
                      ecology_color(sid), S->time, sel_found, sel_found, 1.0f);
        ui_text_strong(sel_found ? ua_species_name(sid) : "未知物种",
                       bar.x + 44.0f, bar.y + 5.0f, F_LABEL, COL_TEXT);

        char line[192];
        int pp[4];
        int pn = ua_species_parents(sid, pp, 4);
        if (!sel_found) {
            /* v14 W12：空心节点不泄露名称，和图鉴锁定条目口径一致。 */
            int anchor = -1;
            for (int k = 0; k < pn && anchor < 0; ++k) {
                if (pp[k] >= 0 && t_found[pp[k]]) anchor = pp[k];
            }
            for (int c = 1; c < SPECIES_COUNT && anchor < 0; ++c) {
                if (!t_found[c]) continue;
                int cp[4];
                int cn = ua_species_parents(c, cp, 4);
                for (int k = 0; k < cn; ++k) {
                    if (cp[k] == sid) { anchor = c; break; }
                }
            }
            if (anchor >= 0) {
                snprintf(line, sizeof(line), "未知物种 · 阶段 %d · 与 %s 相邻", st,
                         ua_species_name(anchor));
            } else {
                snprintf(line, sizeof(line), "未知物种 · 阶段 %d · 尚未探索", st);
            }
        } else if (pn > 0) {
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
        char zt[32];
        snprintf(zt, sizeof(zt), "缩放 ×%.1f", zoom);
        ui_text_right(zt, panel.x + panel.width - 24.0f, panel.y + panel.height - 25.0f,
                      F_CAPTION, COL_FAINT);
    }
}

/* ---------------- 突变图谱 ---------------- */


static Rectangle mut_list_panel_rect(void)
{
    return (Rectangle){ 60.0f, 52.0f, 1160.0f, 612.0f };
}

/* 盒子区与列数：绘制和滚轮命中都从这里取，避免两套几何 */
static void mut_list_layout(const GameState *game, Rectangle *area_out, int *cols_out,
                            float *cw_out, float *ch_out)
{
    Rectangle panel = mut_list_panel_rect();
    int slots = game->run.active_species_count;
    int cols = slots <= 2 ? (slots < 1 ? 1 : slots) : 2;
    Rectangle area = { panel.x + 20.0f, panel.y + 78.0f, panel.width - 40.0f, 366.0f };
    float gap = 12.0f;
    *area_out = area;
    *cols_out = cols;
    *cw_out = (area.width - gap * (float)(cols - 1)) / (float)cols;
    *ch_out = (slots > 2) ? (area.height - gap) * 0.5f : area.height;
}

static Rectangle mut_box_rect(const GameState *game, int si)
{
    Rectangle area; int cols; float cw, ch;
    mut_list_layout(game, &area, &cols, &cw, &ch);
    float gap = 12.0f;
    return (Rectangle){ area.x + (float)(si % cols) * (cw + gap),
                        area.y + (float)(si / cols) * (ch + gap), cw, ch };
}

/* 盒子里真正放格子的区域：表头占 52px，底部留 4px */
static Rectangle mut_grid_rect(Rectangle box)
{
    return (Rectangle){ box.x + 14.0f, box.y + 52.0f, box.width - 28.0f, box.height - 56.0f };
}

#define MUT_GRID_COLS 3
#define MUT_GRID_ROW_H 26.0f

static int mut_owned_count(const SpeciesState *sp)
{
    int n = 0;
    for (int m = 0; m < MUTATION_COUNT; ++m) {
        if (sp->mutations[m]) ++n;
    }
    return n;
}

/* 内容总高 - 可见高度 = 可滚距离；装得下时为 0 */

MutListScroll screens_mut_list_scroll_target(const GameState *game, Vector2 m)
{
    MutListScroll out = { -1, MUT_GRID_ROW_H, 0.0f };
    int slots = game->run.active_species_count;
    if (slots > MAX_ACTIVE_SPECIES) slots = MAX_ACTIVE_SPECIES;
    for (int si = 0; si < slots; ++si) {
        Rectangle grid = mut_grid_rect(mut_box_rect(game, si));
        if (!CheckCollisionPointRec(m, grid)) continue;
        int rows = (mut_owned_count(&game->run.active_species[si]) + MUT_GRID_COLS - 1) / MUT_GRID_COLS;
        int vis = (int)(grid.height / MUT_GRID_ROW_H);
        if (vis < 1) vis = 1;
        float maxs = (float)(rows - vis) * MUT_GRID_ROW_H;
        out.slot = si;
        out.max_scroll = maxs > 0.0f ? maxs : 0.0f;
        return out;
    }
    return out;
}

static void draw_mut_list_overlay(const GameState *game, const UIState *S)
{
    Rectangle panel = mut_list_panel_rect();
    ui_halo(panel, R_PANEL, COL_DNA, 0.10f);
    ui_panel(panel, R_PANEL, COL_PANEL, Fade(COL_DNA, 0.30f));

    ui_text_strong("突变图谱", panel.x + 24.0f, panel.y + 14.0f, F_TITLE, COL_TEXT);
    ui_text("本轮已经获得的全部突变，按物种分栏；格子多到装不下时，把鼠标放到那一栏上滚轮翻看",
            panel.x + 24.0f, panel.y + 44.0f, F_CAPTION, COL_DIM);

    UiButton *close = btn_find(S, UI_BTN_CLOSE_OVERLAY, -1);
    if (close) ui_btn_draw(close, "关闭", NULL, is_hot(S, close));

    int slots = game->run.active_species_count;
    Rectangle area; int cols; float cw, ch;
    mut_list_layout(game, &area, &cols, &cw, &ch);
    float gap = 12.0f;

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

        int owned = mut_owned_count(sp);
        Rectangle grid = mut_grid_rect(box);
        int rows_total = (owned + MUT_GRID_COLS - 1) / MUT_GRID_COLS;
        int vis_rows = (int)(grid.height / MUT_GRID_ROW_H);
        if (vis_rows < 1) vis_rows = 1;
        float scroll_max = (float)(rows_total - vis_rows) * MUT_GRID_ROW_H;
        if (scroll_max < 0.0f) scroll_max = 0.0f;
        bool scrollable = scroll_max > 0.0f;
        float scroll = S->mut_scroll[si];
        if (scroll < 0.0f) scroll = 0.0f;
        if (scroll > scroll_max) scroll = scroll_max;

        char line[160];
        if (scrollable) {
            snprintf(line, sizeof(line), "%s · 阶段 %d · %d 项突变 · 滚轮看全部",
                     ua_ecology_name(sid), ua_species_stage(sid), owned);
        } else {
            snprintf(line, sizeof(line), "%s · 阶段 %d · %d 项突变",
                     ua_ecology_name(sid), ua_species_stage(sid), owned);
        }
        ui_text(line, box.x + 48.0f, box.y + 32.0f, F_CAPTION, COL_DIM);

        /* 格子 3 列 × N 行；装不下时整块用滚轮翻，滚动条画在右缘。
         * 必须裁剪：滚动中的格子会越出网格区，压到下面的表头/共振条上。 */
        float sb_w = scrollable ? 10.0f : 0.0f;
        float gw = (grid.width - 12.0f - sb_w) / (float)MUT_GRID_COLS;
        ui_clip_begin(grid);
        int drawn = 0;
        for (int m = 0; m < MUTATION_COUNT; ++m) {
            if (!sp->mutations[m]) continue;
            int rr = drawn / MUT_GRID_COLS, cc = drawn % MUT_GRID_COLS;
            float cy = grid.y + (float)rr * MUT_GRID_ROW_H - scroll;
            ++drawn;
            if (cy + MUT_GRID_ROW_H <= grid.y || cy >= grid.y + grid.height) continue;
            Rectangle cell = { grid.x + (float)cc * (gw + 6.0f), cy, gw, MUT_GRID_ROW_H - 5.0f };
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
        }
        ui_clip_end();

        if (owned == 0) {
            ui_text("还没有获得任何突变 · 按 M 打开突变面板",
                    grid.x, grid.y + 6.0f, F_CAPTION, COL_FAINT);
        }

        if (scrollable) {
            /* 滚动条：轨道 + 滑块，滑块位置直接用 scroll/scroll_max，
             * 与上面格子共用一个 offset，滚到哪就看到哪。 */
            Rectangle track = { grid.x + grid.width - 5.0f, grid.y, 4.0f, grid.height };
            DrawRectangleRounded(track, 2.0f, 4, Fade(COL_LINE, 0.35f));
            float ratio = (float)vis_rows / (float)rows_total;
            float thumb_h = grid.height * ratio;
            if (thumb_h < 18.0f) thumb_h = 18.0f;
            float thumb_y = grid.y + (grid.height - thumb_h) * (scroll / scroll_max);
            DrawRectangleRounded((Rectangle){ track.x, thumb_y, 4.0f, thumb_h }, 2.0f, 4,
                                 Fade(COL_DNA, 0.75f));
            /* 首尾各一条渐隐边，暗示上下还有内容 */
            if (scroll > 0.5f) {
                DrawRectangleGradientV((int)grid.x, (int)grid.y, (int)grid.width, 12,
                                       Fade(COL_BG_DEEP, 0.55f), Fade(COL_BG_DEEP, 0.0f));
            }
            if (scroll < scroll_max - 0.5f) {
                DrawRectangleGradientV((int)grid.x, (int)(grid.y + grid.height - 12),
                                       (int)grid.width, 12, Fade(COL_BG_DEEP, 0.0f),
                                       Fade(COL_BG_DEEP, 0.55f));
            }
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

