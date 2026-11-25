/*
 * ui_internal.h  ——  Member C 私有头文件（A/B 不需要包含它）
 *
 * 设计要点：
 *   1. 逻辑画布固定 1280×800，窗口变化时整体缩放并letterbox，布局代码不用管窗口尺寸。
 *   2. UI 自己的状态全部放在 g_ui（static 于 ui.c 之外可见），绝不写进 GameState。
 *   3. 每帧先 build 一次布局（矩形表），再判点击，最后用同一份矩形绘制 ——
 *      避免“绘制顺序”和“命中顺序”不一致导致弹层下面的按钮被误点。
 */
#ifndef UI_INTERNAL_H
#define UI_INTERNAL_H

#include "raylib.h"
#include "game_types.h"

#define UI_W 1280
#define UI_H 800

/* 超采样档位上下限。**必须是整数**：绘制侧所有 1px 描边（DrawRectangleLinesEx /
 * DrawLineEx / DrawRectangleRoundedLinesEx）乘上整数 SS 之后仍是整数设备像素，
 * 不会出现 1.5px 这种半覆盖描边；同时保证 BaseSize = UI_FONT_BASE_SIZE/UI_SS_MAX
 * 的推导成立（120/3 = 40 = 全局最大字号）。改这里必须同步复核这两件事。 */
#define UI_SS_MIN 1
#define UI_SS_MAX 3

/* ---------------- 页面与弹层 ---------------- */

typedef enum {
    UI_SCREEN_MAIN = 0,
    UI_SCREEN_END_ERA,
    UI_SCREEN_LEGACY
} UiScreen;

typedef enum {
    UI_OVL_NONE = 0,
    UI_OVL_MUTATION,
    UI_OVL_SPECIATION,
    UI_OVL_CODEX,
    UI_OVL_TREE
} UiOverlay;

/* ---------------- 按钮 ----------------
 * 每种按钮只描述“意图”，真正的 GameAction 在 ui.c 的 button_to_action 里统一转换，
 * 保证 UI 任何地方都不直接改 GameState。 */
typedef enum {
    UI_BTN_NONE = 0,
    UI_BTN_BACKDROP,            /* 弹层遮罩：点空白处关闭，只做命中不做绘制 */
    UI_BTN_CLICK,               /* → ACTION_CLICK_ATP */
    UI_BTN_UPGRADE,             /* target_id = RunUpgradeType → ACTION_BUY_RUN_UPGRADE */
    UI_BTN_OPEN_MUTATION,       /* → ACTION_GENERATE_MUTATION_OFFER */
    UI_BTN_CHOOSE_MUTATION,     /* option = 候选下标 → ACTION_CHOOSE_MUTATION */
    UI_BTN_OPEN_SPECIATION,     /* 仅打开弹层，不发动作 */
    UI_BTN_SPECIATE,            /* target_id = 物种 → ACTION_SPECIATE */
    UI_BTN_TOGGLE_KEEP_PARENT,  /* 仅切换 option，不发动作 */
    UI_BTN_OPEN_END_ERA,        /* → ACTION_OPEN_END_ERA */
    UI_BTN_CANCEL_END_ERA,      /* → ACTION_CANCEL_END_ERA */
    UI_BTN_CONFIRM_END_ERA,     /* → ACTION_CONFIRM_END_ERA */
    UI_BTN_BUY_LEGACY,          /* target_id = LegacyBranch → ACTION_BUY_LEGACY */
    UI_BTN_START_NEW_RUN,       /* → ACTION_START_NEW_RUN */
    UI_BTN_OPEN_CODEX,          /* 仅打开弹层 */
    UI_BTN_OPEN_TREE,           /* 仅打开弹层 */
    UI_BTN_CLOSE_OVERLAY,       /* 仅关闭弹层 */
    UI_BTN_CODEX_TAB,           /* option = 0 物种 / 1 突变 / 2 记录 */
    UI_BTN_COUNT
} UiButtonId;

