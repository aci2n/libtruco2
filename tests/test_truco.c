#include "truco.h"

#include <stdio.h>
#include <stdlib.h>

static unsigned int tests_run = 0u;

#define CHECK(expr)                                                          \
    do {                                                                     \
        ++tests_run;                                                         \
        if (!(expr)) {                                                       \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
            exit(1);                                                         \
        }                                                                    \
    } while (0)

static void expect_ok(truco_status status)
{
    CHECK(status == TRUCO_OK);
}

static int has_command(const truco_legal_actions *actions, truco_command command)
{
    unsigned int index;

    for (index = 0u; index < actions->count; ++index) {
        if (actions->commands[index] == command) {
            return 1;
        }
    }

    return 0;
}

static void test_card_ranking(void)
{
    truco_card one_espada = truco_make_card(TRUCO_SUIT_ESPADA, 1u);
    truco_card one_basto = truco_make_card(TRUCO_SUIT_BASTO, 1u);
    truco_card seven_espada = truco_make_card(TRUCO_SUIT_ESPADA, 7u);
    truco_card seven_oro = truco_make_card(TRUCO_SUIT_ORO, 7u);
    truco_card three_copa = truco_make_card(TRUCO_SUIT_COPA, 3u);
    truco_card three_oro = truco_make_card(TRUCO_SUIT_ORO, 3u);
    truco_card four_basto = truco_make_card(TRUCO_SUIT_BASTO, 4u);

    CHECK(truco_card_compare(one_espada, one_basto) > 0);
    CHECK(truco_card_compare(one_basto, seven_espada) > 0);
    CHECK(truco_card_compare(seven_espada, seven_oro) > 0);
    CHECK(truco_card_compare(seven_oro, three_copa) > 0);
    CHECK(truco_card_compare(three_copa, three_oro) == 0);
    CHECK(truco_card_compare(four_basto, one_espada) < 0);
    CHECK(!truco_card_is_valid(truco_make_card(TRUCO_SUIT_COPA, 8u)));
}

static void test_deck_and_envido_values(void)
{
    truco_card deck[TRUCO_DECK_SIZE];
    truco_card thirty_three[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_ESPADA, 7u},
        {TRUCO_SUIT_ESPADA, 6u},
        {TRUCO_SUIT_ORO, 12u}
    };
    truco_card face_cards[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_COPA, 12u},
        {TRUCO_SUIT_ORO, 11u},
        {TRUCO_SUIT_BASTO, 10u}
    };

    expect_ok(truco_deck(deck, TRUCO_DECK_SIZE));
    CHECK(truco_envido_points(thirty_three) == 33u);
    CHECK(truco_envido_points(face_cards) == 0u);
}

static void start_two_player_hand(truco_game *game)
{
    truco_config config;

    truco_config_default(&config, 2u);
    truco_config_set_seed(&config, 7u);
    truco_config_set_initial_dealer(&config, 1u);
    expect_ok(truco_game_init(game, &config));
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_START_HAND));
    CHECK(truco_game_current_player(game) == 0u);
}

static void force_hand(truco_game *game,
                       unsigned int player,
                       const truco_card cards[TRUCO_HAND_CARDS])
{
    expect_ok(truco_game_set_hand(game, player, cards));
}

static void install_basic_two_player_hands(truco_game *game)
{
    truco_card player0[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_ESPADA, 1u},
        {TRUCO_SUIT_COPA, 4u},
        {TRUCO_SUIT_COPA, 5u}
    };
    truco_card player1[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_ORO, 7u},
        {TRUCO_SUIT_COPA, 6u},
        {TRUCO_SUIT_ORO, 4u}
    };

    force_hand(game, 0u, player0);
    force_hand(game, 1u, player1);
}

static void play_basic_two_player_hand(truco_game *game)
{
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_PLAY_CARD_0));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_PLAY_CARD_0));
    CHECK(truco_game_trick_winner(game, 0u) == 0);
    CHECK(truco_game_current_player(game) == 0u);

    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_PLAY_CARD_1));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_PLAY_CARD_1));
    CHECK(truco_game_trick_winner(game, 1u) == 1);
    CHECK(truco_game_current_player(game) == 1u);

    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_PLAY_CARD_2));
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_PLAY_CARD_2));
}

static void test_two_player_hand_resolution(void)
{
    truco_game game;

    start_two_player_hand(&game);
    CHECK(truco_game_apply(&game, 0u, TRUCO_CMD_START_HAND) == TRUCO_ERR_INVALID_STATE);
    install_basic_two_player_hands(&game);
    play_basic_two_player_hand(&game);

    CHECK(truco_game_phase(&game) == TRUCO_PHASE_HAND_OVER);
    CHECK(truco_game_hand_winner(&game) == 0);
    CHECK(truco_game_score(&game, 0u) == 1u);
    CHECK(truco_game_score(&game, 1u) == 0u);
}

