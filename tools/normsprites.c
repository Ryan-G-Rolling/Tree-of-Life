/* normsprites.c —— 把透明底生物插画统一成"主体居中、四周留边"的正方精灵。
 *
 * 为什么需要：AI 生成的 1536 方图里，主体常常顶到画布边缘（菌丝、蜂巢、
 * 神经网最明显）。直接拿去当精灵用，图鉴里会看到被裁掉的方块，展台上还会
 * 盖住旁边物种的名字。这里按 alpha 包围盒裁紧，再等比缩放到 canvas*fill
 * 以内居中，四周补透明。
 *
 * 采样是直通 alpha 的双线性（逐通道独立插值，不做预乘），所以半透明的
 * 膜、触手、菌丝不会被压成黑边。
 *
 * 用法：normsprites <fill 0..1> <canvas> <png...>
 */
#include "raylib.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const Image *g_src;

static void sample(double fx, double fy, unsigned char out[4])
{
    const Image *s = g_src;
    if (fx < 0) fx = 0;
    if (fy < 0) fy = 0;
    if (fx > s->width - 1) fx = s->width - 1;
    if (fy > s->height - 1) fy = s->height - 1;

    int x0 = (int)floor(fx), y0 = (int)floor(fy);
    int x1 = x0 + 1 < s->width ? x0 + 1 : x0;
    int y1 = y0 + 1 < s->height ? y0 + 1 : y0;
    double tx = fx - x0, ty = fy - y0;

    for (int c = 0; c < 4; ++c) {
        double v00 = ((unsigned char *)s->data)[(y0 * s->width + x0) * 4 + c];
        double v10 = ((unsigned char *)s->data)[(y0 * s->width + x1) * 4 + c];
        double v01 = ((unsigned char *)s->data)[(y1 * s->width + x0) * 4 + c];
        double v11 = ((unsigned char *)s->data)[(y1 * s->width + x1) * 4 + c];
        double v = (v00 * (1 - tx) + v10 * tx) * (1 - ty) + (v01 * (1 - tx) + v11 * tx) * ty;
        int iv = (int)lround(v);
        out[c] = (unsigned char)(iv < 0 ? 0 : (iv > 255 ? 255 : iv));
    }
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: normsprites <fill> <canvas> <png...>\n");
        return 2;
    }

    double fill = atof(argv[1]);
    int canvas = atoi(argv[2]);
    if (fill <= 0.0 || fill > 1.0) fill = 0.86;
    if (canvas < 64 || canvas > 4096) canvas = 1536;

    int n = 0;
    for (int i = 3; i < argc; ++i) {
        Image src = LoadImage(argv[i]);
        if (!IsImageValid(src) || src.format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) {
            if (IsImageValid(src)) ImageFormat(&src, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
            else {
                fprintf(stderr, "skip (invalid): %s\n", argv[i]);
                continue;
            }
        }

        int minX = src.width, minY = src.height, maxX = -1, maxY = -1;
        for (int y = 0; y < src.height; ++y) {
            for (int x = 0; x < src.width; ++x) {
                if (((unsigned char *)src.data)[(y * src.width + x) * 4 + 3] > 10) {
                    if (x < minX) minX = x;
                    if (x > maxX) maxX = x;
                    if (y < minY) minY = y;
                    if (y > maxY) maxY = y;
                }
            }
        }
        if (maxX < 0) {
            fprintf(stderr, "skip (empty): %s\n", argv[i]);
            UnloadImage(src);
            continue;
        }

        int bw = maxX - minX + 1;
        int bh = maxY - minY + 1;
        double scale = fmin((canvas * fill) / (double)bw, (canvas * fill) / (double)bh);
        int dw = (int)lround(bw * scale);
        int dh = (int)lround(bh * scale);
        int ox = (canvas - dw) / 2;
        int oy = (canvas - dh) / 2;

        Image out = GenImageColor(canvas, canvas, BLANK);
        g_src = &src;
        for (int y = 0; y < canvas; ++y) {
            for (int x = 0; x < canvas; ++x) {
                unsigned char *px = &((unsigned char *)out.data)[(y * canvas + x) * 4];
                if (x < ox || y < oy || x >= ox + dw || y >= oy + dh) {
                    px[0] = px[1] = px[2] = px[3] = 0;
                    continue;
                }
                double fx = (x - ox + 0.5) / scale + minX - 0.5;
                double fy = (y - oy + 0.5) / scale + minY - 0.5;
                sample(fx, fy, px);
            }
        }

        ExportImage(out, argv[i]);
        printf("%-14s bbox %dx%d -> %dx%d\n", GetFileName(argv[i]), bw, bh, dw, dh);

        UnloadImage(out);
        UnloadImage(src);
        ++n;
    }

    printf("normalized %d file(s)\n", n);
    return 0;
}
