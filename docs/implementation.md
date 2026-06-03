# Technical implementation

This document describes how `libtruco2` is implemented internally and how the
current design supports embeddable 1v1 and 2v2 Argentine Truco games while
leaving space for future 3v3 support.

## Goals and constraints

The library is intentionally small and conservative:

- Written in C99.
- No runtime dependencies.
- Buildable with GNU-style Make tooling.
- Embeddable in applications that want to own memory and persistence.
- Deterministic when callers provide a seed.
- Flexible enough for 2-player and 4-player tables today, with fixed capacity
  for 6 players reserved for future 3v3 rules.

The implementation avoids callbacks, threads, global mutable state, and I/O
inside the engine. Game state lives in a heap-allocated opaque `truco_game`
created with `truco_game_create` and released with `truco_game_delete`.

## Public API layout

The public API is declared in `include/truco.h`. It exposes:

- Compile-time constants:
  - `TRUCO_DECK_SIZE`: 40 cards.
  - `TRUCO_HAND_CARDS`: 3 cards per player.
  - `TRUCO_MAX_PLAYERS`: 6, reserving space for 3v3.
  - `TRUCO_MAX_TEAMS`: 2.
- Value types:
  - `truco_card`
  - `truco_legal_commands`
- Opaque type:
  - `truco_game` (forward-declared in the header, defined in `src/truco.c`)
- Small enums for status codes, suits, phases, and commands.
- Stateless card/deck helpers.
- `truco_game_apply`, the only public game mutation entry point.
- `truco_game_legal_commands`, the non-mutating command discovery API.
- Bid metadata getters: `truco_game_pending_truco_value`,
  `truco_game_pending_envido_points`, `truco_game_next_truco_value`.

The API uses explicit status returns rather than `errno`. Functions return
`TRUCO_OK` on success or a negative `truco_status` value on failure.

## Command dispatch and legal action discovery

Clients mutate game state with scoped `truco_command` values:

```c
truco_game_apply(&game, player, TRUCO_CMD_PLAY_CARD_0);
truco_game_apply(&game, player, TRUCO_CMD_CALL_REAL_ENVIDO);
truco_game_apply(&game, player, TRUCO_CMD_ACCEPT_BID);
```

Specific operations such as playing a card, calling Envido, accepting a bid, or
starting a hand are private helpers inside `src/truco.c`. The public API stays
small and protocol-like: clients submit a command and the engine validates and
applies it.

Clients should use `truco_game_legal_commands` to discover valid commands for a
player before calling `truco_game_apply`. The caller allocates a
`truco_legal_commands` value (typically on the stack) and passes a pointer; the
engine fills `count` and `commands[]` up to `TRUCO_MAX_LEGAL_COMMANDS`.

Bid labels are not bundled with the command list. Use
`truco_game_next_truco_value` when raising, `truco_game_pending_truco_value`
when answering Truco, and `truco_game_pending_envido_points` when answering
Envido.

The command list makes clients simple: render each command, let the user or bot
choose one, then pass the chosen enum back to `truco_game_apply`.

Internally, legal action discovery uses the same `can_*` predicates as the
dispatch helpers. This keeps command availability and command execution aligned.

## Memory and ownership

`truco_game` is opaque and heap-allocated. `truco_game_create` initializes the
object; `truco_game_init` is the public reset entry point:

```c
truco_game *game = truco_game_create();
truco_game_set_player_count(game, 4);
truco_game_init(game);
truco_game_delete(game);
```

Internally, `struct truco_game` contains fixed arrays sized by
`TRUCO_MAX_PLAYERS` and `TRUCO_HAND_CARDS`:

- `hands[player][slot]`
- `played_slots[player][slot]`
- `trick_cards[trick][player]`
- `trick_played[trick][player]`
- `trick_winner_team[trick]`
- `trick_winner_player[trick]`

