#include "truco.h"

#include <string.h>

typedef enum envido_bid {
    ENVIDO = 2,
    REAL_ENVIDO = 3,
    FALTA_ENVIDO = -1
} envido_bid;

static int is_supported_player_count(unsigned int player_count)
{
    return player_count == 2u || player_count == 4u;
}

static unsigned int default_target_score(unsigned int target_score)
{
    return target_score == 0u ? 30u : target_score;
}

static unsigned int next_random(unsigned int *state)
{
    *state = (*state * 1664525u) + 1013904223u;
    return *state;
}

static unsigned int card_face_value(truco_card card)
{
    if (card.rank >= 10u) {
        return 0u;
    }
    return card.rank;
}

static int same_team(const truco_game *game, unsigned int left, unsigned int right)
{
    return game->config.team_for_player[left] == game->config.team_for_player[right];
}

static unsigned int team_for(const truco_game *game, unsigned int player)
{
    return game->config.team_for_player[player];
}

static unsigned int opposing_team(unsigned int team)
{
    return team == 0u ? 1u : 0u;
}

static int has_any_card_been_played(const truco_game *game)
{
    unsigned int player;
    unsigned int slot;

    for (player = 0u; player < game->config.player_count; ++player) {
        for (slot = 0u; slot < TRUCO_HAND_CARDS; ++slot) {
            if (game->played_slots[player][slot]) {
                return 1;
            }
        }
    }

    return 0;
}

static unsigned int falta_envido_points(const truco_game *game,
                                        unsigned int calling_team)
{
    unsigned int other = opposing_team(calling_team);
    unsigned int leader = game->score[calling_team] > game->score[other]
                              ? game->score[calling_team]
                              : game->score[other];

    if (leader < 15u) {
        return 15u - leader;
    }

    return game->config.target_score - leader;
}

static int compare_mano_order(const truco_game *game,
                              unsigned int left_player,
                              unsigned int right_player)
{
    unsigned int player_count = game->config.player_count;
    unsigned int left_distance = (left_player + player_count - game->mano) % player_count;
    unsigned int right_distance = (right_player + player_count - game->mano) % player_count;

    if (left_distance < right_distance) {
        return -1;
    }
    if (left_distance > right_distance) {
        return 1;
    }
    return 0;
}

static void add_score(truco_game *game, unsigned int team, unsigned int points)
{
    game->score[team] += points;
    if (game->score[team] >= game->config.target_score) {
        game->phase = TRUCO_PHASE_GAME_OVER;
    }
}

static int compute_hand_winner(const truco_game *game)
{
    int first = game->trick_winner_team[0];
    int second = game->trick_winner_team[1];
    int third = game->trick_winner_team[2];
    unsigned int mano_team = team_for(game, game->mano);

    if (game->current_trick == 0u) {
        return -2;
    }

    if (game->current_trick == 1u) {
        if (first == -1 && second != -1) {
            return second;
        }
        if (first != -1 && second == -1) {
            return first;
        }
        if (first != -1 && first == second) {
            return first;
        }
        return -2;
    }

    if (first == -1) {
        if (second != -1) {
            return second;
        }
        if (third != -1) {
            return third;
        }
        return (int)mano_team;
    }

    if (second == -1) {
        return first;
    }

    if (first == second) {
        return first;
    }

    if (third != -1) {
        return third;
    }

    return first;
}

static void resolve_current_trick(truco_game *game)
{
    unsigned int player;
    int best_player = -1;
    int best_team = -1;
    int tied_across_teams = 0;

    for (player = 0u; player < game->config.player_count; ++player) {
        if (!game->trick_played[game->current_trick][player]) {
            continue;
        }

        if (best_player < 0) {
            best_player = (int)player;
            best_team = (int)team_for(game, player);
            tied_across_teams = 0;
            continue;
        }

        {
            int cmp = truco_card_compare(game->trick_cards[game->current_trick][player],
                                         game->trick_cards[game->current_trick][best_player]);
            if (cmp > 0) {
                best_player = (int)player;
                best_team = (int)team_for(game, player);
                tied_across_teams = 0;
            } else if (cmp == 0 && !same_team(game, player, (unsigned int)best_player)) {
                tied_across_teams = 1;
            }
        }
    }

    if (tied_across_teams) {
        game->trick_winner_team[game->current_trick] = -1;
        game->trick_winner_player[game->current_trick] = (int)game->trick_leader;
    } else {
        game->trick_winner_team[game->current_trick] = best_team;
        game->trick_winner_player[game->current_trick] = best_player;
    }
}

