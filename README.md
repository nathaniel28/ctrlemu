# ctrlemu

Ctrlemu turns keyboard input into controller input by writing to a virtual device. This program was written to enable multiple people to use the keyboard over Steam Remote Play or Parsec.

## Usage

`./ctrlemu file` where `file` is the keyboard input device (such as `/dev/input/event3`). You can find out which one is your keyboard with `cat /proc/bus/input/devices`. Reading from this file may require superuser privileges.

Alternatively, you can pipe the output of this file into the program. With no arguments, it will read from stdin.

## Building

`make`.

## Limitations

Currently, the mapping of keys to controller buttons/joysticks are hardcoded. In the future, they may be read from a file.

Requiring superuser privileges is not ideal, but since it needs to read input when you are not focused on the terminal it is running in (because you're probably using it to play a game) it is necessary.

This program will not compile for Windows.