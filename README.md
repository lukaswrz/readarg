# readopt

## Overview

This is an option parsing library.

The following option formats are recognized:
* `-f`
* `-fvalue`
* `-f value`
* `-asdf` (grouped short options)
* `-asdfvalue`
* `-asdf value`
* `--file`
* `--file value`
* `--file=value`
* `-f value1 -f value2 ...` (multiple values are represented in an array)
* `-f value1 operand1 -f value2 operand2` (operands intermixed with options)

It permutes `argv` to handle multiple values for each option and to assign
values to operands.

`mdoc(7)` `SYNOPSIS` sections and plaintext usage messages can be generated as
well (via `readopt_put_usage`).

An example can be found in `test/test.c`.

## Building

Build and install the library by either using

```
$ ninja
# ninja install
```

or

```
$ make
# make install
```

By default, the library will be installed to `/usr/local/lib`, which may not be
in your library path. You can change this by modifying the `$prefix` in the
`config.ninja` or by overriding the `$(PREFIX)` variable for the `Makefile`.