typedef struct {
    UiButtonId id;
    Rectangle rect;
    int target_id;
    int option;
    bool enabled;
    float press;   /* 0..1 按下动画 */
    /* 0..1 hover 抬升插值。**必须跨帧保留**：screens_build 每帧重建矩形表，
     * btn_add 里的 memset 会把它清零，因此 btn_add 会从上一帧的布局快照
     * 继承同 (id,target,option) 按钮的 hover/press（见 screens.c s_prev）。 */
    float hover;
} UiButton;

#define MAX_UI_BUTTONS 32

typedef struct {
    UiButton items[MAX_UI_BUTTONS];
    int count;
} UiLayout;

/* ---------------- UI 私有状态 ---------------- */

typedef struct {
    UiScreen screen;
    UiOverlay overlay;
    UiLayout layout;

    int hot;          /* 鼠标悬停的按钮下标，-1 表示无 */
    int pressed;      /* 按下的按钮下标 */
    float press_anim; /* 点击生命体的扩散动画 */

    int keep_parent;  /* 分化弹层：0 不保留亲本，1 保留 */
    int codex_tab;

    float toast_t;    /* 提示条剩余时间 */
    char toast[192];
    bool toast_bad;

    float flash;      /* 点击反馈闪光 */
    float ovl_t;      /* 弹层淡入进度 */
    float time;       /* 全局时间，用于呼吸/漂浮动画 */
    int last_phase;
    float pop_pulse[MAX_ACTIVE_SPECIES];
} UIState;

extern UIState g_ui;

/* ---------------- 主题 ----------------
 * 35 个 token：25 个基础面/文字/语义 + 7 个物种身份色 + 3 个背景层（01 §2.2/§2.2b）。
 * 全部定义在 render.c 顶部。任何括号形式的 Color 复合字面量都是 bug（01 §2.2 R0-1 / 附录 A）。 */

extern const Color COL_BG;
extern const Color COL_BG_DEEP;
extern const Color COL_PANEL;
extern const Color COL_PANEL_HI;
extern const Color COL_CHROME;         /* 顶栏/底栏固定 chrome 条（0–88 / 736–800） */
extern const Color COL_STAGE;          /* 生命体展台凹陷面 */
extern const Color COL_SURFACE_OFF;    /* 未解锁卡 / 空槽 / 禁用按钮 */
extern const Color COL_PANEL_HOT;      /* hover / 按下 / 选中填充 */
extern const Color COL_BAR_TRACK;      /* 所有进度条槽底 */
extern const Color COL_LINE;
extern const Color COL_LINE_STRONG;    /* 图表轴线 / 焦点框（CR 3.92） */
extern const Color COL_TEXT;
extern const Color COL_DIM;
extern const Color COL_FAINT;
extern const Color COL_ACCENT;
extern const Color COL_ATP;
extern const Color COL_BIOMASS;
extern const Color COL_DNA;
extern const Color COL_EP;
extern const Color COL_BAD;
extern const Color COL_GOOD;
extern const Color COL_RARE;
extern const Color COL_EP_SURFACE;     /* EP 专属容器底 */
extern const Color COL_BAD_SURFACE;    /* 危机横幅 / 负向 toast 底 */
extern const Color COL_ACCENT_SURFACE; /* 正向 toast 底 / “当前”卡底 */

/* 物种身份色：统一相对亮度 L=0.340、S≈0.50，只靠色相区分身份（01 §2.4）。
 * 只做填充与色轨，永不做文字色；饱和度上限 0.55。 */
extern const Color COL_SP0;  /* 原始细胞 150° */
extern const Color COL_SP1;  /* 光合细菌  95° */
extern const Color COL_SP2;  /* 化能细菌  40° */
extern const Color COL_SP3;  /* 藻类真核 177° */
extern const Color COL_SP4;  /* 原生动物 337° */
extern const Color COL_SP5;  /* 多细胞群体 250° */
extern const Color COL_SP6;  /* 极端环境群体 300° */

