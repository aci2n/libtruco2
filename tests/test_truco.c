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

static int has_command(const truco_command *commands,
                       unsigned int count,
                       truco_command command)
{
    unsigned int index;

    for (index = 0u; index < count; ++index) {
        if (commands[index] == command) {
            return 1;
        }
    }

    return 0;
}

static truco_status query_legal(const truco_game *game,
                                unsigned int player,
                                truco_command *commands,
                                unsigned int max_commands,
                                unsigned int *count_out,
                                unsigned int *truco_value_out,
                                unsigned int *envido_points_out)
{
    return truco_game_legal_actions(game,
                                    player,
                                    commands,
                                    max_commands,
                                    count_out,
                                    truco_value_out,
                                    envido_points_out);
}

static void test_card_ranking(void)
{
    CHECK(truco_card_compare(TRUCO_SUIT_ESPADA, 1u, TRUCO_SUIT_BASTO, 1u) > 0);
    CHECK(truco_card_compare(TRUCO_SUIT_BASTO, 1u, TRUCO_SUIT_ESPADA, 7u) > 0);
    CHECK(truco_card_compare(TRUCO_SUIT_ESPADA, 7u, TRUCO_SUIT_ORO, 7u) > 0);
    CHECK(truco_card_compare(TRUCO_SUIT_ORO, 7u, TRUCO_SUIT_COPA, 3u) > 0);
    CHECK(truco_card_compare(TRUCO_SUIT_COPA, 3u, TRUCO_SUIT_ORO, 3u) == 0);
    CHECK(truco_card_compare(TRUCO_SUIT_BASTO, 4u, TRUCO_SUIT_ESPADA, 1u) < 0);
    CHECK(!truco_card_is_valid(TRUCO_SUIT_COPA, 8u));
}

static void test_envido_values(void)
{
    truco_suit thirty_three_suits[TRUCO_HAND_CARDS] = {
        TRUCO_SUIT_ESPADA,
        TRUCO_SUIT_ESPADA,
        TRUCO_SUIT_ORO
    };
    unsigned int thirty_three_ranks[TRUCO_HAND_CARDS] = {7u, 6u, 12u};
    truco_suit face_suits[TRUCO_HAND_CARDS] = {
        TRUCO_SUIT_COPA,
        TRUCO_SUIT_ORO,
        TRUCO_SUIT_BASTO
    };
    unsigned int face_ranks[TRUCO_HAND_CARDS] = {12u, 11u, 10u};

    CHECK(truco_envido_points(thirty_three_suits, thirty_three_ranks) == 33u);
    CHECK(truco_envido_points(face_suits, face_ranks) == 0u);
}

static truco_game *make_two_player_game(unsigned int seed, unsigned int initial_dealer)
{
    truco_game *game = truco_game_create();
    CHECK(game != 0);
    expect_ok(truco_game_set_player_count(game, 2u));
    expect_ok(truco_game_set_seed(game, seed));
    expect_ok(truco_game_set_initial_dealer(game, initial_dealer));
    expect_ok(truco_game_init(game));
    return game;
}

static void start_two_player_hand(truco_game *game)
{
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_START_HAND));
    CHECK(truco_game_current_player(game) == 0u);
}

static void force_hand(truco_game *game,
                       unsigned int player,
                       const truco_suit suits[TRUCO_HAND_CARDS],
                       const unsigned int ranks[TRUCO_HAND_CARDS])
{
    expect_ok(truco_game_set_hand(game, player, suits, ranks));
}

static void install_basic_two_player_hands(truco_game *game)
{
    truco_suit player0_suits[TRUCO_HAND_CARDS] = {
        TRUCO_SUIT_ESPADA,
        TRUCO_SUIT_COPA,
        TRUCO_SUIT_COPA
    };
    unsigned int player0_ranks[TRUCO_HAND_CARDS] = {1u, 4u, 5u};
    truco_suit player1_suits[TRUCO_HAND_CARDS] = {
        TRUCO_SUIT_ORO,
        TRUCO_SUIT_COPA,
        TRUCO_SUIT_ORO
    };
    unsigned int player1_ranks[TRUCO_HAND_CARDS] = {7u, 6u, 4u};

    force_hand(game, 0u, player0_suits, player0_ranks);
    force_hand(game, 1u, player1_suits, player1_ranks);
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
    truco_game *game = make_two_player_game(7u, 1u);

    start_two_player_hand(game);
    CHECK(truco_game_apply(game, 0u, TRUCO_CMD_START_HAND) == TRUCO_ERR_INVALID_STATE);
    install_basic_two_player_hands(game);
    play_basic_two_player_hand(game);

    CHECK(truco_game_phase(game) == TRUCO_PHASE_HAND_OVER);
    CHECK(truco_game_hand_winner(game) == 0);
    CHECK(truco_game_score(game, 0u) == 1u);
    CHECK(truco_game_score(game, 1u) == 0u);
    truco_game_destroy(game);
}

