/* imcrop —— 质检用小工具：从 PNG 裁剪一块区域并可放大倍数导出，用于 1:1 检查 4K 文字锐度。
   用法: imcrop in.png out.png x y w h [scale]   （坐标按输入图真实像素） */
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc < 7) {
        printf("usage: imcrop in.png out.png x y w h [scale]\n");
        return 1;
    }
    Image img = LoadImage(argv[1]);
    if (img.data == NULL) { printf("load failed\n"); return 1; }

    int x = atoi(argv[3]), y = atoi(argv[4]);
    int w = atoi(argv[5]), h = atoi(argv[6]);
    float s = (argc > 7) ? (float)atof(argv[7]) : 1.0f;

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + w > img.width)  w = img.width - x;
    if (y + h > img.height) h = img.height - y;

    ImageCrop(&img, (Rectangle){ (float)x, (float)y, (float)w, (float)h });
    if (s > 1.01f) {
        ImageResizeNN(&img, (int)(w * s), (int)(h * s));
    }
    ExportImage(img, argv[2]);
    printf("%s -> %s  %dx%d (scale %.2f) from %d,%d\n", argv[1], argv[2], img.width, img.height, s, x, y);
    UnloadImage(img);
    return 0;
}