static void finish_hand(truco_game *game, unsigned int winner_team)
{
    game->last_hand_winner = (int)winner_team;
    add_score(game, winner_team, game->truco_value);
    if (game->phase != TRUCO_PHASE_GAME_OVER) {
        game->phase = TRUCO_PHASE_HAND_OVER;
    }
}

static int is_valid_bid(envido_bid bid)
{
    return bid == ENVIDO || bid == REAL_ENVIDO || bid == FALTA_ENVIDO;
}

static int is_valid_player(const truco_game *game, unsigned int player)
{
    return game != 0 && player < game->config.player_count;
}

static int can_start_hand(const truco_game *game)
{
    return game != 0 &&
           (game->phase == TRUCO_PHASE_READY || game->phase == TRUCO_PHASE_HAND_OVER);
}

static unsigned int next_truco_value(const truco_game *game)
{
    unsigned int value = game->truco_value + 1u;
    return value < 2u ? 2u : value;
}

static int can_play_card(const truco_game *game,
                         unsigned int player,
                         unsigned int card_index)
{
    return is_valid_player(game, player) &&
           card_index < TRUCO_HAND_CARDS &&
           game->phase == TRUCO_PHASE_PLAYING &&
           game->pending_truco_value == 0u &&
           game->envido_pending_team < 0 &&
           player == game->current_player &&
           !game->played_slots[player][card_index];
}

static int can_raise_truco(const truco_game *game, unsigned int player)
{
    unsigned int player_team;

    if (!is_valid_player(game, player) ||
        game->phase != TRUCO_PHASE_PLAYING ||
        game->pending_truco_value != 0u ||
        game->envido_pending_team >= 0 ||
        game->truco_value >= 4u) {
        return 0;
    }

    player_team = team_for(game, player);
    return game->last_truco_team != (int)player_team;
}

static int can_answer_truco(const truco_game *game, unsigned int player)
{
    unsigned int player_team;

    if (!is_valid_player(game, player) ||
        game->phase != TRUCO_PHASE_PLAYING ||
        game->pending_truco_value == 0u) {
        return 0;
    }

    player_team = team_for(game, player);
    return game->pending_truco_team != (int)player_team;
}

static int can_call_envido(const truco_game *game,
                           unsigned int player,
                           envido_bid bid)
{
    return is_valid_player(game, player) &&
           is_valid_bid(bid) &&
           game->phase == TRUCO_PHASE_PLAYING &&
           game->pending_truco_value == 0u &&
           !game->envido_resolved &&
           game->envido_pending_team < 0 &&
           !has_any_card_been_played(game);
}

static int can_answer_envido(const truco_game *game, unsigned int player)
{
    unsigned int player_team;

    if (!is_valid_player(game, player) ||
        game->phase != TRUCO_PHASE_PLAYING ||
        game->envido_pending_team < 0) {
        return 0;
    }

    player_team = team_for(game, player);
    return game->envido_pending_team != (int)player_team;
}

void truco_config_default(truco_config *config, unsigned int player_count)
{
    unsigned int player;

    if (config == 0) {
        return;
    }

    memset(config, 0, sizeof(*config));
    config->player_count = player_count;
    config->target_score = 30u;
    config->seed = 1u;
    config->initial_dealer = player_count == 0u ? 0u : player_count - 1u;

    for (player = 0u; player < TRUCO_MAX_PLAYERS; ++player) {
        config->team_for_player[player] = (unsigned char)(player % TRUCO_MAX_TEAMS);
    }
}

truco_status truco_game_init(truco_game *game, const truco_config *config)
{
    truco_config local_config;
    unsigned int player;

    if (game == 0) {
        return TRUCO_ERR_INVALID_ARGUMENT;
    }

    if (config == 0) {
        truco_config_default(&local_config, 2u);
        config = &local_config;
    }

    if (config->player_count > TRUCO_MAX_PLAYERS || config->player_count < 2u) {
        return TRUCO_ERR_INVALID_ARGUMENT;
    }

    if (!is_supported_player_count(config->player_count)) {
        return TRUCO_ERR_UNSUPPORTED_RULES;
    }

    memset(game, 0, sizeof(*game));
    game->config = *config;
    game->config.target_score = default_target_score(config->target_score);
    game->rng_state = config->seed == 0u ? 1u : config->seed;
    game->dealer = config->initial_dealer % config->player_count;
    game->phase = TRUCO_PHASE_READY;
    game->last_hand_winner = -1;

    for (player = 0u; player < config->player_count; ++player) {
        if (game->config.team_for_player[player] >= TRUCO_MAX_TEAMS) {
            return TRUCO_ERR_INVALID_ARGUMENT;
        }
    }

    return TRUCO_OK;
}

