<img src="cvx-logo.png" alt="CVX Logo" width="120" align="right">
<p>&nbsp;</p>

<h1 align="center">CVX <small>(Compact Virtual eXecutor)</small></h1>

<p align="center">
  <strong>is a simple, lightweight shell written in C, designed to be fast, flexible, and pleasant to use.</strong>
</p>

<p align="center">
  <img src="https://github.com/JHXStudioriginal/CVX-Shell/actions/workflows/ci.yml/badge.svg" alt="CI Status">
  <img src="https://img.shields.io/github/v/release/JHXStudioriginal/CVX-Shell?color=blue" alt="GitHub release">
  <img src="https://img.shields.io/badge/license-EOSL--V3-orange" alt="License">
</p>



### 🚀 Features:
* Runs **normal Linux commands**, supports **pipes** and **redirections** (`>`, `>>`, `<`, `<<` heredoc)
* Supports **conditional statements** (`if`/`else`) and **loops** (`for`, `while`, `until`)
* Command chaining with `&&` and `||`
* Pipelines with `|`
* Tiny, fast C implementation with line editing powered by [linenoise](https://github.com/antirez/linenoise)

### 🛠 Built-in commands:

| Category | Commands |
| :--- | :--- |
| **Filesystem** | `cd`, `pwd`, `ls` |
| **Process** | `jobs`, `fg`, `bg`, `exec`, `exit` |
| **Variables** | `export`, `alias`, `unalias`, `echo` |
| **Scripting** | `break`, `continue`, `:`, `functions`, `delfunc` |
| **Utility** | `help`, `history` |

### ⚙️ Arguments:
* `cvx --version`, `cvx -v`, `cvx -version` — shows shell version
* `cvx -c "<command>"` — run specified command and exit
* `cvx -l` — loads `/etc/profile` and `~/.profile`

### 📂 Configuration:
* Custom prompt, startup dir, and history toggle via `/etc/cvx.conf` and `~/.cvx.conf`

---

###### See [wiki](https://github.com/JHXStudioriginal/CVX-Shell/wiki) on project page to read more.

### 💻 Installation:

Using Git + Make

```
git clone https://github.com/JHXStudioriginal/CVX-Shell.git
cd CVX-Shell
make
./cvx
```

---------------------------------------------------------------------------------------------------------------------------------------------

###### CVX Shell is under the [Elasna Open Source License V3](https://github.com/JHXStudioriginal/Elasna-License/blob/main/LICENSE), but includes [linenoise](https://github.com/antirez/linenoise) by [antirez](https://github.com/antirez), which is licensed under the BSD 2-Clause License. See relevant files for details.
