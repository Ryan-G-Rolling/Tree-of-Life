/*
 * embedded.c  ——  资源内嵌层实现
 *
 * 符号来自 objcopy：
 *     objcopy -I binary -O pe-x86-64 -B i386:x86-64 font.ttf font.o
 * 会在目标文件里生成 _binary_font_ttf_start / _end / _size 三个符号，
 * 分别指向数据首字节、尾后一字节和字节数。这里用 _start/_end 相减算长度，
 * 比引用 _size（绝对符号，类型是 ABS）更省事，也不会踩到指针宽度换算。
 *
 * 只在 make dist（-DEMBED_ASSETS）时链接那些目标文件；普通构建没有它们。
 */
#include "embedded.h"

#ifdef EMBED_ASSETS

extern const unsigned char _binary_font_ttf_start[];
extern const unsigned char _binary_font_ttf_end[];
extern const unsigned char _binary_nebula_png_start[];
extern const unsigned char _binary_nebula_png_end[];
extern const unsigned char _binary_specimen_png_start[];
extern const unsigned char _binary_specimen_png_end[];
extern const unsigned char _binary_click_wav_start[];
extern const unsigned char _binary_click_wav_end[];
extern const unsigned char _binary_buy_wav_start[];
extern const unsigned char _binary_buy_wav_end[];
extern const unsigned char _binary_evolve_wav_start[];
extern const unsigned char _binary_evolve_wav_end[];

/* 19 张形态族插画：objcopy 符号取自 build/emb/ 下的短文件名。
 * 列表顺序必须与 tex.h 的 TexMorph 完全对应。 */
#define EMB_MORPH_LIST(X) \
    X(coccus)    X(rod)       X(eukaryote) X(colony) X(tissue) \
    X(symbio)    X(neural)    X(techno)    X(diatom) X(ciliate) \
    X(amoeba)    X(filament)  X(spiral)    X(fish)   X(jelly) \
    X(fern)      X(fungus)    X(coral)     X(hive)

#define EMB_MORPH_DECL(name) \
    extern const unsigned char _binary_morph_##name##_png_start[]; \
    extern const unsigned char _binary_morph_##name##_png_end[];
EMB_MORPH_LIST(EMB_MORPH_DECL)

#define EMB_MORPH_START(name) _binary_morph_##name##_png_start,
#define EMB_MORPH_END(name)   _binary_morph_##name##_png_end,

static const unsigned char *const k_start[EMB_COUNT] = {
    _binary_font_ttf_start,
    _binary_nebula_png_start,
    _binary_specimen_png_start,
    _binary_click_wav_start,
    _binary_buy_wav_start,
    _binary_evolve_wav_start,
    EMB_MORPH_LIST(EMB_MORPH_START)
};

static const unsigned char *const k_end[EMB_COUNT] = {
    _binary_font_ttf_end,
    _binary_nebula_png_end,
    _binary_specimen_png_end,
    _binary_click_wav_end,
    _binary_buy_wav_end,
    _binary_evolve_wav_end,
    EMB_MORPH_LIST(EMB_MORPH_END)
};

#endif /* EMBED_ASSETS */

bool emb_get(EmbId id, const unsigned char **data, int *size)
{
#ifdef EMBED_ASSETS
    if ((int)id < 0 || (int)id >= (int)EMB_COUNT) return false;
    if (k_end[id] <= k_start[id]) return false;
    if (data) *data = k_start[id];
    if (size) *size = (int)(k_end[id] - k_start[id]);
    return true;
#else
    (void)id;
    (void)data;
    (void)size;
    return false;
#endif
}

bool emb_any(void)
{
#ifdef EMBED_ASSETS
    return true;
#else
    return false;
#endif
}
