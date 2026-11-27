/*
 * habitat.c  ——  生境背景层（见 habitat.h）
 *
 * 8 种生境，每种只有三种材料：
 *   ① 竖直渐变（底色，2 次顶点插值，不会出色带）
 *   ② 地形剪影（三角形 / 圆角矩形 / 圆，全部收在 area 内或由调用方裁剪）
 *   ③ 粒子（气泡上浮、雪下落、孢子上升、星尘外扩；位置由 time 取模推进）
 *
 * 配色比主题色更暗一档：生境是背景，不能让生物的荧光色失去对比。
 * 边缘另有一圈 14px 的渐隐，既收掉直角色块，也让生境与面板圆角自然衔接。
 */
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "raylib.h"
#include "habitat.h"
#include "codex.h"
#include "ui_internal.h"

typedef struct {
    const char *name;
    const char *note;
    Color top;      /* 渐变顶 */
    Color bottom;   /* 渐变底 */
    Color glow;     /* 环境光 / 远处光晕 */
    Color accent;   /* 点缀（热泉橙 / 珊瑚粉 / 窗灯暖黄 …） */
} HabitatDef;

static const HabitatDef k_hab[HABITAT_COUNT] = {
    { "深海热泉", "硫化物烟柱撑着最早的生命，光到不了这里。",
      {  10,  28,  36, 255 }, {   4,  10,  16, 255 },
      {  70, 150, 150, 255 }, { 255, 150,  84, 255 } },
    { "透光浅海", "阳光还能穿过水层，光合谱系在这里铺开。",
      {  12,  46,  56, 255 }, {   4,  17,  26, 255 },
      { 120, 220, 200, 255 }, { 150, 235, 190, 255 } },
    { "珊瑚礁", "礁体与共生伙伴搭出立体的住处，鱼群绕着枝杈。",
      {  16,  44,  54, 255 }, {   5,  15,  24, 255 },
      {  90, 200, 210, 255 }, { 255, 150, 190, 255 } },
    { "极地冰海", "零度上下的水与浮冰，只有极端谱系熬得过去。",
      {  10,  28,  44, 255 }, {   3,  10,  18, 255 },
      { 150, 200, 255, 255 }, { 205, 235, 255, 255 } },
    { "湿地与森林", "水退到岸上，绿色把大陆盖住了。",
      {  23,  37,  26, 255 }, {   9,  14,  10, 255 },
      { 190, 200, 120, 255 }, { 255, 210, 130, 255 } },
    { "城市与技术圈", "钢铁、灯光与电网，把整颗星球接上了电源。",
      {  22,  24,  38, 255 }, {   7,   8,  15, 255 },
      { 140, 170, 255, 255 }, { 255, 200, 120, 255 } },
    { "轨道与深空", "母星只剩一道弧，文明住进了轨道。",
      {   5,  10,  24, 255 }, {   1,   2,   6, 255 },
      { 110, 160, 255, 255 }, { 150, 210, 255, 255 } },
    { "奇点与星云", "物质与信息混成同一片光，尺度已经失效。",
      {  20,  10,  34, 255 }, {   4,   2,  10, 255 },
      { 180, 130, 255, 255 }, { 255, 235, 190, 255 } },
};

static float h01(unsigned int n)
{
    n = (n ^ 61u) ^ (n >> 16);
    n *= 9u;
    n = n ^ (n >> 4);
    n *= 0x27d4eb2du;
    n = n ^ (n >> 15);
    return (float)(n & 0xFFFFFFu) / 16777215.0f;
}

int habitat_for_stage(int stage)
{
    if (stage <= 1) return HABITAT_HYDROTHERMAL;
    if (stage <= 3) return HABITAT_SUNLIT_SEA;
    if (stage <= 5) return HABITAT_REEF;
    if (stage <= 7) return HABITAT_WETLAND;
    if (stage <= 9) return HABITAT_CITY;
    if (stage == 10) return HABITAT_ORBIT;
    return HABITAT_SINGULARITY;
}

static bool has(const char *hay, const char *needle)
{
    return hay && needle && hay[0] && needle[0] && strstr(hay, needle) != NULL;
}

/* 名字里的线索优先于阶段：v14 的 264 个物种里有"深海化能鱼""极地苔原""菌藻地衣"
 * 这类跨阶段的旁支，只按阶段会给它们套错背景。 */
