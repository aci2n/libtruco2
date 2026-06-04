#include "truco.h"
#include "truco_internal.h"

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

static int has_command(const truco_legal_commands *legal, truco_command command)
{
    size_t index;

    for (index = 0u; index < legal->count; ++index) {
        if (legal->commands[index] == command) {
            return 1;
        }
    }

    return 0;
}

static void configure_two_player_game(truco_game *game)
{
    expect_ok(truco_game_set_player_count(game, 2u));
    expect_ok(truco_game_set_seed(game, 7u));
    expect_ok(truco_game_set_initial_dealer(game, 1u));
}

static truco_game *create_two_player_game(void)
{
    truco_game *game = truco_game_create();

    CHECK(game != 0);
    configure_two_player_game(game);

    return game;
}

static void start_two_player_hand(truco_game *game)
{
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_START_HAND));
    CHECK(truco_game_current_player(game) == 0u);
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

    expect_ok(truco_game_set_hand(game, 0u, player0));
    expect_ok(truco_game_set_hand(game, 1u, player1));
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
    truco_game *game = create_two_player_game();

    start_two_player_hand(game);
    CHECK(truco_game_apply(game, 0u, TRUCO_CMD_START_HAND) == TRUCO_ERR_INVALID_STATE);
    install_basic_two_player_hands(game);
    play_basic_two_player_hand(game);

    CHECK(truco_game_phase(game) == TRUCO_PHASE_HAND_OVER);
    CHECK(truco_game_hand_winner(game) == 0);
    CHECK(truco_game_score(game, 0u) == 1u);
    CHECK(truco_game_score(game, 1u) == 0u);

    truco_game_delete(&game);
}

static void test_truco_bidding(void)
{
    truco_game *game = create_two_player_game();

    start_two_player_hand(game);
    install_basic_two_player_hands(game);
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_RAISE_TRUCO));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_ACCEPT_BID));
    play_basic_two_player_hand(game);
    CHECK(truco_game_score(game, 0u) == 2u);

    expect_ok(truco_game_init(game));
    configure_two_player_game(game);
    start_two_player_hand(game);
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_RAISE_TRUCO));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_REJECT_BID));
    CHECK(truco_game_phase(game) == TRUCO_PHASE_HAND_OVER);
    CHECK(truco_game_score(game, 0u) == 1u);

    truco_game_delete(&game);
}

