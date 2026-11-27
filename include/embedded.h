#ifndef EMBEDDED_H
#define EMBEDDED_H

#include <stdbool.h>

/*
 * embedded.h  ——  资源内嵌层
 *
 * 单文件发布版（make dist）用 objcopy 把 assets/ 里的字体、纹理、音效
 * 编成目标文件链进 exe，运行时直接从内存读，不需要旁边的 assets/ 目录。
 *
 * 普通构建没有定义 EMBED_ASSETS，也没有那些符号：emb_get() 一律返回 false，
 * 调用处自动退回原来的文件路径 —— 开发期的热替换（改图 / 换字体）照旧有效。
 */

typedef enum {
    EMB_FONT = 0,     /* assets/fonts/ui_zh.ttf */
    EMB_NEBULA,       /* assets/tex/nebula.png   */
    EMB_SPECIMEN,     /* assets/tex/specimen.png */
    EMB_CLICK,        /* assets/sfx/click.wav    */
    EMB_BUY,          /* assets/sfx/buy.wav      */
    EMB_EVOLVE,       /* assets/sfx/evolve.wav   */
    EMB_COUNT
} EmbId;

/* 取内嵌资源。取到则 *data 指向只读数据、*size 是字节数，返回 true。
 * 编译时没内嵌（普通构建）就返回 false。 */
bool emb_get(EmbId id, const unsigned char **data, int *size);

/* 是否有任何内嵌资源（启动日志用：区分"单文件版"与"读目录版"）。 */
bool emb_any(void);

#endif /* EMBEDDED_H */