int habitat_for_species(int species_id, int stage)
{
    const CodexEntry *e = codex_entry(CODEX_SPECIES, species_id);
    const char *n = e && e->name ? e->name : "";

    if (has(n, "奇点") || has(n, "超越") || has(n, "虚空") || has(n, "多元") ||
        has(n, "法则") || has(n, "终焉") || has(n, "观测者") || has(n, "织体") ||
        has(n, "理念") || has(n, "无界")) {
        return HABITAT_SINGULARITY;
    }
    if (has(n, "轨道") || has(n, "太空") || has(n, "戴森") || has(n, "恒星") ||
        has(n, "行星") || has(n, "卫星") || has(n, "星际") || has(n, "星舰")) {
        return HABITAT_ORBIT;
    }
    if (has(n, "礁") || has(n, "珊瑚") || has(n, "海葵") || has(n, "贻贝") ||
        has(n, "海绵") || has(n, "腕足")) {
        return HABITAT_REEF;
    }
    if (has(n, "极地") || has(n, "苔原") || has(n, "冰") || has(n, "冻") ||
        has(n, "雪")) {
        return HABITAT_POLAR;
    }
    if (has(n, "热泉") || has(n, "深海") || has(n, "化能") || has(n, "硫") ||
        has(n, "管虫") || has(n, "古菌")) {
        return HABITAT_HYDROTHERMAL;
    }
    if (has(n, "森林") || has(n, "林") || has(n, "湿地") || has(n, "苔藓") ||
        has(n, "草原") || has(n, "陆") || has(n, "地衣") || has(n, "菌根") ||
        has(n, "腐生") || has(n, "树")) {
        return HABITAT_WETLAND;
    }
    if (has(n, "城") || has(n, "都市") || has(n, "技术圈") || has(n, "工业") ||
        has(n, "工厂") || has(n, "蜂群") || has(n, "文明") || has(n, "政体")) {
        return HABITAT_CITY;
    }
    if (has(n, "藻") || has(n, "光合") || has(n, "蓝细菌") || has(n, "菌席")) {
        return HABITAT_SUNLIT_SEA;
    }
    return habitat_for_stage(stage);
}

const char *habitat_name(int habitat)
{
    if (habitat < 0 || habitat >= HABITAT_COUNT) return "未知生境";
    return k_hab[habitat].name;
}

const char *habitat_note(int habitat)
{
    if (habitat < 0 || habitat >= HABITAT_COUNT) return "";
    return k_hab[habitat].note;
}

Color habitat_accent(int habitat)
{
    if (habitat < 0 || habitat >= HABITAT_COUNT) return COL_DIM;
    return k_hab[habitat].accent;
}

/* ---------------- 图元 ---------------- */

static void fill_v(Rectangle a, Color top, Color bottom)
{
    DrawRectangleGradientV((int)a.x, (int)a.y, (int)a.width, (int)a.height, top, bottom);
}

/* 上浮粒子：气泡、悬浮絮状物。位置只由 time 推进，静止时完全可复现。 */
static void drift_up(Rectangle a, int n, float t, float speed, float r0, float r1,
                     Color c, float alpha, unsigned int seed)
{
    if (n <= 0) return;
    for (int i = 0; i < n; ++i) {
        unsigned int s = seed + (unsigned int)i * 37u;
        float sp = speed * (0.55f + 0.90f * h01(s));
        float u = fmodf(h01(s + 11u) + t * sp / fmaxf(a.height, 1.0f), 1.0f);
        float x = a.x + 8.0f + h01(s + 23u) * (a.width - 16.0f) +
                  sinf(t * 0.65f + (float)i * 1.7f) * 4.5f;
        float y = a.y + a.height * (1.0f - u);
        float r = r0 + (r1 - r0) * h01(s + 41u);
        float al = alpha * (0.55f + 0.45f * (1.0f - u));
        DrawCircleV((Vector2){ x, y }, r * 2.4f, Fade(c, al * 0.26f));
        DrawCircleV((Vector2){ x, y }, r, Fade(c, al));
    }
}

/* 下落粒子：极地雪、城市粉尘。 */
static void drift_down(Rectangle a, int n, float t, float speed, float r0, float r1,
                       Color c, float alpha, unsigned int seed)
{
    if (n <= 0) return;
    for (int i = 0; i < n; ++i) {
        unsigned int s = seed + (unsigned int)i * 53u;
        float sp = speed * (0.60f + 0.80f * h01(s));
        float u = fmodf(h01(s + 7u) + t * sp / fmaxf(a.height, 1.0f), 1.0f);
        float x = a.x + 6.0f + h01(s + 29u) * (a.width - 12.0f) +
                  sinf(t * 0.9f + (float)i) * 6.0f;
        float y = a.y + a.height * u;
        float r = r0 + (r1 - r0) * h01(s + 43u);
        DrawCircleV((Vector2){ x, y }, r, Fade(c, alpha * (0.45f + 0.55f * u)));
    }
}