static truco_status start_hand(truco_game *game)
{
    truco_card deck[TRUCO_DECK_SIZE];
    unsigned int player;
    unsigned int slot;
    unsigned int index = 0u;

    if (game == 0) {
        return TRUCO_ERR_INVALID_ARGUMENT;
    }

    if (!can_start_hand(game)) {
        return TRUCO_ERR_INVALID_STATE;
    }

    if (truco_deck(deck, TRUCO_DECK_SIZE) != TRUCO_OK) {
        return TRUCO_ERR_INVALID_STATE;
    }

    truco_shuffle(deck, TRUCO_DECK_SIZE, &game->rng_state);

    memset(game->played_slots, 0, sizeof(game->played_slots));
    memset(game->trick_played, 0, sizeof(game->trick_played));
    for (slot = 0u; slot < TRUCO_HAND_CARDS; ++slot) {
        game->trick_winner_team[slot] = -2;
        game->trick_winner_player[slot] = -1;
    }

    for (slot = 0u; slot < TRUCO_HAND_CARDS; ++slot) {
        for (player = 0u; player < game->config.player_count; ++player) {
            game->hands[player][slot] = deck[index++];
        }
    }

    game->mano = (game->dealer + 1u) % game->config.player_count;
    game->current_player = game->mano;
    game->trick_leader = game->mano;
    game->current_trick = 0u;
    game->truco_value = 1u;
    game->pending_truco_value = 0u;
    game->pending_truco_team = -1;
    game->last_truco_team = -1;
    game->envido_pending_team = -1;
    game->envido_pending_points = 0u;
    game->envido_resolved = 0;
    game->last_hand_winner = -1;
    game->phase = TRUCO_PHASE_PLAYING;
    game->dealer = (game->dealer + 1u) % game->config.player_count;

    return TRUCO_OK;
}

truco_card truco_make_card(truco_suit suit, unsigned int rank)
{
    truco_card card;

    card.suit = (unsigned char)suit;
    card.rank = (unsigned char)rank;
    return card;
}

int truco_card_is_valid(truco_card card)
{
    if (card.suit > TRUCO_SUIT_COPA) {
        return 0;
    }

    if (card.rank >= 1u && card.rank <= 7u) {
        return 1;
    }

    return card.rank >= 10u && card.rank <= 12u;
}

int truco_card_power(truco_card card)
{
    if (!truco_card_is_valid(card)) {
        return -1;
    }

    if (card.rank == 1u && card.suit == TRUCO_SUIT_ESPADA) {
        return 14;
    }
    if (card.rank == 1u && card.suit == TRUCO_SUIT_BASTO) {
        return 13;
    }
    if (card.rank == 7u && card.suit == TRUCO_SUIT_ESPADA) {
        return 12;
    }
    if (card.rank == 7u && card.suit == TRUCO_SUIT_ORO) {
        return 11;
    }
    if (card.rank == 3u) {
        return 10;
    }
    if (card.rank == 2u) {
        return 9;
    }
    if (card.rank == 1u) {
        return 8;
    }
    if (card.rank == 12u) {
        return 7;
    }
    if (card.rank == 11u) {
        return 6;
    }
    if (card.rank == 10u) {
        return 5;
    }
    if (card.rank == 7u) {
        return 4;
    }
    if (card.rank == 6u) {
        return 3;
    }
    if (card.rank == 5u) {
        return 2;
    }
    return 1;
}

int truco_card_compare(truco_card left, truco_card right)
{
    int left_power = truco_card_power(left);
    int right_power = truco_card_power(right);

    if (left_power > right_power) {
        return 1;
    }
    if (left_power < right_power) {
        return -1;
    }
    return 0;
}

