#include "truco.h"
#include "truco_internal.h"

#include <stdlib.h>
#include <string.h>

struct truco_game {
  unsigned int player_count;
  unsigned int target_score;
  unsigned int seed;
  unsigned int initial_dealer;
  unsigned char team_for_player[TRUCO_MAX_PLAYERS];
  unsigned int rng_state;
  truco_phase phase;
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
  unsigned char flor_enabled;
  int flor_blocks_envido;
  int flor_resolved;
  int flor_pending_team;
  unsigned int flor_pending_points;
  int last_hand_winner; /* 0/1 team, or -1 if no hand finished yet */
  unsigned int score[TRUCO_MAX_TEAMS];
  truco_card hands[TRUCO_MAX_PLAYERS][TRUCO_HAND_CARDS];
  /* [player][slot]: 1 if that hand card was played to the table */
  unsigned char played_slots[TRUCO_MAX_PLAYERS][TRUCO_HAND_CARDS];
  /* [trick][player]: card on the table for that trick */
  truco_card trick_cards[TRUCO_HAND_CARDS][TRUCO_MAX_PLAYERS];
  /* [trick][player]: 1 if that player already played in that trick */
  unsigned char trick_played[TRUCO_HAND_CARDS][TRUCO_MAX_PLAYERS];
  /* per trick: 0/1 winner team, -1 parda, -2 not played yet (see TRUCO_TRICK_*) */
  int trick_winner_team[TRUCO_HAND_CARDS];
  int trick_winner_player[TRUCO_HAND_CARDS];
};

typedef enum envido_bid {
  ENVIDO = 2,          /* fixed envido stake */
  REAL_ENVIDO = 3,     /* fixed real-envido stake */
  FALTA_ENVIDO = -1    /* sentinel: stake from falta_envido_points(), not 2/3 */
} envido_bid;

/*
 * Internal sentinels (public getters document their own return values).
 *
 * trick_winner_team[trick]:
 *   TRUCO_TRICK_UNPLAYED (-2)  trick not resolved yet
 *   TRUCO_TRICK_PARDA (-1)     parda: tie across teams; trick_leader keeps the trick
 *   0 / 1                      winning team index
 *
 * compute_hand_winner (internal):
 *   TRUCO_HAND_UNDECIDED (-2)  fewer than two tricks finished, or no winner yet
 *
 * pending_*_team / last_hand_winner when unset: -1 (TRUCO_NO_TEAM)
 *
 * Rules constants:
 *   default target score 30; falta envido uses 15 until someone reaches 15
 *   truco raises capped at 4 (vale cuatro)
 *   flor awards TRUCO_FLOR_POINTS (3) when enabled
 */
#define TRUCO_TRICK_UNPLAYED (-2)
#define TRUCO_TRICK_PARDA (-1)
#define TRUCO_HAND_UNDECIDED (-2)
#define TRUCO_NO_TEAM (-1)
#define TRUCO_DEFAULT_TARGET_SCORE 30u
#define TRUCO_FALTA_ENVIDO_FLOOR 15u
#define TRUCO_TRUCO_MAX_VALUE 4u

static int is_supported_player_count(unsigned int player_count);
static unsigned int next_random(unsigned int *state);
static unsigned int card_face_value(truco_card card);
static int same_team(const truco_game *game, unsigned int left, unsigned int right);
static unsigned int team_for(const truco_game *game, unsigned int player);
static unsigned int opposing_team(unsigned int team);
static int has_any_card_been_played(const truco_game *game);
static unsigned int falta_envido_points(const truco_game *game, unsigned int calling_team);
static int compare_mano_order(const truco_game *game, unsigned int left_player, unsigned int right_player);
static void add_score(truco_game *game, unsigned int team, unsigned int points);
static int compute_hand_winner(const truco_game *game);
static void resolve_current_trick(truco_game *game);
static void finish_hand(truco_game *game, unsigned int winner_team);
static int is_valid_bid(envido_bid bid);
static int is_valid_player(const truco_game *game, unsigned int player);
static int can_start_hand(const truco_game *game, unsigned int player);
static unsigned int next_truco_value(const truco_game *game);
static int can_play_card(const truco_game *game, unsigned int player, unsigned int card_index);
static int can_raise_truco(const truco_game *game, unsigned int player);
static int can_answer_truco(const truco_game *game, unsigned int player);
static int can_call_envido(const truco_game *game, unsigned int player, envido_bid bid);
static int can_answer_envido(const truco_game *game, unsigned int player);
static int hand_has_flor(const truco_game *game);
static int opposing_team_has_flor(const truco_game *game, unsigned int team);
static int can_call_flor(const truco_game *game, unsigned int player);
static int can_answer_flor(const truco_game *game, unsigned int player);
static truco_status call_flor(truco_game *game, unsigned int player);
static truco_status accept_flor(truco_game *game, unsigned int player);
static truco_status decline_flor(truco_game *game, unsigned int player);
static void apply_default_teams(truco_game *game);
static truco_status start_hand(truco_game *game, unsigned int player);
static truco_status play_card(truco_game *game, unsigned int player, unsigned int card_index);
static truco_status raise_truco(truco_game *game, unsigned int player);
static truco_status accept_truco(truco_game *game, unsigned int player);
static truco_status decline_truco(truco_game *game, unsigned int player);
static truco_status call_envido(truco_game *game, unsigned int player, envido_bid bid);
static truco_status accept_envido(truco_game *game, unsigned int player);
static truco_status decline_envido(truco_game *game, unsigned int player);
static int can_go_to_deck(const truco_game *game, unsigned int player);
static truco_status go_to_deck(truco_game *game, unsigned int player);
static void add_legal_command(truco_legal_commands *out, truco_command command);

