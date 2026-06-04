# libtruco2 — run gdb from the repo root: gdb
# If GDB ignores this file, add to ~/.gdbinit: set auto-load local-gdbinit on

set breakpoint pending on
set print pretty on
set print array on
set print array-indexes on
set pagination off

file engine/build/test_truco
directory engine/src
directory engine/include

document libtruco
  Default target is build/test_truco. Examples:
    run
    break truco_game_apply
    break truco.c:758 if player == 0
    bt
end

define truco-bt
  backtrace full
end
document truco-bt
  Full backtrace with locals (alias for: bt full).
end
