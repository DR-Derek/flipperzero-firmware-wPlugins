#include <furi.h> // For FURI_LOG_D
#include <furi_hal.h> // For RTC handling

#include "state_management.h"
#include "constants.h"
#include "feature_management.h"
#include "settings_management.h"
#include "save_restore.h"
#include "game_structs.h"

static uint32_t get_current_timestamp() {
    FuriHalRtcDateTime current_time;
    furi_hal_rtc_get_datetime(&current_time);
    return furi_hal_rtc_datetime_to_timestamp(&current_time);
}

static void fast_forward_state(struct GameState* game_state) {
    MASK_VIBRO_SOUND(game_state)
    struct GameEvents events = {0};
    generate_new_random_events(game_state, &events);
    process_events(game_state, events);
    UNMASK_VIBRO_SOUND(game_state)
}

static void init_persistent_state_object(struct GameState* game_state) {
    // Init the struct with default values
    uint32_t current_timestamp = get_current_timestamp();
    game_state->persistent.stage = EGG;

    // Init every individual feature
    init_xp(game_state, current_timestamp);
    init_hu(game_state, current_timestamp);
    init_hp(game_state, current_timestamp);
}

void init_state(struct GameState* game_state) {
    // Try to load the state from the storage
    if(!load_state_from_file(&game_state->persistent)) {
        init_persistent_state_object(game_state);
    } else {
        // State loaded from file. Actualize it up to
        // the current timestamp.
        FURI_LOG_D(LOG_TAG, "Fast forwarding persisted state to current time");
        fast_forward_state(game_state);
    }
    game_state->next_animation_index = 0;
}

void persist_state(struct GameState* game_state) {
    bool result = save_state_to_file(&game_state->persistent);
    if(!result) {
        furi_crash("Unable to save state to storage");
    }
}

void reset_state(struct GameState* game_state) {
    init_persistent_state_object(game_state);
}

static void _generate_new_random_event(
    uint32_t timestamp,
    struct GameState* game_state,
    struct GameEvents* game_events) {
    if(game_state->persistent.stage == DEAD) {
        FURI_LOG_D(LOG_TAG, "Received generate request, but stage is DEAD");
        // Can't do much
        return;
    }
    // Check every individual feature
    check_xp(game_state, timestamp, game_events);
    check_hu(game_state, timestamp, game_events);
    check_hp(game_state, timestamp, game_events);
}

void generate_new_random_events(struct GameState* game_state, struct GameEvents* game_events) {
    uint32_t current_timestamp = get_current_timestamp();
    _generate_new_random_event(current_timestamp, game_state, game_events);
    return;
}

bool process_events(struct GameState* game_state, struct GameEvents game_events) {
    bool new_events = false;

    // Process every individual feature
    new_events |= apply_xp(game_state, game_events);
    new_events |= apply_hu(game_state, game_events);
    new_events |= apply_hp(game_state, game_events);

    if(new_events) {
        correct_state(game_state);
    }

    return new_events;
}

void get_state_str(const struct GameState* game_state, char* str, size_t size) {
    size_t copied = 0;
    copied = snprintf(str, size, "Stage: %s\n", LIFE_STAGE_STRING[game_state->persistent.stage]);

    // Append every individual feature
    str += copied;
    size -= copied;
    copied = get_text_xp(game_state, str, size);
    str += copied;
    size -= copied;
    copied = snprintf(str, size, "\n");
    str += copied;
    size -= copied;
    copied = get_text_hu(game_state, str, size);
    str += copied;
    size -= copied;
    copied = snprintf(str, size, "\n");
    str += copied;
    size -= copied;
    copied = get_text_hp(game_state, str, size);
}

void give_candy(struct GameState* game_state, struct GameEvents* game_events) {
    uint32_t current_timestamp = get_current_timestamp();
    generate_hu(game_state, current_timestamp, game_events);
}

void give_pill(struct GameState* game_state, struct GameEvents* game_events) {
    uint32_t current_timestamp = get_current_timestamp();
    generate_hp(game_state, current_timestamp, game_events);
}