/* lifecycle */

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
    unsigned int player;
    unsigned int player_count = 2u;

    if (game == 0) {
        return TRUCO_ERR_INVALID_ARGUMENT;
    }

    game->player_count = player_count;
    game->target_score = TRUCO_DEFAULT_TARGET_SCORE;
    game->seed = 1u;
    game->initial_dealer = player_count - 1u;
    apply_default_teams(game);
    game->rng_state = game->seed;
    game->dealer = game->initial_dealer % player_count;
    game->phase = TRUCO_PHASE_READY;
    game->last_hand_winner = TRUCO_NO_TEAM;
    game->score[0] = 0u;
    game->score[1] = 0u;
    game->mano = 0u;
    game->current_player = 0u;
    game->trick_leader = 0u;
    game->current_trick = 0u;
    game->truco_value = 0u;
    game->pending_truco_value = 0u;
    game->pending_truco_team = TRUCO_NO_TEAM;
    game->last_truco_team = TRUCO_NO_TEAM;
    game->envido_pending_team = TRUCO_NO_TEAM;
    game->envido_pending_points = 0u;
    game->envido_resolved = 0;
    game->flor_enabled = 0;
    game->flor_blocks_envido = 0;
    game->flor_resolved = 0;
    game->flor_pending_team = TRUCO_NO_TEAM;
    game->flor_pending_points = 0u;
    memset(game->played_slots, 0, sizeof(game->played_slots));
    memset(game->trick_played, 0, sizeof(game->trick_played));
    memset(game->hands, 0, sizeof(game->hands));
    memset(game->trick_cards, 0, sizeof(game->trick_cards));

    for (player = 0u; player < TRUCO_HAND_CARDS; ++player) {
        game->trick_winner_team[player] = TRUCO_TRICK_UNPLAYED;
        game->trick_winner_player[player] = TRUCO_NO_TEAM;
    }

    return TRUCO_OK;
}

