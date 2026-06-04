#include "truco_hand_internal.h"

#include <stdlib.h>
#include <string.h>
#ifdef DEBUG
#include <assert.h>
#endif

typedef enum envido_bid {
  ENVIDO = 2,
  REAL_ENVIDO = 3,
  FALTA_ENVIDO = -1
} envido_bid;

static truco_event_log *active_log;

static int is_valid_player(const truco_game *game, unsigned int player);

/* static helpers */

static int is_supported_player_count(unsigned int player_count) {
  return player_count == 2u || player_count == 4u;
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
      if (game->hand.played_slots[player][slot]) {
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
      (left_player + player_count - game->hand.mano) % player_count;
  unsigned int right_distance =
      (right_player + player_count - game->hand.mano) % player_count;

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
  if (active_log != 0) {
    truco_event_log_emit_score(active_log, team, points);
  }
  if (game->score[team] >= game->target_score) {
    game->phase = TRUCO_PHASE_GAME_OVER;
    if (active_log != 0) {
      truco_event_log_emit_game_over(active_log);
    }
  }
}

/* returns team 0/1, TRUCO_HAND_UNDECIDED, or mano team when all tricks are parda */
static int compute_hand_winner(const truco_game *game) {
  int first = game->hand.trick_winner_team[0];
  int second = game->hand.trick_winner_team[1];
  int third = game->hand.trick_winner_team[2];
  unsigned int mano_team = team_for(game, game->hand.mano);

  if (game->hand.current_trick == 0u) {
    return TRUCO_HAND_UNDECIDED;
  }

  if (game->hand.current_trick == 1u) {
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
    if (!game->hand.trick_played[game->hand.current_trick][player]) {
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
          game->hand.trick_cards[game->hand.current_trick][player],
          game->hand.trick_cards[game->hand.current_trick][best_player]);
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
    game->hand.trick_winner_team[game->hand.current_trick] = TRUCO_TRICK_PARDA;
    game->hand.trick_winner_player[game->hand.current_trick] = (int)game->hand.trick_leader;
    if (active_log != 0) {
      truco_event_log_emit_trick_won(active_log, game->hand.current_trick,
                                     TRUCO_TRICK_PARDA,
                                     game->hand.trick_leader);
    }
  } else {
    game->hand.trick_winner_team[game->hand.current_trick] = best_team;
    game->hand.trick_winner_player[game->hand.current_trick] = best_player;
    if (active_log != 0) {
      truco_event_log_emit_trick_won(active_log, game->hand.current_trick,
                                     (unsigned int)best_team,
                                     (unsigned int)best_player);
    }
  }
}

static void finish_hand(truco_game *game, unsigned int winner_team) {
  game->last_hand_winner = (int)winner_team;
  add_score(game, winner_team, game->hand.truco_value);
  if (active_log != 0) {
    truco_event_log_emit_hand_finished(active_log, winner_team);
  }
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

static int hand_is_playing(const truco_game *game)
{
    return game != 0 && game->phase == TRUCO_PHASE_PLAYING;
}

static int no_bid_interrupt_pending(const truco_game *game)
{
    return game->hand.pending_truco_value == 0u &&
           game->hand.envido_pending_team < 0 && game->hand.flor_pending_team < 0;
}

static int is_current_player(const truco_game *game, unsigned int player)
{
    return is_valid_player(game, player) && player == game->hand.current_player;
}

static int flor_blocks_player_play(const truco_game *game, unsigned int player)
{
    return game->flor_enabled != 0u && !game->hand.flor_resolved &&
           truco_has_flor(game->hand.hands[player]);
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
  unsigned int value = game->hand.truco_value + 1u;
  return value < 2u ? 2u : value;
}

static int can_play_card(const truco_game *game, unsigned int player,
                         unsigned int card_index) {
  return is_valid_player(game, player) && card_index < TRUCO_HAND_CARDS &&
         hand_is_playing(game) && no_bid_interrupt_pending(game) &&
         is_current_player(game, player) &&
         !game->hand.played_slots[player][card_index] &&
         !flor_blocks_player_play(game, player);
}

static int can_raise_truco(const truco_game *game, unsigned int player) {
  unsigned int player_team;

  /* TRUCO_TRUCO_MAX_VALUE is vale cuatro; same team cannot raise twice in a row */
  if (!is_valid_player(game, player) || game->phase != TRUCO_PHASE_PLAYING ||
      player != game->hand.current_player || game->hand.pending_truco_value != 0u ||
      game->hand.envido_pending_team >= 0 || game->hand.flor_pending_team >= 0 ||
      game->hand.truco_value >= TRUCO_TRUCO_MAX_VALUE) {
    return 0;
  }

  player_team = team_for(game, player);
  return game->hand.last_truco_team != (int)player_team;
}

static int can_answer_truco(const truco_game *game, unsigned int player) {
  unsigned int player_team;

  if (!is_valid_player(game, player) || game->phase != TRUCO_PHASE_PLAYING ||
      game->hand.pending_truco_value == 0u || game->hand.envido_pending_team >= 0) {
    return 0;
  }

  player_team = team_for(game, player);
  return game->hand.pending_truco_team != (int)player_team;
}

static int envido_phase_open(const truco_game *game) {
  return !game->hand.envido_resolved && !has_any_card_been_played(game) &&
         !game->hand.flor_blocks_envido;
}

static int can_call_envido_initial(const truco_game *game, unsigned int player,
                                   envido_bid bid) {
  if (!is_valid_player(game, player) || !is_valid_bid(bid) ||
      game->phase != TRUCO_PHASE_PLAYING || game->hand.envido_pending_team >= 0 ||
      !envido_phase_open(game)) {
    return 0;
  }

  if (game->flor_enabled && truco_has_flor(game->hand.hands[player])) {
    return 0;
  }

  if (game->hand.pending_truco_value != 0u) {
    return can_answer_truco(game, player);
  }

  if (player != game->hand.current_player || game->hand.flor_pending_team >= 0) {
    return 0;
  }

  return 1;
}

static int can_counter_envido(const truco_game *game, unsigned int player,
                              envido_bid bid) {
  unsigned int player_team;

  if (!is_valid_player(game, player) || !is_valid_bid(bid) ||
      game->phase != TRUCO_PHASE_PLAYING || game->hand.envido_pending_team < 0 ||
      !envido_phase_open(game)) {
    return 0;
  }

  if (game->flor_enabled && truco_has_flor(game->hand.hands[player])) {
    return 0;
  }

  player_team = team_for(game, player);
  return game->hand.envido_pending_team != (int)player_team;
}

static int can_answer_envido(const truco_game *game, unsigned int player) {
  unsigned int player_team;

  if (!is_valid_player(game, player) || game->phase != TRUCO_PHASE_PLAYING ||
      game->hand.envido_pending_team < 0) {
    return 0;
  }

  player_team = team_for(game, player);
  return game->hand.envido_pending_team != (int)player_team;
}

static int hand_has_flor(const truco_game *game) {
  unsigned int player;

  for (player = 0u; player < game->player_count; ++player) {
    if (truco_has_flor(game->hand.hands[player])) {
      return 1;
    }
  }

  return 0;
}

static int opposing_team_has_flor(const truco_game *game, unsigned int team) {
  unsigned int player;

  for (player = 0u; player < game->player_count; ++player) {
    if (team_for(game, player) != team && truco_has_flor(game->hand.hands[player])) {
      return 1;
    }
  }

  return 0;
}

static int can_call_flor(const truco_game *game, unsigned int player) {
  return is_valid_player(game, player) && game->flor_enabled != 0u &&
         game->phase == TRUCO_PHASE_PLAYING && player == game->hand.current_player &&
         !game->hand.flor_resolved && game->hand.flor_pending_team < 0 &&
         game->hand.pending_truco_value == 0u && game->hand.envido_pending_team < 0 &&
         !has_any_card_been_played(game) &&
         truco_has_flor(game->hand.hands[player]);
}

static int can_answer_flor(const truco_game *game, unsigned int player) {
  unsigned int player_team;

  if (!is_valid_player(game, player) || game->phase != TRUCO_PHASE_PLAYING ||
      game->hand.flor_pending_team < 0) {
    return 0;
  }

  player_team = team_for(game, player);
  return game->hand.flor_pending_team != (int)player_team;
}

static truco_status hand_start(truco_game *game, unsigned int player) {
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

  memset(game->hand.played_slots, 0, sizeof(game->hand.played_slots));
  memset(game->hand.trick_played, 0, sizeof(game->hand.trick_played));
  for (slot = 0u; slot < TRUCO_HAND_CARDS; ++slot) {
    game->hand.trick_winner_team[slot] = TRUCO_TRICK_UNPLAYED;
    game->hand.trick_winner_player[slot] = TRUCO_NO_TEAM;
  }

  for (slot = 0u; slot < TRUCO_HAND_CARDS; ++slot) {
    for (seat = 0u; seat < game->player_count; ++seat) {
      game->hand.hands[seat][slot] = deck[index++];
    }
  }

  game->hand.mano = (game->dealer + 1u) % game->player_count;
  game->hand.current_player = game->hand.mano;
  game->hand.trick_leader = game->hand.mano;
  game->hand.current_trick = 0u;
  game->hand.truco_value = 1u;
  game->hand.pending_truco_value = 0u;
  game->hand.pending_truco_team = TRUCO_NO_TEAM;
  game->hand.last_truco_team = TRUCO_NO_TEAM;
  game->hand.envido_pending_team = TRUCO_NO_TEAM;
  game->hand.envido_pending_points = 0u;
  game->hand.envido_resolved = 0;
  game->hand.flor_blocks_envido = game->flor_enabled != 0u && hand_has_flor(game);
  game->hand.flor_resolved = game->hand.flor_blocks_envido ? 0 : 1;
  game->hand.flor_pending_team = TRUCO_NO_TEAM;
  game->hand.flor_pending_points = 0u;
  game->last_hand_winner = TRUCO_NO_TEAM;
  game->phase = TRUCO_PHASE_PLAYING;
  game->dealer = (game->dealer + 1u) % game->player_count;

  if (active_log != 0) {
    truco_event_log_emit_hand_started(active_log, game->hand.mano);
  }

  return TRUCO_OK;
}

static truco_status play_card(truco_game *game, unsigned int player,
                              unsigned int card_index) {
  int hand_winner;

  if (game == 0 || player >= game->player_count ||
      card_index >= TRUCO_HAND_CARDS) {
    return TRUCO_ERR_INVALID_ARGUMENT;
  }

  if (!hand_is_playing(game) || !no_bid_interrupt_pending(game)) {
    return TRUCO_ERR_INVALID_STATE;
  }

  if (player != game->hand.current_player) {
    return TRUCO_ERR_NOT_PLAYERS_TURN;
  }

  if (game->hand.played_slots[player][card_index]) {
    return TRUCO_ERR_CARD_ALREADY_PLAYED;
  }

  if (flor_blocks_player_play(game, player)) {
    return TRUCO_ERR_INVALID_STATE;
  }

  game->hand.played_slots[player][card_index] = 1u;
  game->hand.trick_cards[game->hand.current_trick][player] =
      game->hand.hands[player][card_index];
  game->hand.trick_played[game->hand.current_trick][player] = 1u;

  if (active_log != 0) {
    truco_event_log_emit_card_played(
        active_log, player, game->hand.current_trick,
        (unsigned char)card_index,
        game->hand.trick_cards[game->hand.current_trick][player]);
  }

  game->hand.current_player = (game->hand.current_player + 1u) % game->player_count;

  if (game->hand.current_player != game->hand.trick_leader) {
    return TRUCO_OK;
  }

  resolve_current_trick(game);
  hand_winner = compute_hand_winner(game);
  if (hand_winner >= 0) {
    finish_hand(game, (unsigned int)hand_winner);
    return TRUCO_OK;
  }

  if (game->hand.current_trick + 1u >= TRUCO_HAND_CARDS) {
    finish_hand(game, team_for(game, game->hand.mano));
    return TRUCO_OK;
  }

  if (game->hand.trick_winner_player[game->hand.current_trick] >= 0 &&
      game->hand.trick_winner_team[game->hand.current_trick] != TRUCO_TRICK_PARDA) {
    game->hand.trick_leader =
        (unsigned int)game->hand.trick_winner_player[game->hand.current_trick];
  }

  game->hand.current_player = game->hand.trick_leader;
  game->hand.current_trick++;

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
  game->hand.pending_truco_team = (int)player_team;
  game->hand.pending_truco_value = next_truco_value(game);

  if (active_log != 0) {
    truco_event_log_emit_simple(active_log, TRUCO_EVENT_TRUCO_RAISED, player,
                                player_team, game->hand.pending_truco_value);
  }

  return TRUCO_OK;
}

static truco_status accept_truco(truco_game *game, unsigned int player) {
  if (game == 0 || player >= game->player_count) {
    return TRUCO_ERR_INVALID_ARGUMENT;
  }

  if (!can_answer_truco(game, player)) {
    return TRUCO_ERR_INVALID_STATE;
  }

  game->hand.truco_value = game->hand.pending_truco_value;
  game->hand.last_truco_team = game->hand.pending_truco_team;
  game->hand.pending_truco_value = 0u;
  game->hand.pending_truco_team = TRUCO_NO_TEAM;

  if (active_log != 0) {
    truco_event_log_emit_simple(active_log, TRUCO_EVENT_TRUCO_ACCEPTED, player,
                                team_for(game, player), game->hand.truco_value);
  }

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

  winner_team = (unsigned int)game->hand.pending_truco_team;
  game->hand.pending_truco_value = 0u;
  game->hand.pending_truco_team = TRUCO_NO_TEAM;
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
    game->hand.flor_pending_team = (int)player_team;
    game->hand.flor_pending_points = TRUCO_FLOR_POINTS;
    return TRUCO_OK;
  }

  add_score(game, player_team, TRUCO_FLOR_POINTS);
  game->hand.flor_resolved = 1;
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
    unsigned int points = truco_flor_points(game->hand.hands[player_index]);
    if (!truco_has_flor(game->hand.hands[player_index])) {
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

  add_score(game, winner_team, game->hand.flor_pending_points);
  game->hand.flor_pending_team = TRUCO_NO_TEAM;
  game->hand.flor_pending_points = 0u;
  game->hand.flor_resolved = 1;
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

  winner_team = (unsigned int)game->hand.flor_pending_team;
  add_score(game, winner_team, game->hand.flor_pending_points);
  game->hand.flor_pending_team = TRUCO_NO_TEAM;
  game->hand.flor_pending_points = 0u;
  game->hand.flor_resolved = 1;
  return TRUCO_OK;
}

static truco_status call_envido(truco_game *game, unsigned int player,
                                envido_bid bid) {
  unsigned int player_team;
  unsigned int points;

  if (game == 0 || player >= game->player_count || !is_valid_bid(bid)) {
    return TRUCO_ERR_INVALID_ARGUMENT;
  }

  if (game->hand.envido_pending_team >= 0) {
    if (!can_counter_envido(game, player, bid)) {
      return TRUCO_ERR_INVALID_STATE;
    }

    player_team = team_for(game, player);
    if (bid == FALTA_ENVIDO) {
      game->hand.envido_pending_points = falta_envido_points(game, player_team);
    } else {
      game->hand.envido_pending_points += (unsigned int)bid;
    }
    game->hand.envido_pending_team = (int)player_team;
    if (active_log != 0) {
      truco_event_log_emit_simple(active_log, TRUCO_EVENT_ENVIDO_CALLED, player,
                                  player_team, game->hand.envido_pending_points);
    }
    return TRUCO_OK;
  }

  if (!can_call_envido_initial(game, player, bid)) {
    return TRUCO_ERR_INVALID_STATE;
  }

  player_team = team_for(game, player);
  points = bid == FALTA_ENVIDO ? falta_envido_points(game, player_team)
                               : (unsigned int)bid;

  game->hand.envido_pending_team = (int)player_team;
  game->hand.envido_pending_points = points;

  if (active_log != 0) {
    truco_event_log_emit_simple(active_log, TRUCO_EVENT_ENVIDO_CALLED, player,
                                player_team, points);
  }

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
    unsigned int points = truco_envido_points(game->hand.hands[player_index]);
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

  add_score(game, winner_team, game->hand.envido_pending_points);
  game->hand.envido_pending_team = TRUCO_NO_TEAM;
  game->hand.envido_pending_points = 0u;
  game->hand.envido_resolved = 1;

  if (active_log != 0) {
    truco_event_log_emit_simple(active_log, TRUCO_EVENT_ENVIDO_ACCEPTED, player,
                                winner_team, 0u);
  }

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

  winner_team = (unsigned int)game->hand.envido_pending_team;
  add_score(game, winner_team, 1u);
  game->hand.envido_pending_team = TRUCO_NO_TEAM;
  game->hand.envido_pending_points = 0u;
  game->hand.envido_resolved = 1;

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
  if (game->hand.pending_truco_value != 0u || game->hand.envido_pending_team >= 0 ||
      game->hand.flor_pending_team >= 0) {
    return 0;
  }
  return player == game->hand.current_player;
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

  if (can_answer_envido(game, player)) {
    winner_team = (unsigned int)game->hand.envido_pending_team;
    add_score(game, winner_team, 1u);
    game->hand.envido_pending_team = TRUCO_NO_TEAM;
    game->hand.envido_pending_points = 0u;
    game->hand.envido_resolved = 1;
    if (game->hand.pending_truco_value != 0u) {
      return TRUCO_OK;
    }
    finish_hand(game, opposing_team(player_team));
    return TRUCO_OK;
  }

  if (can_answer_flor(game, player)) {
    winner_team = (unsigned int)game->hand.flor_pending_team;
    add_score(game, winner_team, game->hand.flor_pending_points);
    game->hand.flor_pending_team = TRUCO_NO_TEAM;
    game->hand.flor_pending_points = 0u;
    game->hand.flor_resolved = 1;
    return TRUCO_OK;
  }

  if (can_answer_truco(game, player)) {
    winner_team = (unsigned int)game->hand.pending_truco_team;
    game->hand.pending_truco_value = 0u;
    game->hand.pending_truco_team = TRUCO_NO_TEAM;
    if (!game->hand.envido_resolved) {
      add_score(game, winner_team, 1u);
      game->hand.envido_resolved = 1;
    }
    finish_hand(game, winner_team);
    return TRUCO_OK;
  }

  winner_team = opposing_team(player_team);
  if (!game->hand.envido_resolved && !game->hand.flor_blocks_envido) {
    add_score(game, winner_team, 1u);
    game->hand.envido_resolved = 1;
  }
  if (!game->hand.flor_resolved && game->hand.flor_blocks_envido) {
    add_score(game, winner_team, TRUCO_FLOR_POINTS);
    game->hand.flor_resolved = 1;
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

void truco_hand_clear(truco_hand *hand)
{
    unsigned int slot;

    if (hand == 0) {
        return;
    }

    memset(hand, 0, sizeof(*hand));
    hand->pending_truco_team = TRUCO_NO_TEAM;
    hand->last_truco_team = TRUCO_NO_TEAM;
    hand->envido_pending_team = TRUCO_NO_TEAM;
    hand->flor_pending_team = TRUCO_NO_TEAM;
    for (slot = 0u; slot < TRUCO_HAND_CARDS; ++slot) {
        hand->trick_winner_team[slot] = TRUCO_TRICK_UNPLAYED;
        hand->trick_winner_player[slot] = TRUCO_NO_TEAM;
    }
}

truco_hand_subphase truco_hand_subphase_of(const truco_game *game)
{
    if (!hand_is_playing(game)) {
        return TRUCO_HAND_SUB_NONE;
    }
    if (game->hand.envido_pending_team >= 0) {
        return TRUCO_HAND_SUB_ENVIDO_PENDING;
    }
    if (game->hand.flor_pending_team >= 0) {
        return TRUCO_HAND_SUB_FLOR_PENDING;
    }
    if (game->hand.pending_truco_value != 0u) {
        return TRUCO_HAND_SUB_TRUCO_PENDING;
    }
    return TRUCO_HAND_SUB_TRICK;
}

#ifdef DEBUG
static void assert_hand_invariants(const truco_game *game)
{
    if (!hand_is_playing(game)) {
        return;
    }
    if (game->hand.flor_pending_team >= 0) {
        assert(game->hand.envido_pending_team < 0);
        assert(game->hand.pending_truco_value == 0u);
    }
}
#endif

static void legal_add_trick_play_commands(const truco_game *game,
                                          unsigned int player,
                                          truco_legal_commands *out)
{
    unsigned int slot;

    for (slot = 0u; slot < TRUCO_HAND_CARDS; ++slot) {
        if (can_play_card(game, player, slot)) {
            add_legal_command(out, (truco_command)(TRUCO_CMD_PLAY_CARD_0 + slot));
        }
    }
}

static void legal_add_truco_commands(const truco_game *game,
                                     unsigned int player,
                                     truco_legal_commands *out)
{
    if (can_raise_truco(game, player)) {
        add_legal_command(out, TRUCO_CMD_RAISE_TRUCO);
    }
    if (can_answer_truco(game, player)) {
        add_legal_command(out, TRUCO_CMD_ACCEPT_BID);
        add_legal_command(out, TRUCO_CMD_REJECT_BID);
    }
}

static void legal_add_flor_commands(const truco_game *game,
                                    unsigned int player,
                                    truco_legal_commands *out)
{
    if (can_call_flor(game, player)) {
        add_legal_command(out, TRUCO_CMD_CALL_FLOR);
    }
    if (can_answer_flor(game, player)) {
        add_legal_command(out, TRUCO_CMD_ACCEPT_BID);
        add_legal_command(out, TRUCO_CMD_REJECT_BID);
    }
}

static void legal_add_envido_commands(const truco_game *game,
                                      unsigned int player,
                                      truco_legal_commands *out)
{
    if (can_call_envido_initial(game, player, ENVIDO)) {
        add_legal_command(out, TRUCO_CMD_CALL_ENVIDO);
    }
    if (can_call_envido_initial(game, player, REAL_ENVIDO)) {
        add_legal_command(out, TRUCO_CMD_CALL_REAL_ENVIDO);
    }
    if (can_call_envido_initial(game, player, FALTA_ENVIDO)) {
        add_legal_command(out, TRUCO_CMD_CALL_FALTA_ENVIDO);
    }
    if (can_answer_envido(game, player)) {
        add_legal_command(out, TRUCO_CMD_ACCEPT_BID);
        add_legal_command(out, TRUCO_CMD_REJECT_BID);
        if (can_counter_envido(game, player, ENVIDO)) {
            add_legal_command(out, TRUCO_CMD_CALL_ENVIDO);
        }
        if (can_counter_envido(game, player, REAL_ENVIDO)) {
            add_legal_command(out, TRUCO_CMD_CALL_REAL_ENVIDO);
        }
        if (can_counter_envido(game, player, FALTA_ENVIDO)) {
            add_legal_command(out, TRUCO_CMD_CALL_FALTA_ENVIDO);
        }
    }
}

truco_status truco_hand_legal_commands(const truco_game *game,
                                       unsigned int player,
                                       truco_legal_commands *out)
{
    if (out == 0 || !is_valid_player(game, player)) {
        return TRUCO_ERR_INVALID_ARGUMENT;
    }

    memset(out, 0, sizeof(*out));

    if (can_start_hand(game, player)) {
        add_legal_command(out, TRUCO_CMD_START_HAND);
        return TRUCO_OK;
    }

    if (!hand_is_playing(game)) {
        return TRUCO_OK;
    }

    legal_add_trick_play_commands(game, player, out);
    legal_add_truco_commands(game, player, out);
    legal_add_flor_commands(game, player, out);
    legal_add_envido_commands(game, player, out);

    if (can_go_to_deck(game, player)) {
        add_legal_command(out, TRUCO_CMD_GO_TO_DECK);
    }

    return TRUCO_OK;
}

truco_status truco_hand_apply(truco_game *game,
                              unsigned int player,
                              truco_command command,
                              truco_event_log *log)
{
    truco_status status;

    if (!is_valid_player(game, player)) {
        return TRUCO_ERR_INVALID_ARGUMENT;
    }

    active_log = log;

    switch (command) {
    case TRUCO_CMD_START_HAND:
        status = hand_start(game, player);
        break;
    case TRUCO_CMD_PLAY_CARD_0:
        status = play_card(game, player, 0u);
        break;
    case TRUCO_CMD_PLAY_CARD_1:
        status = play_card(game, player, 1u);
        break;
    case TRUCO_CMD_PLAY_CARD_2:
        status = play_card(game, player, 2u);
        break;
    case TRUCO_CMD_RAISE_TRUCO:
        status = raise_truco(game, player);
        break;
    case TRUCO_CMD_CALL_ENVIDO:
        status = call_envido(game, player, ENVIDO);
        break;
    case TRUCO_CMD_CALL_REAL_ENVIDO:
        status = call_envido(game, player, REAL_ENVIDO);
        break;
    case TRUCO_CMD_CALL_FALTA_ENVIDO:
        status = call_envido(game, player, FALTA_ENVIDO);
        break;
    case TRUCO_CMD_CALL_FLOR:
        status = call_flor(game, player);
        break;
    case TRUCO_CMD_ACCEPT_BID:
        if (can_answer_envido(game, player)) {
            status = accept_envido(game, player);
        } else if (can_answer_flor(game, player)) {
            status = accept_flor(game, player);
        } else if (can_answer_truco(game, player)) {
            status = accept_truco(game, player);
        } else {
            status = TRUCO_ERR_INVALID_STATE;
        }
        break;
    case TRUCO_CMD_REJECT_BID:
        if (can_answer_envido(game, player)) {
            status = decline_envido(game, player);
        } else if (can_answer_flor(game, player)) {
            status = decline_flor(game, player);
        } else if (can_answer_truco(game, player)) {
            status = decline_truco(game, player);
        } else {
            status = TRUCO_ERR_INVALID_STATE;
        }
        break;
    case TRUCO_CMD_GO_TO_DECK:
        status = go_to_deck(game, player);
        break;
    case TRUCO_CMD_NONE:
    default:
        status = TRUCO_ERR_INVALID_ARGUMENT;
        break;
    }

    active_log = 0;

#ifdef DEBUG
    if (status == TRUCO_OK) {
        assert_hand_invariants(game);
    }
#endif

    return status;
}

