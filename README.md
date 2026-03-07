# CVX (Compact Virtual eXecutor) is a simple, lightweight shell written in C, designed to be fast, flexible, and pleasant to use.

### Features:
* Runs **normal Linux commands**, supports **pipes** and **redirections** (`>`, `>>`, `<`, `<<` heredoc)
* Built-in commands:
  - `cd` (with `~` support)
  - `pwd` (`-L` logical, `-P` physical, `--help`)
  - `help` (shows all built-ins)
  - `ls` (auto `--color=auto`)
  - `history` (shows command history)
  - `alias [<name>-<command>]` (creates a alias)
  - `unalias [name]` (removes the specified alias)
  - `echo` (supports env vars)
  - `export [VAR=value]` (sets environment variables)
  - `jobs` (list background jobs)
  - `fg` (resume job in foreground)
  - `bg` (resume job in background)
  - `exit` (exit the shell)
  - `functions` (lists all defined functions.)
  - `delfunc [name]` (deletes the specified function.)
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

`git clone https://github.com/JHXStudioriginal/CVX-Shell.git`

`cd CVX-Shell`

2. Build with make:

`sudo make install`

3. Run it

`cvx`

---------------------------------------------------------------------------------------------------------------------------------------------

###### CVX Shell is under the [Elasna Open Source License V3](https://github.com/JHXStudioriginal/Elasna-License/blob/main/LICENSE), but includes [linenoise](https://github.com/antirez/linenoise) by [antirez](https://github.com/antirez), which is licensed under the BSD 2-Clause License. See relevant files for details.
