#include "truco_server.h"
#include "truco_server_session.h"

#include <stdio.h>
#include <string.h>

static unsigned int tests_run;

#define CHECK(cond)                         \
    do {                                    \
        ++tests_run;                        \
        if (!(cond)) {                      \
            fprintf(stderr,                 \
                    "FAIL %s:%d: %s\n",     \
                    __FILE__, __LINE__, #cond); \
            return;                         \
        }                                   \
    } while (0)

static void expect_ok(truco_server_status status)
{
    CHECK(status == TRUCO_SERVER_OK);
}

static unsigned int find_command_index(const char *response, const char *label)
{
    const char *cursor = response;

    if (response == 0 || label == 0) {
        return 0u;
    }

    while ((cursor = strstr(cursor, label)) != 0) {
        const char *line_start = cursor;
        unsigned int index = 0u;

        while (line_start > response && line_start[-1] != '\n') {
            --line_start;
        }
        if (sscanf(line_start, "  %u)", &index) == 1 ||
            sscanf(line_start, "%u)", &index) == 1) {
            return index;
        }
        ++cursor;
    }
    return 0u;
}

static void start_two_player_hand(const char *token,
                                  char *response0,
                                  char *response1,
                                  size_t response_size)
{
    expect_ok(truco_server_line(token, 1u, "1", response1, response_size));
    CHECK(strstr(response1, "phase: PLAYING") != 0);
    expect_ok(truco_server_line(token, 0u, "HELP", response0, response_size));
    CHECK(strstr(response0, "phase: PLAYING") != 0);
}

static void test_host_join_and_start(void)
{
    char token[TRUCO_SERVER_TOKEN_LEN + 1u];
    char response[TRUCO_SERVER_RESPONSE_MAX];
    unsigned int player;
    truco_server_status status;

    truco_server_reset();

    expect_ok(truco_server_host(2u, 0, token, &player, response, sizeof(response)));
    CHECK(player == 0u);
    CHECK(strlen(token) == TRUCO_SERVER_TOKEN_LEN);
    CHECK(strstr(response, "token:") != 0);
    CHECK(strstr(response, "you are player 0") != 0);
    CHECK(strstr(response, "commands:") != 0);
    CHECK(strstr(response, "rules: flor=off") != 0);

    expect_ok(truco_server_join(token, &player, response, sizeof(response)));
    CHECK(player == 1u);
    CHECK(strstr(response, "you are player 1") != 0);
    CHECK(strstr(response, "waiting for players") == 0);

    expect_ok(truco_server_line(token, 1u, "1", response, sizeof(response)));
    CHECK(strstr(response, "phase: PLAYING") != 0);
    CHECK(strstr(response, "your hand:") != 0);

    status = truco_server_line(token, 0u, "9", response, sizeof(response));
    CHECK(status != TRUCO_SERVER_OK);
}

static void test_command_number_plays_card(void)
{
    char token[TRUCO_SERVER_TOKEN_LEN + 1u];
    char response[TRUCO_SERVER_RESPONSE_MAX];
    unsigned int player;

    truco_server_reset();
    expect_ok(truco_server_host(2u, 0, token, &player, response, sizeof(response)));
    expect_ok(truco_server_join(token, &player, response, sizeof(response)));

    expect_ok(truco_server_line(token, 1u, "1", response, sizeof(response)));
    expect_ok(truco_server_line(token, 0u, "1", response, sizeof(response)));
    expect_ok(truco_server_line(token, 1u, "1", response, sizeof(response)));
    CHECK(strstr(response, "table:") != 0);
    CHECK(strstr(response, "trick 0:") != 0);
    CHECK(strstr(response, "p0=") != 0);
    CHECK(strstr(response, "p1=") != 0);
}

static void test_token_validation(void)
{
    char response[TRUCO_SERVER_RESPONSE_MAX];
    unsigned int player;

    truco_server_reset();
    CHECK(truco_server_join("bad", &player, response, sizeof(response)) ==
          TRUCO_SERVER_ERR_NOT_FOUND);
    CHECK(truco_server_join("ABC12", &player, response, sizeof(response)) ==
          TRUCO_SERVER_ERR_NOT_FOUND);
}

static void test_quit_kills_session(void)
{
    char token[TRUCO_SERVER_TOKEN_LEN + 1u];
    char response[TRUCO_SERVER_RESPONSE_MAX];
    unsigned int player;

    truco_server_reset();
    expect_ok(truco_server_host(2u, 0, token, &player, response, sizeof(response)));
    expect_ok(truco_server_join(token, &player, response, sizeof(response)));

    expect_ok(truco_server_quit(token, 0u, response, sizeof(response)));
    CHECK(strstr(response, "session ended") != 0);
    CHECK(truco_server_session_get(token) == 0);
    CHECK(truco_server_join(token, &player, response, sizeof(response)) ==
          TRUCO_SERVER_ERR_NOT_FOUND);
    CHECK(truco_server_line(token, 0u, "HELP", response, sizeof(response)) ==
          TRUCO_SERVER_ERR_NOT_FOUND);
}

static void test_parse_helpers(void)
{
    unsigned int count;
    int flor_enabled;
    char token[TRUCO_SERVER_TOKEN_LEN + 1u];
    unsigned int index;
    char line[32];

    CHECK(truco_server_parse_host("HOST", &count, &flor_enabled));
    CHECK(count == 2u);
    CHECK(flor_enabled == 0);
    CHECK(truco_server_parse_host("host 4", &count, &flor_enabled));
    CHECK(count == 4u);
    CHECK(flor_enabled == 0);
    CHECK(truco_server_parse_host("HOST FLOR", &count, &flor_enabled));
    CHECK(count == 2u);
    CHECK(flor_enabled == 1);
    CHECK(truco_server_parse_host("HOST 4 FLOR", &count, &flor_enabled));
    CHECK(count == 4u);
    CHECK(flor_enabled == 1);
    CHECK(truco_server_parse_host("HOST FLOR 2", &count, &flor_enabled));
    CHECK(count == 2u);
    CHECK(flor_enabled == 1);
    CHECK(!truco_server_parse_host("JOIN ABC123", &count, &flor_enabled));
    CHECK(!truco_server_parse_host("HOST 3", &count, &flor_enabled));

    CHECK(truco_server_parse_join("JOIN ABC123", token));
    CHECK(strcmp(token, "ABC123") == 0);

    strcpy(line, " 2 ");
    CHECK(truco_server_parse_command_index(line, &index));
    CHECK(index == 2u);
}

static void test_host_flor_shows_flor_in_view(void)
{
    char token[TRUCO_SERVER_TOKEN_LEN + 1u];
    char response[TRUCO_SERVER_RESPONSE_MAX];
    unsigned int player;

    truco_server_reset();
    expect_ok(truco_server_host(2u, 1, token, &player, response, sizeof(response)));
    CHECK(strstr(response, "rules: flor=on") != 0);
    expect_ok(truco_server_join(token, &player, response, sizeof(response)));
    start_two_player_hand(token, response, response, sizeof(response));
    CHECK(strstr(response, "flor:") != 0);
}

static void test_envido_primero_on_truco_in_view(void)
{
    char token[TRUCO_SERVER_TOKEN_LEN + 1u];
    char response0[TRUCO_SERVER_RESPONSE_MAX];
    char response1[TRUCO_SERVER_RESPONSE_MAX];
    unsigned int player;
    unsigned int raise_truco;
    unsigned int call_envido;

    truco_server_reset();
    expect_ok(truco_server_host(2u, 0, token, &player, response0, sizeof(response0)));
    expect_ok(truco_server_join(token, &player, response1, sizeof(response1)));
    start_two_player_hand(token, response0, response1, sizeof(response0));

    raise_truco = find_command_index(response0, "RAISE TRUCO");
    CHECK(raise_truco != 0u);
    snprintf(response0, sizeof(response0), "%u", raise_truco);
    expect_ok(truco_server_line(token, 0u, response0, response0, sizeof(response0)));
    CHECK(strstr(response0, "pending truco: 2") != 0);

    expect_ok(truco_server_line(token, 1u, "HELP", response1, sizeof(response1)));
    call_envido = find_command_index(response1, "CALL ENVIDO");
    CHECK(call_envido != 0u);
    snprintf(response1, sizeof(response1), "%u", call_envido);
    expect_ok(truco_server_line(token, 1u, response1, response1, sizeof(response1)));
    CHECK(strstr(response1, "pending envido: 2") != 0);
    CHECK(strstr(response1, "pending truco: 2") != 0);
}

static void test_envido_counter_in_command_list(void)
{
    char token[TRUCO_SERVER_TOKEN_LEN + 1u];
    char response0[TRUCO_SERVER_RESPONSE_MAX];
    char response1[TRUCO_SERVER_RESPONSE_MAX];
    unsigned int player;
    unsigned int call_envido;
    unsigned int counter_envido;

    truco_server_reset();
    expect_ok(truco_server_host(2u, 0, token, &player, response0, sizeof(response0)));
    expect_ok(truco_server_join(token, &player, response1, sizeof(response1)));
    start_two_player_hand(token, response0, response1, sizeof(response0));

    call_envido = find_command_index(response0, "CALL ENVIDO");
    CHECK(call_envido != 0u);
    snprintf(response0, sizeof(response0), "%u", call_envido);
    expect_ok(truco_server_line(token, 0u, response0, response0, sizeof(response0)));
    CHECK(strstr(response0, "pending envido: 2") != 0);

    expect_ok(truco_server_line(token, 1u, "HELP", response1, sizeof(response1)));
    counter_envido = find_command_index(response1, "CALL ENVIDO");
    CHECK(counter_envido != 0u);
    snprintf(response1, sizeof(response1), "%u", counter_envido);
    expect_ok(truco_server_line(token, 1u, response1, response1, sizeof(response1)));
    CHECK(strstr(response1, "pending envido: 4") != 0);
}

int main(void)
{
    test_parse_helpers();
    test_token_validation();
    test_host_join_and_start();
    test_command_number_plays_card();
    test_quit_kills_session();
    test_host_flor_shows_flor_in_view();
    test_envido_primero_on_truco_in_view();
    test_envido_counter_in_command_list();

    printf("ok - %u checks\n", tests_run);
    return 0;
}
