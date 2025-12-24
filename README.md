# Unix Shell in C

A Unix-like shell implemented in C, designed to demonstrate solid understanding of **operating systems fundamentals**, **process management**, and **POSIX system programming**.  
This project was built with a strong focus on correctness, clarity, and real shell behavior.

---

## Overview

This shell supports:
- Execution of external commands
- Built-in commands
- Pipelines (`|`)
- Output and error redirections
- Advanced command parsing
- Tab autocompletion using `readline`

The project is intentionally scoped to focus on **core OS concepts** rather than implementing a full Bash clone.

---

## Key Features

### Command Execution
- Execution via `fork`, `execvp`, and `waitpid`
- PATH-based executable resolution
- Proper parent / child process separation

### Built-in Commands
Supported built-ins:
- `cd`
- `pwd`
- `echo`
- `type`
- `exit`

Behavior:
- Built-ins executed **normally** affect the parent shell
- Built-ins executed **inside pipelines** run in child processes only  
  (POSIX-consistent behavior)

Example:
```bash
cd /tmp | pwd   # does NOT change parent directory
```

## Pipelines

- Supports multiple chained commands using `|`
- Correct pipe creation and teardown
- File descriptor redirection using `pipe` and `dup2`
- Built-ins fully supported inside pipelines

### Examples
```bash
echo hello | wc -c
pwd | cat
type ls | cat
```

## Redirections

### Standard output
- `>`
- `>>`

### Standard error
- `2>`
- `2>>`

### Examples
```bash
ls > out.txt
ls >> out.txt
ls notfound 2> err.txt
```

## Parsing

Custom tokenizer supporting:
- Quoted strings (`'...'`, `"..."`)
- Escaped characters (`\`)
- Arbitrary whitespace
- Clean argument splitting

Implemented manually without relying on high-level parsing utilities.

---

## Autocompletion

Implemented using `readline`:
- Tab completion for built-in commands
- Tab completion for all executables found in `$PATH`
- Dynamic PATH scanning
- Duplicate-safe completion list

---

## Features

### Basic Command Execution
```bash
$ ls
$ pwd
$ echo hello world
```

### Pipeline with Built-ins
```bash
$ ls > files.txt
$ ls notfound 2> error.log
```

### Autocompletion
```bash
$ ec<TAB>   -> echo
$ pw<TAB>   -> pwd
```

---

## Project Structure

.
├── main.c          # Shell implementation
├── strlist.c       # Dynamic string list (autocomplete support)
├── strlist.h
└── README.md

---

## Build and Run

### Compilation
```bash
gcc -Wall -Wextra -Werror main.c strlist.c -lreadline -o minishell

```

### Execution
```bash
./minishell
```

---

## Technical Highlights

This project demonstrates:
- Strong understanding of the Unix process model
- Correct use of low-level system calls:
  - `fork`
  - `execvp`
  - `pipe`
  - `dup2`
  - `waitpid`
  - `open`, `close`
- Careful file descriptor lifecycle management
- Clean separation between parsing, execution, and built-ins
- POSIX-consistent shell behavior
- Support for built-ins inside pipelines
- Modular and readable C code

---

## Known Limitations

The following features are intentionally not implemented:
- Input redirection (`<`)
- Environment variable expansion (`$VAR`)
- Globbing (`*`, `?`)
- Background execution (`&`)
- Job control (`fg`, `bg`)
- Signal handling (`Ctrl+C`, `Ctrl+Z`)

---

## Possible Extensions

- Input redirection `<`
- Combined redirections with pipelines
- Environment variable expansion
- Signal handling (`SIGINT`, `SIGQUIT`)
- Background execution
- Job control

---

## License

This project is provided for educational purposes.  
You are free to study, modify, and extend it.