/* 气泡：一圈亮边 + 一点高光，比纯圆点更像气泡。 */
static void bubbles(Rectangle a, int n, float t, Color c, float alpha, unsigned int seed)
{
    if (n <= 0) return;
    for (int i = 0; i < n; ++i) {
        unsigned int s = seed + (unsigned int)i * 61u;
        float sp = 7.0f + 9.0f * h01(s);
        float u = fmodf(h01(s + 13u) + t * sp / fmaxf(a.height, 1.0f), 1.0f);
        float x = a.x + 10.0f + h01(s + 31u) * (a.width - 20.0f) +
                  sinf(t * 0.8f + (float)i * 2.1f) * 3.0f;
        float y = a.y + a.height * (1.0f - u);
        float r = 1.6f + 3.2f * h01(s + 47u);
        DrawCircleV((Vector2){ x, y }, r, Fade(c, alpha * 0.22f));
        DrawRing((Vector2){ x, y }, r * 0.74f, r, 0.0f, 360.0f, 18, Fade(c, alpha * 0.55f));
        DrawCircleV((Vector2){ x - r * 0.30f, y - r * 0.32f }, r * 0.24f, Fade(WHITE, alpha * 0.45f));
    }
}

/* 起伏的地形线：一串首尾相接的三角形，读作礁脊 / 远山 / 树线。 */
static void ridge(Rectangle a, float base_y, float amp, int n, Color c, unsigned int seed)
{
    if (n <= 0) return;
    float step = a.width / (float)n;
    for (int i = 0; i < n; ++i) {
        float x0 = a.x + (float)i * step;
        float x1 = x0 + step;
        float xm = x0 + step * 0.5f;
        float h = amp * (0.45f + 0.85f * h01(seed + (unsigned int)i * 17u));
        float yb = base_y + amp * 0.9f;
        DrawTriangle((Vector2){ x0, yb }, (Vector2){ xm, base_y - h }, (Vector2){ x1, yb }, c);
        DrawTriangle((Vector2){ x1, yb }, (Vector2){ xm, base_y - h }, (Vector2){ x0, yb }, c);
    }
}

/* 边缘收口：四边 14px 渐隐到面板底色，压掉直角色块，融进圆角。 */
static void edge_fade(Rectangle a)
{
    Color d = { 3, 6, 12, 255 };
    float w = 15.0f;
    if (a.width < w * 4.0f || a.height < w * 4.0f) w = 8.0f;
    DrawRectangleGradientV((int)a.x, (int)a.y, (int)a.width, (int)w,
                           Fade(d, 0.80f), Fade(d, 0.0f));
    DrawRectangleGradientV((int)a.x, (int)(a.y + a.height - w), (int)a.width, (int)w,
                           Fade(d, 0.0f), Fade(d, 0.72f));
    DrawRectangleGradientH((int)a.x, (int)a.y, (int)w, (int)a.height,
                           Fade(d, 0.70f), Fade(d, 0.0f));
    DrawRectangleGradientH((int)(a.x + a.width - w), (int)a.y, (int)w, (int)a.height,
                           Fade(d, 0.0f), Fade(d, 0.70f));
}

/* ---------------- 各生境 ---------------- */

static void draw_hydrothermal(Rectangle a, const HabitatDef *H, float t, float d)
{
    fill_v(a, H->top, H->bottom);
    /* 热泉的暖辉：从底部往上渗，深海没有阳光，唯一的光源就是热液 */
    DrawRectangleGradientV((int)a.x, (int)(a.y + a.height * 0.45f), (int)a.width,
                           (int)(a.height * 0.55f), Fade(H->accent, 0.0f),
                           Fade(H->accent, 0.055f));

    float base = a.y + a.height;
    for (int i = 0; i < 2; ++i) {
        float cx = a.x + a.width * (i == 0 ? 0.27f : 0.71f);
        float hgt = a.height * (i == 0 ? 0.20f : 0.145f);
        float w = a.width * 0.052f;
        /* 烟囱：柱身 + 两个肩台，剪影比纯矩形更像岩体。
         * 用近黑剪影 + 顶部一圈暖色轮廓光，深海底色再暗也能读出形状。 */
        DrawRectangle((int)(cx - w * 0.62f), (int)(base - hgt), (int)(w * 1.24f),
                      (int)hgt, Fade((Color){ 5, 9, 12, 255 }, 0.98f));
        DrawRectangle((int)(cx - w * 0.95f), (int)(base - hgt * 0.55f), (int)(w * 1.9f),
                      (int)(hgt * 0.55f), Fade((Color){ 5, 9, 12, 255 }, 0.98f));
        DrawLineEx((Vector2){ cx - w * 0.62f, base - hgt },
                   (Vector2){ cx + w * 0.62f, base - hgt },
                   1.2f, Fade(H->accent, 0.30f));
        /* 烟柱：细而长，越往上越宽、越淡。圆太大就变成两盏灯。 */
        for (int k = 0; k < 7; ++k) {
            float u = (float)k / 6.0f;
            float y = base - hgt - a.height * 0.34f * u;
            float rr = w * (0.42f + 1.55f * u);
            float wob = sinf(t * 0.45f + (float)k * 0.9f) * (2.0f + 3.0f * u);
            DrawCircleV((Vector2){ cx + wob, y }, rr,
                        Fade(H->accent, 0.060f * (1.0f - u * 0.70f)));
        }
    }
    bubbles(a, (int)(9.0f * d + 0.5f), t, (Color){ 150, 220, 220, 255 }, 0.34f, 17u);
    drift_up(a, (int)(8.0f * d + 0.5f), t, 5.0f, 0.7f, 1.5f,
             (Color){ 130, 190, 190, 255 }, 0.17f, 5u);
    /* 底部沉积：一层几乎黑的收边 */
    DrawRectangleGradientV((int)a.x, (int)(a.y + a.height * 0.78f), (int)a.width,
                           (int)(a.height * 0.22f), BLANK, Fade((Color){ 2, 5, 8, 255 }, 0.85f));
}