/* 背景层（01 §2.2b，供 02-atmosphere 的氛围层使用，界面层本轮不引用）。
 * 三个色与 COL_BG 的 CR 都刻意 < 1.10 —— “单个 token 自己几乎看不见”，
 * 可见度全部来自多层叠加（02 §1.2 铁律：背景像素 CR ≤ 1.25）。 */
extern const Color COL_CELL;      /* 背景细胞面 / 培养皿内壁（CR vs COL_BG = 1.086） */
extern const Color COL_VIGNETTE;  /* 四周暗角（同样锁 < 1.10） */
extern const Color COL_GHOST;     /* 图鉴“未发现”轮廓；**只能配 COL_SURFACE_OFF 底**（CR 1.19），
                                   * 在 COL_PANEL_HI 上只有 1.09 不达标 */

/* ---------------- 度量（01 §4.1 / §4.2 / §6.1 / §6.3） ----------------
 * 只允许这些值。写了枚举外的字号/圆角/描边/间距就是 bug（R0-2）。 */

/* 圆角：四档，只有四档 */
#define R_PANEL   16.0f            /* 弹层大面板、页面级面板 */
#define R_CARD    12.0f            /* 卡片、物种卡、图鉴格、展台、EP 块 */
#define R_BTN     10.0f            /* 按钮、页签、chip 容器 */
#define R_CHIP(h) ((h) * 0.5f)     /* 胶囊 chip：恒为高度的一半 */

/* 描边：必须是整数逻辑像素（SS 会放大 2–3 倍，非整数宽度会一段粗一段细） */
#define W_HAIRLINE  1.0f
#define W_EMPHASIS  2.0f

/* Z-A1 临时妥协（team-lead 已批准临时值，待逐边反锯齿落地后改回 W_HAIRLINE）。
 *
 * **只用于 DrawLineEx 这类"线级"描边，绝不给 ui_panel 的嵌套填充用。**
 *
 * 为什么是 1.05：线宽 w 在 SS 档位下的物理像素 = w×SS。w=1.0 时线段若落在非整数
 * 坐标上，光栅化会把覆盖拆给相邻两行（各 50%），线看起来是半强度、断续的；
 * 加 5% 余量后 floor(1.05×SS) 在 SS=1/2/3 下稳定给 1/2/3，且保证至少一整行满覆盖。
 *
 * 为什么不能顺手把她改成全局 1.05：ui_panel 的描边是"外圈整块画 border 色、
 * 内缩 W_HAIRLINE 再画 fill"的嵌套填充。内缩量 1.05 在 SS=3 时落在
 * 3.15 物理像素处 ⇒ 边框读作 3 整行 + 1 行 15% 过渡；SS=1 时 1 整行 + 1 行 5%。
 * 那正是 01 §2.5 明确要消灭的"过渡行"。ENG-02 已实测锁定 ui_panel 在
 * W_HAIRLINE=1.0 下 SS=1/2/3 → 1/2/3 物理像素、满强度、零过渡行（证据见
 * docs/screens/v2/ 的边框逐像素量测）。**别动 W_HAIRLINE 本身。** */
#define W_HAIRLINE_AA 1.05f

/* 间距：基础单位 4，1280×800 刚好 320×200 格 */
#define S_MARGIN   20.0f
#define S_SECTION  16.0f
#define S_PAD      16.0f
#define S_GAP       8.0f
#define S_TIGHT     4.0f
#define S_SAFE     12.0f

/* 控件高度：四档 */
#define H_PRIMARY    44.0f         /* 主操作按钮 */
#define H_SECONDARY  34.0f         /* 次操作（页签 / 关闭 / 顶栏入口） */
#define H_INLINE     28.0f         /* 卡片内嵌小按钮 */
#define H_CHIP       20.0f         /* chip / 标签 / 徽标 */

/* 字号阶梯：六档（01 §3.1）。全局下限 12、上限 38（受 §7.3-C2 约束）。 */
#define F_CAPTION    12.0f
#define F_BODY       14.0f
#define F_LABEL      16.0f
#define F_TITLE      20.0f
#define F_DISPLAY    26.0f
#define F_HERO       38.0f

