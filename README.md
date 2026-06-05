# C Shell

This repository contains a custom Unix-like shell implemented in C.
The project supports command execution, built-in commands, pipelines,
I/O redirection, background jobs, command history, and sequential command
execution.

## Project Layout

- `shell/Makefile` builds the shell binary.
- `shell/include/` contains public headers for each module.
- `shell/src/` contains the implementation of the shell features.

## Requirements

- GCC or another C99-compatible compiler
- A POSIX-like environment

## Build

From the repository root:

```bash
cd shell
make
```

This produces `shell.out` inside the `shell/` directory.

To clean build artifacts:

```bash
cd shell
make clean
```

## Run

After building, start the shell with:

```bash
./shell.out
```

The shell displays a custom prompt based on the current user, host, and working directory.

## Features

- External command execution
- Built-in commands: `hop`, `reveal`, `activities`, `ping`, `log`, `echo`, `bg`, and `fg`
- Command history stored in `~/.shell_log`
- Background jobs using `&`
- Sequential execution with `;`
- Pipelines using `|`
- Input, output, and append redirection using `<`, `>`, and `>>`
- Signal handling for `Ctrl+C` and `Ctrl+Z`

## Built-ins

- `hop [path]` changes the current directory.
- `reveal` lists files in the current or given directory.
- `activities` shows active background or stopped jobs managed by the shell.
- `ping <pid> <signal_number>` sends a signal to a process.
- `log` prints the command history.
- `log execute <index>` re-runs a command from history.
- `log purge` clears history.
- `echo` prints its arguments.

## Notes

- The shell uses `shell/Makefile` for compilation and links all modules from `shell/src/`.
- History is updated automatically for supported commands, with duplicate and size limits handled by the log module.
- Some job-control commands are present in the interface and may be expanded further by the project implementation.