static void draw_sunlit_sea(Rectangle a, const HabitatDef *H, float t, float d)
{
    fill_v(a, H->top, H->bottom);
    /* 水面亮带 */
    DrawRectangleGradientV((int)a.x, (int)a.y, (int)a.width, (int)(a.height * 0.26f),
                           Fade(H->glow, 0.105f), BLANK);
    /* 斜射光柱：竖直条带从顶部渐隐，宽窄不一，读作穿透水层的光 */
    int beams = (int)(5.0f * d + 1.0f);
    for (int i = 0; i < beams; ++i) {
        float u = (float)i / (float)(beams > 1 ? beams - 1 : 1);
        float x = a.x + a.width * (0.08f + 0.84f * u) +
                  sinf(t * 0.16f + (float)i) * 6.0f;
        float w = a.width * (0.035f + 0.045f * h01((unsigned int)i * 31u + 7u));
        float hh = a.height * (0.55f + 0.35f * h01((unsigned int)i * 43u + 3u));
        DrawRectangleGradientV((int)(x - w * 0.5f), (int)a.y, (int)w, (int)hh,
                               Fade(H->glow, 0.088f), Fade(H->glow, 0.0f));
    }
    bubbles(a, (int)(7.0f * d + 0.5f), t, (Color){ 190, 240, 235, 255 }, 0.30f, 23u);
    drift_up(a, (int)(12.0f * d + 0.5f), t, 4.0f, 0.6f, 1.3f,
             (Color){ 170, 225, 210, 255 }, 0.16f, 9u);
    /* 海床：一条低矮的礁脊 + 底部暗带。旧写法用圆角矩形，读起来像两颗按钮；
     * 脊线要矮、要密，否则锯齿会变成一排山，把物种名压在"山峰"上。 */
    float base = a.y + a.height;
    ridge(a, base - a.height * 0.095f, a.height * 0.024f, 14,
          Fade((Color){ 5, 13, 17, 255 }, 0.98f), 29u);
    DrawRectangleGradientV((int)a.x, (int)(base - a.height * 0.11f), (int)a.width,
                           (int)(a.height * 0.11f), BLANK, Fade((Color){ 2, 6, 9, 255 }, 0.86f));
    for (int i = 0; i < 3; ++i) {
        float gx = a.x + a.width * (0.22f + 0.28f * (float)i);
        float gy = base - a.height * (0.055f + 0.02f * (float)i);
        DrawLineEx((Vector2){ gx, gy }, (Vector2){ gx + a.width * 0.05f, gy - a.height * 0.012f },
                   1.0f, Fade(H->glow, 0.22f));
    }
}

static void draw_reef(Rectangle a, const HabitatDef *H, float t, float d)
{
    fill_v(a, H->top, H->bottom);
    DrawRectangleGradientV((int)a.x, (int)a.y, (int)a.width, (int)(a.height * 0.20f),
                           Fade(H->glow, 0.06f), BLANK);

    float base = a.y + a.height;
    /* 礁盘：一道低矮的脊线，避免底部一条硬直线 */
    ridge(a, base - a.height * 0.12f, a.height * 0.030f, 12,
          Fade((Color){ 4, 12, 17, 255 }, 0.98f), 11u);

    /* 三丛珊瑚：主干 + 分枝，用圆点串起来；粉色与青色交错 */
    for (int i = 0; i < 3; ++i) {
        float cx = a.x + a.width * (0.20f + 0.30f * (float)i);
        float hgt = a.height * (0.16f + 0.09f * h01((unsigned int)i * 19u + 7u));
        Color c = (i % 2) ? H->accent : H->glow;
        float sw = sinf(t * 0.55f + (float)i * 1.9f) * 2.2f;   /* 水流摆动 */
        for (int k = 0; k < 5; ++k) {
            float u = (float)k / 4.0f;
            float y = base - hgt * u;
            float r = 5.5f - 3.2f * u;
            float dx = (k % 2 ? 1.0f : -1.0f) * (2.0f + 5.0f * u) + sw * u * 1.6f;
            DrawCircleV((Vector2){ cx + dx, y }, r, Fade(c, 0.40f));
            if (k == 2) {
                DrawLineEx((Vector2){ cx + dx, y }, (Vector2){ cx + dx * 2.4f, y - hgt * 0.22f },
                           2.0f, Fade(c, 0.34f));
                DrawCircleV((Vector2){ cx + dx * 2.4f, y - hgt * 0.22f }, 3.4f, Fade(c, 0.36f));
            }
        }
    }

    /* 鱼群：一串小点横穿，快慢不同，明暗不同 */
    int fish = (int)(6.0f * d + 0.5f);
    for (int i = 0; i < fish; ++i) {
        unsigned int s = 71u + (unsigned int)i * 29u;
        float sp = 8.0f + 12.0f * h01(s);
        float x = a.x + fmodf(h01(s + 5u) * a.width + t * sp, a.width);
        float y = a.y + a.height * (0.30f + 0.42f * h01(s + 17u)) +
                  sinf(t * 1.1f + (float)i) * 3.0f;
        float r = 1.4f + 1.5f * h01(s + 37u);
        DrawCircleV((Vector2){ x, y }, r * 2.2f, Fade(H->glow, 0.12f));
        DrawCircleV((Vector2){ x, y }, r, Fade(H->accent, 0.52f));
    }
    bubbles(a, (int)(8.0f * d + 0.5f), t, (Color){ 170, 230, 235, 255 }, 0.30f, 33u);
    DrawRectangleGradientV((int)a.x, (int)(base - a.height * 0.14f), (int)a.width,
                           (int)(a.height * 0.14f), BLANK, Fade((Color){ 3, 8, 12, 255 }, 0.78f));
}