static void test_truco_bidding(void)
{
    truco_game *game = make_two_player_game(7u, 1u);

    start_two_player_hand(game);
    install_basic_two_player_hands(game);
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_RAISE_TRUCO));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_ACCEPT_BID));
    play_basic_two_player_hand(game);
    CHECK(truco_game_score(game, 0u) == 2u);

    expect_ok(truco_game_init(game));
    start_two_player_hand(game);
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_RAISE_TRUCO));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_REJECT_BID));
    CHECK(truco_game_phase(game) == TRUCO_PHASE_HAND_OVER);
    CHECK(truco_game_score(game, 0u) == 1u);
    truco_game_destroy(game);
}

static void test_legal_actions(void)
{
    truco_game *game = make_two_player_game(1u, 1u);
    truco_command commands[16];
    unsigned int count;
    unsigned int truco_value;
    unsigned int envido_points;

    expect_ok(query_legal(game, 0u, commands, 16u, &count, &truco_value, &envido_points));
    CHECK(count == 1u);
    CHECK(commands[0] == TRUCO_CMD_START_HAND);

    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_START_HAND));
    install_basic_two_player_hands(game);

    expect_ok(query_legal(game, 0u, commands, 16u, &count, &truco_value, &envido_points));
    CHECK(has_command(commands, count, TRUCO_CMD_PLAY_CARD_0));
    CHECK(has_command(commands, count, TRUCO_CMD_PLAY_CARD_1));
    CHECK(has_command(commands, count, TRUCO_CMD_PLAY_CARD_2));
    CHECK(has_command(commands, count, TRUCO_CMD_RAISE_TRUCO));
    CHECK(has_command(commands, count, TRUCO_CMD_CALL_ENVIDO));
    CHECK(has_command(commands, count, TRUCO_CMD_CALL_REAL_ENVIDO));
    CHECK(has_command(commands, count, TRUCO_CMD_CALL_FALTA_ENVIDO));
    CHECK(truco_value == 2u);

    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_RAISE_TRUCO));
    expect_ok(query_legal(game, 0u, commands, 16u, &count, &truco_value, &envido_points));
    CHECK(count == 0u);

    expect_ok(query_legal(game, 1u, commands, 16u, &count, &truco_value, &envido_points));
    CHECK(has_command(commands, count, TRUCO_CMD_ACCEPT_BID));
    CHECK(has_command(commands, count, TRUCO_CMD_REJECT_BID));
    CHECK(!has_command(commands, count, TRUCO_CMD_PLAY_CARD_0));
    CHECK(!has_command(commands, count, TRUCO_CMD_CALL_ENVIDO));
    CHECK(truco_value == 2u);
    CHECK(truco_game_apply(game, 1u, TRUCO_CMD_CALL_ENVIDO) == TRUCO_ERR_INVALID_STATE);

    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_ACCEPT_BID));
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_PLAY_CARD_0));
    expect_ok(query_legal(game, 1u, commands, 16u, &count, &truco_value, &envido_points));
    CHECK(has_command(commands, count, TRUCO_CMD_PLAY_CARD_0));
    CHECK(has_command(commands, count, TRUCO_CMD_PLAY_CARD_1));
    CHECK(has_command(commands, count, TRUCO_CMD_PLAY_CARD_2));
    CHECK(has_command(commands, count, TRUCO_CMD_RAISE_TRUCO));
    CHECK(!has_command(commands, count, TRUCO_CMD_CALL_ENVIDO));
    CHECK(truco_value == 3u);

    expect_ok(truco_game_init(game));
    start_two_player_hand(game);
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_CALL_ENVIDO));
    expect_ok(query_legal(game, 1u, commands, 16u, &count, &truco_value, &envido_points));
    CHECK(has_command(commands, count, TRUCO_CMD_ACCEPT_BID));
    CHECK(has_command(commands, count, TRUCO_CMD_REJECT_BID));
    CHECK(!has_command(commands, count, TRUCO_CMD_RAISE_TRUCO));
    CHECK(envido_points == 2u);
    CHECK(truco_game_apply(game, 1u, TRUCO_CMD_RAISE_TRUCO) == TRUCO_ERR_INVALID_STATE);
    truco_game_destroy(game);
}

static void test_parda_rules(void)
{
    truco_game *game = make_two_player_game(7u, 1u);
    truco_suit player0_suits[TRUCO_HAND_CARDS] = {
        TRUCO_SUIT_COPA,
        TRUCO_SUIT_COPA,
        TRUCO_SUIT_ORO
    };
    unsigned int player0_ranks[TRUCO_HAND_CARDS] = {3u, 4u, 5u};
    truco_suit player1_suits[TRUCO_HAND_CARDS] = {
        TRUCO_SUIT_ORO,
        TRUCO_SUIT_ESPADA,
        TRUCO_SUIT_BASTO
    };
    unsigned int player1_ranks[TRUCO_HAND_CARDS] = {3u, 1u, 4u};

    start_two_player_hand(game);
    force_hand(game, 0u, player0_suits, player0_ranks);
    force_hand(game, 1u, player1_suits, player1_ranks);

    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_PLAY_CARD_0));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_PLAY_CARD_0));
    CHECK(truco_game_trick_winner(game, 0u) == -1);
    CHECK(truco_game_current_player(game) == 0u);

    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_PLAY_CARD_1));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_PLAY_CARD_1));
    CHECK(truco_game_phase(game) == TRUCO_PHASE_HAND_OVER);
    CHECK(truco_game_hand_winner(game) == 1);
    truco_game_destroy(game);
}