static void test_truco_bidding(void)
{
    truco_game game;

    start_two_player_hand(&game);
    install_basic_two_player_hands(&game);
    expect_ok(truco_game_apply(&game, 0u, TRUCO_CMD_RAISE_TRUCO));
    expect_ok(truco_game_apply(&game, 1u, TRUCO_CMD_ACCEPT_BID));
    play_basic_two_player_hand(&game);
    CHECK(truco_game_score(&game, 0u) == 2u);

    start_two_player_hand(&game);
    expect_ok(truco_game_apply(&game, 0u, TRUCO_CMD_RAISE_TRUCO));
    expect_ok(truco_game_apply(&game, 1u, TRUCO_CMD_REJECT_BID));
    CHECK(truco_game_phase(&game) == TRUCO_PHASE_HAND_OVER);
    CHECK(truco_game_score(&game, 0u) == 1u);
}

static void test_legal_actions(void)
{
    truco_config config;
    truco_game game;
    truco_legal_actions actions;

    truco_config_default(&config, 2u);
    truco_config_set_initial_dealer(&config, 1u);
    expect_ok(truco_game_init(&game, &config));

    expect_ok(truco_game_legal_actions(&game, 0u, &actions));
    CHECK(actions.count == 1u);
    CHECK(actions.commands[0] == TRUCO_CMD_START_HAND);

    expect_ok(truco_game_apply(&game, 0u, TRUCO_CMD_START_HAND));
    install_basic_two_player_hands(&game);

    expect_ok(truco_game_legal_actions(&game, 0u, &actions));
    CHECK(has_command(&actions, TRUCO_CMD_PLAY_CARD_0));
    CHECK(has_command(&actions, TRUCO_CMD_PLAY_CARD_1));
    CHECK(has_command(&actions, TRUCO_CMD_PLAY_CARD_2));
    CHECK(has_command(&actions, TRUCO_CMD_RAISE_TRUCO));
    CHECK(has_command(&actions, TRUCO_CMD_CALL_ENVIDO));
    CHECK(has_command(&actions, TRUCO_CMD_CALL_REAL_ENVIDO));
    CHECK(has_command(&actions, TRUCO_CMD_CALL_FALTA_ENVIDO));
    CHECK(actions.truco_value == 2u);

    expect_ok(truco_game_apply(&game, 0u, TRUCO_CMD_RAISE_TRUCO));
    expect_ok(truco_game_legal_actions(&game, 0u, &actions));
    CHECK(actions.count == 0u);

    expect_ok(truco_game_legal_actions(&game, 1u, &actions));
    CHECK(has_command(&actions, TRUCO_CMD_ACCEPT_BID));
    CHECK(has_command(&actions, TRUCO_CMD_REJECT_BID));
    CHECK(!has_command(&actions, TRUCO_CMD_PLAY_CARD_0));
    CHECK(!has_command(&actions, TRUCO_CMD_CALL_ENVIDO));
    CHECK(actions.truco_value == 2u);
    CHECK(truco_game_apply(&game, 1u, TRUCO_CMD_CALL_ENVIDO) == TRUCO_ERR_INVALID_STATE);

    expect_ok(truco_game_apply(&game, 1u, TRUCO_CMD_ACCEPT_BID));
    expect_ok(truco_game_apply(&game, 0u, TRUCO_CMD_PLAY_CARD_0));
    expect_ok(truco_game_legal_actions(&game, 1u, &actions));
    CHECK(has_command(&actions, TRUCO_CMD_PLAY_CARD_0));
    CHECK(has_command(&actions, TRUCO_CMD_PLAY_CARD_1));
    CHECK(has_command(&actions, TRUCO_CMD_PLAY_CARD_2));
    CHECK(has_command(&actions, TRUCO_CMD_RAISE_TRUCO));
    CHECK(!has_command(&actions, TRUCO_CMD_CALL_ENVIDO));
    CHECK(actions.truco_value == 3u);

    start_two_player_hand(&game);
    expect_ok(truco_game_apply(&game, 0u, TRUCO_CMD_CALL_ENVIDO));
    expect_ok(truco_game_legal_actions(&game, 1u, &actions));
    CHECK(has_command(&actions, TRUCO_CMD_ACCEPT_BID));
    CHECK(has_command(&actions, TRUCO_CMD_REJECT_BID));
    CHECK(!has_command(&actions, TRUCO_CMD_RAISE_TRUCO));
    CHECK(actions.envido_points == 2u);
    CHECK(truco_game_apply(&game, 1u, TRUCO_CMD_RAISE_TRUCO) == TRUCO_ERR_INVALID_STATE);
}

