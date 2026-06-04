#include "truco_server.h"

#include "truco_server_session.h"

#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define TRUCO_SERVER_DEFAULT_PORT 5555u
#define TRUCO_SERVER_MAX_CLIENTS 64u
#define TRUCO_SERVER_LINE_MAX 256u

typedef struct truco_client {
    int active;
    int fd;
    int authed;
    char token[TRUCO_SERVER_TOKEN_LEN + 1u];
    unsigned int player;
    char line[TRUCO_SERVER_LINE_MAX];
    size_t line_len;
} truco_client;

static truco_client clients[TRUCO_SERVER_MAX_CLIENTS];

static void send_text(int fd, const char *text)
{
    size_t left;
    const char *cursor;

    if (fd < 0 || text == 0) {
        return;
    }
    left = strlen(text);
    cursor = text;
    while (left > 0u) {
        ssize_t written = write(fd, cursor, left);
        if (written <= 0) {
            return;
        }
        cursor += written;
        left -= (size_t)written;
    }
}

static void close_client(size_t index)
{
    if (clients[index].fd >= 0) {
        close(clients[index].fd);
    }
    memset(&clients[index], 0, sizeof(clients[index]));
    clients[index].fd = -1;
}

static int alloc_client(int fd)
{
    size_t i;

    for (i = 0u; i < TRUCO_SERVER_MAX_CLIENTS; ++i) {
        if (!clients[i].active) {
            memset(&clients[i], 0, sizeof(clients[i]));
            clients[i].active = 1;
            clients[i].fd = fd;
            clients[i].authed = 0;
            clients[i].line_len = 0u;
            return (int)i;
        }
    }
    return -1;
}

static void end_session_clients(const char *token)
{
    size_t i;

    for (i = 0u; i < TRUCO_SERVER_MAX_CLIENTS; ++i) {
        if (!clients[i].active || !clients[i].authed ||
            strncmp(clients[i].token, token, TRUCO_SERVER_TOKEN_LEN) != 0) {
            continue;
        }
        send_text(clients[i].fd, "session ended\r\n");
        clients[i].authed = 0;
        clients[i].token[0] = '\0';
        clients[i].player = 0u;
        clients[i].line_len = 0u;
    }
}

static void broadcast_session(const char *token)
{
    char response[TRUCO_SERVER_RESPONSE_MAX];
    size_t i;
    struct truco_server_session *session;

    session = truco_server_session_get(token);
    if (session == 0) {
        return;
    }

    for (i = 0u; i < TRUCO_SERVER_MAX_CLIENTS; ++i) {
        if (!clients[i].active || !clients[i].authed ||
            strncmp(clients[i].token, token, TRUCO_SERVER_TOKEN_LEN) != 0) {
            continue;
        }
        if (truco_server_format_view(session, clients[i].player, response,
                                     sizeof(response)) != 0) {
            continue;
        }
        send_text(clients[i].fd, response);
        send_text(clients[i].fd, "\r\n");
    }
}

static void handle_line(size_t client_index, const char *line)
{
    char response[TRUCO_SERVER_RESPONSE_MAX];
    char buffer[TRUCO_SERVER_LINE_MAX];
    truco_server_status status;
    unsigned int player_count;
    int flor_enabled;
    char token[TRUCO_SERVER_TOKEN_LEN + 1u];
    unsigned int player;
    unsigned int command_index;
    struct truco_server_session *session;

    strncpy(buffer, line, sizeof(buffer) - 1u);
    buffer[sizeof(buffer) - 1u] = '\0';
    truco_server_normalize_line(buffer);

    if (!clients[client_index].authed) {
        if (truco_server_parse_host(buffer, &player_count, &flor_enabled)) {
            status = truco_server_host(player_count, flor_enabled, token, &player,
                                       response, sizeof(response));
            if (status == TRUCO_SERVER_OK) {
                clients[client_index].authed = 1;
                memcpy(clients[client_index].token, token,
                       TRUCO_SERVER_TOKEN_LEN + 1u);
                clients[client_index].player = player;
                send_text(clients[client_index].fd,
                          "hosted. share token to join.\r\n");
                send_text(clients[client_index].fd, response);
                send_text(clients[client_index].fd, "\r\n");
            } else {
                send_text(clients[client_index].fd, response);
            }
            return;
        }

        if (truco_server_parse_join(buffer, token)) {
            status = truco_server_join(token, &player, response, sizeof(response));
            if (status == TRUCO_SERVER_OK) {
                clients[client_index].authed = 1;
                memcpy(clients[client_index].token, token,
                       TRUCO_SERVER_TOKEN_LEN + 1u);
                clients[client_index].player = player;
                send_text(clients[client_index].fd, "joined.\r\n");
                broadcast_session(token);
            } else {
                send_text(clients[client_index].fd, response);
            }
            return;
        }

        send_text(clients[client_index].fd,
                  "commands: HOST [2|4] [FLOR] | JOIN TOKEN\r\n");
        return;
    }

    truco_server_uppercase_line(buffer);
    if (strcmp(buffer, "HELP") == 0) {
        session = truco_server_session_get(clients[client_index].token);
        if (session != 0 &&
            truco_server_format_view(session, clients[client_index].player,
                                     response, sizeof(response)) == 0) {
            send_text(clients[client_index].fd, response);
            send_text(clients[client_index].fd, "\r\n");
        }
        return;
    }

    if (strcmp(buffer, "QUIT") == 0) {
        char ended_token[TRUCO_SERVER_TOKEN_LEN + 1u];

        memcpy(ended_token, clients[client_index].token,
               TRUCO_SERVER_TOKEN_LEN + 1u);
        status = truco_server_quit(ended_token, clients[client_index].player,
                                   response, sizeof(response));
        send_text(clients[client_index].fd, response);
        if (status == TRUCO_SERVER_OK) {
            end_session_clients(ended_token);
        }
        return;
    }

    if (!truco_server_parse_command_index(buffer, &command_index)) {
        send_text(clients[client_index].fd, "error: enter 1..n, HELP, or QUIT\r\n");
        return;
    }

    session = truco_server_session_get(clients[client_index].token);
    if (session == 0) {
        send_text(clients[client_index].fd, "error: session not found\r\n");
        return;
    }

    if (truco_server_apply_command(session, clients[client_index].player,
                                 command_index, response,
                                 sizeof(response)) != 0) {
        send_text(clients[client_index].fd, "error: command failed\r\n");
        return;
    }

    broadcast_session(clients[client_index].token);
}