static void draw_polar(Rectangle a, const HabitatDef *H, float t, float d)
{
    fill_v(a, H->top, H->bottom);
    /* 极光：顶部一抹冷绿，很淡 */
    DrawRectangleGradientV((int)a.x, (int)a.y, (int)a.width, (int)(a.height * 0.32f),
                           Fade((Color){ 120, 230, 200, 255 }, 0.045f), BLANK);
    /* 冰层：从顶边垂下的尖锥 */
    {
        int n = 7;
        float step = a.width / (float)n;
        for (int i = 0; i < n; ++i) {
            float x0 = a.x + (float)i * step;
            float hh = a.height * (0.045f + 0.075f * h01(41u + (unsigned int)i * 13u));
            DrawTriangle((Vector2){ x0, a.y }, (Vector2){ x0 + step * 0.5f, a.y + hh },
                         (Vector2){ x0 + step, a.y }, Fade(H->accent, 0.10f));
        }
    }
    drift_down(a, (int)(20.0f * d + 0.5f), t, 14.0f, 0.8f, 1.7f,
               (Color){ 235, 248, 255, 255 }, 0.46f, 19u);

    float base = a.y + a.height;
    /* 冰面：白蓝色渐变 + 两道裂纹 */
    DrawRectangleGradientV((int)a.x, (int)(base - a.height * 0.22f), (int)a.width,
                           (int)(a.height * 0.22f), BLANK, Fade((Color){ 120, 165, 200, 255 }, 0.38f));
    DrawRectangleGradientV((int)a.x, (int)(base - a.height * 0.20f), (int)a.width,
                           (int)(a.height * 0.20f), BLANK, Fade((Color){ 4, 10, 16, 255 }, 0.45f));
    for (int i = 0; i < 2; ++i) {
        float y = base - a.height * (0.10f + 0.055f * (float)i);
        float x0 = a.x + a.width * (0.12f + 0.30f * (float)i);
        Vector2 p0 = { x0, y };
        Vector2 p1 = { x0 + a.width * 0.16f, y - a.height * 0.028f };
        Vector2 p2 = { p1.x + a.width * 0.14f, y + a.height * 0.014f };
        DrawLineEx(p0, p1, 1.0f, Fade(H->accent, 0.30f));
        DrawLineEx(p1, p2, 1.0f, Fade(H->accent, 0.22f));
    }
    /* 远处冰山 */
    ridge(a, base - a.height * 0.24f, a.height * 0.075f, 5,
          Fade((Color){ 58, 88, 114, 255 }, 0.55f), 57u);
}

