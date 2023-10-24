# ctrlemu

Ctrlemu turns keyboard input into controller input by writing to a virtual device. This program was written to enable multiple people to use the keyboard over Steam Remote Play or Parsec.

## Usage

`./ctrlemu input config`

Where `input` (default stdin) is the keyboard input device (such as `/dev/input/event3`). You can find out which one is your keyboard with `cat /proc/bus/input/devices`. Reading from the keyboard device file may require superuser privileges.

Where `config` (default "keys.conf") is the configuration file. Lines in this file are structured as follows: `input_code ":" output_type [ opposite_code ] output_code output_value`. `opposite_code` is only specified if `output_type` == `EV_ABS`. An example configuration file is provided in `keys.conf`.

Alternatively, you can pipe the output of the input device file into the program. With no arguments, it will read from stdin.

## Building

`make all`.
See Makefile for other rules.

## Limitations

Requiring superuser privileges is not ideal, but since it needs to read input when you are not focused on the terminal it is running in (because you're probably using it to play a game) it is necessary.

This program will only compile for Linux on account of requiring `<linux/uinput.h>`.