static void test_legal_actions(void)
{
    truco_game *game = truco_game_create();
    truco_legal_commands legal;

    CHECK(game != 0);
    expect_ok(truco_game_set_player_count(game, 2u));
    expect_ok(truco_game_set_initial_dealer(game, 1u));

    expect_ok(truco_game_legal_commands(game, 0u, &legal));
    CHECK(legal.count == 0u);

    expect_ok(truco_game_legal_commands(game, 1u, &legal));
    CHECK(legal.count == 1u);
    CHECK(legal.commands[0] == TRUCO_CMD_START_HAND);

    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_START_HAND));
    install_basic_two_player_hands(game);

    expect_ok(truco_game_legal_commands(game, 0u, &legal));
    CHECK(has_command(&legal, TRUCO_CMD_PLAY_CARD_0));
    CHECK(has_command(&legal, TRUCO_CMD_PLAY_CARD_1));
    CHECK(has_command(&legal, TRUCO_CMD_PLAY_CARD_2));
    CHECK(has_command(&legal, TRUCO_CMD_RAISE_TRUCO));
    CHECK(has_command(&legal, TRUCO_CMD_CALL_ENVIDO));
    CHECK(has_command(&legal, TRUCO_CMD_CALL_REAL_ENVIDO));
    CHECK(has_command(&legal, TRUCO_CMD_CALL_FALTA_ENVIDO));
    CHECK(has_command(&legal, TRUCO_CMD_GO_TO_DECK));

    expect_ok(truco_game_legal_commands(game, 1u, &legal));
    CHECK(!has_command(&legal, TRUCO_CMD_PLAY_CARD_0));
    CHECK(!has_command(&legal, TRUCO_CMD_RAISE_TRUCO));
    CHECK(!has_command(&legal, TRUCO_CMD_CALL_ENVIDO));
    CHECK(!has_command(&legal, TRUCO_CMD_GO_TO_DECK));
    CHECK(truco_game_next_truco_value(game) == 2u);

    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_RAISE_TRUCO));
    expect_ok(truco_game_legal_commands(game, 0u, &legal));
    CHECK(legal.count == 0u);

    expect_ok(truco_game_legal_commands(game, 1u, &legal));
    CHECK(has_command(&legal, TRUCO_CMD_ACCEPT_BID));
    CHECK(has_command(&legal, TRUCO_CMD_REJECT_BID));
    CHECK(has_command(&legal, TRUCO_CMD_GO_TO_DECK));
    CHECK(!has_command(&legal, TRUCO_CMD_PLAY_CARD_0));
    CHECK(!has_command(&legal, TRUCO_CMD_CALL_ENVIDO));
    CHECK(truco_game_pending_truco_value(game) == 2u);
    CHECK(truco_game_apply(game, 1u, TRUCO_CMD_CALL_ENVIDO) == TRUCO_ERR_INVALID_STATE);

    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_ACCEPT_BID));
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_PLAY_CARD_0));
    expect_ok(truco_game_legal_commands(game, 1u, &legal));
    CHECK(has_command(&legal, TRUCO_CMD_PLAY_CARD_0));
    CHECK(has_command(&legal, TRUCO_CMD_PLAY_CARD_1));
    CHECK(has_command(&legal, TRUCO_CMD_PLAY_CARD_2));
    CHECK(has_command(&legal, TRUCO_CMD_RAISE_TRUCO));
    CHECK(!has_command(&legal, TRUCO_CMD_CALL_ENVIDO));
    CHECK(truco_game_next_truco_value(game) == 3u);

    expect_ok(truco_game_init(game));
    configure_two_player_game(game);
    start_two_player_hand(game);
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_CALL_ENVIDO));
    expect_ok(truco_game_legal_commands(game, 1u, &legal));
    CHECK(has_command(&legal, TRUCO_CMD_ACCEPT_BID));
    CHECK(has_command(&legal, TRUCO_CMD_REJECT_BID));
    CHECK(!has_command(&legal, TRUCO_CMD_RAISE_TRUCO));
    CHECK(truco_game_pending_envido_points(game) == 2u);
    CHECK(truco_game_apply(game, 1u, TRUCO_CMD_RAISE_TRUCO) == TRUCO_ERR_INVALID_STATE);

    truco_game_delete(&game);
}

static void test_parda_rules(void)
{
    truco_game *game = create_two_player_game();
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

    start_two_player_hand(game);
    expect_ok(truco_game_set_hand(game, 0u, player0));
    expect_ok(truco_game_set_hand(game, 1u, player1));

    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_PLAY_CARD_0));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_PLAY_CARD_0));
    CHECK(truco_game_trick_winner(game, 0u) == -1);
    CHECK(truco_game_current_player(game) == 0u);

    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_PLAY_CARD_1));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_PLAY_CARD_1));
    CHECK(truco_game_phase(game) == TRUCO_PHASE_HAND_OVER);
    CHECK(truco_game_hand_winner(game) == 1);

    truco_game_delete(&game);
}

static void test_envido_resolution(void)
{
    truco_game *game = create_two_player_game();
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

    start_two_player_hand(game);
    expect_ok(truco_game_set_hand(game, 0u, player0));
    expect_ok(truco_game_set_hand(game, 1u, player1));
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_CALL_ENVIDO));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_ACCEPT_BID));
    CHECK(truco_game_score(game, 0u) == 2u);
    CHECK(truco_game_score(game, 1u) == 0u);

    expect_ok(truco_game_init(game));
    configure_two_player_game(game);
    start_two_player_hand(game);
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_CALL_REAL_ENVIDO));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_REJECT_BID));
    CHECK(truco_game_score(game, 0u) == 1u);

    truco_game_delete(&game);
}

