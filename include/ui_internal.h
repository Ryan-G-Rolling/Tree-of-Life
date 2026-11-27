/*
 * ui_internal.h  ——  Member C 私有头文件（A/B 不需要包含它）
 *
 * 视觉方向 3.0「Primordial Cosmos」：
 *   深空藏青底 + 金色 UI + 生物荧光主体，参考 Cookie Clicker 的养成手感与
 *   Cell to Singularity 的宇宙尺度美术。
 *
 * 设计要点：
 *   1. 逻辑画布固定 1280×720（16:9）。窗口变化时整体缩放并 letterbox，
 *      布局代码不用管窗口尺寸；--shot 以 SS=3 导出 3840×2160 真 4K UHD。
 *   2. UI 自己的状态全部放在 g_ui（static 于 ui.c 之外可见），绝不写进 GameState。
 *   3. 每帧先 build 一次布局（矩形表），再判点击，最后用同一份矩形绘制 ——
 *      避免“绘制顺序”和“命中顺序”不一致导致弹层下面的按钮被误点。
 */
#ifndef UI_INTERNAL_H
#define UI_INTERNAL_H

#include "raylib.h"
#include "game_types.h"

#define UI_W 1280
#define UI_H 720

/* 超采样档位上下限。**必须是整数**：绘制侧所有 1px 描边乘上整数 SS 之后仍是
 * 整数设备像素，不会出现 1.5px 这种半覆盖描边。1280×720 × 3 = 3840×2160，
 * 正好是 UHD 4K 的整数倍，截图不经过任何缩放。 */
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
    UI_OVL_TREE,
    UI_OVL_MUT_LIST,    /* 突变图谱：本轮已获得的全部突变 + 已触发共振 */
    UI_OVL_CODEX_DETAIL
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
    UI_BTN_CHOOSE_MUTATION,
    UI_BTN_REFRESH_MUTATION,     /* → ACTION_REFRESH_MUTATIONS（v14 E3） */     /* option = 候选下标 → ACTION_CHOOSE_MUTATION */
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
    UI_BTN_OPEN_MUT_LIST,       /* 仅打开“突变图谱”弹层 */
    UI_BTN_TREE_NODE,           /* 进化树节点：target_id = 物种，选中后高亮它的整条路线 */
    UI_BTN_TREE_SKY,            /* 星图空白区：按住拖动平移（不关闭弹层） */
    UI_BTN_TREE_RESET,          /* 星图：复位缩放与平移 */
    UI_BTN_CLOSE_OVERLAY,       /* 仅关闭弹层 */
    UI_BTN_CODEX_TAB,           /* option = 0 物种 / 1 突变 / 2 记录 */
    UI_BTN_STORE_TAB,           /* 主界面商店页签：option = 0 机能 / 1 分化 */
    UI_BTN_INFO_TAB,            /* 情报面板页签：option = 0 下一步 / 1 产出 / 2 纪录 */
    UI_BTN_SPECIATION_PAGE,
    UI_BTN_COLLECT_BUFF,
    UI_BTN_BUY_INSIGHT,
    UI_BTN_CODEX_ENTRY,
    UI_BTN_CODEX_PAGE,
    UI_BTN_CODEX_BACK,
    UI_BTN_COUNT
} UiButtonId;

typedef struct {
    UiButtonId id;
    Rectangle rect;
    int target_id;
    int option;
    bool enabled;
    float press;   /* 0..1 按下动画 */
    /* 0..1 hover 插值。**必须跨帧保留**：screens_build 每帧重建矩形表，
     * btn_add 里的 memset 会把它清零，因此 btn_add 会从上一帧的布局快照
     * 继承同 (id,target,option) 按钮的 hover/press（见 screens.c s_prev）。 */
    float hover;
} UiButton;

#define MAX_UI_BUTTONS (MAX_SPECIES + 64)
#define UI_SPECIATION_PAGE_SIZE 4
#define UI_CODEX_PAGE_SIZE 12