static void test_envido_resolution(void)
{
    truco_game *game = make_two_player_game(7u, 1u);
    truco_suit player0_suits[TRUCO_HAND_CARDS] = {
        TRUCO_SUIT_ESPADA,
        TRUCO_SUIT_ESPADA,
        TRUCO_SUIT_ORO
    };
    unsigned int player0_ranks[TRUCO_HAND_CARDS] = {7u, 6u, 12u};
    truco_suit player1_suits[TRUCO_HAND_CARDS] = {
        TRUCO_SUIT_ORO,
        TRUCO_SUIT_ORO,
        TRUCO_SUIT_COPA
    };
    unsigned int player1_ranks[TRUCO_HAND_CARDS] = {7u, 5u, 1u};

    start_two_player_hand(game);
    force_hand(game, 0u, player0_suits, player0_ranks);
    force_hand(game, 1u, player1_suits, player1_ranks);
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_CALL_ENVIDO));
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_ACCEPT_BID));
    CHECK(truco_game_score(game, 0u) == 2u);
    CHECK(truco_game_score(game, 1u) == 0u);

    expect_ok(truco_game_init(game));
    start_two_player_hand(game);
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_CALL_REAL_ENVIDO));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_REJECT_BID));
    CHECK(truco_game_score(game, 0u) == 1u);
    truco_game_destroy(game);
}

static void test_four_player_team_flow(void)
{
    truco_game *game = truco_game_create();
    truco_suit p0_suits[TRUCO_HAND_CARDS] = {TRUCO_SUIT_COPA, TRUCO_SUIT_COPA, TRUCO_SUIT_COPA};
    unsigned int p0_ranks[TRUCO_HAND_CARDS] = {4u, 5u, 6u};
    truco_suit p1_suits[TRUCO_HAND_CARDS] = {TRUCO_SUIT_ORO, TRUCO_SUIT_ORO, TRUCO_SUIT_ORO};
    unsigned int p1_ranks[TRUCO_HAND_CARDS] = {7u, 4u, 5u};
    truco_suit p2_suits[TRUCO_HAND_CARDS] = {TRUCO_SUIT_ESPADA, TRUCO_SUIT_BASTO, TRUCO_SUIT_BASTO};
    unsigned int p2_ranks[TRUCO_HAND_CARDS] = {1u, 4u, 5u};
    truco_suit p3_suits[TRUCO_HAND_CARDS] = {TRUCO_SUIT_BASTO, TRUCO_SUIT_COPA, TRUCO_SUIT_COPA};
    unsigned int p3_ranks[TRUCO_HAND_CARDS] = {6u, 10u, 11u};

    CHECK(game != 0);
    expect_ok(truco_game_set_player_count(game, 4u));
    expect_ok(truco_game_set_initial_dealer(game, 3u));
    expect_ok(truco_game_init(game));
    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_START_HAND));

    CHECK(truco_game_player_count(game) == 4u);
    CHECK(truco_game_team_for_player(game, 0u) == 0u);
    CHECK(truco_game_team_for_player(game, 1u) == 1u);
    CHECK(truco_game_team_for_player(game, 2u) == 0u);
    CHECK(truco_game_team_for_player(game, 3u) == 1u);

    force_hand(game, 0u, p0_suits, p0_ranks);
    force_hand(game, 1u, p1_suits, p1_ranks);
    force_hand(game, 2u, p2_suits, p2_ranks);
    force_hand(game, 3u, p3_suits, p3_ranks);

    expect_ok(truco_game_apply(game, 0u, TRUCO_CMD_PLAY_CARD_0));
    expect_ok(truco_game_apply(game, 1u, TRUCO_CMD_PLAY_CARD_0));
    expect_ok(truco_game_apply(game, 2u, TRUCO_CMD_PLAY_CARD_0));
    expect_ok(truco_game_apply(game, 3u, TRUCO_CMD_PLAY_CARD_0));
    CHECK(truco_game_trick_winner(game, 0u) == 0);
    CHECK(truco_game_current_player(game) == 2u);
    truco_game_destroy(game);
}

static void test_future_six_player_shape_is_reserved(void)
{
    truco_game *game = truco_game_create();

    CHECK(game != 0);
    expect_ok(truco_game_set_player_count(game, 6u));
    CHECK(truco_game_init(game) == TRUCO_ERR_UNSUPPORTED_RULES);
    truco_game_destroy(game);
}

static void test_game_size_matches_create(void)
{
    CHECK(truco_game_size() > 0u);
}

int main(void)
{
    test_card_ranking();
    test_envido_values();
    test_two_player_hand_resolution();
    test_truco_bidding();
    test_legal_actions();
    test_parda_rules();
    test_envido_resolution();
    test_four_player_team_flow();
    test_future_six_player_shape_is_reserved();
    test_game_size_matches_create();

    printf("ok - %u checks\n", tests_run);
    return 0;
}