static void draw_wetland(Rectangle a, const HabitatDef *H, float t, float d)
{
    fill_v(a, H->top, H->bottom);
    /* 天光：顶部暖黄，越往下越暗（树冠把光挡住了） */
    DrawRectangleGradientV((int)a.x, (int)a.y, (int)a.width, (int)(a.height * 0.42f),
                           Fade(H->accent, 0.07f), BLANK);

    float horizon = a.y + a.height * 0.63f;
    /* 远山：极低对比，只给一点起伏 */
    ridge(a, horizon - a.height * 0.055f, a.height * 0.05f, 7,
          Fade((Color){ 12, 24, 17, 255 }, 0.72f), 13u);
    /* 树线：一排圆冠，深浅两层。剪影必须比底色更暗，否则整片糊成一块 */
    for (int layer = 0; layer < 2; ++layer) {
        int n = layer == 0 ? 6 : 4;
        for (int i = 0; i < n; ++i) {
            unsigned int s = (unsigned int)(layer * 31 + i * 17 + 3);
            float cx = a.x + a.width * ((float)i + 0.5f) / (float)n +
                       (layer ? 0.04f : -0.03f) * a.width;
            float cy = horizon - a.height * (0.03f + 0.05f * h01(s));
            float r = a.height * (layer ? 0.055f : 0.038f) * (0.8f + 0.5f * h01(s + 7u));
            Color c = layer ? (Color){ 5, 12, 8, 255 } : (Color){ 9, 19, 12, 255 };
            DrawCircleV((Vector2){ cx, cy }, r, Fade(c, 0.95f));
            DrawCircleV((Vector2){ cx - r * 0.7f, cy + r * 0.35f }, r * 0.72f, Fade(c, 0.95f));
            DrawCircleV((Vector2){ cx + r * 0.7f, cy + r * 0.30f }, r * 0.66f, Fade(c, 0.95f));
            DrawRectangle((int)(cx - r * 0.10f), (int)cy, (int)(r * 0.20f),
                          (int)(horizon - cy + 2.0f), Fade((Color){ 4, 9, 6, 255 }, 0.92f));
        }
    }
    /* 地平线：一条暖光，读作落日 / 天光，给暗绿一个落点 */
    DrawLineEx((Vector2){ a.x + 6.0f, horizon }, (Vector2){ a.x + a.width - 6.0f, horizon },
               1.0f, Fade(H->accent, 0.12f));
    /* 地面 */
    DrawRectangleGradientV((int)a.x, (int)horizon, (int)a.width,
                           (int)(a.y + a.height - horizon), Fade((Color){ 9, 15, 10, 255 }, 0.90f),
                           Fade((Color){ 4, 8, 6, 255 }, 0.98f));
    /* 湿地水光：一条横线，慢慢呼吸 */
    DrawLineEx((Vector2){ a.x + a.width * 0.10f, horizon + a.height * 0.035f },
               (Vector2){ a.x + a.width * 0.62f, horizon + a.height * 0.035f },
               1.2f, Fade(H->glow, 0.10f + 0.04f * sinf(t * 0.7f)));

    /* 孢子 / 萤火：上升的暖点，亮度各自呼吸 */
    int n = (int)(11.0f * d + 0.5f);
    for (int i = 0; i < n; ++i) {
        unsigned int s = 91u + (unsigned int)i * 43u;
        float sp = 3.0f + 5.0f * h01(s);
        float u = fmodf(h01(s + 11u) + t * sp / fmaxf(a.height, 1.0f), 1.0f);
        float x = a.x + 10.0f + h01(s + 23u) * (a.width - 20.0f) +
                  sinf(t * 0.5f + (float)i * 1.3f) * 7.0f;
        float y = a.y + a.height * (1.0f - u);
        float tw = 0.45f + 0.55f * (0.5f + 0.5f * sinf(t * (0.8f + 1.1f * h01(s + 5u)) + (float)i));
        float r = 1.0f + 1.6f * h01(s + 61u);
        DrawCircleV((Vector2){ x, y }, r * 3.4f, Fade(H->accent, 0.055f * tw));
        DrawCircleV((Vector2){ x, y }, r, Fade(H->accent, 0.50f * tw));
    }
}