void truco_game_destroy(truco_game *game) {
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


/* configuration */

truco_status truco_game_set_player_count(truco_game *game,
                                         unsigned int player_count) {
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
                                         unsigned int target_score) {
  if (game == 0) {
    return TRUCO_ERR_INVALID_ARGUMENT;
  }

  game->target_score = target_score;
  return TRUCO_OK;
}

truco_status truco_game_set_seed(truco_game *game, unsigned int seed) {
  if (game == 0) {
    return TRUCO_ERR_INVALID_ARGUMENT;
  }

  game->seed = seed == 0u ? 1u : seed;
  game->rng_state = game->seed;
  return TRUCO_OK;
}

truco_status truco_game_set_initial_dealer(truco_game *game,
                                           unsigned int initial_dealer) {
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
                                            unsigned int team) {
  if (game == 0 || player >= TRUCO_MAX_PLAYERS || team >= TRUCO_MAX_TEAMS) {
    return TRUCO_ERR_INVALID_ARGUMENT;
  }

  game->team_for_player[player] = (unsigned char)team;
  return TRUCO_OK;
}

truco_status truco_game_set_flor_enabled(truco_game *game, int enabled) {
  if (game == 0) {
    return TRUCO_ERR_INVALID_ARGUMENT;
  }

  game->flor_enabled = enabled ? 1u : 0u;
  return TRUCO_OK;
}


/* gameplay */

truco_status truco_game_apply(truco_game *game, unsigned int player,
                              truco_command command) {
  if (!is_valid_player(game, player)) {
    return TRUCO_ERR_INVALID_ARGUMENT;
  }

  switch (command) {
  case TRUCO_CMD_START_HAND:
    return start_hand(game, player);
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
  case TRUCO_CMD_CALL_FLOR:
    return call_flor(game, player);
  case TRUCO_CMD_ACCEPT_BID:
    if (can_answer_truco(game, player)) {
      return accept_truco(game, player);
    }
    if (can_answer_flor(game, player)) {
      return accept_flor(game, player);
    }
    if (can_answer_envido(game, player)) {
      return accept_envido(game, player);
    }
    return TRUCO_ERR_INVALID_STATE;
  case TRUCO_CMD_REJECT_BID:
    if (can_answer_truco(game, player)) {
      return decline_truco(game, player);
    }
    if (can_answer_flor(game, player)) {
      return decline_flor(game, player);
    }
    if (can_answer_envido(game, player)) {
      return decline_envido(game, player);
    }
    return TRUCO_ERR_INVALID_STATE;
  case TRUCO_CMD_GO_TO_DECK:
    return go_to_deck(game, player);
  case TRUCO_CMD_NONE:
  default:
    return TRUCO_ERR_INVALID_ARGUMENT;
  }
}

truco_status truco_game_legal_commands(const truco_game *game,
                                       unsigned int player,
                                       truco_legal_commands *out) {
  unsigned int slot;

  if (out == 0 || !is_valid_player(game, player)) {
    return TRUCO_ERR_INVALID_ARGUMENT;
  }

  memset(out, 0, sizeof(*out));

  if (can_start_hand(game, player)) {
    add_legal_command(out, TRUCO_CMD_START_HAND);
    return TRUCO_OK;
  }

  if (game->phase != TRUCO_PHASE_PLAYING) {
    return TRUCO_OK;
  }

  for (slot = 0u; slot < TRUCO_HAND_CARDS; ++slot) {
    if (can_play_card(game, player, slot)) {
      add_legal_command(out, (truco_command)(TRUCO_CMD_PLAY_CARD_0 + slot));
    }
  }

  if (can_raise_truco(game, player)) {
    add_legal_command(out, TRUCO_CMD_RAISE_TRUCO);
  }

  if (can_answer_truco(game, player)) {
    add_legal_command(out, TRUCO_CMD_ACCEPT_BID);
    add_legal_command(out, TRUCO_CMD_REJECT_BID);
  }

  if (can_call_flor(game, player)) {
    add_legal_command(out, TRUCO_CMD_CALL_FLOR);
  }

  if (can_answer_flor(game, player)) {
    add_legal_command(out, TRUCO_CMD_ACCEPT_BID);
    add_legal_command(out, TRUCO_CMD_REJECT_BID);
  }

  if (can_call_envido(game, player, ENVIDO)) {
    add_legal_command(out, TRUCO_CMD_CALL_ENVIDO);
  }
  if (can_call_envido(game, player, REAL_ENVIDO)) {
    add_legal_command(out, TRUCO_CMD_CALL_REAL_ENVIDO);
  }
  if (can_call_envido(game, player, FALTA_ENVIDO)) {
    add_legal_command(out, TRUCO_CMD_CALL_FALTA_ENVIDO);
  }

  if (can_answer_envido(game, player)) {
    add_legal_command(out, TRUCO_CMD_ACCEPT_BID);
    add_legal_command(out, TRUCO_CMD_REJECT_BID);
  }

  if (can_go_to_deck(game, player)) {
    add_legal_command(out, TRUCO_CMD_GO_TO_DECK);
  }

  return TRUCO_OK;
}

unsigned int truco_game_pending_truco_value(const truco_game *game) {
  if (game == 0 || game->pending_truco_value == 0u) {
    return 0u;
  }
  return game->pending_truco_value;
}

unsigned int truco_game_pending_envido_points(const truco_game *game) {
  if (game == 0 || game->envido_pending_team < 0) {
    return 0u;
  }
  return game->envido_pending_points;
}

unsigned int truco_game_pending_flor_points(const truco_game *game) {
  if (game == 0 || game->flor_pending_team < 0) {
    return 0u;
  }
  return game->flor_pending_points;
}

unsigned int truco_game_next_truco_value(const truco_game *game) {
  if (game == 0) {
    return 0u;
  }
  return next_truco_value(game);
}

unsigned int truco_game_player_count(const truco_game *game) {
  if (game == 0) {
    return 0u;
  }
  return game->player_count;
}

unsigned int truco_game_team_for_player(const truco_game *game,
                                        unsigned int player) {
  if (game == 0 || player >= game->player_count) {
    return TRUCO_MAX_TEAMS;
  }
  return team_for(game, player);
}

truco_phase truco_game_phase(const truco_game *game) {
  if (game == 0) {
    return TRUCO_PHASE_READY;
  }
  return game->phase;
}

unsigned int truco_game_current_player(const truco_game *game) {
  if (game == 0) {
    return 0u;
  }
  return game->current_player;
}

int truco_game_flor_enabled(const truco_game *game) {
  if (game == 0) {
    return 0;
  }
  return game->flor_enabled != 0u;
}

int truco_game_player_has_flor(const truco_game *game, unsigned int player) {
  if (!is_valid_player(game, player)) {
    return 0;
  }
  return truco_has_flor(game->hands[player]);
}

unsigned int truco_game_score(const truco_game *game, unsigned int team) {
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
    return game->trick_winner_team[trick];
}

truco_status truco_game_hand_card(const truco_game *game, unsigned int player,
                                  unsigned int slot, truco_card *card_out) {
  if (game == 0 || card_out == 0 || !is_valid_player(game, player) ||
      slot >= TRUCO_HAND_CARDS) {
    return TRUCO_ERR_INVALID_ARGUMENT;
  }

  *card_out = game->hands[player][slot];
  return TRUCO_OK;
}

truco_status truco_game_trick_card(const truco_game *game, unsigned int trick,
                                   unsigned int player, truco_card *card_out) {
  if (game == 0 || card_out == 0 || trick >= TRUCO_HAND_CARDS ||
      !is_valid_player(game, player)) {
    return TRUCO_ERR_INVALID_ARGUMENT;
  }
  if (!game->trick_played[trick][player]) {
    return TRUCO_ERR_INVALID_STATE;
  }

  *card_out = game->trick_cards[trick][player];
  return TRUCO_OK;
}

unsigned int truco_game_hand_envido(const truco_game *game,
                                    unsigned int player) {
  if (!is_valid_player(game, player)) {
    return 0u;
  }

  return truco_envido_points(game->hands[player]);
}

unsigned int truco_game_hand_flor(const truco_game *game,
                                  unsigned int player) {
  if (!is_valid_player(game, player)) {
    return 0u;
  }

  return truco_flor_points(game->hands[player]);
}

truco_status truco_game_set_hand(truco_game *game, unsigned int player,
                                 const truco_card cards[TRUCO_HAND_CARDS]) {
  unsigned int slot;

  if (game == 0 || cards == 0 || !is_valid_player(game, player)) {
    return TRUCO_ERR_INVALID_ARGUMENT;
  }

  for (slot = 0u; slot < TRUCO_HAND_CARDS; ++slot) {
    game->hands[player][slot] = cards[slot];
    game->played_slots[player][slot] = 0u;
  }

  if (game->phase == TRUCO_PHASE_PLAYING && game->flor_enabled != 0u &&
      game->flor_pending_team < 0 && !has_any_card_been_played(game)) {
    game->flor_blocks_envido = hand_has_flor(game);
    game->flor_resolved = game->flor_blocks_envido ? 0 : 1;
  }

  return TRUCO_OK;
}

truco_status truco_game_set_score(truco_game *game, unsigned int team,
                                  unsigned int score) {
  if (game == 0 || team >= TRUCO_MAX_TEAMS) {
    return TRUCO_ERR_INVALID_ARGUMENT;
  }

  game->score[team] = score;
  return TRUCO_OK;
}


/* card / deck */

truco_card truco_make_card(truco_suit suit, unsigned int rank) {
  truco_card card;

  card.suit = suit;
  card.rank = (unsigned char)rank;
  return card;
}

int truco_card_is_valid(truco_card card) {
  if (card.suit > TRUCO_SUIT_COPA) {
    return 0;
  }

  if (card.rank >= 1u && card.rank <= 7u) {
    return 1;
  }

  return card.rank >= 10u && card.rank <= 12u;
}

int truco_card_power(truco_card card) {
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

int truco_card_compare(truco_card left, truco_card right) {
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

unsigned int truco_envido_points(const truco_card cards[TRUCO_HAND_CARDS]) {
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
        unsigned int pair_value =
            20u + card_face_value(cards[i]) + card_face_value(cards[j]);
        if (pair_value > best) {
          best = pair_value;
        }
      }
    }
  }

  return best;
}

