#ifndef TRUCO_H
#define TRUCO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TRUCO_VERSION_MAJOR 0
#define TRUCO_VERSION_MINOR 2
#define TRUCO_VERSION_PATCH 0

#define TRUCO_DECK_SIZE 40u
#define TRUCO_HAND_CARDS 3u
#define TRUCO_MAX_LEGAL_COMMANDS 12u
#define TRUCO_MAX_PLAYERS 6u
#define TRUCO_MAX_TEAMS 2u

#define TRUCO_CONFIG_STORAGE_BYTES 32u
#define TRUCO_GAME_STORAGE_BYTES 256u

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
    unsigned char storage[TRUCO_CONFIG_STORAGE_BYTES];
} truco_config;

typedef struct truco_game {
    unsigned char storage[TRUCO_GAME_STORAGE_BYTES];
} truco_game;

void truco_config_default(truco_config *config, unsigned int player_count);
truco_status truco_config_set_seed(truco_config *config, unsigned int seed);
truco_status truco_config_set_initial_dealer(truco_config *config,
                                             unsigned int initial_dealer);
truco_status truco_config_set_target_score(truco_config *config,
                                           unsigned int target_score);
truco_status truco_config_set_team_for_player(truco_config *config,
                                              unsigned int player,
                                              unsigned int team);
unsigned int truco_config_player_count(const truco_config *config);

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
unsigned int truco_game_dealer(const truco_game *game);
unsigned int truco_game_mano(const truco_game *game);
unsigned int truco_game_score(const truco_game *game, unsigned int team);
unsigned int truco_game_target_score(const truco_game *game);
unsigned int truco_game_truco_value(const truco_game *game);
int truco_game_hand_winner(const truco_game *game);
int truco_game_trick_winner(const truco_game *game, unsigned int trick);

truco_status truco_game_hand_card(const truco_game *game,
                                  unsigned int player,
                                  unsigned int slot,
                                  truco_card *out);
int truco_game_hand_slot_played(const truco_game *game,
                                unsigned int player,
                                unsigned int slot);

truco_status truco_game_set_hand(truco_game *game,
                                 unsigned int player,
                                 const truco_card cards[TRUCO_HAND_CARDS]);

#ifdef __cplusplus
}
#endif

#endif
