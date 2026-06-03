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
  without changing the public API.

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

## Embedding

Include `truco.h` and link either the static or shared library.

```c
#include "truco.h"

int main(void)
{
    truco_game *game = truco_game_create();
    truco_command commands[16];
    unsigned int count;

    if (game == 0) {
        return 1;
    }

    if (truco_game_set_player_count(game, 4) != TRUCO_OK ||
        truco_game_set_seed(game, 42) != TRUCO_OK ||
        truco_game_init(game) != TRUCO_OK ||
        truco_game_apply(game, 0, TRUCO_CMD_START_HAND) != TRUCO_OK) {
        truco_game_destroy(game);
        return 1;
    }

    if (truco_game_legal_actions(game, 0, commands, 16, &count, 0, 0) == TRUCO_OK) {
        for (unsigned int i = 0; i < count; ++i) {
            render_button(commands[i]);
        }
    }

    truco_game_destroy(game);
    return 0;
}
```

The library allocates game state with `truco_game_create` and releases it with
`truco_game_destroy`. `truco_game_size` reports the allocation size for callers
that prefer their own allocators.

Configure a game before calling `truco_game_init`:

- `truco_game_set_player_count`
- `truco_game_set_seed`
- `truco_game_set_initial_dealer`
- `truco_game_set_target_score`
- `truco_game_set_team_for_player`

Clients mutate the game by applying scoped commands. They can discover valid
commands without mutating the game by passing a caller-owned `truco_command`
buffer to `truco_game_legal_actions`.

For a technical description of the implementation, see
[`docs/implementation.md`](docs/implementation.md).

## Table configuration

Use `truco_game_set_player_count(game, player_count)` to select the standard
layout:

- `player_count = 2`: players 0 and 1 are opposing teams.
- `player_count = 4`: players 0/2 vs. 1/3.
- `player_count = 6`: reserved capacity; initialization returns
  `TRUCO_ERR_UNSUPPORTED_RULES` until the special 3v3 rules are implemented.

Teams can be customized before initialization with
`truco_game_set_team_for_player(game, player, team)`. The current engine supports
two teams.

## Tests

```sh
make test
```

The unit tests are self-contained C code in `tests/test_truco.c`; they do not
require a test framework.
