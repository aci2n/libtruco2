#include "truco_hand_internal.h"

#include <string.h>

void truco_event_log_init(truco_event_log *log)
{
    if (log == 0) {
        return;
    }
    log->count = 0u;
}

static void truco_event_log_push(truco_event_log *log, truco_event_kind kind)
{
    truco_event *event;

    if (log == 0 || log->count >= TRUCO_MAX_EVENTS_PER_APPLY) {
        return;
    }

    event = &log->events[log->count++];
    memset(event, 0, sizeof(*event));
    event->kind = kind;
}

void truco_event_log_emit_hand_started(truco_event_log *log, unsigned int mano)
{
    truco_event *event;

    truco_event_log_push(log, TRUCO_EVENT_HAND_STARTED);
    if (log == 0 || log->count == 0u) {
        return;
    }
    event = &log->events[log->count - 1u];
    event->player = mano;
}

void truco_event_log_emit_card_played(truco_event_log *log,
                                      unsigned int player,
                                      unsigned int trick,
                                      unsigned char slot,
                                      truco_card card)
{
    truco_event *event;

    truco_event_log_push(log, TRUCO_EVENT_CARD_PLAYED);
    if (log == 0 || log->count == 0u) {
        return;
    }
    event = &log->events[log->count - 1u];
    event->player = player;
    event->trick = trick;
    event->card_slot = slot;
    event->card = card;
}

void truco_event_log_emit_trick_won(truco_event_log *log,
                                    unsigned int trick,
                                    unsigned int team,
                                    unsigned int player)
{
    truco_event *event;

    truco_event_log_push(log, TRUCO_EVENT_TRICK_WON);
    if (log == 0 || log->count == 0u) {
        return;
    }
    event = &log->events[log->count - 1u];
    event->trick = trick;
    event->team = team;
    event->player = player;
}

void truco_event_log_emit_hand_finished(truco_event_log *log, unsigned int team)
{
    truco_event *event;

    truco_event_log_push(log, TRUCO_EVENT_HAND_FINISHED);
    if (log == 0 || log->count == 0u) {
        return;
    }
    event = &log->events[log->count - 1u];
    event->team = team;
}

void truco_event_log_emit_score(truco_event_log *log,
                                unsigned int team,
                                unsigned int amount)
{
    truco_event *event;

    truco_event_log_push(log, TRUCO_EVENT_SCORE_CHANGED);
    if (log == 0 || log->count == 0u) {
        return;
    }
    event = &log->events[log->count - 1u];
    event->team = team;
    event->amount = amount;
}

void truco_event_log_emit_game_over(truco_event_log *log)
{
    truco_event_log_push(log, TRUCO_EVENT_GAME_OVER);
}

void truco_event_log_emit_simple(truco_event_log *log,
                                 truco_event_kind kind,
                                 unsigned int player,
                                 unsigned int team,
                                 unsigned int amount)
{
    truco_event *event;

    truco_event_log_push(log, kind);
    if (log == 0 || log->count == 0u) {
        return;
    }
    event = &log->events[log->count - 1u];
    event->player = player;
    event->team = team;
    event->amount = amount;
}

void truco_event_log_copy_to_result(const truco_event_log *log,
                                    truco_apply_result *result)
{
    if (log == 0 || result == 0) {
        return;
    }

    result->event_count = log->count;
    if (log->count > 0u) {
        memcpy(result->events, log->events,
               log->count * sizeof(result->events[0]));
    }
}