typedef struct {
    UiButton items[MAX_UI_BUTTONS];
    int count;
} UiLayout;

/* ---------------- 点击粒子（爽感系统） ---------------- */

#define UI_FX_MAX 96

typedef struct {
    float x, y;
    float vx, vy;
    float t;       /* 剩余寿命 0..1 */
    float ttl;     /* 初始寿命（秒） */
    float size;
    float drag;
    int shape;     /* 0 圆点 / 1 星芒 / 2 十字 */
    Color col;
} UiFx;

/* ---------------- UI 私有状态 ---------------- */

typedef struct {
    UiScreen screen;
    UiOverlay overlay;
    UiLayout layout;

    int hot;          /* 鼠标悬停的按钮下标，-1 表示无 */
    int pressed;      /* 按下的按钮下标 */
    float press_anim; /* 点击生命体的扩散动画 */
    float punch;      /* 点击时主体的膨胀回弹 0..1 */

    int keep_parent;  /* 分化弹层：0 不保留亲本，1 保留 */
    int codex_tab;
    int store_tab;    /* 主界面商店：0 机能升级 / 1 分支分化 */
    int info_tab;     /* 主界面情报面板：0 下一步 / 1 产出 / 2 纪录 */
    int tree_sel;     /* 进化树选中的物种，-1 表示跟随主谱系 */
    /* 星图视图：以圆心为原点缩放，再叠加平移。默认 1.0/0/0 看全貌 */
    float tree_zoom;
    float tree_pan_x, tree_pan_y;
    bool tree_dragging;
    int speciation_page;
    int codex_page;
    int codex_detail_id;
    int codex_detail_kind; /* 0 species / 1 mutation; transient, never saved */

    /* 产出曲线：三条资源各自 72 点环形缓冲（每 0.25s 采一次 = 18 秒窗口）。
     * 只用于画面，不参与任何数值计算。 */
    float hist[3][72];
    int hist_n;
    float hist_acc;
    int ach_seen[8];  /* 成就解锁比对（Cookie Clicker 式“叮”一声的提示） */

    float toast_t;    /* 提示条剩余时间 */
    char toast[192];
    bool toast_bad;

    /* 采集漂浮数字：点一次皿就冒一个 "+x ATP"，1 → 0 的计时自己驱动上升与淡出。 */
    float gain_t[3];
    double gain_val[3];
    int gain_slot;

    float flash;      /* 点击反馈闪光 */
    float ovl_t;      /* 弹层淡入进度 */

    /* 危机演出：进入危机的那一帧由 crisis_watch() 置位，之后按包络衰减。
     * 抖屏在最终合成里按同一包络平移整帧，所以三者共享一个时间基准。 */
    int prev_env_id;          /* 上一帧的环境，用于检测"刚进入危机" */
    int crisis_env;           /* 正在演出的危机环境编号，-1 表示无 */
    float crisis_t;           /* 演出剩余秒数（2.2 → 0） */
    float shake;              /* 抖屏强度包络 0..1 */
    float crisis_flash;       /* 全屏闪光 0..1 */
    float time;       /* 全局时间，用于呼吸/漂浮动画 */
    int last_phase;
    float pop_pulse[MAX_ACTIVE_SPECIES];

    UiFx fx[UI_FX_MAX];
    int fx_n;
} UIState;

extern UIState g_ui;
/* Pure local navigation, also exercised by window-free integration tests. */
void ui_handle_local_button(const UiButton *button, const GameState *game);
void ui_dismiss_overlay(void);

/* ---------------- 主题 ----------------
 * 深空底 + 金色品牌色 + 生物荧光语义色。
 * 所有颜色只在 render.c 顶部定义一次；其他文件必须引用 COL_* 名字。 */