int truco_has_flor(const truco_card cards[TRUCO_HAND_CARDS]) {
  if (cards == 0) {
    return 0;
  }

  return cards[0].suit == cards[1].suit && cards[1].suit == cards[2].suit;
}

unsigned int truco_flor_points(const truco_card cards[TRUCO_HAND_CARDS]) {
  unsigned int total = 20u;
  unsigned int slot;

  if (!truco_has_flor(cards)) {
    return 0u;
  }

  for (slot = 0u; slot < TRUCO_HAND_CARDS; ++slot) {
    total += card_face_value(cards[slot]);
  }

  return total;
}

truco_status truco_deck(truco_card *cards, size_t count) {
  truco_suit suit;
  unsigned int rank;
  size_t index = 0u;

  if (cards == 0 || count < TRUCO_DECK_SIZE) {
    return TRUCO_ERR_INVALID_ARGUMENT;
  }

  for (suit = TRUCO_SUIT_ESPADA; suit <= TRUCO_SUIT_COPA; ++suit) {
    for (rank = 1u; rank <= 12u; ++rank) {
      if (rank == 8u || rank == 9u) {
        continue;
      }
      cards[index++] = truco_make_card(suit, rank);
    }
  }

  return TRUCO_OK;
}

truco_status truco_shuffle(truco_card *cards, size_t count,
                           unsigned int *seed) {
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


/* static helpers */

static int is_supported_player_count(unsigned int player_count) {
  return player_count == 2u || player_count == 4u;
}

static unsigned int next_random(unsigned int *state) {
  *state = (*state * 1664525u) + 1013904223u;
  return *state;
}

static unsigned int card_face_value(truco_card card) {
  if (card.rank >= 10u) {
    return 0u;
  }
  return card.rank;
}

static int same_team(const truco_game *game, unsigned int left,
                     unsigned int right) {
  return game->team_for_player[left] == game->team_for_player[right];
}

static unsigned int team_for(const truco_game *game, unsigned int player) {
  return game->team_for_player[player];
}

static unsigned int opposing_team(unsigned int team) {
  return team == 0u ? 1u : 0u;
}

static int has_any_card_been_played(const truco_game *game) {
  unsigned int player;
  unsigned int slot;

  for (player = 0u; player < game->player_count; ++player) {
    for (slot = 0u; slot < TRUCO_HAND_CARDS; ++slot) {
      if (game->played_slots[player][slot]) {
        return 1;
      }
    }
  }

  return 0;
}

/* falta envido: points to TRUCO_FALTA_ENVIDO_FLOOR (15), else to target_score */
static unsigned int falta_envido_points(const truco_game *game,
                                        unsigned int calling_team) {
  unsigned int other = opposing_team(calling_team);
  unsigned int leader = game->score[calling_team] > game->score[other]
                            ? game->score[calling_team]
                            : game->score[other];

  if (leader < TRUCO_FALTA_ENVIDO_FLOOR) {
    return TRUCO_FALTA_ENVIDO_FLOOR - leader;
  }

  return game->target_score - leader;
}

static int compare_mano_order(const truco_game *game, unsigned int left_player,
                              unsigned int right_player) {
  unsigned int player_count = game->player_count;
  unsigned int left_distance =
      (left_player + player_count - game->mano) % player_count;
  unsigned int right_distance =
      (right_player + player_count - game->mano) % player_count;

  if (left_distance < right_distance) {
    return -1;
  }
  if (left_distance > right_distance) {
    return 1;
  }
  return 0;
}

static void add_score(truco_game *game, unsigned int team,
                      unsigned int points) {
  game->score[team] += points;
  if (game->score[team] >= game->target_score) {
    game->phase = TRUCO_PHASE_GAME_OVER;
  }
}

/* returns team 0/1, TRUCO_HAND_UNDECIDED, or mano team when all tricks are parda */
static int compute_hand_winner(const truco_game *game) {
  int first = game->trick_winner_team[0];
  int second = game->trick_winner_team[1];
  int third = game->trick_winner_team[2];
  unsigned int mano_team = team_for(game, game->mano);

  if (game->current_trick == 0u) {
    return TRUCO_HAND_UNDECIDED;
  }

  if (game->current_trick == 1u) {
    /* two tricks played: parda on trick 0 lets trick 1 decide the hand */
    if (first == TRUCO_TRICK_PARDA && second != TRUCO_TRICK_PARDA) {
      return second;
    }
    if (first != TRUCO_TRICK_PARDA && second == TRUCO_TRICK_PARDA) {
      return first;
    }
    if (first != TRUCO_TRICK_PARDA && first == second) {
      return first;
    }
    return TRUCO_HAND_UNDECIDED;
  }

  if (first == TRUCO_TRICK_PARDA) {
    if (second != TRUCO_TRICK_PARDA) {
      return second;
    }
    if (third != TRUCO_TRICK_PARDA) {
      return third;
    }
    return (int)mano_team;
  }

  if (second == TRUCO_TRICK_PARDA) {
    return first;
  }

  if (first == second) {
    return first;
  }

  if (third != TRUCO_TRICK_PARDA) {
    return third;
  }

  return first;
}

static void resolve_current_trick(truco_game *game)
{
  unsigned int player;
  int best_player = -1; /* no winning card yet */
  int best_team = -1;
  int tied_across_teams = 0;

  for (player = 0u; player < game->player_count; ++player) {
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
      int cmp = truco_card_compare(
          game->trick_cards[game->current_trick][player],
          game->trick_cards[game->current_trick][best_player]);
      if (cmp > 0) {
        best_player = (int)player;
        best_team = (int)team_for(game, player);
        tied_across_teams = 0;
      } else if (cmp == 0 &&
                 !same_team(game, player, (unsigned int)best_player)) {
        tied_across_teams = 1;
      }
    }
  }

  if (tied_across_teams) {
    game->trick_winner_team[game->current_trick] = TRUCO_TRICK_PARDA;
    game->trick_winner_player[game->current_trick] = (int)game->trick_leader;
  } else {
    game->trick_winner_team[game->current_trick] = best_team;
    game->trick_winner_player[game->current_trick] = best_player;
  }
}

