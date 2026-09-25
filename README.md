# ugrep

`ugrep` is a grep-like C command for source files containing Unicode mathematical
alphanumeric glyphs. It normalizes common mathematical alphanumeric symbols, so
`proc` can match `𝐩𝐫𝐨𝐜`, while output remains the original source line.

Build and install:

```sh
make
sudo make install                 # installs /usr/local/bin/ugrep
make PREFIX="$HOME/.local" install
```

The command supports automatic glyph detection, Unicode mode (`-u`), system-grep
mode (`-t`), line numbers (`-n`), counts (`-c`), recursive searches (`-r`), and
standard filename controls. Ordinary files are delegated to `/usr/bin/grep` by
default; this prevents recursion if `ugrep` is also installed as `grep`.

```sh
ugrep -n proc program.u68
ugrep -u proc program.u68
ugrep -t error ordinary.log
some-command | ugrep -n proc
```

The first 100 lines are probed by default. `.ugreprc` is read from the current
directory, then `$HOME/.ugreprc`:

```ini
[ugrep]
probe_lines = 100
grep = /usr/bin/grep
```

Use `--probe-lines N` or `--config FILE` to override these settings. Run
`ugrep --help` for the complete option list.

The C implementation has no external runtime dependencies; it uses POSIX regex
and UTF-8 decoding. The glyph conversion table covers the mathematical
alphanumeric blocks used by the project.