unsigned int truco_envido_points(const truco_card cards[TRUCO_HAND_CARDS])
{
    unsigned int best = 0u;
    unsigned int i;
    unsigned int j;

    if (cards == 0) {
        return 0u;
    }

    for (i = 0u; i < TRUCO_HAND_CARDS; ++i) {
        unsigned int value = card_face_value(cards[i]);
        if (value > best) {
            best = value;
        }
    }

    for (i = 0u; i < TRUCO_HAND_CARDS; ++i) {
        for (j = i + 1u; j < TRUCO_HAND_CARDS; ++j) {
            if (cards[i].suit == cards[j].suit) {
                unsigned int pair_value = 20u + card_face_value(cards[i]) + card_face_value(cards[j]);
                if (pair_value > best) {
                    best = pair_value;
                }
            }
        }
    }

    return best;
}

truco_status truco_deck(truco_card *cards, size_t count)
{
    unsigned int suit;
    unsigned int rank;
    size_t index = 0u;

    if (cards == 0 || count < TRUCO_DECK_SIZE) {
        return TRUCO_ERR_INVALID_ARGUMENT;
    }

    for (suit = 0u; suit < 4u; ++suit) {
        for (rank = 1u; rank <= 12u; ++rank) {
            if (rank == 8u || rank == 9u) {
                continue;
            }
            cards[index++] = truco_make_card((truco_suit)suit, rank);
        }
    }

    return TRUCO_OK;
}

truco_status truco_shuffle(truco_card *cards, size_t count, unsigned int *seed)
{
    size_t i;

    if (cards == 0 || seed == 0) {
        return TRUCO_ERR_INVALID_ARGUMENT;
    }

    if (count < 2u) {
        return TRUCO_OK;
    }

    for (i = count - 1u; i > 0u; --i) {
        size_t j = (size_t)(next_random(seed) % (unsigned int)(i + 1u));
        truco_card tmp = cards[i];
        cards[i] = cards[j];
        cards[j] = tmp;
    }

    return TRUCO_OK;
}

static truco_status play_card(truco_game *game,
                              unsigned int player,
                              unsigned int card_index)
{
    int hand_winner;

    if (game == 0 || player >= game->config.player_count || card_index >= TRUCO_HAND_CARDS) {
        return TRUCO_ERR_INVALID_ARGUMENT;
    }

    if (game->phase != TRUCO_PHASE_PLAYING ||
        game->pending_truco_value != 0u ||
        game->envido_pending_team >= 0) {
        return TRUCO_ERR_INVALID_STATE;
    }

    if (player != game->current_player) {
        return TRUCO_ERR_NOT_PLAYERS_TURN;
    }

    if (game->played_slots[player][card_index]) {
        return TRUCO_ERR_CARD_ALREADY_PLAYED;
    }

    game->played_slots[player][card_index] = 1u;
    game->trick_cards[game->current_trick][player] = game->hands[player][card_index];
    game->trick_played[game->current_trick][player] = 1u;

    game->current_player = (game->current_player + 1u) % game->config.player_count;

    if (game->current_player != game->trick_leader) {
        return TRUCO_OK;
    }

    resolve_current_trick(game);
    hand_winner = compute_hand_winner(game);
    if (hand_winner >= 0) {
        finish_hand(game, (unsigned int)hand_winner);
        return TRUCO_OK;
    }

    if (game->current_trick + 1u >= TRUCO_HAND_CARDS) {
        finish_hand(game, team_for(game, game->mano));
        return TRUCO_OK;
    }

    if (game->trick_winner_player[game->current_trick] >= 0 &&
        game->trick_winner_team[game->current_trick] != -1) {
        game->trick_leader = (unsigned int)game->trick_winner_player[game->current_trick];
    }

    game->current_player = game->trick_leader;
    game->current_trick++;

    return TRUCO_OK;
}

static truco_status raise_truco(truco_game *game, unsigned int player)
{
    unsigned int player_team;

    if (game == 0 || player >= game->config.player_count) {
        return TRUCO_ERR_INVALID_ARGUMENT;
    }

    if (!can_raise_truco(game, player)) {
        return TRUCO_ERR_INVALID_STATE;
    }

    player_team = team_for(game, player);
    game->pending_truco_team = (int)player_team;
    game->pending_truco_value = next_truco_value(game);

    return TRUCO_OK;
}

static truco_status accept_truco(truco_game *game, unsigned int player)
{
    if (game == 0 || player >= game->config.player_count) {
        return TRUCO_ERR_INVALID_ARGUMENT;
    }

    if (!can_answer_truco(game, player)) {
        return TRUCO_ERR_INVALID_STATE;
    }

    game->truco_value = game->pending_truco_value;
    game->last_truco_team = game->pending_truco_team;
    game->pending_truco_value = 0u;
    game->pending_truco_team = -1;

    return TRUCO_OK;
}