static void draw_city(Rectangle a, const HabitatDef *H, float t, float d)
{
    fill_v(a, H->top, H->bottom);
    /* 地平线雾光 */
    DrawRectangleGradientV((int)a.x, (int)(a.y + a.height * 0.55f), (int)a.width,
                           (int)(a.height * 0.45f), BLANK, Fade(H->glow, 0.09f));

    float base = a.y + a.height;
    int n = (int)(11.0f * d + 3.0f);
    float step = a.width / (float)n;
    for (int i = 0; i < n; ++i) {
        unsigned int s = 131u + (unsigned int)i * 23u;
        float w = step * (0.62f + 0.26f * h01(s));
        float hh = a.height * (0.075f + 0.165f * h01(s + 7u));
        float x = a.x + (float)i * step + (step - w) * 0.5f;
        Color body = (i % 3 == 0) ? (Color){ 19, 25, 42, 255 } : (Color){ 14, 19, 32, 255 };
        DrawRectangle((int)x, (int)(base - hh), (int)w, (int)hh, Fade(body, 0.97f));
        /* 楼顶亮线：没有它，楼体在深底上只剩一片黑 */
        DrawLineEx((Vector2){ x, base - hh }, (Vector2){ x + w, base - hh },
                   1.0f, Fade(H->glow, 0.22f));
        DrawLineEx((Vector2){ x, base - hh }, (Vector2){ x, base },
                   1.0f, Fade(H->glow, 0.08f));
        /* 楼顶天线 */
        if (h01(s + 31u) > 0.62f) {
            DrawLineEx((Vector2){ x + w * 0.5f, base - hh },
                       (Vector2){ x + w * 0.5f, base - hh - a.height * 0.035f },
                       1.0f, Fade(COL_LINE_STRONG, 0.42f));
            float blink = 0.30f + 0.55f * (0.5f + 0.5f * sinf(t * 2.0f + (float)i));
            DrawCircleV((Vector2){ x + w * 0.5f, base - hh - a.height * 0.035f }, 1.2f,
                        Fade(COL_BAD, blink * 0.65f));
        }
        /* 窗：2 列 × k 行，暖黄与冰蓝交错；少量做呼吸 */
        int rows = (int)(hh / (a.height * 0.030f));
        if (rows > 7) rows = 7;
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < 2; ++c) {
                unsigned int ws = s + (unsigned int)(r * 7 + c * 3);
                if (h01(ws) < 0.30f) continue;
                float wx = x + w * (0.22f + 0.34f * (float)c);
                float wy = base - hh + a.height * 0.020f + (float)r * a.height * 0.030f;
                bool warm = h01(ws + 5u) > 0.42f;
                float al = 0.30f + 0.26f * h01(ws + 11u);
                if (h01(ws + 17u) > 0.80f) al *= 0.6f + 0.4f * sinf(t * 1.6f + (float)ws);
                DrawRectangle((int)wx, (int)wy, (int)fmaxf(1.2f, w * 0.17f),
                              (int)fmaxf(1.2f, a.height * 0.013f),
                              Fade(warm ? H->accent : (Color){ 150, 210, 255, 255 }, al));
            }
        }
    }
    /* 底部雾：紫灰，压住楼的根部 */
    DrawRectangleGradientV((int)a.x, (int)(base - a.height * 0.24f), (int)a.width,
                           (int)(a.height * 0.24f), BLANK, Fade((Color){ 26, 24, 40, 255 }, 0.45f));
    /* 高空数据带：两条极淡的水平线 */
    for (int i = 0; i < 2; ++i) {
        float y = a.y + a.height * (0.18f + 0.09f * (float)i);
        DrawLineEx((Vector2){ a.x + 8.0f, y }, (Vector2){ a.x + a.width - 8.0f, y },
                   1.0f, Fade(H->glow, 0.045f + 0.02f * sinf(t * 0.4f + (float)i)));
    }
    drift_up(a, (int)(7.0f * d + 0.5f), t, 4.0f, 0.6f, 1.2f,
             (Color){ 190, 205, 235, 255 }, 0.10f, 61u);
}

static void draw_orbit(Rectangle a, const HabitatDef *H, float t, float d)
{
    fill_v(a, H->top, H->bottom);

    /* 星场：比全屏背景更密一点，让展台里也有纵深感 */
    int n = (int)(26.0f * d + 6.0f);
    for (int i = 0; i < n; ++i) {
        unsigned int s = 211u + (unsigned int)i * 31u;
        float x = a.x + h01(s) * a.width;
        float y = a.y + h01(s + 7u) * a.height * 0.82f;
        float r = 0.5f + 1.1f * h01(s + 13u) * h01(s + 17u);
        float tw = 0.45f + 0.55f * (0.5f + 0.5f * sinf(t * (0.5f + h01(s + 19u)) + (float)i));
        DrawCircleV((Vector2){ x, y }, r, Fade(COL_TEXT, 0.55f * tw));
    }

    /* 母星边缘：圆心在画框下方外面，只露一段冷光弧。
     * 调用方必须保证在 scissor 里（screens.c 会先 ui_clip_begin）。 */
    Vector2 pc = { a.x + a.width * 0.30f, a.y + a.height * 1.72f };
    float R = a.width * 0.92f;
    DrawCircleV(pc, R, Fade((Color){ 8, 20, 36, 255 }, 0.95f));
    DrawCircleGradient((int)pc.x, (int)pc.y, R,
                       Fade((Color){ 10, 26, 46, 255 }, 0.95f),
                       Fade((Color){ 3, 8, 16, 255 }, 0.95f));
    DrawRing(pc, R - 1.2f, R + 0.8f, 200.0f, 340.0f, 96, Fade(H->glow, 0.22f));
    DrawRing(pc, R - 7.0f, R - 6.0f, 210.0f, 330.0f, 96, Fade(H->glow, 0.10f));

    /* 轨道：两条椭圆，卫星点沿椭圆缓慢移动 */
    Vector2 oc = { a.x + a.width * 0.66f, a.y + a.height * 0.34f };
    for (int i = 0; i < 2; ++i) {
        float rx = a.width * (0.26f + 0.10f * (float)i);
        float ry = a.height * (0.10f + 0.045f * (float)i);
        DrawEllipseLines((int)oc.x, (int)oc.y, rx, ry, Fade(H->glow, 0.10f));
        float ang = t * (0.16f + 0.09f * (float)i) + (float)i * 2.1f;
        Vector2 p = { oc.x + cosf(ang) * rx, oc.y + sinf(ang) * ry };
        DrawCircleV(p, 1.6f, Fade(H->accent, 0.75f));
        DrawCircleV(p, 5.0f, Fade(H->accent, 0.10f));
    }
    drift_up(a, (int)(6.0f * d + 0.5f), t, 3.0f, 0.5f, 1.0f,
             (Color){ 170, 205, 255, 255 }, 0.12f, 77u);
}

