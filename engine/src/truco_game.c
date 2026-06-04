#include "truco.h"
#include "truco_hand_internal.h"

#include <stdlib.h>
#include <string.h>

static void apply_default_teams(truco_game *game)
{
    unsigned int player;

    for (player = 0u; player < TRUCO_MAX_PLAYERS; ++player) {
        game->team_for_player[player] = (unsigned char)(player % TRUCO_MAX_TEAMS);
    }
}

size_t truco_game_size(void)
{
    return sizeof(truco_game);
}

truco_game *truco_game_create(void)
{
    truco_game *game = (truco_game *)calloc(1, truco_game_size());

    if (game == 0) {
        return 0;
    }

    truco_game_init(game);
    return game;
}

truco_status truco_game_init(truco_game *game)
{
    if (game == 0) {
        return TRUCO_ERR_INVALID_ARGUMENT;
    }

    memset(game, 0, sizeof(*game));
    game->player_count = 2u;
    game->target_score = TRUCO_DEFAULT_TARGET_SCORE;
    game->seed = 1u;
    game->initial_dealer = 1u;
    apply_default_teams(game);
    game->rng_state = game->seed;
    game->dealer = game->initial_dealer % game->player_count;
    game->phase = TRUCO_PHASE_READY;
    game->last_hand_winner = TRUCO_NO_TEAM;
    truco_hand_clear(&game->hand);

    return TRUCO_OK;
}

void truco_game_destroy(truco_game *game)
{
    if (game == 0) {
        return;
    }
    free(game);
}

void truco_game_delete(truco_game **game)
{
    if (game == 0) {
        return;
    }

    truco_game_destroy(*game);
    *game = 0;
}

truco_status truco_game_set_player_count(truco_game *game,
                                         unsigned int player_count)
{
    if (game == 0) {
        return TRUCO_ERR_INVALID_ARGUMENT;
    }

    if (player_count > TRUCO_MAX_PLAYERS || player_count < 2u) {
        return TRUCO_ERR_INVALID_ARGUMENT;
    }

    game->player_count = player_count;
    if (game->target_score == 0u) {
        game->target_score = TRUCO_DEFAULT_TARGET_SCORE;
    }
    if (game->seed == 0u) {
        game->seed = 1u;
    }
    game->initial_dealer = player_count - 1u;
    if (game->phase == TRUCO_PHASE_READY) {
        game->dealer = game->initial_dealer % player_count;
    }
    apply_default_teams(game);

    return TRUCO_OK;
}

truco_status truco_game_set_target_score(truco_game *game,
                                         unsigned int target_score)
{
    if (game == 0) {
        return TRUCO_ERR_INVALID_ARGUMENT;
    }

    game->target_score = target_score;
    return TRUCO_OK;
}

truco_status truco_game_set_seed(truco_game *game, unsigned int seed)
{
    if (game == 0) {
        return TRUCO_ERR_INVALID_ARGUMENT;
    }

    game->seed = seed == 0u ? 1u : seed;
    game->rng_state = game->seed;
    return TRUCO_OK;
}

truco_status truco_game_set_initial_dealer(truco_game *game,
                                           unsigned int initial_dealer)
{
    if (game == 0) {
        return TRUCO_ERR_INVALID_ARGUMENT;
    }

    game->initial_dealer = initial_dealer;
    if (game->phase == TRUCO_PHASE_READY) {
        game->dealer = initial_dealer % game->player_count;
    }
    return TRUCO_OK;
}

truco_status truco_game_set_team_for_player(truco_game *game,
                                            unsigned int player,
                                            unsigned int team)
{
    if (game == 0 || player >= TRUCO_MAX_PLAYERS || team >= TRUCO_MAX_TEAMS) {
        return TRUCO_ERR_INVALID_ARGUMENT;
    }

    game->team_for_player[player] = (unsigned char)team;
    return TRUCO_OK;
}

truco_status truco_game_set_flor_enabled(truco_game *game, int enabled)
{
    if (game == 0) {
        return TRUCO_ERR_INVALID_ARGUMENT;
    }

    game->flor_enabled = enabled ? 1u : 0u;
    return TRUCO_OK;
}

truco_status truco_game_apply(truco_game *game,
                              unsigned int player,
                              truco_command command)
{
    if (game == 0) {
        return TRUCO_ERR_INVALID_ARGUMENT;
    }

    return truco_hand_apply(game, player, command);
}

truco_status truco_game_legal_commands(const truco_game *game,
                                       unsigned int player,
                                       truco_legal_commands *out)
{
    return truco_hand_legal_commands(game, player, out);
}

unsigned int truco_game_pending_truco_value(const truco_game *game)
{
    if (game == 0 || game->hand.pending_truco_value == 0u) {
        return 0u;
    }
    return game->hand.pending_truco_value;
}

unsigned int truco_game_pending_envido_points(const truco_game *game)
{
    if (game == 0 || game->hand.envido_pending_team < 0) {
        return 0u;
    }
    return game->hand.envido_pending_points;
}

unsigned int truco_game_pending_flor_points(const truco_game *game)
{
    if (game == 0 || game->hand.flor_pending_team < 0) {
        return 0u;
    }
    return game->hand.flor_pending_points;
}

unsigned int truco_game_next_truco_value(const truco_game *game)
{
    unsigned int value;

    if (game == 0) {
        return 0u;
    }

    value = game->hand.truco_value + 1u;
    return value < 2u ? 2u : value;
}