/* 行高与字号绑定（01 §6.2），不要单独调行高 */
#define LH_CAPTION   18.0f
#define LH_BODY      22.0f
#define LH_LABEL     24.0f
#define LH_TITLE     30.0f
#define LH_DISPLAY   38.0f
#define LH_HERO      52.0f

/* ---------------- 绘制原语（render.c） ---------------- */

void ui_font_init(void);
void ui_font_shutdown(void);
/* 登记会被显示的中文串（在 ui_font_init 之前调用），用于生成字形集。 */
void ui_font_register(const char *utf8);

float ui_text(const char *text, float x, float y, float size, Color color);
float ui_text_w(const char *text, float size);
void ui_text_center(const char *text, float cx, float y, float size, Color color);
void ui_text_right(const char *text, float rx, float y, float size, Color color);

void ui_panel(Rectangle r, float radius, Color fill, Color border);
void ui_panel_soft(Rectangle r, float radius, Color fill);
void ui_bar(Rectangle r, float t, Color fill, Color back);
void ui_divider(float x1, float x2, float y, Color color);

/* 两色插值：用于同一 token 家族的中间态（hover / 选中），避免为此再写字面量 */
Color ui_color_mix(Color a, Color b, float t);

void ui_fmt_num(double v, char *out, int n);
void ui_fmt_rate(double v, char *out, int n);
void ui_fmt_time(double seconds, char *out, int n);

/* 按钮：负责绘制与按下动画，返回是否被本帧按下（不含命中判定，命中在 input.c） */
void ui_btn_draw(UiButton *b, const char *title, const char *sub, bool hot);

/* 小装饰：角标与圆点 */
void ui_tag(Rectangle r, const char *text, Color color);
void ui_dot(float cx, float cy, float radius, Color color);

/* ---------------- 超采样画布（ui.c） ----------------
 * 逻辑坐标永远是 1280×800；离屏画布的实际像素密度由 SS（超采样因子）决定：
 *     SS = clamp(ceil(min(winW/1280, winH/800)), 1, 3)
 * 画布尺寸 = UI_W*SS × UI_H*SS，绘制时用 Camera2D(zoom=SS) 把逻辑坐标放大，
 * 合成到窗口时再整张下采样 —— 等价于 SSAA，4K 窗口下 SS=3 正好 1:1 无插值。
 * 界面代码（screens.c / render.c 的绘制原语）完全不用知道 SS 的存在。 */
void ui_set_forced_ss(int ss); /* 0 = 按窗口自动推导；1..3 = 强制（--ss / --shot 用） */
int ui_canvas_ss(void);        /* 当前画布实际 SS，1..3 */

/* ---------------- 输入（input.c） ---------------- */

void input_begin_frame(void);
float input_ui_scale(void);
float input_ui_off_x(void);
float input_ui_off_y(void);
Vector2 input_mouse_virtual(void);
bool input_click_pressed(void);
bool input_click_released(void);
bool input_key_pressed(int key); /* 转发 raylib，集中管理便于换键位 */
int input_hit_topmost(const UiLayout *layout, const UIState *state);

/* ---------------- 页面（screens.c） ---------------- */

void screens_build(UiLayout *out, const GameState *game, const UIState *state);
void screens_draw(const GameState *game, const UIState *state, const ActionResult *last_result);

/* 调试/截图用：强制切到某个页面或弹层（只在 --shot 模式下由 main.c 调用） */
void ui_debug_force(UiScreen screen, UiOverlay overlay);
void ui_debug_set_codex_tab(int tab);
void ui_debug_set_keep_parent(int v);
/* 直接把当前逻辑画布导出成 png（不依赖窗口帧缓冲，避免双缓冲空白） */
void ui_debug_capture(const GameState *game, const char *path);
/* 把“按钮 id → GameAction”的映射暴露给自检用，返回 false 表示这是纯 UI 按钮 */
bool ui_debug_map_button(int id, int target, int option, GameAction *out);

#endif /* UI_INTERNAL_H */
