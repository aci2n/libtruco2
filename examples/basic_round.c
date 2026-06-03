#include "truco.h"

#include <stdio.h>

static const char *suit_name(unsigned int suit)
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
    truco_config config;
    truco_game game;
    unsigned int player;
    unsigned int slot;

    truco_config_default(&config, 4u);
    config.seed = 42u;

    if (truco_game_init(&game, &config) != TRUCO_OK ||
        truco_game_apply(&game, 0u, TRUCO_CMD_START_HAND) != TRUCO_OK) {
        fprintf(stderr, "could not start truco game\n");
        return 1;
    }

    printf("current player: %u\n", truco_game_current_player(&game));
    for (player = 0u; player < truco_game_player_count(&game); ++player) {
        printf("player %u team %u:", player, truco_game_team_for_player(&game, player));
        for (slot = 0u; slot < TRUCO_HAND_CARDS; ++slot) {
            truco_card card = game.hands[player][slot];
            printf(" %u-%s", (unsigned int)card.rank, suit_name(card.suit));
        }
        printf(" envido=%u\n", truco_envido_points(game.hands[player]));
    }

    return 0;
}
