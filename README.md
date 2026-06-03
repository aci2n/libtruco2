# libtruco2

`libtruco2` is a small standalone C library for embedding an Argentine Truco
game engine. It has no runtime dependencies and builds with GNU-style tools.

The current engine supports:

- 1v1 games with two players.
- 2v2 games with four players and alternating default teams.
- Argentine Truco card ranking for the 40-card Spanish deck.
- Hand dealing, trick play, parda/tied-trick resolution, and score keeping.
- Truco / retruco / vale cuatro bidding.
- Envido, real envido, and falta envido resolution.
- Fixed-capacity state for up to six players so 3v3 support can be added later
  without changing the public struct layout.

3v3 games are intentionally reserved for a future rule module because they have
special table and scoring rules. Initializing a six-player game currently returns
`TRUCO_ERR_UNSUPPORTED_RULES`.

## Build

```sh
make
```

This creates:

- `lib/libtruco.a`
- `lib/libtruco.so`

Useful targets:

```sh
make test
make examples
make install PREFIX=/usr/local
make clean
```

The build uses only the C compiler, `ar`, and standard shell tools. Override
toolchain variables in the usual GNU Make style:

```sh
make CC=gcc CFLAGS='-std=c99 -Wall -Wextra -O2 -g -fPIC'
```

## Embedding

Include `truco.h` and link either the static or shared library.

```c
#include "truco.h"

int main(void)
{
    truco_game *game = truco_game_create();

    if (game == 0) {
        return 1;
    }

    if (truco_game_set_player_count(game, 4) != TRUCO_OK ||
        truco_game_set_seed(game, 42) != TRUCO_OK ||
        truco_game_init(game) != TRUCO_OK) {
        truco_game_delete(game);
        return 1;
    }

    if (truco_game_apply(game, 0, TRUCO_CMD_START_HAND) != TRUCO_OK) {
        truco_game_delete(game);
        return 1;
    }

    truco_game_delete(game);
    return 0;
}
```

`truco_game` is an opaque type. `truco_game_create` allocates a game and
initializes it with default two-player settings. Adjust the table with
`truco_game_set_*`, then call `truco_game_init` to apply those settings and
reset scores and runtime state. Call `truco_game_delete` when finished.

Clients mutate the game by applying scoped commands. They can discover valid
commands without mutating the game:

```c
truco_legal_commands legal;

if (truco_game_legal_commands(game, player, &legal) == TRUCO_OK) {
    for (size_t i = 0; i < legal.count; ++i) {
        render_button(legal.commands[i]);
    }
}

/* Bid labels come from separate getters, e.g. truco_game_pending_truco_value. */
truco_game_apply(game, player, legal.commands[selected]);
```

For a technical description of the implementation, see
[`docs/implementation.md`](docs/implementation.md).

## Table configuration

After `truco_game_create`, configure the table with setters and call
`truco_game_init` to apply them (or to reset an in-progress game):

- `truco_game_set_player_count(game, 2)`: players 0 and 1 are opposing teams.
- `truco_game_set_player_count(game, 4)`: players 0/2 vs. 1/3 by default.
- `truco_game_set_player_count(game, 6)`: reserved capacity; `truco_game_init`
  returns `TRUCO_ERR_UNSUPPORTED_RULES` until the special 3v3 rules are
  implemented.

Optional setters:

- `truco_game_set_target_score` (default 30)
- `truco_game_set_seed` (default 1)
- `truco_game_set_initial_dealer` (default last seat)
- `truco_game_set_team_for_player` for custom team layouts

## Tests

```sh
make test
```

The unit tests are self-contained C code in `tests/test_truco.c`; they do not
require a test framework.
