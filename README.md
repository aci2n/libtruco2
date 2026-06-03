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
    truco_config config;
    truco_game game;

    truco_config_default(&config, 4);
    config.seed = 42;

    if (truco_game_init(&game, &config) != TRUCO_OK) {
        return 1;
    }

    if (truco_game_start_hand(&game) != TRUCO_OK) {
        return 1;
    }

    return 0;
}
```

The API does not allocate memory. `truco_game` is a plain C struct that callers
can own directly, place in larger application state, serialize with their own
format, or reset by calling `truco_game_init`.

## Table configuration

Use `truco_config_default(&config, player_count)` to start from the standard
layout:

- `player_count = 2`: players 0 and 1 are opposing teams.
- `player_count = 4`: players 0/2 vs. 1/3.
- `player_count = 6`: reserved capacity; initialization returns
  `TRUCO_ERR_UNSUPPORTED_RULES` until the special 3v3 rules are implemented.

Teams can be customized before initialization with
`config.team_for_player[player]`. The current engine supports two teams.

## Tests

```sh
make test
```

The unit tests are self-contained C code in `tests/test_truco.c`; they do not
require a test framework.
