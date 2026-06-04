#include "truco_internal.h"

#include <stdlib.h>

static unsigned int card_face_value(truco_card card)
{
    if (card.rank >= 10u) {
        return 0u;
    }
    return card.rank;
}

static unsigned int next_random(unsigned int *state)
{
    *state = (*state * 1664525u) + 1013904223u;
    return *state;
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
