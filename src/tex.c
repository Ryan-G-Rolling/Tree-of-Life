/*
 * tex.c  ——  摄影纹理层实现
 *
 * 生命周期：懒加载。第一次 tex_ready()/tex_draw() 时才尝试读取，
 * 之后一直常驻显存（1 张 1024×880 RGBA ≈ 3.6 MiB，可接受）。
 * 读不到文件不是错误：`g_ok` 保持 false，所有绘制静默跳过。
 */
#include "tex.h"

#include <stdbool.h>
#include <stddef.h>

#include "embedded.h"

#define TEX_PATH "assets/tex/specimen.png"

static Texture2D g_tex;
static bool g_tried;
static bool g_ok;

static void tex_ensure(void)
{
    if (g_tried) return;
    g_tried = true;

    /* 内嵌版不读盘；普通构建缺文件就静默跳过（这一层本来就是可选的） */
    const unsigned char *emb = NULL;
    int emb_size = 0;
    if (emb_get(EMB_SPECIMEN, &emb, &emb_size)) {
        Image img = LoadImageFromMemory(".png", emb, emb_size);
        if (!IsImageValid(img)) return;
        g_tex = LoadTextureFromImage(img);
        UnloadImage(img);
    } else {
        if (!FileExists(TEX_PATH)) return;
        g_tex = LoadTexture(TEX_PATH);
    }
    g_ok = (g_tex.id != 0 && g_tex.width > 0 && g_tex.height > 0);
}

bool tex_ready(void)
{
    tex_ensure();
    return g_ok;
}

void tex_draw(Rectangle dst, Color tint)
{
    tex_ensure();
    if (!g_ok) return;

    Rectangle src = { 0.0f, 0.0f, (float)g_tex.width, (float)g_tex.height };
    DrawTexturePro(g_tex, src, dst, (Vector2){ 0.0f, 0.0f }, 0.0f, tint);
}

/* ================= 生物插图层 =================
 * 第 17 版：生物体不再只靠矢量拼形状，而是接一张 AI 生成的透明底科学插画
 * （Seedream 5.5，1536×1536）。19 个形态族各一张。
 * 第 18 版：改成"归一化后原尺寸加载"——插画先由 tools/normsprites 统一成
 * 主体居中、四周留 14% 透明边的方图，再按 1536 上限进显存。原先压到 1024，
 * 在 3 倍超采样的 4K 截图里，展台主格上 ~660px 的标本会明显比 UI 文字软；
 * 现在只降一档，纹理和 1:1 像素几乎对上。mipmap 保留，图鉴小图标仍然干净。 */

#define MORPH_DIR "assets/tex/morph"
#define MORPH_MAX_TEX 1536

static Texture2D g_morph[TEXM_COUNT];
static bool g_morph_tried[TEXM_COUNT];
static bool g_morph_ok[TEXM_COUNT];

static const char *const k_morph_file[TEXM_COUNT] = {
    "coccus.png",    "rod.png",       "eukaryote.png", "colony.png",
    "tissue.png",    "symbio.png",    "neural.png",    "techno.png",
    "diatom.png",    "ciliate.png",   "amoeba.png",    "filament.png",
    "spiral.png",    "fish.png",      "jelly.png",     "fern.png",
    "fungus.png",    "coral.png",     "hive.png"
};

void tex_shutdown(void)
{
    if (g_ok) UnloadTexture(g_tex);
    g_tex = (Texture2D){ 0 };
    g_ok = false;
    g_tried = false;

    for (int i = 0; i < TEXM_COUNT; ++i) {
        if (g_morph_ok[i]) UnloadTexture(g_morph[i]);
        g_morph[i] = (Texture2D){ 0 };
        g_morph_ok[i] = false;
        g_morph_tried[i] = false;
    }
}

static void morph_ensure(int id)
{
    if (id < 0 || id >= TEXM_COUNT) return;
    if (g_morph_tried[id]) return;
    g_morph_tried[id] = true;

    const char *path = TextFormat("%s/%s", MORPH_DIR, k_morph_file[id]);
    if (!FileExists(path)) return;

    Image img = LoadImage(path);
    if (!IsImageValid(img)) return;

    if (img.width > MORPH_MAX_TEX || img.height > MORPH_MAX_TEX) {
        ImageResize(&img, MORPH_MAX_TEX, MORPH_MAX_TEX);
    }

    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    if (tex.id == 0 || tex.width <= 0) return;

    GenTextureMipmaps(&tex);
    SetTextureFilter(tex, TEXTURE_FILTER_TRILINEAR);

    g_morph[id] = tex;
    g_morph_ok[id] = true;
}

bool tex_morph_ready(int morph)
{
    morph_ensure(morph);
    return (morph >= 0 && morph < TEXM_COUNT) ? g_morph_ok[morph] : false;
}

bool tex_morph_draw(int morph, Rectangle dst, Color tint)
{
    morph_ensure(morph);
    if (morph < 0 || morph >= TEXM_COUNT || !g_morph_ok[morph]) return false;

    Texture2D tex = g_morph[morph];
    Rectangle src = { 0.0f, 0.0f, (float)tex.width, (float)tex.height };
    DrawTexturePro(tex, src, dst, (Vector2){ 0.0f, 0.0f }, 0.0f, tint);
    return true;
}