This makes embedding simple for games, servers, bots, tests, and simulations.
Callers can place `truco_game` inside larger state containers or serialize the
fields with their own format. The library does not promise a stable binary
serialization format for the public struct; embedders that persist games should
version their own serialized representation.

## Table configuration

The caller configures the table with setters such as
`truco_game_set_player_count` before `truco_game_init`:

The default team assignment alternates players by index:

- 1v1: player 0 vs player 1.
- 2v2: players 0 and 2 vs players 1 and 3.
- 3v3 capacity: players 0, 2, 4 vs players 1, 3, 5, but initialization is not
  enabled yet.

`truco_game_init` currently accepts only `player_count == 2` or
`player_count == 4`. Six-player games return `TRUCO_ERR_UNSUPPORTED_RULES` so
the capacity is visible without pretending that 3v3 rule differences are solved.

Callers may customize `config.team_for_player[]` before initialization as long
as each active player maps to team `0` or `1`.

## Game phases

The engine tracks coarse state with `truco_phase`:

- `TRUCO_PHASE_READY`: initialized but no active hand.
- `TRUCO_PHASE_PLAYING`: a hand is active.
- `TRUCO_PHASE_HAND_OVER`: a hand ended and another hand can be started.
- `TRUCO_PHASE_GAME_OVER`: a team reached `target_score`.

`TRUCO_CMD_START_HAND` is valid only from `READY` or `HAND_OVER`. Starting a new
hand during `PLAYING` would overwrite an in-progress hand, so dispatch returns
`TRUCO_ERR_INVALID_STATE`.

## Dealing and randomness

`truco_deck` creates the 40-card Spanish deck by iterating suits and ranks
1 through 12 while skipping 8 and 9.

`truco_shuffle` uses a small deterministic linear congruential generator and a
Fisher-Yates shuffle. This is not intended to be cryptographically secure. It is
intended to be dependency-free, portable, and reproducible for tests and bots.

The private `start_hand` helper behind `TRUCO_CMD_START_HAND`:

1. Builds a fresh deck.
2. Shuffles with `game->rng_state`.
3. Deals three rounds of one card per player.
4. Sets `mano` to the player after the dealer.
5. Sets `current_player` and `trick_leader` to `mano`.
6. Resets per-hand bidding, envido, trick, and card-play state.
7. Advances the dealer for the next hand.

## Card ranking

`truco_card_power` implements Argentine Truco ordering. Higher returned values
win:

1. 1 espada
2. 1 basto
3. 7 espada
4. 7 oro
5. Any 3
6. Any 2
7. Remaining 1s
8. 12s
9. 11s
10. 10s
11. Remaining 7s
12. 6s
13. 5s
14. 4s

Cards with the same power tie even if suits differ. `truco_card_compare`
normalizes ranking to `-1`, `0`, or `1`.

## Trick flow

The private `play_card` helper behind `TRUCO_CMD_PLAY_CARD_0`,
`TRUCO_CMD_PLAY_CARD_1`, and `TRUCO_CMD_PLAY_CARD_2` enforces:

- Active phase is `TRUCO_PHASE_PLAYING`.
- No unresolved Truco bid.
- No unresolved Envido bid.
- It is the player's turn.
- The chosen hand slot has not already been played.

After each play, `current_player` advances in table order. When turn order comes
back to `trick_leader`, every active player has contributed one card and the
trick is resolved.

`resolve_current_trick` selects the strongest card. If the strongest card is
tied across teams, the trick winner team is stored as `-1`, representing
`parda`. If the tie is only between partners, the team still wins the trick.

The next trick starts with the winning player. For a tied trick, the previous
leader remains the leader, matching the mano advantage behavior used elsewhere
in the hand resolution.

## Hand resolution and parda

`compute_hand_winner` examines the trick result array:

- A team that wins the first two non-tied tricks wins the hand.
- If the first trick is tied, the second non-tied trick can decide the hand.
- If later tricks tie, earlier decisive trick results retain priority according
  to Argentine Truco parda behavior.
