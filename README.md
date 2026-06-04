# libtruco2

`libtruco2` is a small standalone C library for embedding an Argentine Truco
game engine, plus a simple telnet-compatible multiplayer server.

## Layout

- `engine/` — Truco rules library (`libtruco.a`, public header `truco.h`)
- `server/` — Telnet server that hosts/joins tables and drives the engine

## Build

```sh
make
make test
```

This builds the engine, runs engine unit tests, builds the server, and runs
server unit tests.

Engine only:

```sh
make -C engine
make -C engine test
```

Server (requires engine library):

```sh
make -C server
make -C server test
./server/build/truco_server
```

## Telnet server

Default port `5555` (override with `TRUCO_PORT`).

Terminal 1:

```sh
./server/build/truco_server
telnet localhost 5555
HOST
```

Use `HOST 2`, `HOST 4`, or add `FLOR` for optional flor rules (`HOST FLOR`, `HOST 4 FLOR`).
The view includes `rules: flor=on|off`.

Note the 6-character token in the response, then terminal 2:

```sh
telnet localhost 5555
JOIN ABC123
```

Replace `ABC123` with your token. Commands are numbered in the `commands:` list:

```text
1
```

Use `HELP` to reprint state. Use `QUIT` to end the session for everyone. After each
command you get the table and your hand.

## Embedding the engine

Include `engine/include/truco.h` and link `engine/lib/libtruco.a`. The repo also
ships `engine/include/truco_internal.h` for tests and in-tree tools; it is not
installed by `make install`.

See [`engine/docs/implementation.md`](engine/docs/implementation.md) for engine
details.

## Engine features

- 1v1 games with two players.
- 2v2 games with four players and alternating default teams.
- Argentine Truco card ranking for the 40-card Spanish deck.
- Hand dealing, trick play, parda/tied-trick resolution, and score keeping.
- Truco / retruco / vale cuatro bidding.
- Envido, real envido, and falta envido resolution.
- Envido está primero (counter envido while truco is pending) and envido counter-chains.
- Optional flor (`truco_game_set_flor_enabled()` / `HOST FLOR`).
- `TRUCO_CMD_GO_TO_DECK` (ir al mazo).

3v3 games are reserved for a future rule module. Six-player tables return
`TRUCO_ERR_UNSUPPORTED_RULES` from `TRUCO_CMD_START_HAND`.
