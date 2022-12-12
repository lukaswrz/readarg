# readarg

## Features

* Short options (`-f`)
  * Short options with a value directly following the option character
    (`-fvalue`)
  * Short options with a value as a separate `argv` element (`-f value`)
  * Grouped short options (`-asdf`, `-asdfvalue`, `-asdf value`)
* Long options (`--file`)
  * Long options with a value separated by an equal sign (`--file=value`)
  * Long options with a value as a separate `argv` element (`--file value`)
* Multiple values are represented in an array (`-f value1 -f value2 ...`)
* Operands mixed with options (`-f value1 operand1 -f value2 operand2`)

## Usage

Installing this library is as simple as downloading the header file, dropping
it into your project directory and including it. Alternatively, you could choose
to use a Git submodule. In any case, attribution is not required.

It is required that one file in your project defines the
`READARG_IMPLEMENTATION` macro before including the `readarg.h` header file,
as with any other single-header library.

An example for how to use readarg can be found in `test/test.c`. If you want to
see how readarg represents options and operands, run `test.bash`.

## Terminology

If you're wondering what exactly the difference between an option, an operand or
an argument is, you can skim this document to get a better idea:
[POSIX Utility Conventions](https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap12.html)

## Internals

readarg permutes `argv` to handle multiple values for each option and to assign
values to operands. The advantage of this approach is as follows:

* Allocations are not needed because the memory provided by `argv` is reused
* It's fairly simple to represent all of this data in an intuitive data
  structure (in my opinion anyway)
