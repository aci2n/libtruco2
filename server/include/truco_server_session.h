#ifndef TRUCO_SERVER_SESSION_H
#define TRUCO_SERVER_SESSION_H

#include "truco_server.h"

#include "truco.h"

struct truco_server_session;

void truco_server_session_reset(void);

struct truco_server_session *truco_server_session_host(unsigned int player_count,
                                                       int flor_enabled,
                                                       char token_out[TRUCO_SERVER_TOKEN_LEN + 1u],
                                                       unsigned int *player_out);

struct truco_server_session *truco_server_session_join(const char *token,
                                                         unsigned int *player_out);

struct truco_server_session *truco_server_session_get(const char *token);

int truco_server_session_destroy(const char *token);

truco_game *truco_server_session_game(struct truco_server_session *session);

unsigned int truco_server_session_player_count(struct truco_server_session *session);

int truco_server_session_slots_full(struct truco_server_session *session);

const char *truco_server_session_token(struct truco_server_session *session);

int truco_server_format_view(struct truco_server_session *session,
                             unsigned int viewer,
                             char *response,
                             size_t response_size);

int truco_server_apply_command(struct truco_server_session *session,
                               unsigned int player,
                               unsigned int command_index,
                               char *response,
                               size_t response_size);

void truco_server_normalize_line(char *line);

void truco_server_uppercase_line(char *line);

int truco_server_parse_host(const char *line,
                            unsigned int *player_count_out,
                            int *flor_enabled_out);

int truco_server_parse_join(const char *line, char token_out[TRUCO_SERVER_TOKEN_LEN + 1u]);

int truco_server_parse_command_index(const char *line, unsigned int *index_out);

#endif