static void consume_input(size_t client_index)
{
    char chunk[128];
    ssize_t nread;
    size_t i;

    nread = read(clients[client_index].fd, chunk, sizeof(chunk));
    if (nread <= 0) {
        close_client(client_index);
        return;
    }

    for (i = 0u; (size_t)nread > i; ++i) {
        char ch = chunk[i];

        if (clients[client_index].line_len + 1u >= sizeof(clients[client_index].line)) {
            clients[client_index].line_len = 0u;
        }
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            clients[client_index].line[clients[client_index].line_len] = '\0';
            handle_line(client_index, clients[client_index].line);
            clients[client_index].line_len = 0u;
            continue;
        }
        clients[client_index].line[clients[client_index].line_len++] = ch;
    }
}

static unsigned int read_port(void)
{
    const char *env = getenv("TRUCO_PORT");
    char *end = 0;
    long value;

    if (env == 0 || env[0] == '\0') {
        return TRUCO_SERVER_DEFAULT_PORT;
    }
    value = strtol(env, &end, 10);
    if (env == end || value <= 0 || value > 65535) {
        return TRUCO_SERVER_DEFAULT_PORT;
    }
    return (unsigned int)value;
}

int main(void)
{
    int listen_fd;
    struct sockaddr_in address;
    int reuse = 1;
    unsigned int port = read_port();
    size_t i;

    truco_server_reset();
    for (i = 0u; i < TRUCO_SERVER_MAX_CLIENTS; ++i) {
        clients[i].fd = -1;
    }

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return 1;
    }
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons((uint16_t)port);

    if (bind(listen_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind");
        close(listen_fd);
        return 1;
    }
    if (listen(listen_fd, 16) < 0) {
        perror("listen");
        close(listen_fd);
        return 1;
    }

    printf("truco server listening on %u (telnet localhost %u)\n", port, port);

    for (;;) {
        struct pollfd fds[TRUCO_SERVER_MAX_CLIENTS + 1u];
        int client_map[TRUCO_SERVER_MAX_CLIENTS + 1u];
        nfds_t nfds = 0u;
        nfds_t poll_index;

        fds[nfds].fd = listen_fd;
        fds[nfds].events = POLLIN;
        client_map[nfds] = -1;
        ++nfds;

        for (i = 0u; i < TRUCO_SERVER_MAX_CLIENTS; ++i) {
            if (clients[i].active && clients[i].fd >= 0) {
                fds[nfds].fd = clients[i].fd;
                fds[nfds].events = POLLIN;
                client_map[nfds] = (int)i;
                ++nfds;
            }
        }

        if (poll(fds, nfds, -1) < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("poll");
            break;
        }

        for (poll_index = 0u; poll_index < nfds; ++poll_index) {
            if (!(fds[poll_index].revents & POLLIN)) {
                continue;
            }
            if (client_map[poll_index] < 0) {
                int client_fd = accept(listen_fd, 0, 0);
                int slot;

                if (client_fd < 0) {
                    perror("accept");
                    continue;
                }
                slot = alloc_client(client_fd);
                if (slot < 0) {
                    send_text(client_fd, "error: server full\r\n");
                    close(client_fd);
                    continue;
                }
                send_text(client_fd,
                          "libtruco2 server\r\n"
                          "HOST [2|4] [FLOR] - create table\r\n"
                          "JOIN TOKEN - join with 6-char token\r\n"
                          "QUIT - end session (in game)\r\n");
                continue;
            }
            consume_input((size_t)client_map[poll_index]);
        }
    }

    close(listen_fd);
    return 0;
}