unsigned int truco_game_player_count(const truco_game *game)
{
    if (game == 0) {
        return 0u;
    }
    return game->player_count;
}

unsigned int truco_game_team_for_player(const truco_game *game,
                                        unsigned int player)
{
    if (game == 0 || player >= game->player_count) {
        return TRUCO_MAX_TEAMS;
    }
    return game->team_for_player[player];
}

truco_phase truco_game_phase(const truco_game *game)
{
    if (game == 0) {
        return TRUCO_PHASE_READY;
    }
    return game->phase;
}

truco_hand_subphase truco_game_hand_subphase(const truco_game *game)
{
    return truco_hand_subphase_of(game);
}

truco_pending_bid truco_game_pending_bid(const truco_game *game)
{
    return truco_hand_pending_bid_of(game);
}

unsigned int truco_game_current_player(const truco_game *game)
{
    if (game == 0) {
        return 0u;
    }
    return game->hand.current_player;
}

int truco_game_flor_enabled(const truco_game *game)
{
    if (game == 0) {
        return 0;
    }
    return game->flor_enabled != 0u;
}

int truco_game_player_has_flor(const truco_game *game, unsigned int player)
{
    if (game == 0 || player >= game->player_count) {
        return 0;
    }
    return truco_has_flor(game->hand.hands[player]);
}

unsigned int truco_game_score(const truco_game *game, unsigned int team)
{
    if (game == 0 || team >= TRUCO_MAX_TEAMS) {
        return 0u;
    }
    return game->score[team];
}

int truco_game_hand_winner(const truco_game *game)
{
    if (game == 0 || (game->phase != TRUCO_PHASE_HAND_OVER &&
                      game->phase != TRUCO_PHASE_GAME_OVER)) {
        return TRUCO_NO_TEAM;
    }

    return game->last_hand_winner;
}

int truco_game_trick_winner(const truco_game *game, unsigned int trick)
{
    if (game == 0 || trick >= TRUCO_HAND_CARDS) {
        return TRUCO_TRICK_UNPLAYED;
    }
    return game->hand.trick_winner_team[trick];
}

truco_status truco_game_hand_card(const truco_game *game, unsigned int player,
                                  unsigned int slot, truco_card *card_out)
{
    if (game == 0 || card_out == 0 || player >= game->player_count ||
        slot >= TRUCO_HAND_CARDS) {
        return TRUCO_ERR_INVALID_ARGUMENT;
    }

    *card_out = game->hand.hands[player][slot];
    return TRUCO_OK;
}

truco_status truco_game_trick_card(const truco_game *game, unsigned int trick,
                                   unsigned int player, truco_card *card_out)
{
    if (game == 0 || card_out == 0 || trick >= TRUCO_HAND_CARDS ||
        player >= game->player_count) {
        return TRUCO_ERR_INVALID_ARGUMENT;
    }
    if (!game->hand.trick_played[trick][player]) {
        return TRUCO_ERR_INVALID_STATE;
    }

    *card_out = game->hand.trick_cards[trick][player];
    return TRUCO_OK;
}

unsigned int truco_game_hand_envido(const truco_game *game,
                                    unsigned int player)
{
    if (game == 0 || player >= game->player_count) {
        return 0u;
    }

    return truco_envido_points(game->hand.hands[player]);
}

unsigned int truco_game_hand_flor(const truco_game *game,
                                  unsigned int player)
{
    if (game == 0 || player >= game->player_count) {
        return 0u;
    }

    return truco_flor_points(game->hand.hands[player]);
}

truco_status truco_game_set_hand(truco_game *game, unsigned int player,
                                 const truco_card cards[TRUCO_HAND_CARDS])
{
    unsigned int slot;

    if (game == 0 || cards == 0 || player >= game->player_count) {
        return TRUCO_ERR_INVALID_ARGUMENT;
    }

    for (slot = 0u; slot < TRUCO_HAND_CARDS; ++slot) {
        game->hand.hands[player][slot] = cards[slot];
        game->hand.played_slots[player][slot] = 0u;
    }

    if (game->phase == TRUCO_PHASE_PLAYING && game->flor_enabled != 0u &&
        game->hand.flor_pending_team < 0) {
        unsigned int p;
        int any_played = 0;

        for (p = 0u; p < game->player_count; ++p) {
            unsigned int s;
            for (s = 0u; s < TRUCO_HAND_CARDS; ++s) {
                if (game->hand.played_slots[p][s]) {
                    any_played = 1;
                    break;
                }
            }
            if (any_played) {
                break;
            }
        }

        if (!any_played) {
            unsigned int seat;
            int has_flor = 0;

            for (seat = 0u; seat < game->player_count; ++seat) {
                if (truco_has_flor(game->hand.hands[seat])) {
                    has_flor = 1;
                    break;
                }
            }
            game->hand.flor_blocks_envido = has_flor;
            game->hand.flor_resolved = has_flor ? 0 : 1;
        }
    }

    return TRUCO_OK;
}

truco_status truco_game_set_score(truco_game *game, unsigned int team,
                                  unsigned int score)
{
    if (game == 0 || team >= TRUCO_MAX_TEAMS) {
        return TRUCO_ERR_INVALID_ARGUMENT;
    }

    game->score[team] = score;
    return TRUCO_OK;
}