static void test_four_player_team_flow(void)
{
    truco_game *game = truco_game_create();
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

    CHECK(game != 0);
    expect_ok(truco_game_set_player_count(game, 4u));
    expect_ok(truco_game_set_initial_dealer(game, 3u));
    expect_ok(truco_game_apply(game, 3u, TRUCO_CMD_START_HAND));

    CHECK(truco_game_player_count(game) == 4u);
    CHECK(truco_game_team_for_player(game, 0u) == 0u);
    CHECK(truco_game_team_for_player(game, 1u) == 1u);
    CHECK(truco_game_team_for_player(game, 2u) == 0u);
    CHECK(truco_game_team_for_player(game, 3u) == 1u);

    expect_ok(truco_game_set_hand(game, 0u, p0));
    expect_ok(truco_game_set_hand(game, 1u, p1));
    expect_ok(truco_game_set_hand(game, 2u, p2));
    expect_ok(truco_game_set_hand(game, 3u, p3));

    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_PLAY_CARD_0));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_PLAY_CARD_0));
    expect_ok(truco_game_apply(game, 2u, TRUCO_CMD_PLAY_CARD_0));
    expect_ok(truco_game_apply(game, 3u, TRUCO_CMD_PLAY_CARD_0));
    CHECK(truco_game_trick_winner(game, 0u) == 0);
    CHECK(truco_game_current_player(game) == 2u);

    truco_game_delete(&game);
}

static void test_go_to_deck(void)
{
    truco_game *game = create_two_player_game();

    start_two_player_hand(game);
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_GO_TO_DECK));
    CHECK(truco_game_phase(game) == TRUCO_PHASE_HAND_OVER);
    CHECK(truco_game_hand_winner(game) == 1);
    CHECK(truco_game_score(game, 0u) == 0u);
    CHECK(truco_game_score(game, 1u) == 2u);

    expect_ok(truco_game_init(game));
    configure_two_player_game(game);
    start_two_player_hand(game);
    install_basic_two_player_hands(game);
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_CALL_ENVIDO));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_ACCEPT_BID));
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_GO_TO_DECK));
    CHECK(truco_game_phase(game) == TRUCO_PHASE_HAND_OVER);
    CHECK(truco_game_hand_winner(game) == 1);
    CHECK(truco_game_score(game, 1u) == 3u);

    expect_ok(truco_game_init(game));
    configure_two_player_game(game);
    start_two_player_hand(game);
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_CALL_ENVIDO));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_GO_TO_DECK));
    CHECK(truco_game_phase(game) == TRUCO_PHASE_HAND_OVER);
    CHECK(truco_game_hand_winner(game) == 0);
    CHECK(truco_game_score(game, 0u) == 2u);

    expect_ok(truco_game_init(game));
    configure_two_player_game(game);
    start_two_player_hand(game);
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_RAISE_TRUCO));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_GO_TO_DECK));
    CHECK(truco_game_phase(game) == TRUCO_PHASE_HAND_OVER);
    CHECK(truco_game_hand_winner(game) == 0);
    CHECK(truco_game_score(game, 0u) == 2u);

    truco_game_delete(&game);
}

static void test_future_six_player_shape_is_reserved(void)
{
    truco_game *game = truco_game_create();

    CHECK(game != 0);
    expect_ok(truco_game_set_player_count(game, 6u));
    CHECK(TRUCO_MAX_PLAYERS == 6u);
    CHECK(truco_game_apply(game, 0u, TRUCO_CMD_START_HAND) == TRUCO_ERR_UNSUPPORTED_RULES);

    truco_game_delete(&game);
}