static void finish_hand(truco_game *game, unsigned int winner_team) {
  game->last_hand_winner = (int)winner_team;
  add_score(game, winner_team, game->truco_value);
  if (game->phase != TRUCO_PHASE_GAME_OVER) {
    game->phase = TRUCO_PHASE_HAND_OVER;
  }
}

static int is_valid_bid(envido_bid bid) {
  return bid == ENVIDO || bid == REAL_ENVIDO || bid == FALTA_ENVIDO;
}

static int is_valid_player(const truco_game *game, unsigned int player) {
  return game != 0 && player < game->player_count;
}

static unsigned int dealing_player(const truco_game *game)
{
  if (game->phase == TRUCO_PHASE_READY) {
    return game->initial_dealer % game->player_count;
  }
  return game->dealer;
}

static int can_start_hand(const truco_game *game, unsigned int player) {
  return is_valid_player(game, player) &&
         (game->phase == TRUCO_PHASE_READY ||
          game->phase == TRUCO_PHASE_HAND_OVER) &&
         player == dealing_player(game);
}

static unsigned int next_truco_value(const truco_game *game) {
  unsigned int value = game->truco_value + 1u;
  return value < 2u ? 2u : value;
}

static int can_play_card(const truco_game *game, unsigned int player,
                         unsigned int card_index) {
  return is_valid_player(game, player) && card_index < TRUCO_HAND_CARDS &&
         game->phase == TRUCO_PHASE_PLAYING &&
         game->pending_truco_value == 0u && game->envido_pending_team < 0 &&
         game->flor_pending_team < 0 && player == game->current_player &&
         !game->played_slots[player][card_index] &&
         (!game->flor_enabled || game->flor_resolved ||
          !truco_has_flor(game->hands[player]));
}

static int can_raise_truco(const truco_game *game, unsigned int player) {
  unsigned int player_team;

  /* TRUCO_TRUCO_MAX_VALUE is vale cuatro; same team cannot raise twice in a row */
  if (!is_valid_player(game, player) || game->phase != TRUCO_PHASE_PLAYING ||
      player != game->current_player || game->pending_truco_value != 0u ||
      game->envido_pending_team >= 0 || game->flor_pending_team >= 0 ||
      game->truco_value >= TRUCO_TRUCO_MAX_VALUE) {
    return 0;
  }

  player_team = team_for(game, player);
  return game->last_truco_team != (int)player_team;
}