static truco_status decline_truco(truco_game *game, unsigned int player)
{
    unsigned int winner_team;

    if (game == 0 || player >= game->config.player_count) {
        return TRUCO_ERR_INVALID_ARGUMENT;
    }

    if (!can_answer_truco(game, player)) {
        return TRUCO_ERR_INVALID_STATE;
    }

    winner_team = (unsigned int)game->pending_truco_team;
    game->pending_truco_value = 0u;
    game->pending_truco_team = -1;
    finish_hand(game, winner_team);

    return TRUCO_OK;
}

static truco_status call_envido(truco_game *game,
                                unsigned int player,
                                envido_bid bid)
{
    unsigned int player_team;
    unsigned int points;

    if (game == 0 || player >= game->config.player_count || !is_valid_bid(bid)) {
        return TRUCO_ERR_INVALID_ARGUMENT;
    }

    if (!can_call_envido(game, player, bid)) {
        return TRUCO_ERR_INVALID_STATE;
    }

    player_team = team_for(game, player);
    points = bid == FALTA_ENVIDO ? falta_envido_points(game, player_team) : (unsigned int)bid;

    game->envido_pending_team = (int)player_team;
    game->envido_pending_points = points;

    return TRUCO_OK;
}

static truco_status accept_envido(truco_game *game, unsigned int player)
{
    unsigned int best_points[TRUCO_MAX_TEAMS] = {0u, 0u};
    unsigned int best_player[TRUCO_MAX_TEAMS] = {0u, 0u};
    int seen_team[TRUCO_MAX_TEAMS] = {0, 0};
    unsigned int player_index;
    unsigned int winner_team = 0u;

    if (game == 0 || player >= game->config.player_count) {
        return TRUCO_ERR_INVALID_ARGUMENT;
    }

    if (!can_answer_envido(game, player)) {
        return TRUCO_ERR_INVALID_STATE;
    }

    for (player_index = 0u; player_index < game->config.player_count; ++player_index) {
        unsigned int team = team_for(game, player_index);
        unsigned int points = truco_envido_points(game->hands[player_index]);
        if (!seen_team[team] ||
            points > best_points[team] ||
            (points == best_points[team] &&
             compare_mano_order(game, player_index, best_player[team]) < 0)) {
            best_points[team] = points;
            best_player[team] = player_index;
            seen_team[team] = 1;
        }
    }

    if (best_points[1] > best_points[0]) {
        winner_team = 1u;
    } else if (best_points[1] == best_points[0] &&
               compare_mano_order(game, best_player[1], best_player[0]) < 0) {
        winner_team = 1u;
    }

    add_score(game, winner_team, game->envido_pending_points);
    game->envido_pending_team = -1;
    game->envido_pending_points = 0u;
    game->envido_resolved = 1;

    return TRUCO_OK;
}

static truco_status decline_envido(truco_game *game, unsigned int player)
{
    unsigned int winner_team;

    if (game == 0 || player >= game->config.player_count) {
        return TRUCO_ERR_INVALID_ARGUMENT;
    }

    if (!can_answer_envido(game, player)) {
        return TRUCO_ERR_INVALID_STATE;
    }

    winner_team = (unsigned int)game->envido_pending_team;
    add_score(game, winner_team, 1u);
    game->envido_pending_team = -1;
    game->envido_pending_points = 0u;
    game->envido_resolved = 1;

    return TRUCO_OK;
}

