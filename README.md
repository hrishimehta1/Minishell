# EnhancedMiniShell

EnhancedMiniShell is a feature-rich shell implementation written in C. This project demonstrates advanced functionalities of a Unix-like shell, including executing commands, handling input/output redirection, managing processes, and job control.

## Features

- Execute basic shell commands
- Command history
- Input and output redirection
- Background process execution
- Job control (`fg`, `bg`, `jobs`)
- Piping
- Environment variable management
- Built-in commands (`cd`, `exit`, `history`, `setenv`, `unsetenv`, `pwd`, `lf`, `lp`)
- Signal handling

## Getting Started

### Prerequisites

To build and run EnhancedMiniShell, you need:

- GCC (GNU Compiler Collection)
- Make

### Building EnhancedMiniShell

Clone the repository and navigate to the project directory:

```bash
git clone https://github.com/yourusername/EnhancedMiniShell.git
cd EnhancedMiniShell
```

Compile the project using the Makefile:

```bash
make
```

### Running EnhancedMiniShell

After building the project, you can run the shell by executing:

```bash
./minishell
```

### Usage

Once EnhancedMiniShell is running, you can type commands as you would in a typical Unix shell. For example:

```sh
$ ls -l
$ cat file.txt
$ grep "search_term" file.txt > output.txt
$ ls | grep "pattern"
$ setenv VAR value
$ unsetenv VAR
$ fg 1
$ bg 1
$ jobs
$ pwd
$ lf
$ lp
```

## Contributing

Contributions are welcome! Please open an issue or submit a pull request.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