extern const Color COL_BG;
extern const Color COL_BG_DEEP;
extern const Color COL_BG_LIFT;
extern const Color COL_PANEL;
extern const Color COL_PANEL_HI;
extern const Color COL_CHROME;
extern const Color COL_STAGE;
extern const Color COL_SURFACE_OFF;
extern const Color COL_PANEL_HOT;
extern const Color COL_BAR_TRACK;
extern const Color COL_LINE;
extern const Color COL_LINE_STRONG;
extern const Color COL_TEXT;
extern const Color COL_DIM;
extern const Color COL_FAINT;
extern const Color COL_ACCENT;         /* 品牌金 */
extern const Color COL_GOLD_DEEP;
extern const Color COL_ATP;
extern const Color COL_BIOMASS;
extern const Color COL_DNA;
extern const Color COL_EP;
extern const Color COL_BAD;
extern const Color COL_GOOD;
extern const Color COL_RARE;
extern const Color COL_TEAL;
extern const Color COL_VIOLET;
extern const Color COL_EP_SURFACE;
extern const Color COL_BAD_SURFACE;
extern const Color COL_ACCENT_SURFACE;
extern const Color COL_CELL;
extern const Color COL_VIGNETTE;
extern const Color COL_GHOST;

/* ---------------- 度量 ----------------
 * 圆角四档；描边整数逻辑像素；间距 4 的倍数；字号六档。 */

#define R_PANEL   18.0f
#define R_CARD    14.0f
#define R_BTN     10.0f
#define R_CHIP(h) ((h) * 0.5f)

#define W_HAIRLINE  1.0f
#define W_EMPHASIS  2.0f
#define W_HAIRLINE_AA 1.05f

#define S_MARGIN   16.0f
#define S_SECTION  12.0f
#define S_PAD      16.0f
#define S_GAP       8.0f
#define S_TIGHT     4.0f
#define S_SAFE     10.0f

#define H_PRIMARY    48.0f
#define H_SECONDARY  36.0f
#define H_INLINE     28.0f
#define H_CHIP       20.0f

#define F_CAPTION    12.0f
#define F_BODY       14.0f
#define F_LABEL      16.0f
#define F_TITLE      21.0f
#define F_DISPLAY    28.0f
#define F_HERO       38.0f

#define LH_CAPTION   18.0f
#define LH_BODY      22.0f
#define LH_LABEL     24.0f
#define LH_TITLE     30.0f
#define LH_DISPLAY   38.0f
#define LH_HERO      50.0f

/* ---------------- 绘制原语（render.c） ---------------- */

void ui_font_init(void);
void ui_font_shutdown(void);
/* 登记会被显示的中文串（在 ui_font_init 之前调用），用于生成字形集。 */
void ui_font_register(const char *utf8);

float ui_text(const char *text, float x, float y, float size, Color color);
float ui_text_w(const char *text, float size);
void ui_text_center(const char *text, float cx, float y, float size, Color color);
void ui_text_right(const char *text, float rx, float y, float size, Color color);
/* 带深色落影的大字（数字/标题用；小字不要用，会糊） */
void ui_text_shadow(const char *text, float x, float y, float size, Color color, Color shadow);
/* 轻微加粗：同一串在 x/y 各偏移半像素重画两次（仅 ≥20px 使用） */
void ui_text_strong(const char *text, float x, float y, float size, Color color);
void ui_text_center_strong(const char *text, float cx, float y, float size, Color color);

/* 逻辑坐标裁剪：后续绘制被限制在 r 内（不能嵌套） */
void ui_clip_begin(Rectangle r);
void ui_clip_end(void);

void ui_panel(Rectangle r, float radius, Color fill, Color border);
void ui_panel_soft(Rectangle r, float radius, Color fill);
/* 面板外的一圈柔光（焦点/稀有/当前路径） */
void ui_halo(Rectangle r, float radius, Color col, float alpha);
/* 竖直渐变矩形（GPU 顶点插值，不产生色带） */
void ui_gradient(Rectangle r, Color top, Color bottom);
/* 柔光球：从中心向外衰减的发光体（叠加混合，画在暗底上） */
void ui_glow(Vector2 c, float radius, Color col, float alpha);

