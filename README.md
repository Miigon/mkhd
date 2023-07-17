# mkhd
A hotkey daemon for macOS with layer support.

Based on [skhd](https://github.com/koekeishiya/skhd), with the extra love and attention it deserves.

mkhd uses a pid-file to make sure that only one instance is running at any moment in time. This also allows for the ability to trigger
a manual reload of the config file by invoking `mkhd --reload` at any time while an instance of mkhd is running. The pid-file is saved
as `/tmp/mkhd_$USER.pid` and so the user that is running mkhd must have write permission to said path.
When running as a service (through launchd) log files can be found at `/tmp/mkhd_$USER.out.log` and `/tmp/mkhd_$USER.err.log`.

## features

new features compared to skhd:
 - layers, layer-based hotkey engine
 - binding to both keydown and keyup
 - better code quality, memory safety, extensibility and more...


### Install

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

### Usage

// todo(0.1.0): update usage

```
--install-service: Install launchd service file into ~/Library/LaunchAgents/com.koekeishiya.mkhd.plist
    mkhd --install-service

--uninstall-service: Remove launchd service file ~/Library/LaunchAgents/com.koekeishiya.mkhd.plist
    mkhd --uninstall-service

--start-service: Run mkhd as a service through launchd
    mkhd --start-service

--restart-service: Restart mkhd service
    mkhd --restart-service

--stop-service: Stop mkhd service from running
    mkhd --stop-service

-V | --verbose: Output debug information
    mkhd -V

-P | --profile: Output profiling information
    mkhd -P

-v | --version: Print version number to stdout
    mkhd -v

-c | --config: Specify location of config file
    mkhd -c ~/.mkhdrc

-o | --observe: Output keycode and modifiers of event. Ctrl+C to quit
    mkhd -o

-r | --reload: Signal a running instance of mkhd to reload its config file
    mkhd -r

-h | --no-hotload: Disable system for hotloading config file
    mkhd -h

-k | --key: Synthesize a keypress (same syntax as when defining a hotkey)
    mkhd -k "shift + alt - 7"

-t | --text: Synthesize a line of text
    mkhd -t "hello, worldシ"
```

### Configuration

The default configuration file is located at one of the following places (in order):

 - `$XDG_CONFIG_HOME/mkhd/mkhdrc`
 - `$HOME/.config/mkhd/mkhdrc`
 - `$HOME/.mkhdrc`

A different location can be specified with the *--config | -c* argument.

A sample config is available [here](https://github.com/miigon/mkhd/blob/master/examples/mkhdrc)

A list of all built-in modifier and literal keywords can be found [here](https://github.com/koekeishiya/skhd/issues/1)

A hotkey is written according to the following rules:
```
// todo(0.1.0): document the new syntax
```

Aliases can also be used anywhere a modifier or a key is expected:
```
# alias as modifier
.alias $hyper cmd + alt + ctrl
$hyper - t : open -a Terminal.app

# alias as key
.alias $capslock 0x39
ctrl - $capslock : open -a Notes.app

# alias as mod-key
.alias $exclamation_mark shift - 1
$hyper - $exclamation_mark : open -a "System Preferences.app"

# alias within alias
.alias $terminal_key $hyper + shift - t
$terminal_key : open -a Terminal.app
```

General options that configure the behaviour of mkhd:
```
# specify a file that should be included as an additional config-file.
# treated as an absolutepath if the filename begins with '/' otherwise
# the file is relative to the path of the config-file it was loaded from.

.load "/Users/Koe/.config/partial_mkhdrc"
.load "partial_mkhdrc"

# prevents mkhd from monitoring events for listed processes.

.blacklist [
    "terminal"
    "qutebrowser"
    "kitty"
    "google chrome"
]
```
