#include "truco_server_session.h"

#include "truco.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char *phase_name(truco_phase phase)
{
    switch (phase) {
    case TRUCO_PHASE_READY:
        return "READY";
    case TRUCO_PHASE_PLAYING:
        return "PLAYING";
    case TRUCO_PHASE_HAND_OVER:
        return "HAND_OVER";
    case TRUCO_PHASE_GAME_OVER:
        return "GAME_OVER";
    default:
        return "?";
    }
}

static const char *suit_name(truco_suit suit)
{
    switch (suit) {
    case TRUCO_SUIT_ESPADA:
        return "espada";
    case TRUCO_SUIT_BASTO:
        return "basto";
    case TRUCO_SUIT_ORO:
        return "oro";
    case TRUCO_SUIT_COPA:
        return "copa";
    default:
        return "?";
    }
}

static const char *command_label(truco_command command)
{
    switch (command) {
    case TRUCO_CMD_START_HAND:
        return "START HAND";
    case TRUCO_CMD_PLAY_CARD_0:
        return "PLAY CARD 0";
    case TRUCO_CMD_PLAY_CARD_1:
        return "PLAY CARD 1";
    case TRUCO_CMD_PLAY_CARD_2:
        return "PLAY CARD 2";
    case TRUCO_CMD_RAISE_TRUCO:
        return "RAISE TRUCO";
    case TRUCO_CMD_CALL_ENVIDO:
        return "CALL ENVIDO";
    case TRUCO_CMD_CALL_REAL_ENVIDO:
        return "CALL REAL ENVIDO";
    case TRUCO_CMD_CALL_FALTA_ENVIDO:
        return "CALL FALTA ENVIDO";
    case TRUCO_CMD_CALL_FLOR:
        return "CALL FLOR";
    case TRUCO_CMD_ACCEPT_BID:
        return "ACCEPT BID";
    case TRUCO_CMD_REJECT_BID:
        return "REJECT BID";
    case TRUCO_CMD_GO_TO_DECK:
        return "GO TO DECK";
    default:
        return "UNKNOWN";
    }
}

static const char *status_message(truco_status status)
{
    switch (status) {
    case TRUCO_OK:
        return "ok";
    case TRUCO_ERR_INVALID_ARGUMENT:
        return "invalid argument";
    case TRUCO_ERR_INVALID_STATE:
        return "invalid state";
    case TRUCO_ERR_NOT_PLAYERS_TURN:
        return "not your turn";
    case TRUCO_ERR_CARD_ALREADY_PLAYED:
        return "card already played";
    case TRUCO_ERR_UNSUPPORTED_RULES:
        return "unsupported rules";
    case TRUCO_ERR_OUT_OF_MEMORY:
        return "out of memory";
    default:
        return "error";
    }
}

static void trim_line(char *line)
{
    size_t len;
    char *start;

    if (line == 0) {
        return;
    }
    start = line;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') {
        ++start;
    }
    if (start != line) {
        memmove(line, start, strlen(start) + 1u);
    }
    len = strlen(line);
    while (len > 0u &&
           (line[len - 1u] == ' ' || line[len - 1u] == '\t' || line[len - 1u] == '\r' ||
            line[len - 1u] == '\n')) {
        line[len - 1u] = '\0';
        --len;
    }
}

static void uppercase_word(char *word)
{
    size_t i;

    for (i = 0u; word[i] != '\0'; ++i) {
        if (word[i] >= 'a' && word[i] <= 'z') {
            word[i] = (char)(word[i] - 'a' + 'A');
        }
    }
}

static int append_printf(char *buf, size_t size, size_t *offset, const char *fmt, ...)
{
    int written;
    size_t room;
    va_list args;

    if (*offset >= size) {
        return -1;
    }
    room = size - *offset;
    va_start(args, fmt);
    written = vsnprintf(buf + *offset, room, fmt, args);
    va_end(args);
    if (written < 0 || (size_t)written >= room) {
        return -1;
    }
    *offset += (size_t)written;
    return 0;
}

static int format_card(char *buf, size_t size, truco_card card)
{
    return snprintf(buf, size, "%u-%s", (unsigned int)card.rank, suit_name(card.suit));
}