static void test_falta_envido(void)
{
    truco_game *game = create_two_player_game();
    truco_card strong_envido[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_ESPADA, 7u},
        {TRUCO_SUIT_ESPADA, 6u},
        {TRUCO_SUIT_ORO, 12u}
    };
    truco_card weak_envido[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_ORO, 7u},
        {TRUCO_SUIT_ORO, 5u},
        {TRUCO_SUIT_COPA, 1u}
    };

    start_two_player_hand(game);
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_CALL_FALTA_ENVIDO));
    CHECK(truco_game_pending_envido_points(game) == 15u);
    expect_ok(truco_game_set_hand(game, 0u, strong_envido));
    expect_ok(truco_game_set_hand(game, 1u, weak_envido));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_ACCEPT_BID));
    CHECK(truco_game_score(game, 0u) == 15u);
    CHECK(truco_game_score(game, 1u) == 0u);

    expect_ok(truco_game_init(game));
    configure_two_player_game(game);
    expect_ok(truco_game_set_score(game, 0u, 22u));
    expect_ok(truco_game_set_score(game, 1u, 10u));
    start_two_player_hand(game);
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_CALL_FALTA_ENVIDO));
    CHECK(truco_game_pending_envido_points(game) == 8u);
    expect_ok(truco_game_set_hand(game, 0u, strong_envido));
    expect_ok(truco_game_set_hand(game, 1u, weak_envido));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_REJECT_BID));
    CHECK(truco_game_score(game, 0u) == 23u);

    truco_game_delete(&game);
}

static void test_game_over(void)
{
    truco_game *game = create_two_player_game();

    expect_ok(truco_game_set_target_score(game, 1u));
    start_two_player_hand(game);
    install_basic_two_player_hands(game);
    play_basic_two_player_hand(game);
    CHECK(truco_game_phase(game) == TRUCO_PHASE_GAME_OVER);
    CHECK(truco_game_score(game, 0u) == 1u);

    truco_game_delete(&game);
}

static void test_truco_escalation_and_cap(void)
{
    truco_game *game = create_two_player_game();
    truco_legal_commands legal;
    truco_card player0[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_COPA, 4u},
        {TRUCO_SUIT_COPA, 5u},
        {TRUCO_SUIT_COPA, 6u}
    };
    truco_card player1[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_ESPADA, 1u},
        {TRUCO_SUIT_ORO, 4u},
        {TRUCO_SUIT_ORO, 3u}
    };

    start_two_player_hand(game);
    expect_ok(truco_game_set_hand(game, 0u, player0));
    expect_ok(truco_game_set_hand(game, 1u, player1));

    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_RAISE_TRUCO));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_ACCEPT_BID));
    CHECK(truco_game_next_truco_value(game) == 3u);

    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_PLAY_CARD_0));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_PLAY_CARD_0));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_RAISE_TRUCO));
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_ACCEPT_BID));
    CHECK(truco_game_next_truco_value(game) == 4u);

    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_PLAY_CARD_1));
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_PLAY_CARD_1));
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_RAISE_TRUCO));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_ACCEPT_BID));
    expect_ok(truco_game_legal_commands(game, 0u, &legal));
    CHECK(!has_command(&legal, TRUCO_CMD_RAISE_TRUCO));
    CHECK(truco_game_apply(game, 0u, TRUCO_CMD_RAISE_TRUCO) == TRUCO_ERR_INVALID_STATE);

    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_PLAY_CARD_2));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_PLAY_CARD_2));
    CHECK(truco_game_score(game, 1u) == 4u);

    truco_game_delete(&game);
}

static void test_same_team_cannot_raise_truco_twice(void)
{
    truco_game *game = create_two_player_game();
    truco_legal_commands legal;

    start_two_player_hand(game);
    install_basic_two_player_hands(game);
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_RAISE_TRUCO));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_ACCEPT_BID));
    expect_ok(truco_game_legal_commands(game, 0u, &legal));
    CHECK(!has_command(&legal, TRUCO_CMD_RAISE_TRUCO));
    CHECK(truco_game_apply(game, 0u, TRUCO_CMD_RAISE_TRUCO) == TRUCO_ERR_INVALID_STATE);

    truco_game_delete(&game);
}

static void test_reject_retruco_awards_previous_value(void)
{
    truco_game *game = create_two_player_game();
    truco_card player0[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_COPA, 4u},
        {TRUCO_SUIT_COPA, 5u},
        {TRUCO_SUIT_COPA, 6u}
    };
    truco_card player1[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_ESPADA, 1u},
        {TRUCO_SUIT_ORO, 7u},
        {TRUCO_SUIT_ORO, 4u}
    };

    start_two_player_hand(game);
    expect_ok(truco_game_set_hand(game, 0u, player0));
    expect_ok(truco_game_set_hand(game, 1u, player1));
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_RAISE_TRUCO));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_ACCEPT_BID));
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_PLAY_CARD_0));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_PLAY_CARD_0));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_RAISE_TRUCO));
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_REJECT_BID));
    CHECK(truco_game_phase(game) == TRUCO_PHASE_HAND_OVER);
    CHECK(truco_game_hand_winner(game) == 1);
    CHECK(truco_game_score(game, 1u) == 2u);

    truco_game_delete(&game);
}