- If all tricks are tied, the mano team wins.

The function returns:

- Team `0` or `1` when the hand is decided.
- `-2` when more cards are required.

When a hand ends, `finish_hand` records `last_hand_winner`, awards the current
Truco value, and moves the game to `HAND_OVER` or `GAME_OVER`.

## Truco bidding

The current hand starts with `truco_value == 1`.

`TRUCO_CMD_RAISE_TRUCO` creates a pending raise:

- From 1 to Truco value 2.
- From 2 to Retruco value 3.
- From 3 to Vale Cuatro value 4.

The same team cannot raise twice in a row. While a raise is pending, card play is
blocked until the opposing team accepts or declines.

`TRUCO_CMD_ACCEPT_BID` commits the pending value and records which team made the
last accepted raise when the pending bid is Truco.

`TRUCO_CMD_REJECT_BID` awards the hand immediately to the raising team at the
previously accepted hand value. For the initial Truco call, this is one point.

## Envido

`truco_envido_points` calculates a player's envido score:

- Face cards 10, 11, and 12 count as 0.
- If two cards share a suit, the best same-suit pair scores
  `20 + value_a + value_b`.
- If no pair exists, the score is the highest single card value.

The Envido call commands are valid only before any card has been played and only
once per hand in this initial implementation. They are:

- `TRUCO_CMD_CALL_ENVIDO`: 2 points if accepted.
- `TRUCO_CMD_CALL_REAL_ENVIDO`: 3 points if accepted.
- `TRUCO_CMD_CALL_FALTA_ENVIDO`: points required based on the leading score and
  target.

`TRUCO_CMD_ACCEPT_BID` computes each team's best player score when the pending
bid is Envido. Ties are broken by mano order using `compare_mano_order`.

`TRUCO_CMD_REJECT_BID` awards one point to the calling team when the pending bid
is Envido.

## Scoring and target score

Scores are stored in `score[TRUCO_MAX_TEAMS]`. `target_score` defaults to 30 if
the caller passes zero. `add_score` moves the game to `TRUCO_PHASE_GAME_OVER`
when a team reaches or exceeds the target.

The engine does not split points into buenas/malas bands. It stores absolute
points against the target. Presentation layers can translate this to their
preferred UI.

## Test strategy

The unit tests in `tests/test_truco.c` use a tiny self-contained assertion
macro. No framework is required.

Coverage focuses on:

- Card ranking and invalid cards.
- Deck generation and envido values.
- 1v1 trick and hand resolution.
- Truco raise/accept/decline behavior.
- Parda/tied-trick behavior.
- Envido accept/decline behavior.
- Command dispatch and legal action discovery for ready, playing, pending bid,
  and post-card states.
- 2v2 team assignment and trick flow.
- Reserved 3v3 capacity returning `TRUCO_ERR_UNSUPPORTED_RULES`.

The tests set hands directly through the public `truco_game` struct where
deterministic rule scenarios are needed. That keeps test setup out of the
library's public command API.

## Build artifacts

The `Makefile` builds:

- `lib/libtruco.a`
- `lib/libtruco.so`
- `build/test_truco`
- `build/basic_round`

Generated artifacts live under `build/` and `lib/` and are removed by
`make clean`.

## Future 3v3 support

The implementation deliberately reserves `TRUCO_MAX_PLAYERS == 6`, but 3v3 is
not enabled because it has special rules. Future work should add explicit rule
configuration rather than silently treating 3v3 like 2v2.

Likely extension points:

- Replace `is_supported_player_count` with a rule-set check that can admit
  six-player games only when 3v3-specific behavior is implemented.
- Use `config.flags` or an added rule enum for variants.
- Audit mano, partner, seating, and score behavior for 3v3-specific rules.
- Extend tests with six-player dealing, bidding, trick resolution, and any
  variant-specific scoring.

The existing fixed arrays and team mapping mean 3v3 support should not require a
new storage model, but it should be added as an explicit rule mode with tests.
