/*
 * crop.c  ——  截图局部放大工具（开发自检用，不参与游戏构建）
 *
 *   gcc tools/crop.c -o crop.exe -lraylib -lopengl32 -lgdi32 -lwinmm
 *   ./crop.exe shot_01_main.png out.png 900 90 380 220 2
 */
#include <stdio.h>
#include <stdlib.h>
#include "raylib.h"

int main(int argc, char **argv)
{
    if (argc < 8) {
        printf("usage: crop <in> <out> <x> <y> <w> <h> <scale>\n");
        return 1;
    }
    Image img = LoadImage(argv[1]);
    if (img.data == NULL) {
        printf("load failed: %s\n", argv[1]);
        return 1;
    }
    int x = atoi(argv[3]), y = atoi(argv[4]);
    int w = atoi(argv[5]), h = atoi(argv[6]);
    float scale = (float)atof(argv[7]);

    ImageCrop(&img, (Rectangle){ (float)x, (float)y, (float)w, (float)h });
    if (scale != 1.0f) ImageResizeNN(&img, (int)(w * scale), (int)(h * scale));
    ExportImage(img, argv[2]);
    UnloadImage(img);
    printf("ok %s\n", argv[2]);
    return 0;
}