static void draw_singularity(Rectangle a, const HabitatDef *H, float t, float d)
{
    fill_v(a, H->top, H->bottom);
    /* 星云：两团错开的柔光，很淡，只给色彩 */
    Vector2 c1 = { a.x + a.width * 0.28f, a.y + a.height * 0.34f };
    Vector2 c2 = { a.x + a.width * 0.74f, a.y + a.height * 0.60f };
    DrawCircleGradient((int)c1.x, (int)c1.y, a.width * 0.42f,
                       Fade(COL_VIOLET, 0.095f), Fade(COL_VIOLET, 0.0f));
    DrawCircleGradient((int)c2.x, (int)c2.y, a.width * 0.46f,
                       Fade(COL_TEAL, 0.070f), Fade(COL_TEAL, 0.0f));

    /* 吸积盘：中心一圈偏平的椭圆环，多层叠加；呼吸只改 alpha */
    Vector2 sc = { a.x + a.width * 0.50f, a.y + a.height * 0.50f };
    float pulse = 0.5f + 0.5f * sinf(t * 0.55f);
    for (int i = 0; i < 4; ++i) {
        float k = (float)i / 3.0f;
        float rx = a.width * (0.20f + 0.26f * k);
        float ry = a.height * (0.045f + 0.075f * k);
        DrawEllipseLines((int)sc.x, (int)sc.y, rx, ry,
                         Fade(i < 2 ? H->accent : H->glow, (0.30f - 0.05f * k) * (0.7f + 0.6f * pulse)));
    }
    DrawCircleGradient((int)sc.x, (int)sc.y, a.height * 0.24f,
                       Fade(H->accent, 0.10f + 0.05f * pulse), Fade(H->accent, 0.0f));

    /* 星尘：从中心向外漂，越远越淡 */
    int n = (int)(20.0f * d + 4.0f);
    for (int i = 0; i < n; ++i) {
        unsigned int s = 251u + (unsigned int)i * 37u;
        float ang = h01(s) * 2.0f * PI + t * 0.05f;
        float u = fmodf(h01(s + 11u) + t * 0.035f, 1.0f);
        float rad = (0.10f + 0.55f * u) * a.width;
        float x = sc.x + cosf(ang) * rad;
        float y = sc.y + sinf(ang) * rad * 0.62f;
        float r = 0.6f + 1.4f * h01(s + 23u);
        DrawCircleV((Vector2){ x, y }, r, Fade(H->accent, 0.42f * (1.0f - u)));
    }
}

/* ---------------- 对外 ---------------- */

void habitat_draw(Rectangle area, int habitat, float time, float detail)
{
    if (habitat < 0 || habitat >= HABITAT_COUNT) habitat = 0;
    if (detail < 0.0f) detail = 0.0f;
    if (detail > 1.0f) detail = 1.0f;

    Rectangle a = { area.x + 6.0f, area.y + 6.0f, area.width - 12.0f, area.height - 12.0f };
    if (a.width < 48.0f || a.height < 32.0f) return;

    const HabitatDef *H = &k_hab[habitat];
    switch (habitat) {
    case HABITAT_HYDROTHERMAL: draw_hydrothermal(a, H, time, detail); break;
    case HABITAT_SUNLIT_SEA:   draw_sunlit_sea(a, H, time, detail); break;
    case HABITAT_REEF:         draw_reef(a, H, time, detail); break;
    case HABITAT_POLAR:        draw_polar(a, H, time, detail); break;
    case HABITAT_WETLAND:      draw_wetland(a, H, time, detail); break;
    case HABITAT_CITY:         draw_city(a, H, time, detail); break;
    case HABITAT_ORBIT:        draw_orbit(a, H, time, detail); break;
    default:                   draw_singularity(a, H, time, detail); break;
    }
    edge_fade(a);
}

void habitat_bleed(int habitat, float time, float amount)
{
    if (amount <= 0.0f) return;
    if (habitat < 0 || habitat >= HABITAT_COUNT) return;
    const HabitatDef *H = &k_hab[habitat];
    /* 只压上下边缘，中心留给星云与卡片；alpha 上限 0.055，避免把深空底冲淡 */
    float a = 0.055f * amount;
    float band = 150.0f;
    DrawRectangleGradientV(0, 0, UI_W, (int)band,
                           Fade(H->glow, a * 0.85f), Fade(H->glow, 0.0f));
    DrawRectangleGradientV(0, UI_H - (int)band, UI_W, (int)band,
                           Fade(H->glow, 0.0f), Fade(H->glow, a));
    /* 底部再叠一点暖/冷的地面反光，让"脚下是什么"在整屏也能读到 */
    DrawRectangleGradientV(0, UI_H - 56, UI_W, 56, BLANK, Fade(H->bottom, 0.35f * amount));
}