static void test_all_tricks_parda_mano_wins(void)
{
    truco_game *game = create_two_player_game();
    truco_card player0[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_ORO, 3u},
        {TRUCO_SUIT_ORO, 2u},
        {TRUCO_SUIT_COPA, 5u}
    };
    truco_card player1[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_COPA, 3u},
        {TRUCO_SUIT_ESPADA, 2u},
        {TRUCO_SUIT_BASTO, 5u}
    };

    start_two_player_hand(game);
    expect_ok(truco_game_set_hand(game, 0u, player0));
    expect_ok(truco_game_set_hand(game, 1u, player1));

    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_PLAY_CARD_0));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_PLAY_CARD_0));
    CHECK(truco_game_trick_winner(game, 0u) == -1);

    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_PLAY_CARD_1));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_PLAY_CARD_1));
    CHECK(truco_game_trick_winner(game, 1u) == -1);

    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_PLAY_CARD_2));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_PLAY_CARD_2));
    CHECK(truco_game_trick_winner(game, 2u) == -1);
    CHECK(truco_game_phase(game) == TRUCO_PHASE_HAND_OVER);
    CHECK(truco_game_hand_winner(game) == 0);

    truco_game_delete(&game);
}

static void test_parda_second_trick_defers_to_first_winner(void)
{
    truco_game *game = create_two_player_game();
    truco_card player0[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_ESPADA, 1u},
        {TRUCO_SUIT_COPA, 3u},
        {TRUCO_SUIT_COPA, 4u}
    };
    truco_card player1[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_ORO, 7u},
        {TRUCO_SUIT_ORO, 3u},
        {TRUCO_SUIT_BASTO, 5u}
    };

    start_two_player_hand(game);
    expect_ok(truco_game_set_hand(game, 0u, player0));
    expect_ok(truco_game_set_hand(game, 1u, player1));

    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_PLAY_CARD_0));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_PLAY_CARD_0));
    CHECK(truco_game_trick_winner(game, 0u) == 0);

    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_PLAY_CARD_1));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_PLAY_CARD_1));
    CHECK(truco_game_trick_winner(game, 1u) == -1);
    CHECK(truco_game_phase(game) == TRUCO_PHASE_HAND_OVER);
    CHECK(truco_game_hand_winner(game) == 0);

    truco_game_delete(&game);
}

static void test_parda_first_trick_defers_to_later_trick(void)
{
    truco_game *game = create_two_player_game();
    truco_card player0[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_COPA, 3u},
        {TRUCO_SUIT_ESPADA, 1u},
        {TRUCO_SUIT_COPA, 4u}
    };
    truco_card player1[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_ORO, 3u},
        {TRUCO_SUIT_ORO, 7u},
        {TRUCO_SUIT_BASTO, 4u}
    };

    start_two_player_hand(game);
    expect_ok(truco_game_set_hand(game, 0u, player0));
    expect_ok(truco_game_set_hand(game, 1u, player1));

    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_PLAY_CARD_0));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_PLAY_CARD_0));
    CHECK(truco_game_trick_winner(game, 0u) == -1);

    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_PLAY_CARD_1));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_PLAY_CARD_1));
    CHECK(truco_game_phase(game) == TRUCO_PHASE_HAND_OVER);
    CHECK(truco_game_hand_winner(game) == 0);

    truco_game_delete(&game);
}

