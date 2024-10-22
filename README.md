# smallsh - A Simple Shell in C

`smallsh` is a custom shell program written in C that implements a subset of features found in modern shells like `bash`. This project demonstrates basic shell functionalities such as command execution, input/output redirection, foreground/background process handling, and custom signal management.

## Features

- **Command Prompt**: Displays a `:` prompt for user input.
  
- **Built-in Commands**:
  - `exit`: Exits the shell and terminates all child processes.
  - `cd [directory]`: Changes the current working directory. Defaults to `$HOME` if no directory is provided.
  - `status`: Displays the exit status or terminating signal of the last foreground process.

- **Command Execution**: Executes non-built-in commands using the `exec()` family of functions.

- **Input/Output Redirection**:
  - Supports redirection of input (`<`) and output (`>`).
  - Allows simultaneous input and output redirection.
  
- **Foreground & Background Processes**:
  - Commands ending with `&` are run in the background.
  - Foreground commands block the prompt until completion.

- **Process ID Expansion**: Expands instances of `$$` in a command to the process ID of the shell itself.

- **Signal Handling**:
  - `SIGINT` (`Ctrl+C`): Terminates foreground processes but is ignored by the shell and background processes.
  - `SIGTSTP` (`Ctrl+Z`): Toggles between foreground-only mode (where background commands are ignored) and normal mode.

## Usage

### Running the Shell
Compile and run `smallsh` with the following commands:

```bash
gcc -o smallsh smallsh.c
./smallsh
