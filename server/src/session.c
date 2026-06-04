#include "truco_server_session.h"

#include "truco.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TRUCO_SERVER_MAX_SESSIONS 32u
#define TRUCO_SERVER_TOKEN_CHARS "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"

struct truco_server_session {
    int active;
    char token[TRUCO_SERVER_TOKEN_LEN + 1u];
    truco_game *game;
    unsigned int player_count;
    int slot_used[TRUCO_MAX_PLAYERS];
};

static struct truco_server_session sessions[TRUCO_SERVER_MAX_SESSIONS];
static unsigned int token_counter;

static int token_char_index(char ch)
{
    const char *found = strchr(TRUCO_SERVER_TOKEN_CHARS, ch);

    if (found == 0) {
        return -1;
    }
    return (int)(found - TRUCO_SERVER_TOKEN_CHARS);
}

static int token_is_valid(const char *token)
{
    size_t i;

    if (token == 0) {
        return 0;
    }
    for (i = 0u; i < TRUCO_SERVER_TOKEN_LEN; ++i) {
        char ch = token[i];
        if (ch >= 'a' && ch <= 'z') {
            ch = (char)(ch - 'a' + 'A');
        }
        if (token_char_index(ch) < 0) {
            return 0;
        }
    }
    return token[i] == '\0' || token[i] == '\r' || token[i] == '\n' || token[i] == ' ';
}

static void make_token(char token_out[TRUCO_SERVER_TOKEN_LEN + 1u])
{
    size_t i;

    for (i = 0u; i < TRUCO_SERVER_TOKEN_LEN; ++i) {
        token_out[i] = TRUCO_SERVER_TOKEN_CHARS[token_counter % 36u];
        token_counter += 17u;
    }
    token_out[TRUCO_SERVER_TOKEN_LEN] = '\0';
}

static struct truco_server_session *find_session(const char *token)
{
    size_t i;

    for (i = 0u; i < TRUCO_SERVER_MAX_SESSIONS; ++i) {
        if (sessions[i].active &&
            strncmp(sessions[i].token, token, TRUCO_SERVER_TOKEN_LEN) == 0) {
            return &sessions[i];
        }
    }
    return 0;
}

static struct truco_server_session *alloc_session(void)
{
    size_t i;

    for (i = 0u; i < TRUCO_SERVER_MAX_SESSIONS; ++i) {
        if (!sessions[i].active) {
            memset(&sessions[i], 0, sizeof(sessions[i]));
            sessions[i].active = 1;
            return &sessions[i];
        }
    }
    return 0;
}

void truco_server_session_reset(void)
{
    size_t i;

    for (i = 0u; i < TRUCO_SERVER_MAX_SESSIONS; ++i) {
        if (sessions[i].game != 0) {
            truco_game_destroy(sessions[i].game);
        }
        memset(&sessions[i], 0, sizeof(sessions[i]));
    }
    token_counter = 1u;
    srand(1);
}

struct truco_server_session *truco_server_session_host(unsigned int player_count,
                                                     int flor_enabled,
                                                     char token_out[TRUCO_SERVER_TOKEN_LEN + 1u],
                                                     unsigned int *player_out)
{
    struct truco_server_session *session;

    if (token_out == 0 || player_out == 0) {
        return 0;
    }
    if (player_count != 2u && player_count != 4u) {
        return 0;
    }

    session = alloc_session();
    if (session == 0) {
        return 0;
    }

    session->game = truco_game_create();
    if (session->game == 0) {
        session->active = 0;
        return 0;
    }

    if (truco_game_set_player_count(session->game, player_count) != TRUCO_OK) {
        truco_game_destroy(session->game);
        session->active = 0;
        return 0;
    }

    if (flor_enabled &&
        truco_game_set_flor_enabled(session->game, 1) != TRUCO_OK) {
        truco_game_destroy(session->game);
        session->active = 0;
        return 0;
    }

    make_token(session->token);
    memcpy(token_out, session->token, TRUCO_SERVER_TOKEN_LEN + 1u);
    session->player_count = player_count;
    session->slot_used[0] = 1;
    *player_out = 0u;
    return session;
}

struct truco_server_session *truco_server_session_join(const char *token,
                                                       unsigned int *player_out)
{
    struct truco_server_session *session;
    unsigned int player;

    if (token == 0 || player_out == 0 || !token_is_valid(token)) {
        return 0;
    }

    session = find_session(token);
    if (session == 0) {
        return 0;
    }

    for (player = 0u; player < session->player_count; ++player) {
        if (!session->slot_used[player]) {
            session->slot_used[player] = 1;
            *player_out = player;
            return session;
        }
    }
    return 0;
}

struct truco_server_session *truco_server_session_get(const char *token)
{
    if (token == 0 || !token_is_valid(token)) {
        return 0;
    }
    return find_session(token);
}

int truco_server_session_destroy(const char *token)
{
    struct truco_server_session *session;

    session = find_session(token);
    if (session == 0) {
        return 0;
    }

    if (session->game != 0) {
        truco_game_destroy(session->game);
        session->game = 0;
    }
    session->active = 0;
    memset(session->token, 0, sizeof(session->token));
    memset(session->slot_used, 0, sizeof(session->slot_used));
    session->player_count = 0u;
    return 1;
}

truco_game *truco_server_session_game(struct truco_server_session *session)
{
    if (session == 0) {
        return 0;
    }
    return session->game;
}

unsigned int truco_server_session_player_count(struct truco_server_session *session)
{
    if (session == 0) {
        return 0u;
    }
    return session->player_count;
}

int truco_server_session_slots_full(struct truco_server_session *session)
{
    unsigned int player;

    if (session == 0) {
        return 1;
    }
    for (player = 0u; player < session->player_count; ++player) {
        if (!session->slot_used[player]) {
            return 0;
        }
    }
    return 1;
}

const char *truco_server_session_token(struct truco_server_session *session)
{
    if (session == 0) {
        return "";
    }
    return session->token;
}
