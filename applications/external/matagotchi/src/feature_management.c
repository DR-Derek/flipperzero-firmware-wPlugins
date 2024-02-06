#include <furi.h> // For FURI_LOG_D

#include "feature_management.h"
#include "settings_management.h"
#include "constants.h"
#include "random_generator.h"

/* EXPERIENCE */

void init_xp(struct GameState* game_state, uint32_t current_timestamp) {
    game_state->persistent.xp = 0;
    game_state->persistent.last_recorded_xp_update = current_timestamp;
}

void check_xp(
    const struct GameState* game_state,
    uint32_t current_timestamp,
    struct GameEvents* game_events) {
    uint32_t last_timestamp = game_state->persistent.last_recorded_xp_update;
    uint32_t nb_events = (current_timestamp - last_timestamp) / NEW_XP_FREQUENCY;

    FURI_LOG_D(
        LOG_TAG,
        "check_xp(): current_timestamp=%lu; last_timestamp=%lu; nb_events=%lu",
        current_timestamp,
        last_timestamp,
        nb_events);

    // If some events are extracted, the timestamp will be updated
    // even though there are no XP to add.
    game_events->xp_timestamp = (nb_events) ? current_timestamp : last_timestamp;

    while(nb_events-- > 0) {
        if(toss_a_coin(NEW_XP_PROBABILITY)) {
            game_events->xp++;
        }
    }

    if(game_events->xp) {
        FURI_LOG_I(LOG_TAG, "Gained %lu new XP!", game_events->xp);
    }
}

bool apply_xp(struct GameState* game_state, struct GameEvents game_events) {
    bool state_updated = false;

    if(game_events.xp_timestamp == 0) {
        // XP events not generated
        return false;
    }

    while(game_events.xp > 0 && game_state->persistent.stage != DEAD) {
        state_updated = true;
        uint32_t max_xp_this_stage = MAX_XP_PER_STAGE[game_state->persistent.stage];

        if(game_state->persistent.xp + game_events.xp >= max_xp_this_stage) {
            FURI_LOG_D(
                LOG_TAG,
                "apply_xp(): need to apply %lu new XP, can only %lu "
                "in current stage %u (%lu XP already present in current stage)",
                game_events.xp,
                max_xp_this_stage - game_state->persistent.xp,
                game_state->persistent.stage,
                game_state->persistent.xp);

            game_events.xp -= max_xp_this_stage - game_state->persistent.xp;
            game_state->persistent.xp = 0;
            game_state->persistent.stage++;
            FURI_LOG_I(LOG_TAG, "Evoluted to new stage %u!", game_state->persistent.stage);
            play_level_up(game_state);
            vibrate_long(game_state);
        } else {
            game_state->persistent.xp += game_events.xp;
            game_events.xp = 0;
            FURI_LOG_D(LOG_TAG, "apply_xp(): new total is %lu XP", game_state->persistent.xp);
        }
    }
    game_state->persistent.last_recorded_xp_update = game_events.xp_timestamp;
    return state_updated;
}

int get_text_xp(const struct GameState* game_state, char* str, size_t size) {
    return snprintf(
        str,
        size,
        "XP: %lu/%lu",
        game_state->persistent.xp,
        MAX_XP_PER_STAGE[game_state->persistent.stage]);
}
/* HUNGER */

void init_hu(struct GameState* game_state, uint32_t current_timestamp) {
    game_state->persistent.hu = MAX_HU;
    game_state->persistent.last_recorded_hu_update = current_timestamp;
}

void check_hu(
    const struct GameState* game_state,
    uint32_t current_timestamp,
    struct GameEvents* game_events) {
    uint32_t last_timestamp = game_state->persistent.last_recorded_hu_update;
    uint32_t nb_events = (current_timestamp - last_timestamp) / LOSE_HU_FREQUENCY;

    FURI_LOG_D(
        LOG_TAG,
        "check_hu(): current_timestamp=%lu; last_timestamp=%lu; nb_events=%lu",
        current_timestamp,
        last_timestamp,
        nb_events);

    // If some events are extracted, the timestamp will be updated
    // even though there are no HP to add.
    game_events->hu_timestamp = (nb_events) ? current_timestamp : last_timestamp;

    while(nb_events-- > 0) {
        if(toss_a_coin(LOSE_HU_PROBABILITY)) {
            game_events->hu -= random_uniform(LOSE_HU_MIN, LOSE_HU_MAX);
        }
    }

    if(game_events->hu) {
        FURI_LOG_I(LOG_TAG, "Lost %ld HU!", -(game_events->hu));
    }
}

