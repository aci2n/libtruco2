#include "truco.h"

#include <stdio.h>
#include <stdlib.h>

static const char *suit_name(truco_suit suit)
{
    switch (suit) {
    case TRUCO_SUIT_ESPADA:
        return "espada";
    case TRUCO_SUIT_BASTO:
        return "basto";
    case TRUCO_SUIT_ORO:
        return "oro";
    case TRUCO_SUIT_COPA:
        return "copa";
    default:
        return "?";
    }
}

int main(void)
{
    truco_game *game = truco_game_create();
    unsigned int player;
    unsigned int slot;

    if (game == 0) {
        fprintf(stderr, "could not allocate truco game\n");
        return 1;
    }

    if (truco_game_set_player_count(game, 4u) != TRUCO_OK ||
        truco_game_set_seed(game, 42u) != TRUCO_OK ||
        truco_game_init(game) != TRUCO_OK ||
        truco_game_apply(game, 0u, TRUCO_CMD_START_HAND) != TRUCO_OK) {
        fprintf(stderr, "could not start truco game\n");
        truco_game_destroy(game);
        return 1;
    }

    printf("current player: %u\n", truco_game_current_player(game));
    for (player = 0u; player < truco_game_player_count(game); ++player) {
        printf("player %u team %u:", player, truco_game_team_for_player(game, player));
        for (slot = 0u; slot < TRUCO_HAND_CARDS; ++slot) {
            truco_suit suit;
            unsigned int rank;

            if (truco_game_hand_card(game, player, slot, &suit, &rank) != TRUCO_OK) {
                fprintf(stderr, "could not read hand\n");
                truco_game_destroy(game);
                return 1;
            }
            printf(" %u-%s", rank, suit_name(suit));
        }
        printf(" envido=%u\n", truco_game_envido_points(game, player));
    }

    truco_game_destroy(game);
    return 0;
}
