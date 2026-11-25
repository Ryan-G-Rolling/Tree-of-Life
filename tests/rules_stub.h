#ifndef RULES_STUB_H
#define RULES_STUB_H

#include "game_types.h"

/* Test instrumentation only; never include in a production build. */
typedef struct {
    bool override_rates;
    SpeciesRates rates[MAX_ACTIVE_SPECIES];
    bool override_environment;
    EnvironmentStep environment;
    bool override_reward;
    EraReward reward;
    int rate_calls;
    double observed_atp[MAX_ACTIVE_SPECIES];
    double observed_population[MAX_ACTIVE_SPECIES];
    double environment_atp;
    double environment_population;
} RulesStubControl;

extern RulesStubControl rules_stub;
void rules_stub_reset(void);

#endif
