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

static void test_host_join_and_start(void)
{
    char token[TRUCO_SERVER_TOKEN_LEN + 1u];
    char response[TRUCO_SERVER_RESPONSE_MAX];
    unsigned int player;
    truco_server_status status;

    truco_server_reset();

    expect_ok(truco_server_host(2u, token, &player, response, sizeof(response)));
    CHECK(player == 0u);
    CHECK(strlen(token) == TRUCO_SERVER_TOKEN_LEN);
    CHECK(strstr(response, "token:") != 0);
    CHECK(strstr(response, "you are player 0") != 0);
    CHECK(strstr(response, "commands:") != 0);

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
    expect_ok(truco_server_host(2u, token, &player, response, sizeof(response)));
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
    expect_ok(truco_server_host(2u, token, &player, response, sizeof(response)));
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
    char token[TRUCO_SERVER_TOKEN_LEN + 1u];
    unsigned int index;
    char line[32];

    CHECK(truco_server_parse_host("HOST", &count));
    CHECK(count == 2u);
    CHECK(truco_server_parse_host("host 4", &count));
    CHECK(count == 4u);
    CHECK(!truco_server_parse_host("JOIN ABC123", &count));

    CHECK(truco_server_parse_join("JOIN ABC123", token));
    CHECK(strcmp(token, "ABC123") == 0);

    strcpy(line, " 2 ");
    CHECK(truco_server_parse_command_index(line, &index));
    CHECK(index == 2u);
}

int main(void)
{
    test_parse_helpers();
    test_token_validation();
    test_host_join_and_start();
    test_command_number_plays_card();
    test_quit_kills_session();

    printf("ok - %u checks\n", tests_run);
    return 0;
}