truco_status truco_game_apply(truco_game *game,
                              unsigned int player,
                              truco_command command)
{
    if (!is_valid_player(game, player)) {
        return TRUCO_ERR_INVALID_ARGUMENT;
    }

    switch (command) {
    case TRUCO_CMD_START_HAND:
        return start_hand(game);
    case TRUCO_CMD_PLAY_CARD_0:
        return play_card(game, player, 0u);
    case TRUCO_CMD_PLAY_CARD_1:
        return play_card(game, player, 1u);
    case TRUCO_CMD_PLAY_CARD_2:
        return play_card(game, player, 2u);
    case TRUCO_CMD_RAISE_TRUCO:
        return raise_truco(game, player);
    case TRUCO_CMD_CALL_ENVIDO:
        return call_envido(game, player, ENVIDO);
    case TRUCO_CMD_CALL_REAL_ENVIDO:
        return call_envido(game, player, REAL_ENVIDO);
    case TRUCO_CMD_CALL_FALTA_ENVIDO:
        return call_envido(game, player, FALTA_ENVIDO);
    case TRUCO_CMD_ACCEPT_BID:
        if (can_answer_truco(game, player)) {
            return accept_truco(game, player);
        }
        if (can_answer_envido(game, player)) {
            return accept_envido(game, player);
        }
        return TRUCO_ERR_INVALID_STATE;
    case TRUCO_CMD_REJECT_BID:
        if (can_answer_truco(game, player)) {
            return decline_truco(game, player);
        }
        if (can_answer_envido(game, player)) {
            return decline_envido(game, player);
        }
        return TRUCO_ERR_INVALID_STATE;
    case TRUCO_CMD_NONE:
    default:
        return TRUCO_ERR_INVALID_ARGUMENT;
    }
}

static void add_legal_command(truco_legal_actions *actions,
                              truco_command command)
{
    if (actions->count < TRUCO_MAX_LEGAL_COMMANDS) {
        actions->commands[actions->count++] = command;
    }
}

truco_status truco_game_legal_actions(const truco_game *game,
                                      unsigned int player,
                                      truco_legal_actions *actions)
{
    unsigned int slot;

    if (actions == 0 || !is_valid_player(game, player)) {
        return TRUCO_ERR_INVALID_ARGUMENT;
    }

    memset(actions, 0, sizeof(*actions));

    if (can_start_hand(game)) {
        add_legal_command(actions, TRUCO_CMD_START_HAND);
        return TRUCO_OK;
    }

    if (game->phase != TRUCO_PHASE_PLAYING) {
        return TRUCO_OK;
    }

    for (slot = 0u; slot < TRUCO_HAND_CARDS; ++slot) {
        if (can_play_card(game, player, slot)) {
            add_legal_command(actions, (truco_command)(TRUCO_CMD_PLAY_CARD_0 + slot));
        }
    }

    if (can_raise_truco(game, player)) {
        add_legal_command(actions, TRUCO_CMD_RAISE_TRUCO);
        actions->truco_value = next_truco_value(game);
    }

    if (can_answer_truco(game, player)) {
        add_legal_command(actions, TRUCO_CMD_ACCEPT_BID);
        add_legal_command(actions, TRUCO_CMD_REJECT_BID);
        actions->truco_value = game->pending_truco_value;
    }

    if (can_call_envido(game, player, ENVIDO)) {
        add_legal_command(actions, TRUCO_CMD_CALL_ENVIDO);
    }
    if (can_call_envido(game, player, REAL_ENVIDO)) {
        add_legal_command(actions, TRUCO_CMD_CALL_REAL_ENVIDO);
    }
    if (can_call_envido(game, player, FALTA_ENVIDO)) {
        add_legal_command(actions, TRUCO_CMD_CALL_FALTA_ENVIDO);
    }

    if (can_answer_envido(game, player)) {
        add_legal_command(actions, TRUCO_CMD_ACCEPT_BID);
        add_legal_command(actions, TRUCO_CMD_REJECT_BID);
        actions->envido_points = game->envido_pending_points;
    }

    return TRUCO_OK;
}

unsigned int truco_game_player_count(const truco_game *game)
{
    if (game == 0) {
        return 0u;
    }
    return game->config.player_count;
}

unsigned int truco_game_team_for_player(const truco_game *game,
                                        unsigned int player)
{
    if (game == 0 || player >= game->config.player_count) {
        return TRUCO_MAX_TEAMS;
    }
    return team_for(game, player);
}

truco_phase truco_game_phase(const truco_game *game)
{
    if (game == 0) {
        return TRUCO_PHASE_READY;
    }
    return (truco_phase)game->phase;
}

unsigned int truco_game_current_player(const truco_game *game)
{
    if (game == 0) {
        return 0u;
    }
    return game->current_player;
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
    if (game == 0 ||
        (game->phase != TRUCO_PHASE_HAND_OVER && game->phase != TRUCO_PHASE_GAME_OVER)) {
        return -1;
    }

    return game->last_hand_winner;
}

int truco_game_trick_winner(const truco_game *game, unsigned int trick)
{
    if (game == 0 || trick >= TRUCO_HAND_CARDS) {
        return -2;
    }
    return game->trick_winner_team[trick];
}
