/*
 * input.c  ——  输入层（Member C）
 *
 * 职责：
 *   1. 把窗口像素坐标换算成 1280×800 逻辑坐标（含 letterbox 偏移）。
 *   2. 在布局矩形表里做“最上层命中”，保证弹层按钮优先于底层按钮。
 *   3. 集中管理键位，方便以后改快捷键。
 *
 * 注意：本文件不发 GameAction，只回答“点到了哪个按钮下标”。
 */
#include <math.h>

#include "raylib.h"
#include "ui_internal.h"

static float g_scale = 1.0f;
static float g_off_x = 0.0f;
static float g_off_y = 0.0f;

void input_begin_frame(void)
{
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    if (sw < 1.0f) sw = (float)UI_W;
    if (sh < 1.0f) sh = (float)UI_H;

    float sx = sw / (float)UI_W;
    float sy = sh / (float)UI_H;
    g_scale = sx < sy ? sx : sy;
    if (g_scale <= 0.0f) g_scale = 1.0f;

    g_off_x = (sw - (float)UI_W * g_scale) * 0.5f;
    g_off_y = (sh - (float)UI_H * g_scale) * 0.5f;
}

float input_ui_scale(void) { return g_scale; }
float input_ui_off_x(void) { return g_off_x; }
float input_ui_off_y(void) { return g_off_y; }

Vector2 input_mouse_virtual(void)
{
    Vector2 m = GetMousePosition();
    return (Vector2){ (m.x - g_off_x) / g_scale, (m.y - g_off_y) / g_scale };
}

bool input_click_pressed(void)  { return IsMouseButtonPressed(MOUSE_BUTTON_LEFT); }
bool input_click_released(void) { return IsMouseButtonReleased(MOUSE_BUTTON_LEFT); }
bool input_click_down(void)     { return IsMouseButtonDown(MOUSE_BUTTON_LEFT); }
bool input_key_pressed(int key) { return IsKeyPressed(key); }
float input_wheel(void)         { return GetMouseWheelMove(); }

/* 本帧鼠标位移，换算到逻辑坐标（已除以窗口缩放），星图拖拽直接叠加即可 */
Vector2 input_mouse_delta(void)
{
    Vector2 d = GetMouseDelta();
    float k = (g_scale > 0.0001f) ? g_scale : 1.0f;
    return (Vector2){ d.x / k, d.y / k };
}

/* 从后往前找：后绘制的（弹层）优先，天然实现“上层遮挡”。 */
int input_hit_topmost(const UiLayout *layout, const UIState *state)
{
    (void)state;
    if (!layout || layout->count <= 0) return -1;
    Vector2 m = input_mouse_virtual();
    for (int i = layout->count - 1; i >= 0; --i) {
        Rectangle r = layout->items[i].rect;
        if (CheckCollisionPointRec(m, r)) return i;
    }
    return -1;
}
