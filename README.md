# mkhd
A hotkey daemon for macOS with layer support.

Based on [skhd](https://github.com/koekeishiya/skhd), with the extra love and attention it deserves.

## Features

new features compared to skhd:
 - layers, layer-based hotkey engine
 - custom key alias support
 - macro support
 - builtin synthesize key support with hotkey passthrough (noremap-like behaviour)
 - binding to both keydown and keyup
 - better code quality, memory safety, extensibility and more...


## Install

The first time mkhd is ran, it will request access to the accessibility API.
After access has been granted, the application must be restarted.

*Secure Keyboard Entry* must be disabled for mkhd to receive key-events.

**Homebrew**:

Requires xcode-8 command-line tools.

      // todo(0.1.0): make homebrew tap.

**Source**:

Requires xcode-8 command-line tools.

      git clone https://github.com/miigon/mkhd
      make release      # release version
      make              # debug version

## Configuration

**Please checkout [examples/mkhdrc](https://github.com/miigon/mkhd/blob/main/examples/mkhdrc) for a example configurations, as well as explanations of mkhd features.**

The default configuration file is located at one of the following places (in order):

 - `$XDG_CONFIG_HOME/mkhd/mkhdrc`
 - `$HOME/.config/mkhd/mkhdrc`
 - `$HOME/.mkhdrc`

A different location can be specified with the `--config | -c` argument.

A list of all built-in modifier and literal keywords can be found [here](https://github.com/koekeishiya/skhd/issues/1)

> // todo: migrate this list into mkhd repo. (reference the source file directly?)

## Usage

 - `--install-service`: Install launchd service file into ~/Library/LaunchAgents/net.miigon.mkhd.plist
 - `--uninstall-service`: Remove launchd service file ~/Library/LaunchAgents/net.miigon.mkhd.plist
 - `--start-service`: Run mkhd as a service through launchd
 - `--restart-service`: Restart mkhd service
 - `--stop-service`: Stop mkhd service from running
 - `-v` | `--verbose`: Output normal debug information (intended for end-users)
 - `-V` | `--veryverbose`: Output very verbose debug information (intended for mkhd developer)
 - `-P` | `--profile`: Output profiling information
 - `--version`: Print version info
 - `-c` | `--config`: Specify location of config file
    mkhd -c ~/.mkhdrc
 - `-o` | `--observe`: Output keycode and modifiers of event. Ctrl+C to quit
 - `-r` | `--reload`: Signal a running instance of mkhd to reload its config file
 - `-h` | `--no-hotload`: Disable system for hotloading config file
 - `-k` | `--key`: Synthesize a keypress (same syntax as when defining a hotkey)  
    `mkhd -k "shift + alt - 7"`  
    **note: this option is deprecated. use `.synthkey` action instead.**
 - `-t` | `--text`: Synthesize a line of text  
    `mkhd -t "hello, worldシ"`
    
## Troubleshooting

mkhd uses a pid-file to make sure that only one instance is running at any moment in time. This also allows for the ability to trigger
a manual reload of the config file by invoking `mkhd --reload` at any time while an instance of mkhd is running. The pid-file is saved
as `/tmp/mkhd_$USER.pid` and so the user that is running mkhd must have write permission to said path.
When running as a service (through launchd) log files can be found at `/tmp/mkhd_$USER.out.log` and `/tmp/mkhd_$USER.err.log`.