int truco_server_format_view(struct truco_server_session *session,
                             unsigned int viewer,
                             char *response,
                             size_t response_size)
{
    truco_game *game;
    truco_legal_commands legal;
    size_t offset = 0u;
    unsigned int trick;
    unsigned int player;
    unsigned int slot;
    unsigned int pending_truco;
    unsigned int pending_envido;
    unsigned int pending_flor;

    if (session == 0 || response == 0 || response_size == 0u) {
        return -1;
    }

    game = truco_server_session_game(session);
    if (game == 0 || viewer >= truco_server_session_player_count(session)) {
        return -1;
    }

    response[0] = '\0';
    memset(&legal, 0, sizeof(legal));
    truco_game_legal_commands(game, viewer, &legal);

    if (append_printf(response, response_size, &offset, "token: %s\r\n",
                      truco_server_session_token(session)) != 0) {
        return -1;
    }
    if (append_printf(response, response_size, &offset, "you are player %u (team %u)\r\n",
                      viewer, truco_game_team_for_player(game, viewer)) != 0) {
        return -1;
    }
    if (append_printf(response, response_size, &offset, "phase: %s\r\n",
                      phase_name(truco_game_phase(game))) != 0) {
        return -1;
    }
    if (append_printf(response, response_size, &offset, "current player: %u\r\n",
                      truco_game_current_player(game)) != 0) {
        return -1;
    }
    if (append_printf(response, response_size, &offset, "scores: team0=%u team1=%u\r\n",
                      truco_game_score(game, 0u), truco_game_score(game, 1u)) != 0) {
        return -1;
    }
    if (append_printf(response, response_size, &offset, "rules: flor=%s\r\n",
                      truco_game_flor_enabled(game) ? "on" : "off") != 0) {
        return -1;
    }

    pending_truco = truco_game_pending_truco_value(game);
    pending_envido = truco_game_pending_envido_points(game);
    pending_flor = truco_game_pending_flor_points(game);
    if (pending_truco != 0u &&
        append_printf(response, response_size, &offset, "pending truco: %u\r\n",
                      pending_truco) != 0) {
        return -1;
    }
    if (pending_envido != 0u &&
        append_printf(response, response_size, &offset, "pending envido: %u\r\n",
                      pending_envido) != 0) {
        return -1;
    }
    if (pending_flor != 0u &&
        append_printf(response, response_size, &offset, "pending flor: %u\r\n",
                      pending_flor) != 0) {
        return -1;
    }

    if (!truco_server_session_slots_full(session) &&
        append_printf(response, response_size, &offset,
                      "waiting for players to join\r\n") != 0) {
        return -1;
    }

    if (append_printf(response, response_size, &offset, "table:\r\n") != 0) {
        return -1;
    }
    for (trick = 0u; trick < TRUCO_HAND_CARDS; ++trick) {
        char line[256];
        size_t line_len = 0u;
        int winner = truco_game_trick_winner(game, trick);

        line_len = (size_t)snprintf(line, sizeof(line), "  trick %u:", trick);
        for (player = 0u; player < truco_game_player_count(game); ++player) {
            truco_card card;
            char card_buf[32];
            const char *text = "--";

            if (truco_game_trick_card(game, trick, player, &card) == TRUCO_OK) {
                format_card(card_buf, sizeof(card_buf), card);
                text = card_buf;
            }
            snprintf(line + line_len, sizeof(line) - line_len, " p%u=%s", player, text);
            line_len = strlen(line);
        }
        if (winner >= 0) {
            snprintf(line + line_len, sizeof(line) - line_len, " winner=team%d", winner);
        } else if (winner == -1) {
            snprintf(line + line_len, sizeof(line) - line_len, " winner=parda");
        }
        if (append_printf(response, response_size, &offset, "%s\r\n", line) != 0) {
            return -1;
        }
    }

    if (append_printf(response, response_size, &offset, "your hand:\r\n") != 0) {
        return -1;
    }
    for (slot = 0u; slot < TRUCO_HAND_CARDS; ++slot) {
        truco_card card;
        char card_buf[32];

        if (truco_game_hand_card(game, viewer, slot, &card) != TRUCO_OK) {
            continue;
        }
        format_card(card_buf, sizeof(card_buf), card);
        if (append_printf(response, response_size, &offset, "  %u) %s\r\n", slot, card_buf) != 0) {
            return -1;
        }
    }
    if (append_printf(response, response_size, &offset, "envido: %u\r\n",
                      truco_game_hand_envido(game, viewer)) != 0) {
        return -1;
    }
    if (truco_game_flor_enabled(game) &&
        append_printf(response, response_size, &offset, "flor: %u\r\n",
                      truco_game_hand_flor(game, viewer)) != 0) {
        return -1;
    }

    if (append_printf(response, response_size, &offset, "commands:\r\n") != 0) {
        return -1;
    }
    if (legal.count == 0u) {
        if (append_printf(response, response_size, &offset, "  (none)\r\n") != 0) {
            return -1;
        }
    } else {
        size_t i;

        for (i = 0u; i < legal.count; ++i) {
            if (append_printf(response, response_size, &offset, "  %zu) %s\r\n",
                              i + 1u, command_label(legal.commands[i])) != 0) {
                return -1;
            }
        }
    }

    return 0;
}