static int can_answer_truco(const truco_game *game, unsigned int player) {
  unsigned int player_team;

  if (!is_valid_player(game, player) || game->phase != TRUCO_PHASE_PLAYING ||
      game->pending_truco_value == 0u) {
    return 0;
  }

  player_team = team_for(game, player);
  return game->pending_truco_team != (int)player_team;
}

static int can_call_envido(const truco_game *game, unsigned int player,
                           envido_bid bid) {
  if (!is_valid_player(game, player) || !is_valid_bid(bid) ||
      game->phase != TRUCO_PHASE_PLAYING || player != game->current_player ||
      game->pending_truco_value != 0u || game->flor_pending_team >= 0 ||
      game->envido_resolved || game->envido_pending_team >= 0 ||
      game->flor_blocks_envido || has_any_card_been_played(game)) {
    return 0;
  }

  if (game->flor_enabled && truco_has_flor(game->hands[player])) {
    return 0;
  }

  return 1;
}

static int can_answer_envido(const truco_game *game, unsigned int player) {
  unsigned int player_team;

  if (!is_valid_player(game, player) || game->phase != TRUCO_PHASE_PLAYING ||
      game->envido_pending_team < 0) {
    return 0;
  }

  player_team = team_for(game, player);
  return game->envido_pending_team != (int)player_team;
}

static int hand_has_flor(const truco_game *game) {
  unsigned int player;

  for (player = 0u; player < game->player_count; ++player) {
    if (truco_has_flor(game->hands[player])) {
      return 1;
    }
  }

  return 0;
}

static int opposing_team_has_flor(const truco_game *game, unsigned int team) {
  unsigned int player;

  for (player = 0u; player < game->player_count; ++player) {
    if (team_for(game, player) != team && truco_has_flor(game->hands[player])) {
      return 1;
    }
  }

  return 0;
}

static int can_call_flor(const truco_game *game, unsigned int player) {
  return is_valid_player(game, player) && game->flor_enabled != 0u &&
         game->phase == TRUCO_PHASE_PLAYING && player == game->current_player &&
         !game->flor_resolved && game->flor_pending_team < 0 &&
         game->pending_truco_value == 0u && game->envido_pending_team < 0 &&
         !has_any_card_been_played(game) &&
         truco_has_flor(game->hands[player]);
}

static int can_answer_flor(const truco_game *game, unsigned int player) {
  unsigned int player_team;

  if (!is_valid_player(game, player) || game->phase != TRUCO_PHASE_PLAYING ||
      game->flor_pending_team < 0) {
    return 0;
  }

  player_team = team_for(game, player);
  return game->flor_pending_team != (int)player_team;
}

static void apply_default_teams(truco_game *game) {
  unsigned int player;

  for (player = 0u; player < TRUCO_MAX_PLAYERS; ++player) {
    game->team_for_player[player] = (unsigned char)(player % TRUCO_MAX_TEAMS);
  }
}

static truco_status start_hand(truco_game *game, unsigned int player) {
  truco_card deck[TRUCO_DECK_SIZE];
  unsigned int seat;
  unsigned int slot;
  unsigned int index = 0u;

  if (game == 0) {
    return TRUCO_ERR_INVALID_ARGUMENT;
  }

  if (game->phase != TRUCO_PHASE_READY && game->phase != TRUCO_PHASE_HAND_OVER) {
    return TRUCO_ERR_INVALID_STATE;
  }
  if (!is_supported_player_count(game->player_count)) {
    return TRUCO_ERR_UNSUPPORTED_RULES;
  }
  if (game->phase == TRUCO_PHASE_READY) {
    game->dealer = game->initial_dealer % game->player_count;
  }
  if (!is_valid_player(game, player) || player != game->dealer) {
    return TRUCO_ERR_NOT_PLAYERS_TURN;
  }

  if (truco_deck(deck, TRUCO_DECK_SIZE) != TRUCO_OK) {
    return TRUCO_ERR_INVALID_STATE;
  }

  truco_shuffle(deck, TRUCO_DECK_SIZE, &game->rng_state);

  memset(game->played_slots, 0, sizeof(game->played_slots));
  memset(game->trick_played, 0, sizeof(game->trick_played));
  for (slot = 0u; slot < TRUCO_HAND_CARDS; ++slot) {
    game->trick_winner_team[slot] = TRUCO_TRICK_UNPLAYED;
    game->trick_winner_player[slot] = TRUCO_NO_TEAM;
  }

  for (slot = 0u; slot < TRUCO_HAND_CARDS; ++slot) {
    for (seat = 0u; seat < game->player_count; ++seat) {
      game->hands[seat][slot] = deck[index++];
    }
  }

  game->mano = (game->dealer + 1u) % game->player_count;
  game->current_player = game->mano;
  game->trick_leader = game->mano;
  game->current_trick = 0u;
  game->truco_value = 1u;
  game->pending_truco_value = 0u;
  game->pending_truco_team = TRUCO_NO_TEAM;
  game->last_truco_team = TRUCO_NO_TEAM;
  game->envido_pending_team = TRUCO_NO_TEAM;
  game->envido_pending_points = 0u;
  game->envido_resolved = 0;
  game->flor_blocks_envido = game->flor_enabled != 0u && hand_has_flor(game);
  game->flor_resolved = game->flor_blocks_envido ? 0 : 1;
  game->flor_pending_team = TRUCO_NO_TEAM;
  game->flor_pending_points = 0u;
  game->last_hand_winner = TRUCO_NO_TEAM;
  game->phase = TRUCO_PHASE_PLAYING;
  game->dealer = (game->dealer + 1u) % game->player_count;

  return TRUCO_OK;
}

