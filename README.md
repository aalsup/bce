# BCE: Bash-Completion-Extension

## BASH completion

The BASH shell allows the user to register an arbitrary program 
or script to be used to help the user complete command-line arguments
for a particular application.

In general, the syntax is `complete -c my_less_helper.sh less`.
This would register the script `my_less_helper.sh` for the application `less`.

### How BASH completion works

When the user types `less<tab><tab>` at the BASH shell the 
auto-complete script `my_less_helper.sh` will be invoked. The script
can output any information that the user might find helpful to choose 
the correct program arguments for their needs.

In addition to invoking `my_less_helper.sh`, BASH provides 2 additional
environment variables to the script. The variables are `COMP_LINE` and 
`COMP_POINT`. These variables provide the script with information about
what the user has typed into the shell thus far (`COMP_LINE`) and where
the user's cursor is right now (`COMP_POINT`).

### The problem

Most BASH completion scripts are pretty terrible. Additionally, there
is no easy way for users to share really helpful completion routines,
other than shipping around executable scripts (which is risky).

## Project goals

A program that will provide completion assistance for any other program.
A SQLite database contains all the data necessary to provide a consistent,
portable dataset to provide auto-completion help for command-line applications.

The program will provide a way to easily import/export configurations, so
that users can easily share completion content.

## Project internals

This is an ANSI-C(99) project. The reasons for using C are: portability,
speed, and dependency simplicity.

Tests are written in C++, since the the `Catch2` framework is C++.

The build system is CMake (3.21+).

## Project dependencies

The project requires the following for linking:

- sqlite
- catch2

On MacOS, you should be able to satisfy these dependencies using `brew`.

```bash
$ brew install sqlite
$ brew install catch2
```

## Build/Run configuration

The project should build and run as-is. However, without passing in
the expected environment variables, nothing interesting will be displayed.

#### Example run configuration

```
Executable: bash_completion_extension
Working Directory: /Users/<yada>/Projects/bash_complete_extension
Environment Variables: COMP_LINE="kubectl --namespace=public get pods -o wide";COMP_POINT=16
```

#### Example test configuration

```
Target: tests
Working Directory: /Users/<yada>/Projects/bash_complete_extension
```

## High-level design

Logically, the representation of a `command` is as follows:

```
command
  ├── aliases
  ├── sub-commands
      ├── aliases
      ├── arguments
          ├── options
      ...
  ├── arguments
      ├── options
```

### Import/Export configurations

```bash
$ bce --export kubectl --file=kubectl.db
$ bce --import --file=kubectl.db 
```

### JSON format

```json
{
  "command": {
    "uuid": "str",
    "name": "str",
    "aliases": [
      {
        "uuid": "str",
        "name": "str"
      }
    ],
    "args": [
      {
        "uuid": "str",
        "arg_type": "NONE|OPTION|FILE|TEXT",
        "description": "str",
        "long_name": "str",
        "short_name": "str",
        "opts": [
          {
            "uuid": "str",
            "name": "str"
          }
        ]
      }
    ],
    "sub_commands": [
      {
        "uuid": "str",
        "name": "str",
        "aliases": [],
        "sub_commands": [],
        "args": []
      }
    ]
  }
}
```

### Future capabilities

1. **Import/Export JSON**

```bash
$ bce --export kubectl --format=json --file=kubectl.json
$ bce --import --format=json --file=kubectl.json
```

2. **Improve the order of recommendations**

The cursor position could be used to determine which cmd/arg/opt is most
likely what the user is looking for help with.

3. **Provide a mechanism to easily create new completion data**

Currently, there is no easy way to populate the database for a
new command. Need to consider some approaches for creating new records
for commands, sub-commands, arguments, and options.

4. **Multiple completion databases**

Currently, the application utilizes a single SQLite database. This is probably fine, but it does require import/export
logic in order to load new completion information. Alternatively, we could use discrete SQLite database files for each
command. The application could dynamically load the correct database file (based on the command name). This offers a
clean way to import/export configurations, since each command is self-contained within its own DB file. However, this
would present a challenge for aliases. How would the application know which DB to open if the command-line contains 
an alias (rather than the actual command name).

