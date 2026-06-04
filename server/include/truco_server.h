#ifndef TRUCO_SERVER_H
#define TRUCO_SERVER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TRUCO_SERVER_TOKEN_LEN 6u
#define TRUCO_SERVER_RESPONSE_MAX 4096u

typedef enum truco_server_status {
    TRUCO_SERVER_OK = 0,
    TRUCO_SERVER_ERR_INVALID = -1,
    TRUCO_SERVER_ERR_FULL = -2,
    TRUCO_SERVER_ERR_NOT_FOUND = -3,
    TRUCO_SERVER_ERR_TAKEN = -4,
    TRUCO_SERVER_ERR_NOT_YOUR_TURN = -5,
    TRUCO_SERVER_ERR_STATE = -6
} truco_server_status;

void truco_server_reset(void);

truco_server_status truco_server_host(unsigned int player_count,
                                      int flor_enabled,
                                      char token_out[TRUCO_SERVER_TOKEN_LEN + 1u],
                                      unsigned int *player_out,
                                      char *response,
                                      size_t response_size);

truco_server_status truco_server_join(const char *token,
                                      unsigned int *player_out,
                                      char *response,
                                      size_t response_size);

truco_server_status truco_server_line(const char *token,
                                      unsigned int player,
                                      const char *line,
                                      char *response,
                                      size_t response_size);

truco_server_status truco_server_quit(const char *token,
                                      unsigned int player,
                                      char *response,
                                      size_t response_size);

#ifdef __cplusplus
}
#endif

#endif
