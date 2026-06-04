#ifndef TRUCO_HAND_INTERNAL_H
#define TRUCO_HAND_INTERNAL_H

#include "truco.h"
#include "truco_internal.h"

#include <stddef.h>

#define TRUCO_TRICK_UNPLAYED (-2)
#define TRUCO_TRICK_PARDA (-1)
#define TRUCO_HAND_UNDECIDED (-2)
#define TRUCO_NO_TEAM (-1)
#define TRUCO_DEFAULT_TARGET_SCORE 30u
#define TRUCO_FALTA_ENVIDO_FLOOR 15u
#define TRUCO_TRUCO_MAX_VALUE 4u

typedef struct truco_hand {
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
    int flor_blocks_envido;
    int flor_resolved;
    int flor_pending_team;
    unsigned int flor_pending_points;
    truco_card hands[TRUCO_MAX_PLAYERS][TRUCO_HAND_CARDS];
    unsigned char played_slots[TRUCO_MAX_PLAYERS][TRUCO_HAND_CARDS];
    truco_card trick_cards[TRUCO_HAND_CARDS][TRUCO_MAX_PLAYERS];
    unsigned char trick_played[TRUCO_HAND_CARDS][TRUCO_MAX_PLAYERS];
    int trick_winner_team[TRUCO_HAND_CARDS];
    int trick_winner_player[TRUCO_HAND_CARDS];
} truco_hand;

typedef struct truco_event_log {
    truco_event events[TRUCO_MAX_EVENTS_PER_APPLY];
    size_t count;
} truco_event_log;

struct truco_game {
    unsigned int player_count;
    unsigned int target_score;
    unsigned int seed;
    unsigned int initial_dealer;
    unsigned char team_for_player[TRUCO_MAX_PLAYERS];
    unsigned int rng_state;
    unsigned char flor_enabled;
    truco_phase phase;
    unsigned int dealer;
    int last_hand_winner;
    unsigned int score[TRUCO_MAX_TEAMS];
    truco_hand hand;
};

void truco_event_log_init(truco_event_log *log);
void truco_event_log_copy_to_result(const truco_event_log *log,
                                    truco_apply_result *result);
void truco_event_log_emit_hand_started(truco_event_log *log, unsigned int mano);
void truco_event_log_emit_card_played(truco_event_log *log,
                                      unsigned int player,
                                      unsigned int trick,
                                      unsigned char slot,
                                      truco_card card);
void truco_event_log_emit_trick_won(truco_event_log *log,
                                    unsigned int trick,
                                    unsigned int team,
                                    unsigned int player);
void truco_event_log_emit_hand_finished(truco_event_log *log, unsigned int team);
void truco_event_log_emit_score(truco_event_log *log,
                                unsigned int team,
                                unsigned int amount);
void truco_event_log_emit_game_over(truco_event_log *log);
void truco_event_log_emit_simple(truco_event_log *log,
                                 truco_event_kind kind,
                                 unsigned int player,
                                 unsigned int team,
                                 unsigned int amount);

void truco_hand_clear(truco_hand *hand);
truco_hand_subphase truco_hand_subphase_of(const truco_game *game);

truco_status truco_hand_apply(truco_game *game,
                              unsigned int player,
                              truco_command command,
                              truco_event_log *log);
truco_status truco_hand_legal_commands(const truco_game *game,
                                       unsigned int player,
                                       truco_legal_commands *out);

#endif
