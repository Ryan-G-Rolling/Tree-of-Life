/*
 * tex.c  ——  摄影纹理层实现
 *
 * 生命周期：懒加载。第一次 tex_ready()/tex_draw() 时才尝试读取，
 * 之后一直常驻显存（1 张 1024×880 RGBA ≈ 3.6 MiB，可接受）。
 * 读不到文件不是错误：`g_ok` 保持 false，所有绘制静默跳过。
 */
#include "tex.h"

#include <stdbool.h>

#define TEX_PATH "assets/tex/specimen.png"

static Texture2D g_tex;
static bool g_tried;
static bool g_ok;

static void tex_ensure(void)
{
    if (g_tried) return;
    g_tried = true;

    if (!FileExists(TEX_PATH)) return;

    g_tex = LoadTexture(TEX_PATH);
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

void tex_shutdown(void)
{
    if (g_ok) UnloadTexture(g_tex);
    g_tex = (Texture2D){ 0 };
    g_ok = false;
    g_tried = false;
}
