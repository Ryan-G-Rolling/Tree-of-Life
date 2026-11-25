#include "game_types.h"
#include "game.h"
#include "economy.h"
#include "save.h"
#include "evolution.h"
#include "environment.h"
#include "legacy.h"

int main(void)
{
    GameState state = {0};
    return state.phase != PHASE_RUNNING;
}
