#ifndef TRUCO_INTERNAL_H
#define TRUCO_INTERNAL_H

#include "truco.h"

typedef struct truco_config_impl {
    unsigned int player_count;
    unsigned int target_score;
    unsigned int seed;
    unsigned int initial_dealer;
    unsigned char team_for_player[TRUCO_MAX_PLAYERS];
    unsigned int flags;
} truco_config_impl;

typedef struct truco_game_impl {
    truco_config_impl config;
    unsigned int rng_state;
    unsigned int phase;
    unsigned int dealer;
    unsigned int mano;
    unsigned int current_player;
    unsigned int trick_leader;
    unsigned int current_trick;
    unsigned int truco_value;
    unsigned int pending_truco_value;
    int pending_truco_team;
    int last_truco_team;
    int envido_pending_team;
    unsigned int envido_pending_points;
    int envido_resolved;
    int last_hand_winner;
    unsigned int score[TRUCO_MAX_TEAMS];
    truco_card hands[TRUCO_MAX_PLAYERS][TRUCO_HAND_CARDS];
    unsigned char played_slots[TRUCO_MAX_PLAYERS][TRUCO_HAND_CARDS];
    truco_card trick_cards[TRUCO_HAND_CARDS][TRUCO_MAX_PLAYERS];
    unsigned char trick_played[TRUCO_HAND_CARDS][TRUCO_MAX_PLAYERS];
    int trick_winner_team[TRUCO_HAND_CARDS];
    int trick_winner_player[TRUCO_HAND_CARDS];
} truco_game_impl;

typedef char truco_config_storage_ok
    [(sizeof(truco_config_impl) <= TRUCO_CONFIG_STORAGE_BYTES) ? 1 : -1];
typedef char truco_game_storage_ok
    [(sizeof(truco_game_impl) <= TRUCO_GAME_STORAGE_BYTES) ? 1 : -1];

static inline truco_config_impl *truco_config_unwrap(truco_config *config)
{
    return (truco_config_impl *)(void *)config;
}

static inline const truco_config_impl *truco_config_unwrap_const(const truco_config *config)
{
    return (const truco_config_impl *)(const void *)config;
}

static inline truco_game_impl *truco_game_unwrap(truco_game *game)
{
    return (truco_game_impl *)(void *)game;
}

static inline const truco_game_impl *truco_game_unwrap_const(const truco_game *game)
{
    return (const truco_game_impl *)(const void *)game;
}

#endif