static truco_status play_card(truco_game *game, unsigned int player,
                              unsigned int card_index) {
  int hand_winner;

  if (game == 0 || player >= game->player_count ||
      card_index >= TRUCO_HAND_CARDS) {
    return TRUCO_ERR_INVALID_ARGUMENT;
  }

  if (game->phase != TRUCO_PHASE_PLAYING || game->pending_truco_value != 0u ||
      game->envido_pending_team >= 0 || game->flor_pending_team >= 0) {
    return TRUCO_ERR_INVALID_STATE;
  }

  if (player != game->current_player) {
    return TRUCO_ERR_NOT_PLAYERS_TURN;
  }

  if (game->played_slots[player][card_index]) {
    return TRUCO_ERR_CARD_ALREADY_PLAYED;
  }

  if (game->flor_enabled != 0u && !game->flor_resolved &&
      truco_has_flor(game->hands[player])) {
    return TRUCO_ERR_INVALID_STATE;
  }

  game->played_slots[player][card_index] = 1u;
  game->trick_cards[game->current_trick][player] =
      game->hands[player][card_index];
  game->trick_played[game->current_trick][player] = 1u;

  game->current_player = (game->current_player + 1u) % game->player_count;

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
      game->trick_winner_team[game->current_trick] != TRUCO_TRICK_PARDA) {
    game->trick_leader =
        (unsigned int)game->trick_winner_player[game->current_trick];
  }

  game->current_player = game->trick_leader;
  game->current_trick++;

  return TRUCO_OK;
}

