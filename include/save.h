#ifndef SAVE_H
#define SAVE_H

#include "game_types.h"

typedef enum { SAVE_OK, SAVE_FILE_ERROR, SAVE_BAD_VERSION, SAVE_INVALID_DATA } SaveResult;
SaveResult save_write(const char *path, const GameState *game);
SaveResult save_read(const char *path, GameState *out_game);

#endif