static void test_parda_rules(void)
{
    truco_game game;
    truco_card player0[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_COPA, 3u},
        {TRUCO_SUIT_COPA, 4u},
        {TRUCO_SUIT_ORO, 5u}
    };
    truco_card player1[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_ORO, 3u},
        {TRUCO_SUIT_ESPADA, 1u},
        {TRUCO_SUIT_BASTO, 4u}
    };

    start_two_player_hand(&game);
    force_hand(&game, 0u, player0);
    force_hand(&game, 1u, player1);

    expect_ok(truco_game_apply(&game, 0u, TRUCO_CMD_PLAY_CARD_0));
    expect_ok(truco_game_apply(&game, 1u, TRUCO_CMD_PLAY_CARD_0));
    CHECK(truco_game_trick_winner(&game, 0u) == -1);
    CHECK(truco_game_current_player(&game) == 0u);

    expect_ok(truco_game_apply(&game, 0u, TRUCO_CMD_PLAY_CARD_1));
    expect_ok(truco_game_apply(&game, 1u, TRUCO_CMD_PLAY_CARD_1));
    CHECK(truco_game_phase(&game) == TRUCO_PHASE_HAND_OVER);
    CHECK(truco_game_hand_winner(&game) == 1);
}

static void test_envido_resolution(void)
{
    truco_game game;
    truco_card player0[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_ESPADA, 7u},
        {TRUCO_SUIT_ESPADA, 6u},
        {TRUCO_SUIT_ORO, 12u}
    };
    truco_card player1[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_ORO, 7u},
        {TRUCO_SUIT_ORO, 5u},
        {TRUCO_SUIT_COPA, 1u}
    };

    start_two_player_hand(&game);
    force_hand(&game, 0u, player0);
    force_hand(&game, 1u, player1);
    expect_ok(truco_game_apply(&game, 1u, TRUCO_CMD_CALL_ENVIDO));
    expect_ok(truco_game_apply(&game, 0u, TRUCO_CMD_ACCEPT_BID));
    CHECK(truco_game_score(&game, 0u) == 2u);
    CHECK(truco_game_score(&game, 1u) == 0u);

    start_two_player_hand(&game);
    expect_ok(truco_game_apply(&game, 0u, TRUCO_CMD_CALL_REAL_ENVIDO));
    expect_ok(truco_game_apply(&game, 1u, TRUCO_CMD_REJECT_BID));
    CHECK(truco_game_score(&game, 0u) == 1u);
}

static void test_four_player_team_flow(void)
{
    truco_config config;
    truco_game game;
    truco_card p0[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_COPA, 4u},
        {TRUCO_SUIT_COPA, 5u},
        {TRUCO_SUIT_COPA, 6u}
    };
    truco_card p1[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_ORO, 7u},
        {TRUCO_SUIT_ORO, 4u},
        {TRUCO_SUIT_ORO, 5u}
    };
    truco_card p2[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_ESPADA, 1u},
        {TRUCO_SUIT_BASTO, 4u},
        {TRUCO_SUIT_BASTO, 5u}
    };
    truco_card p3[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_BASTO, 6u},
        {TRUCO_SUIT_COPA, 10u},
        {TRUCO_SUIT_COPA, 11u}
    };

    truco_config_default(&config, 4u);
    truco_config_set_initial_dealer(&config, 3u);
    expect_ok(truco_game_init(&game, &config));
    expect_ok(truco_game_apply(&game, 0u, TRUCO_CMD_START_HAND));

    CHECK(truco_game_player_count(&game) == 4u);
    CHECK(truco_game_team_for_player(&game, 0u) == 0u);
    CHECK(truco_game_team_for_player(&game, 1u) == 1u);
    CHECK(truco_game_team_for_player(&game, 2u) == 0u);
    CHECK(truco_game_team_for_player(&game, 3u) == 1u);

    force_hand(&game, 0u, p0);
    force_hand(&game, 1u, p1);
    force_hand(&game, 2u, p2);
    force_hand(&game, 3u, p3);

    expect_ok(truco_game_apply(&game, 0u, TRUCO_CMD_PLAY_CARD_0));
    expect_ok(truco_game_apply(&game, 1u, TRUCO_CMD_PLAY_CARD_0));
    expect_ok(truco_game_apply(&game, 2u, TRUCO_CMD_PLAY_CARD_0));
    expect_ok(truco_game_apply(&game, 3u, TRUCO_CMD_PLAY_CARD_0));
    CHECK(truco_game_trick_winner(&game, 0u) == 0);
    CHECK(truco_game_current_player(&game) == 2u);
}

static void test_future_six_player_shape_is_reserved(void)
{
    truco_config config;
    truco_game game;

    truco_config_default(&config, 6u);
    CHECK(TRUCO_MAX_PLAYERS == 6u);
    CHECK(truco_game_init(&game, &config) == TRUCO_ERR_UNSUPPORTED_RULES);
}

int main(void)
{
    test_card_ranking();
    test_deck_and_envido_values();
    test_two_player_hand_resolution();
    test_truco_bidding();
    test_legal_actions();
    test_parda_rules();
    test_envido_resolution();
    test_four_player_team_flow();
    test_future_six_player_shape_is_reserved();

    printf("ok - %u checks\n", tests_run);
    return 0;
}