bool apply_hu(struct GameState* game_state, struct GameEvents game_events) {
    int32_t hu = game_events.hu;

    if(game_events.hu_timestamp == 0) {
        // HU events not generated
        return false;
    }

    if(hu < 0) {
        uint32_t lost_hu = (uint32_t)-hu;
        // Lost some HU
        if(game_state->persistent.hu > lost_hu) {
            // There are still HU left
            game_state->persistent.hu -= lost_hu;
        } else {
            // Started to starve
            game_state->persistent.hu = 0;
            FURI_LOG_I(LOG_TAG, "The pet is hungry!");
            play_starvation(game_state);
            vibrate_long(game_state);
        }
    } else if(hu > 0) {
        // Ate some food
        if(game_state->persistent.hu + hu > MAX_HU) {
            // Gained more than max HU
            game_state->persistent.hu = MAX_HU;
        } else {
            game_state->persistent.hu += hu;
        }
    }

    game_state->persistent.last_recorded_hu_update = game_events.hu_timestamp;
    if(hu != 0) {
        FURI_LOG_D(LOG_TAG, "apply_hu(): new total is %lu HU", game_state->persistent.hu);
        return true;
    }
    return false;
}

int get_text_hu(const struct GameState* game_state, char* str, size_t size) {
    return snprintf(str, size, "HU: %lu/%d", game_state->persistent.hu, MAX_HU);
}

/* HEALTH */

void init_hp(struct GameState* game_state, uint32_t current_timestamp) {
    game_state->persistent.hp = MAX_HP;
    game_state->persistent.last_recorded_hp_update = current_timestamp;
}

void check_hp(
    const struct GameState* game_state,
    uint32_t current_timestamp,
    struct GameEvents* game_events) {
    uint32_t last_timestamp = game_state->persistent.last_recorded_hp_update;
    uint32_t nb_events = (current_timestamp - last_timestamp) / CHECK_HP_FREQUENCY;

    FURI_LOG_D(
        LOG_TAG,
        "check_hp(): current_timestamp=%lu; last_timestamp=%lu; nb_events=%lu",
        current_timestamp,
        last_timestamp,
        nb_events);

    // If some events are extracted, the timestamp will be updated
    // even though there are no HP to add.
    game_events->hp_timestamp = (nb_events) ? current_timestamp : last_timestamp;

    while(nb_events-- > 0) {
        // If the pet is hungry or if he got sick
        if(!game_state->persistent.hu || toss_a_coin(LOSE_HP_PROBABILITY)) {
            if(!game_state->persistent.hu) {
                FURI_LOG_I(LOG_TAG, "The pet is losing HP for starvation!");
            } else {
                FURI_LOG_I(LOG_TAG, "The pet is losing HP for an illness!");
                play_ambulance(game_state);
                vibrate_long(game_state);
            }
            game_events->hp -= random_uniform(LOSE_HP_MIN, LOSE_HP_MAX);
        }
    }

    if(game_events->hp) {
        FURI_LOG_I(LOG_TAG, "Lost %ld HP!", -(game_events->hp));
    }
}

bool apply_hp(struct GameState* game_state, struct GameEvents game_events) {
    int32_t hp = game_events.hp;

    if(game_events.hp_timestamp == 0) {
        // HP events not generated
        return false;
    }

    if(hp < 0) {
        uint32_t lost_hp = (uint32_t)-hp;
        // Lost some HP
        if(game_state->persistent.hp > lost_hp) {
            // There are still HP left
            game_state->persistent.hp -= lost_hp;
        } else {
            // Dead
            game_state->persistent.hp = 0;
            game_state->persistent.stage = DEAD;
            FURI_LOG_I(LOG_TAG, "The pet is dead!");
        }
    } else if(hp > 0) {
        // Gained some HP
        if(game_state->persistent.hp + hp > MAX_HP) {
            // Gained more than max HP
            game_state->persistent.hp = MAX_HP;
        } else {
            game_state->persistent.hp += hp;
        }
    }

    game_state->persistent.last_recorded_hp_update = game_events.hp_timestamp;
    if(hp != 0) {
        FURI_LOG_D(LOG_TAG, "apply_hp(): new total is %lu HP", game_state->persistent.hp);
        return true;
    }
    return false;
}

int get_text_hp(const struct GameState* game_state, char* str, size_t size) {
    return snprintf(str, size, "HP: %lu/%d", game_state->persistent.hp, MAX_HP);
}

/* Other functions */

void correct_state(struct GameState* game_state) {
    if(game_state->persistent.stage == DEAD) {
        game_state->persistent.xp = 0;
        game_state->persistent.hu = 0;
        game_state->persistent.hp = 0;
    }
}

void generate_hu(
    struct GameState* game_state,
    uint32_t current_timestamp,
    struct GameEvents* game_events) {
    if(game_state->persistent.stage != DEAD && game_state->persistent.hu < MAX_HU) {
        game_events->hu = random_uniform(MIN_CANDY_HU_RESTORE, MAX_CANDY_HU_RESTORE);
        game_events->hu_timestamp = current_timestamp;
    }
}

void generate_hp(
    struct GameState* game_state,
    uint32_t current_timestamp,
    struct GameEvents* game_events) {
    if(game_state->persistent.stage != DEAD && game_state->persistent.hp < MAX_HP) {
        game_events->hp = random_uniform(MIN_PILL_HP_RESTORE, MAX_PILL_HP_RESTORE);
        game_events->hp_timestamp = current_timestamp;
    }
}
