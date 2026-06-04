#ifndef TRUCO_INTERNAL_H
#define TRUCO_INTERNAL_H

#include "truco.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TRUCO_DECK_SIZE 40u

truco_card truco_make_card(truco_suit suit, unsigned int rank);
int truco_card_is_valid(truco_card card);
int truco_card_power(truco_card card);
int truco_card_compare(truco_card left, truco_card right);
unsigned int truco_envido_points(const truco_card cards[TRUCO_HAND_CARDS]);

truco_status truco_deck(truco_card *cards, size_t count);
truco_status truco_shuffle(truco_card *cards, size_t count, unsigned int *seed);

#ifdef __cplusplus
}
#endif

#endif