int truco_server_apply_command(struct truco_server_session *session,
                               unsigned int player,
                               unsigned int command_index,
                               char *response,
                               size_t response_size)
{
    truco_game *game;
    truco_legal_commands legal;
    truco_status status;
    size_t offset = 0u;

    if (session == 0 || response == 0 || response_size == 0u) {
        return -1;
    }

    game = truco_server_session_game(session);
    if (game == 0 || player >= truco_server_session_player_count(session)) {
        return -1;
    }

    memset(&legal, 0, sizeof(legal));
    if (truco_game_legal_commands(game, player, &legal) != TRUCO_OK) {
        return -1;
    }
    if (command_index == 0u || command_index > legal.count) {
        snprintf(response, response_size, "error: invalid command number\r\n");
        return -1;
    }

    if (!truco_server_session_slots_full(session)) {
        snprintf(response, response_size, "error: waiting for all players\r\n");
        return -1;
    }

    status = truco_game_apply(game, player, legal.commands[command_index - 1u]).status;
    response[0] = '\0';
    if (status != TRUCO_OK) {
        if (append_printf(response, response_size, &offset, "error: %s\r\n",
                          status_message(status)) != 0) {
            return -1;
        }
    } else {
        if (append_printf(response, response_size, &offset, "ok\r\n") != 0) {
            return -1;
        }
    }

    if (truco_server_format_view(session, player, response + offset,
                                 response_size - offset) != 0) {
        return -1;
    }
    return 0;
}

void truco_server_normalize_line(char *line)
{
    trim_line(line);
}

void truco_server_uppercase_line(char *line)
{
    truco_server_normalize_line(line);
    uppercase_word(line);
}

int truco_server_parse_host(const char *line,
                            unsigned int *player_count_out,
                            int *flor_enabled_out)
{
    char buffer[64];
    const char *cursor;
    unsigned int count = 2u;
    int flor = 0;

    if (line == 0 || player_count_out == 0) {
        return 0;
    }
    if (flor_enabled_out != 0) {
        *flor_enabled_out = 0;
    }

    strncpy(buffer, line, sizeof(buffer) - 1u);
    buffer[sizeof(buffer) - 1u] = '\0';
    trim_line(buffer);
    uppercase_word(buffer);

    if (strncmp(buffer, "HOST", 4) != 0) {
        return 0;
    }
    if (buffer[4] != '\0' && buffer[4] != ' ' && buffer[4] != '\t') {
        return 0;
    }

    cursor = buffer + 4u;
    while (*cursor == ' ' || *cursor == '\t') {
        ++cursor;
    }

    while (*cursor != '\0') {
        char word[16];
        size_t word_len = 0u;

        while (*cursor == ' ' || *cursor == '\t') {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }

        while (cursor[word_len] != '\0' && cursor[word_len] != ' ' &&
               cursor[word_len] != '\t' && word_len + 1u < sizeof(word)) {
            word[word_len] = cursor[word_len];
            ++word_len;
        }
        word[word_len] = '\0';
        cursor += word_len;

        if (strcmp(word, "2") == 0) {
            count = 2u;
        } else if (strcmp(word, "4") == 0) {
            count = 4u;
        } else if (strcmp(word, "FLOR") == 0) {
            flor = 1;
        } else {
            return 0;
        }
    }

    *player_count_out = count;
    if (flor_enabled_out != 0) {
        *flor_enabled_out = flor;
    }
    return 1;
}

int truco_server_parse_join(const char *line, char token_out[TRUCO_SERVER_TOKEN_LEN + 1u])
{
    char buffer[64];
    char word[16];

    if (line == 0 || token_out == 0) {
        return 0;
    }

    strncpy(buffer, line, sizeof(buffer) - 1u);
    buffer[sizeof(buffer) - 1u] = '\0';
    trim_line(buffer);
    uppercase_word(buffer);

    if (sscanf(buffer, "JOIN %6s", word) != 1) {
        return 0;
    }
    if (strlen(word) != TRUCO_SERVER_TOKEN_LEN) {
        return 0;
    }
    memcpy(token_out, word, TRUCO_SERVER_TOKEN_LEN + 1u);
    return 1;
}

int truco_server_parse_command_index(const char *line, unsigned int *index_out)
{
    char buffer[32];
    unsigned int value;
    char extra;

    if (line == 0 || index_out == 0) {
        return 0;
    }
    strncpy(buffer, line, sizeof(buffer) - 1u);
    buffer[sizeof(buffer) - 1u] = '\0';
    trim_line(buffer);
    if (sscanf(buffer, "%u %c", &value, &extra) != 1) {
        return 0;
    }
    if (value == 0u) {
        return 0;
    }
    *index_out = value;
    return 1;
}