static void test_envido_tie_breaks_on_mano_order(void)
{
    truco_game *game = truco_game_create();
    truco_card tied_envido[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_ESPADA, 7u},
        {TRUCO_SUIT_ESPADA, 6u},
        {TRUCO_SUIT_ORO, 12u}
    };
    truco_card same_envido[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_BASTO, 7u},
        {TRUCO_SUIT_BASTO, 6u},
        {TRUCO_SUIT_COPA, 12u}
    };

    CHECK(game != 0);
    expect_ok(truco_game_set_player_count(game, 2u));
    expect_ok(truco_game_set_initial_dealer(game, 0u));
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_START_HAND));
    CHECK(truco_game_current_player(game) == 1u);
    expect_ok(truco_game_set_hand(game, 0u, tied_envido));
    expect_ok(truco_game_set_hand(game, 1u, same_envido));
    CHECK(truco_game_hand_envido(game, 0u) == 33u);
    CHECK(truco_game_hand_envido(game, 1u) == 33u);

    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_CALL_ENVIDO));
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_ACCEPT_BID));
    CHECK(truco_game_score(game, 1u) == 2u);
    CHECK(truco_game_score(game, 0u) == 0u);

    truco_game_delete(&game);
}

static void test_envido_only_once_per_hand(void)
{
    truco_game *game = create_two_player_game();

    start_two_player_hand(game);
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_CALL_ENVIDO));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_REJECT_BID));
    CHECK(truco_game_apply(game, 0u, TRUCO_CMD_CALL_REAL_ENVIDO) == TRUCO_ERR_INVALID_STATE);

    truco_game_delete(&game);
}

static void test_four_player_envido_best_player(void)
{
    truco_game *game = truco_game_create();
    truco_card p0[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_COPA, 4u},
        {TRUCO_SUIT_ORO, 5u},
        {TRUCO_SUIT_BASTO, 6u}
    };
    truco_card p1[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_ORO, 4u},
        {TRUCO_SUIT_BASTO, 5u},
        {TRUCO_SUIT_COPA, 6u}
    };
    truco_card p2[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_ESPADA, 7u},
        {TRUCO_SUIT_ESPADA, 6u},
        {TRUCO_SUIT_BASTO, 12u}
    };
    truco_card p3[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_BASTO, 7u},
        {TRUCO_SUIT_BASTO, 5u},
        {TRUCO_SUIT_COPA, 12u}
    };

    CHECK(game != 0);
    expect_ok(truco_game_set_player_count(game, 4u));
    expect_ok(truco_game_set_initial_dealer(game, 3u));
    expect_ok(truco_game_apply(game, 3u, TRUCO_CMD_START_HAND));
    expect_ok(truco_game_set_hand(game, 0u, p0));
    expect_ok(truco_game_set_hand(game, 1u, p1));
    expect_ok(truco_game_set_hand(game, 2u, p2));
    expect_ok(truco_game_set_hand(game, 3u, p3));
    CHECK(truco_game_hand_envido(game, 0u) == 6u);
    CHECK(truco_game_hand_envido(game, 2u) == 33u);
    CHECK(truco_game_hand_envido(game, 1u) == 6u);

    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_CALL_ENVIDO));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_ACCEPT_BID));
    CHECK(truco_game_score(game, 0u) == 2u);
    CHECK(truco_game_score(game, 1u) == 0u);

    truco_game_delete(&game);
}

static void test_four_player_partner_tie_wins_trick(void)
{
    truco_game *game = truco_game_create();
    truco_card p0[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_COPA, 3u},
        {TRUCO_SUIT_COPA, 4u},
        {TRUCO_SUIT_COPA, 5u}
    };
    truco_card p1[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_ORO, 4u},
        {TRUCO_SUIT_ORO, 5u},
        {TRUCO_SUIT_ORO, 6u}
    };
    truco_card p2[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_ESPADA, 3u},
        {TRUCO_SUIT_BASTO, 4u},
        {TRUCO_SUIT_BASTO, 5u}
    };
    truco_card p3[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_BASTO, 6u},
        {TRUCO_SUIT_COPA, 10u},
        {TRUCO_SUIT_COPA, 11u}
    };

    CHECK(game != 0);
    expect_ok(truco_game_set_player_count(game, 4u));
    expect_ok(truco_game_set_initial_dealer(game, 3u));
    expect_ok(truco_game_apply(game, 3u, TRUCO_CMD_START_HAND));
    expect_ok(truco_game_set_hand(game, 0u, p0));
    expect_ok(truco_game_set_hand(game, 1u, p1));
    expect_ok(truco_game_set_hand(game, 2u, p2));
    expect_ok(truco_game_set_hand(game, 3u, p3));

    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_PLAY_CARD_0));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_PLAY_CARD_0));
    expect_ok(truco_game_apply(game, 2u, TRUCO_CMD_PLAY_CARD_0));
    expect_ok(truco_game_apply(game, 3u, TRUCO_CMD_PLAY_CARD_0));
    CHECK(truco_game_trick_winner(game, 0u) == 0);

    truco_game_delete(&game);
}

