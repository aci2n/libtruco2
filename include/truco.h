#ifndef TRUCO_H
#define TRUCO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TRUCO_VERSION_MAJOR 0
#define TRUCO_VERSION_MINOR 1
#define TRUCO_VERSION_PATCH 0

#define TRUCO_DECK_SIZE 40u
#define TRUCO_HAND_CARDS 3u
#define TRUCO_MAX_LEGAL_COMMANDS 12u
#define TRUCO_MAX_PLAYERS 6u
#define TRUCO_MAX_TEAMS 2u

typedef enum truco_status {
    TRUCO_OK = 0,
    TRUCO_ERR_INVALID_ARGUMENT = -1,
    TRUCO_ERR_INVALID_STATE = -2,
    TRUCO_ERR_NOT_PLAYERS_TURN = -3,
    TRUCO_ERR_CARD_ALREADY_PLAYED = -4,
    TRUCO_ERR_UNSUPPORTED_RULES = -5
} truco_status;

typedef enum truco_suit {
    TRUCO_SUIT_ESPADA = 0,
    TRUCO_SUIT_BASTO = 1,
    TRUCO_SUIT_ORO = 2,
    TRUCO_SUIT_COPA = 3
} truco_suit;

typedef enum truco_phase {
    TRUCO_PHASE_READY = 0,
    TRUCO_PHASE_PLAYING = 1,
    TRUCO_PHASE_HAND_OVER = 2,
    TRUCO_PHASE_GAME_OVER = 3
} truco_phase;

typedef enum truco_command {
    TRUCO_CMD_NONE = 0,
    TRUCO_CMD_START_HAND,
    TRUCO_CMD_PLAY_CARD_0,
    TRUCO_CMD_PLAY_CARD_1,
    TRUCO_CMD_PLAY_CARD_2,
    TRUCO_CMD_RAISE_TRUCO,
    TRUCO_CMD_CALL_ENVIDO,
    TRUCO_CMD_CALL_REAL_ENVIDO,
    TRUCO_CMD_CALL_FALTA_ENVIDO,
    TRUCO_CMD_ACCEPT_BID,
    TRUCO_CMD_REJECT_BID
} truco_command;

typedef struct truco_card {
    unsigned char suit;
    unsigned char rank;
} truco_card;

typedef struct truco_legal_actions {
    unsigned int count;
    truco_command commands[TRUCO_MAX_LEGAL_COMMANDS];
    unsigned int truco_value;
    unsigned int envido_points;
} truco_legal_actions;

typedef struct truco_config {
    unsigned int player_count;
    unsigned int target_score;
    unsigned int seed;
    unsigned int initial_dealer;
    unsigned char team_for_player[TRUCO_MAX_PLAYERS];
    unsigned int flags;
} truco_config;

typedef struct truco_game {
    truco_config config;
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
} truco_game;

void truco_config_default(truco_config *config, unsigned int player_count);
truco_status truco_game_init(truco_game *game, const truco_config *config);

truco_card truco_make_card(truco_suit suit, unsigned int rank);
int truco_card_is_valid(truco_card card);
int truco_card_power(truco_card card);
int truco_card_compare(truco_card left, truco_card right);
unsigned int truco_envido_points(const truco_card cards[TRUCO_HAND_CARDS]);

truco_status truco_deck(truco_card *cards, size_t count);
truco_status truco_shuffle(truco_card *cards, size_t count, unsigned int *seed);

truco_status truco_game_apply(truco_game *game,
                              unsigned int player,
                              truco_command command);
truco_status truco_game_legal_actions(const truco_game *game,
                                      unsigned int player,
                                      truco_legal_actions *actions);

unsigned int truco_game_player_count(const truco_game *game);
unsigned int truco_game_team_for_player(const truco_game *game,
                                        unsigned int player);
truco_phase truco_game_phase(const truco_game *game);
unsigned int truco_game_current_player(const truco_game *game);
unsigned int truco_game_score(const truco_game *game, unsigned int team);
int truco_game_hand_winner(const truco_game *game);
int truco_game_trick_winner(const truco_game *game, unsigned int trick);

#ifdef __cplusplus
}
#endif

#endif