void ui_bar(Rectangle r, float t, Color fill, Color back);
void ui_divider(float x1, float x2, float y, Color color);

Color ui_color_mix(Color a, Color b, float t);

void ui_fmt_num(double v, char *out, int n);
void ui_fmt_rate(double v, char *out, int n);
void ui_fmt_time(double seconds, char *out, int n);

/* 按钮：负责绘制与按下动画，返回是否被本帧按下（不含命中判定，命中在 input.c） */
void ui_btn_draw(UiButton *b, const char *title, const char *sub, bool hot);

/* 小装饰：角标与圆点 */
void ui_tag(Rectangle r, const char *text, Color color);
void ui_dot(float cx, float cy, float radius, Color color);

/* ---------------- 超采样画布（ui.c） ---------------- */
void ui_set_forced_ss(int ss);
int ui_canvas_ss(void);

/* ---------------- 点击粒子（ui.c） ---------------- */

void ui_fx_burst(Vector2 c, Color col, int count, float speed);
void ui_fx_tick(float dt);
void ui_fx_draw(Vector2 c, float radius);

/* ---------------- 输入（input.c） ---------------- */

void input_begin_frame(void);
float input_ui_scale(void);
float input_ui_off_x(void);
float input_ui_off_y(void);
Vector2 input_mouse_virtual(void);
bool input_click_pressed(void);
bool input_click_released(void);
bool input_click_down(void);       /* 左键是否按住（星图拖拽用） */
bool input_key_pressed(int key);
float input_wheel(void);           /* 本帧滚轮增量（正=向前） */
Vector2 input_mouse_delta(void);   /* 本帧鼠标位移（逻辑坐标，已除以缩放） */
int input_hit_topmost(const UiLayout *layout, const UIState *state);

/* ---------------- 页面（screens.c） ---------------- */

void screens_build(UiLayout *out, const GameState *game, const UIState *state);
void screens_draw(const GameState *game, const UIState *state, const ActionResult *last_result);

/* 随机增益因子在培养皿内的坐标：build_main（命中框）与 draw_dish（绘制）共用，
 * 保证"看到哪就点到哪"。没有因子时返回皿心，调用方应先判 buffs_factor_visible。 */
Vector2 screens_buff_factor_pos(const GameState *game, const UIState *state);

/* 星图：圆心与平移上限，ui.c 的拖拽/缩放按这两个值夹取，避免把星图拖出画框 */
Vector2 screens_tree_center(void);
float screens_tree_pan_limit(const GameState *game);

/* 调试/截图用：强制切到某个页面或弹层（只在 --shot 模式下由 main.c 调用） */
void ui_debug_force(UiScreen screen, UiOverlay overlay);
void ui_debug_set_codex_tab(int tab);
void ui_debug_set_keep_parent(int v);
void ui_debug_set_store_tab(int tab);
void ui_debug_set_info_tab(int tab);
/* 截图用：模拟一次成功采集，让"皿上浮字 + 闪光 + 粒子"出现在导出帧里 */
void ui_debug_poke_click(double gain);
/* 截图用：直接挂一条提示条 */
void ui_debug_poke_toast(const char *text, bool bad);

/* 截图用：把危机演出拉到指定强度（t 为剩余秒数，2.2 表示刚触发） */
void ui_debug_poke_crisis(int env_id, float t);
/* 直接把当前逻辑画布导出成 png */
void ui_debug_capture(const GameState *game, const char *path);
/* 把“按钮 id → GameAction”的映射暴露给自检用 */
bool ui_debug_map_button(int id, int target, int option, GameAction *out);

#endif /* UI_INTERNAL_H */