static void test_deterministic_deal(void)
{
    truco_game *first = create_two_player_game();
    truco_game *second = create_two_player_game();
    truco_card first_card;
    truco_card second_card;

    start_two_player_hand(first);
    start_two_player_hand(second);
    expect_ok(truco_game_hand_card(first, 0u, 0u, &first_card));
    expect_ok(truco_game_hand_card(second, 0u, 0u, &second_card));
    CHECK(first_card.suit == second_card.suit);
    CHECK(first_card.rank == second_card.rank);

    truco_game_delete(&first);
    truco_game_delete(&second);
}

static void test_envido_can_end_game_mid_hand(void)
{
    truco_game *game = create_two_player_game();
    truco_card strong_envido[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_ESPADA, 7u},
        {TRUCO_SUIT_ESPADA, 6u},
        {TRUCO_SUIT_ORO, 12u}
    };
    truco_card weak_envido[TRUCO_HAND_CARDS] = {
        {TRUCO_SUIT_ORO, 4u},
        {TRUCO_SUIT_ORO, 5u},
        {TRUCO_SUIT_COPA, 6u}
    };

    expect_ok(truco_game_set_target_score(game, 2u));
    start_two_player_hand(game);
    expect_ok(truco_game_set_hand(game, 0u, strong_envido));
    expect_ok(truco_game_set_hand(game, 1u, weak_envido));
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_CALL_ENVIDO));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_ACCEPT_BID));
    CHECK(truco_game_phase(game) == TRUCO_PHASE_GAME_OVER);
    CHECK(truco_game_score(game, 0u) == 2u);
    CHECK(truco_game_apply(game, 0u, TRUCO_CMD_PLAY_CARD_0) == TRUCO_ERR_INVALID_STATE);

    truco_game_delete(&game);
}

static void test_dealer_rotation(void)
{
    truco_game *game = create_two_player_game();
    truco_legal_commands legal;

    expect_ok(truco_game_legal_commands(game, 1u, &legal));
    CHECK(has_command(&legal, TRUCO_CMD_START_HAND));
    expect_ok(truco_game_legal_commands(game, 0u, &legal));
    CHECK(!has_command(&legal, TRUCO_CMD_START_HAND));

    start_two_player_hand(game);
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_GO_TO_DECK));
    CHECK(truco_game_phase(game) == TRUCO_PHASE_HAND_OVER);

    expect_ok(truco_game_legal_commands(game, 0u, &legal));
    CHECK(has_command(&legal, TRUCO_CMD_START_HAND));
    expect_ok(truco_game_legal_commands(game, 1u, &legal));
    CHECK(!has_command(&legal, TRUCO_CMD_START_HAND));

    truco_game_delete(&game);
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
    test_go_to_deck();
    test_four_player_team_flow();
    test_future_six_player_shape_is_reserved();
    test_falta_envido();
    test_game_over();
    test_truco_escalation_and_cap();
    test_same_team_cannot_raise_truco_twice();
    test_reject_retruco_awards_previous_value();
    test_all_tricks_parda_mano_wins();
    test_parda_second_trick_defers_to_first_winner();
    test_parda_first_trick_defers_to_later_trick();
    test_envido_tie_breaks_on_mano_order();
    test_envido_only_once_per_hand();
    test_four_player_envido_best_player();
    test_four_player_partner_tie_wins_trick();
    test_deterministic_deal();
    test_envido_can_end_game_mid_hand();
    test_dealer_rotation();

    printf("ok - %u checks\n", tests_run);
    return 0;
}
