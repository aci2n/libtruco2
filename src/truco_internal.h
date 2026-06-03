#ifndef TRUCO_INTERNAL_H
#define TRUCO_INTERNAL_H

#include "truco.h"

typedef struct truco_card_internal {
    unsigned char suit;
    unsigned char rank;
} truco_card_internal;

typedef struct truco_game_settings {
    unsigned int player_count;
    unsigned int target_score;
    unsigned int seed;
    unsigned int initial_dealer;
    unsigned char team_for_player[TRUCO_MAX_PLAYERS];
    unsigned int flags;
} truco_game_settings;

struct truco_game {
    truco_game_settings settings;
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
    truco_card_internal hands[TRUCO_MAX_PLAYERS][TRUCO_HAND_CARDS];
    unsigned char played_slots[TRUCO_MAX_PLAYERS][TRUCO_HAND_CARDS];
    truco_card_internal trick_cards[TRUCO_HAND_CARDS][TRUCO_MAX_PLAYERS];
    unsigned char trick_played[TRUCO_HAND_CARDS][TRUCO_MAX_PLAYERS];
    int trick_winner_team[TRUCO_HAND_CARDS];
    int trick_winner_player[TRUCO_HAND_CARDS];
};

static inline truco_card_internal truco_card_internal_make(truco_suit suit,
                                                           unsigned int rank)
{
    truco_card_internal card;

    card.suit = (unsigned char)suit;
    card.rank = (unsigned char)rank;
    return card;
}

static inline int truco_card_internal_compare(truco_card_internal left,
                                              truco_card_internal right)
{
    int left_power = truco_card_power((truco_suit)left.suit, left.rank);
    int right_power = truco_card_power((truco_suit)right.suit, right.rank);

    if (left_power > right_power) {
        return 1;
    }
    if (left_power < right_power) {
        return -1;
    }
    return 0;
}

#endif
