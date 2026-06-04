#include "truco_server.h"

#include "truco_server_session.h"

#include <stdio.h>
#include <string.h>

void truco_server_reset(void)
{
    truco_server_session_reset();
}

truco_server_status truco_server_host(unsigned int player_count,
                                      char token_out[TRUCO_SERVER_TOKEN_LEN + 1u],
                                      unsigned int *player_out,
                                      char *response,
                                      size_t response_size)
{
    struct truco_server_session *session;

    if (token_out == 0 || player_out == 0 || response == 0 || response_size == 0u) {
        return TRUCO_SERVER_ERR_INVALID;
    }

    session = truco_server_session_host(player_count, token_out, player_out);
    if (session == 0) {
        snprintf(response, response_size, "error: could not host session\r\n");
        return TRUCO_SERVER_ERR_FULL;
    }

    if (truco_server_format_view(session, *player_out, response, response_size) != 0) {
        return TRUCO_SERVER_ERR_STATE;
    }
    return TRUCO_SERVER_OK;
}

truco_server_status truco_server_join(const char *token,
                                      unsigned int *player_out,
                                      char *response,
                                      size_t response_size)
{
    struct truco_server_session *session;

    if (token == 0 || player_out == 0 || response == 0 || response_size == 0u) {
        return TRUCO_SERVER_ERR_INVALID;
    }

    session = truco_server_session_join(token, player_out);
    if (session == 0) {
        if (truco_server_session_get(token) == 0) {
            snprintf(response, response_size, "error: session not found\r\n");
            return TRUCO_SERVER_ERR_NOT_FOUND;
        }
        snprintf(response, response_size, "error: session full\r\n");
        return TRUCO_SERVER_ERR_TAKEN;
    }

    if (truco_server_format_view(session, *player_out, response, response_size) != 0) {
        return TRUCO_SERVER_ERR_STATE;
    }
    return TRUCO_SERVER_OK;
}

truco_server_status truco_server_line(const char *token,
                                      unsigned int player,
                                      const char *line,
                                      char *response,
                                      size_t response_size)
{
    struct truco_server_session *session;
    char buffer[128];
    unsigned int command_index;

    if (token == 0 || line == 0 || response == 0 || response_size == 0u) {
        return TRUCO_SERVER_ERR_INVALID;
    }

    session = truco_server_session_get(token);
    if (session == 0) {
        snprintf(response, response_size, "error: session not found\r\n");
        return TRUCO_SERVER_ERR_NOT_FOUND;
    }
    if (player >= truco_server_session_player_count(session)) {
        return TRUCO_SERVER_ERR_INVALID;
    }

    strncpy(buffer, line, sizeof(buffer) - 1u);
    buffer[sizeof(buffer) - 1u] = '\0';
    truco_server_uppercase_line(buffer);

    if (strcmp(buffer, "HELP") == 0) {
        if (truco_server_format_view(session, player, response, response_size) != 0) {
            return TRUCO_SERVER_ERR_STATE;
        }
        return TRUCO_SERVER_OK;
    }

    if (strcmp(buffer, "QUIT") == 0) {
        return truco_server_quit(token, player, response, response_size);
    }

    if (!truco_server_parse_command_index(buffer, &command_index)) {
        snprintf(response, response_size,
                 "error: enter a command number (1..n), HELP, or QUIT\r\n");
        return TRUCO_SERVER_ERR_INVALID;
    }

    if (truco_server_apply_command(session, player, command_index, response,
                                   response_size) != 0) {
        return TRUCO_SERVER_ERR_STATE;
    }
    return TRUCO_SERVER_OK;
}

truco_server_status truco_server_quit(const char *token,
                                      unsigned int player,
                                      char *response,
                                      size_t response_size)
{
    struct truco_server_session *session;

    if (token == 0 || response == 0 || response_size == 0u) {
        return TRUCO_SERVER_ERR_INVALID;
    }

    session = truco_server_session_get(token);
    if (session == 0) {
        snprintf(response, response_size, "error: session not found\r\n");
        return TRUCO_SERVER_ERR_NOT_FOUND;
    }
    if (player >= truco_server_session_player_count(session)) {
        return TRUCO_SERVER_ERR_INVALID;
    }

    if (!truco_server_session_destroy(token)) {
        snprintf(response, response_size, "error: session not found\r\n");
        return TRUCO_SERVER_ERR_NOT_FOUND;
    }

    snprintf(response, response_size, "session ended\r\n");
    return TRUCO_SERVER_OK;
}
