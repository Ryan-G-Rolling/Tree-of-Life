/*
 * habitat.h  ——  生命体所处的「生境」背景层
 *
 * 问题：前几个阶段全在海洋里，但阶段 6 之后（湿地 → 森林 → 城市 → 轨道 →
 * 奇点）生物早就不在水里了，展台却一直是同一块深水底，玩家读不出演化尺度。
 *
 * 做法：把「时代」映射成 8 种生境，每个生境自带底色、地形剪影与一组只做
 * alpha/位移的元素。展台、图鉴大图、培养皿与全屏环境光晕共用这一套，
 * 保证同一个物种在任何页面都站在同一片环境里。
 *
 * 硬约定：
 *   1. 只有 C 层可见（screens.c / ui_adapter 的展示文案），不参与任何数值。
 *   2. 不读 GameState，不做裁剪 —— 所有图元都收进传入的 Rectangle，
 *      调用方若已在 scissor 里，自己负责边界（本模块只保证不主动越界）。
 *   3. 阶段编号沿用 ua_species_stage()：0 原始生命 … 11 超越。
 */
#ifndef HABITAT_H
#define HABITAT_H

#include "raylib.h"

typedef enum {
    HABITAT_HYDROTHERMAL = 0,   /* 深海热泉：原始生命、化能谱系 */
    HABITAT_SUNLIT_SEA,         /* 透光浅海：光合谱系 */
    HABITAT_REEF,               /* 珊瑚礁：多细胞、共生体 */
    HABITAT_POLAR,              /* 极地冰海：极端环境谱系 */
    HABITAT_WETLAND,            /* 湿地与森林：登陆之后 */
    HABITAT_CITY,               /* 城市与技术圈 */
    HABITAT_ORBIT,              /* 轨道与深空 */
    HABITAT_SINGULARITY,        /* 奇点与超越 */
    HABITAT_COUNT
} HabitatId;

/* 按阶段回落（物种名没有线索时用） */
int habitat_for_stage(int stage);

/* 先看中文名里的生境线索（热泉 / 礁 / 森林 / 轨道…），再回落阶段。
 * species_id 用于查图鉴中文名；越界时按 stage 处理。 */
int habitat_for_species(int species_id, int stage);

const char *habitat_name(int habitat);
const char *habitat_note(int habitat);   /* 一句环境说明，展台 tooltip 用 */
Color habitat_accent(int habitat);       /* 该生境的点缀色（用于小圆点/标签） */

/* 在 area 内画满生境底：渐变 + 远景 + 粒子。detail 0..1 控制元素密度，
 * 展台用 1.0，图鉴大图用 0.75。不修改任何全局状态。 */
void habitat_draw(Rectangle area, int habitat, float time, float detail);

/* 全屏极淡的环境色晕（只在上/下边缘，不压中心）。用于让整屏氛围跟着时代走，
 * alpha 建议 0.5~1.0，函数内部再乘一个很小的系数。 */
void habitat_bleed(int habitat, float time, float amount);

#endif /* HABITAT_H */
