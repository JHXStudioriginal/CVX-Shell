# CVX (Compact Virtual eXecutor) is a simple, lightweight shell written in C, designed to be fast, flexible, and pleasant to use.
![GitHub release (latest by date)](https://img.shields.io/github/v/release/JHXStudioriginal/CVX-Shell?color=blue) ![GitHub License](https://img.shields.io/github/license/JHXStudioriginal/CVX-Shell?color=orange)


### Features:
* Runs **normal Linux commands**, supports **pipes** and **redirections** (`>`, `>>`, `<`, `<<` heredoc)
* Supports **conditional statements** (`if`/`else`) and **loops** (`for`, `while`, `until`)
* Built-in commands:
  - `cd [dir]` - Change directory (supports `~`)
  - `pwd [-L|-P|--help]` - Print working directory (logical/physical)
  - `help` - Show this help message
  - `ls` - List directory contents (auto `--color=auto`)
  - `history` - Show command history
  - `alias [<name>=<cmd>]` - Create a command alias
  - `unalias [name]` - Remove the specified alias
  - `echo [args]` - Display text (supports environment variables)
  - `export [VAR=value]` - Set environment variables
  - `jobs` - List background jobs
  - `fg` - Resume job in foreground
  - `bg` - Resume job in background
  - `functions` - List all defined functions
  - `delfunc [name]` - Delete the specified function
  - `break [n]` - Exit from within a for, while, or until loop
  - `continue [n]` - Resume the next iteration of an enclosing loop
  - `:` - Null command (returns 0 exit status)
  - `exec [command] [args]` - Replace the shell with the specified command
  - `exit` - Exit the shell
* Command chaining with `&&` and `||`
* Pipelines with `|`
* Aliases & config:
  - Custom prompt, startup dir, history toggle via `/etc/cvx.conf`
  - More in [wiki](https://github.com/JHXStudioriginal/CVX-Shell/wiki/Configuration)
* Arguments:
  - `cvx --version`, `cvx -v`, `cvx -version` shows shell version
  - `cvx -c "<command>"` run it and exit
  - `cvx -l` loads `/etc/profile` and `~/.profile`
* Tiny, fast C implementation
* Line editing powered by [linenoise](https://github.com/antirez/linenoise)

###### See [wiki](https://github.com/JHXStudioriginal/CVX-Shell/wiki) on project page to read more.

### Installation:

Using Git + Make

1. Clone the repository:

```
git clone https://github.com/JHXStudioriginal/CVX-Shell.git
cd CVX-Shell
make
./cvx
```

---------------------------------------------------------------------------------------------------------------------------------------------

###### CVX Shell is under the [Elasna Open Source License V3](https://github.com/JHXStudioriginal/Elasna-License/blob/main/LICENSE), but includes [linenoise](https://github.com/antirez/linenoise) by [antirez](https://github.com/antirez), which is licensed under the BSD 2-Clause License. See relevant files for details.