static truco_status raise_truco(truco_game *game, unsigned int player) {
  unsigned int player_team;

  if (game == 0 || player >= game->player_count) {
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

static truco_status accept_truco(truco_game *game, unsigned int player) {
  if (game == 0 || player >= game->player_count) {
    return TRUCO_ERR_INVALID_ARGUMENT;
  }

  if (!can_answer_truco(game, player)) {
    return TRUCO_ERR_INVALID_STATE;
  }

  game->truco_value = game->pending_truco_value;
  game->last_truco_team = game->pending_truco_team;
  game->pending_truco_value = 0u;
  game->pending_truco_team = TRUCO_NO_TEAM;

  return TRUCO_OK;
}

static truco_status decline_truco(truco_game *game, unsigned int player) {
  unsigned int winner_team;

  if (game == 0 || player >= game->player_count) {
    return TRUCO_ERR_INVALID_ARGUMENT;
  }

  if (!can_answer_truco(game, player)) {
    return TRUCO_ERR_INVALID_STATE;
  }

  winner_team = (unsigned int)game->pending_truco_team;
  game->pending_truco_value = 0u;
  game->pending_truco_team = TRUCO_NO_TEAM;
  finish_hand(game, winner_team);

  return TRUCO_OK;
}

static truco_status call_flor(truco_game *game, unsigned int player) {
  unsigned int player_team;

  if (game == 0 || player >= game->player_count) {
    return TRUCO_ERR_INVALID_ARGUMENT;
  }

  if (!can_call_flor(game, player)) {
    return TRUCO_ERR_INVALID_STATE;
  }

  player_team = team_for(game, player);
  if (opposing_team_has_flor(game, player_team)) {
    game->flor_pending_team = (int)player_team;
    game->flor_pending_points = TRUCO_FLOR_POINTS;
    return TRUCO_OK;
  }

  add_score(game, player_team, TRUCO_FLOR_POINTS);
  game->flor_resolved = 1;
  return TRUCO_OK;
}

static truco_status accept_flor(truco_game *game, unsigned int player) {
  unsigned int best_points[TRUCO_MAX_TEAMS] = {0u, 0u};
  unsigned int best_player[TRUCO_MAX_TEAMS] = {0u, 0u};
  int seen_team[TRUCO_MAX_TEAMS] = {0, 0};
  unsigned int player_index;
  unsigned int winner_team = 0u;

  if (game == 0 || player >= game->player_count) {
    return TRUCO_ERR_INVALID_ARGUMENT;
  }

  if (!can_answer_flor(game, player)) {
    return TRUCO_ERR_INVALID_STATE;
  }

  for (player_index = 0u; player_index < game->player_count; ++player_index) {
    unsigned int team = team_for(game, player_index);
    unsigned int points = truco_flor_points(game->hands[player_index]);
    if (!truco_has_flor(game->hands[player_index])) {
      continue;
    }
    if (!seen_team[team] || points > best_points[team] ||
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

  add_score(game, winner_team, game->flor_pending_points);
  game->flor_pending_team = TRUCO_NO_TEAM;
  game->flor_pending_points = 0u;
  game->flor_resolved = 1;
  return TRUCO_OK;
}

static truco_status decline_flor(truco_game *game, unsigned int player) {
  unsigned int winner_team;

  if (game == 0 || player >= game->player_count) {
    return TRUCO_ERR_INVALID_ARGUMENT;
  }

  if (!can_answer_flor(game, player)) {
    return TRUCO_ERR_INVALID_STATE;
  }

  winner_team = (unsigned int)game->flor_pending_team;
  add_score(game, winner_team, game->flor_pending_points);
  game->flor_pending_team = TRUCO_NO_TEAM;
  game->flor_pending_points = 0u;
  game->flor_resolved = 1;
  return TRUCO_OK;
}

static truco_status call_envido(truco_game *game, unsigned int player,
                                envido_bid bid) {
  unsigned int player_team;
  unsigned int points;

  if (game == 0 || player >= game->player_count || !is_valid_bid(bid)) {
    return TRUCO_ERR_INVALID_ARGUMENT;
  }

  if (!can_call_envido(game, player, bid)) {
    return TRUCO_ERR_INVALID_STATE;
  }

  player_team = team_for(game, player);
  points = bid == FALTA_ENVIDO ? falta_envido_points(game, player_team)
                               : (unsigned int)bid;

  game->envido_pending_team = (int)player_team;
  game->envido_pending_points = points;

  return TRUCO_OK;
}

static truco_status accept_envido(truco_game *game, unsigned int player) {
  unsigned int best_points[TRUCO_MAX_TEAMS] = {0u, 0u};
  unsigned int best_player[TRUCO_MAX_TEAMS] = {0u, 0u};
  int seen_team[TRUCO_MAX_TEAMS] = {0, 0};
  unsigned int player_index;
  unsigned int winner_team = 0u;

  if (game == 0 || player >= game->player_count) {
    return TRUCO_ERR_INVALID_ARGUMENT;
  }

  if (!can_answer_envido(game, player)) {
    return TRUCO_ERR_INVALID_STATE;
  }

  for (player_index = 0u; player_index < game->player_count; ++player_index) {
    unsigned int team = team_for(game, player_index);
    unsigned int points = truco_envido_points(game->hands[player_index]);
    if (!seen_team[team] || points > best_points[team] ||
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
  game->envido_pending_team = TRUCO_NO_TEAM;
  game->envido_pending_points = 0u;
  game->envido_resolved = 1;

  return TRUCO_OK;
}

static truco_status decline_envido(truco_game *game, unsigned int player) {
  unsigned int winner_team;

  if (game == 0 || player >= game->player_count) {
    return TRUCO_ERR_INVALID_ARGUMENT;
  }

  if (!can_answer_envido(game, player)) {
    return TRUCO_ERR_INVALID_STATE;
  }

  winner_team = (unsigned int)game->envido_pending_team;
  add_score(game, winner_team, 1u);
  game->envido_pending_team = TRUCO_NO_TEAM;
  game->envido_pending_points = 0u;
  game->envido_resolved = 1;

  return TRUCO_OK;
}

static int can_go_to_deck(const truco_game *game, unsigned int player) {
  if (!is_valid_player(game, player) || game->phase != TRUCO_PHASE_PLAYING) {
    return 0;
  }
  if (can_answer_truco(game, player) || can_answer_envido(game, player) ||
      can_answer_flor(game, player)) {
    return 1;
  }
  if (game->pending_truco_value != 0u || game->envido_pending_team >= 0 ||
      game->flor_pending_team >= 0) {
    return 0;
  }
  return player == game->current_player;
}

static truco_status go_to_deck(truco_game *game, unsigned int player) {
  unsigned int player_team;
  unsigned int winner_team;

  if (game == 0 || player >= game->player_count) {
    return TRUCO_ERR_INVALID_ARGUMENT;
  }

  if (!can_go_to_deck(game, player)) {
    return TRUCO_ERR_INVALID_STATE;
  }

  player_team = team_for(game, player);

  if (can_answer_truco(game, player)) {
    winner_team = (unsigned int)game->pending_truco_team;
    game->pending_truco_value = 0u;
    game->pending_truco_team = TRUCO_NO_TEAM;
    if (!game->envido_resolved) {
      add_score(game, winner_team, 1u);
      game->envido_resolved = 1;
    }
    finish_hand(game, winner_team);
    return TRUCO_OK;
  }

  if (can_answer_envido(game, player)) {
    winner_team = (unsigned int)game->envido_pending_team;
    add_score(game, winner_team, 1u);
    game->envido_pending_team = TRUCO_NO_TEAM;
    game->envido_pending_points = 0u;
    game->envido_resolved = 1;
    finish_hand(game, opposing_team(player_team));
    return TRUCO_OK;
  }

  if (can_answer_flor(game, player)) {
    winner_team = (unsigned int)game->flor_pending_team;
    add_score(game, winner_team, game->flor_pending_points);
    game->flor_pending_team = TRUCO_NO_TEAM;
    game->flor_pending_points = 0u;
    game->flor_resolved = 1;
    return TRUCO_OK;
  }

  winner_team = opposing_team(player_team);
  if (!game->envido_resolved && !game->flor_blocks_envido) {
    add_score(game, winner_team, 1u);
    game->envido_resolved = 1;
  }
  if (!game->flor_resolved && game->flor_blocks_envido) {
    add_score(game, winner_team, TRUCO_FLOR_POINTS);
    game->flor_resolved = 1;
  }
  finish_hand(game, winner_team);
  return TRUCO_OK;
}

static void add_legal_command(truco_legal_commands *out,
                              truco_command command) {
  if (out->count < TRUCO_MAX_LEGAL_COMMANDS) {
    out->commands[out->count++] = command;
  }
}

