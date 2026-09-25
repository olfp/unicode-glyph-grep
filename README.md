# ugrep

`ugrep` is a grep-like command for source files containing Unicode mathematical
alphanumeric glyphs. It normalizes matching text with Unicode NFKC, so `proc`
can match `𝐩𝐫𝐨𝐜`, while output remains the original source line.

```sh
./ugrep -n proc program.u68
```

## Modes and options

By default, regular files are probed for mathematical glyphs. The first 100
lines are inspected (configurable below). Unicode files use normalized matching;
ordinary files are delegated to the configured system grep. Pipes are buffered
because stdin cannot be rewound.

```text
-h, --no-filename       suppress filename prefixes
-H, --with-filename     always print filename prefixes
-n, --line-number       print line numbers
a, --ignore-case        ignore case
-v, --invert-match      select non-matching lines
-c, --count             print matching-line counts
-m N, --max-count N     stop after N matches per input
-r, --recursive         search directories recursively
-u, --unicode           force Unicode normalization mode
-t, --no-unicode        force the system grep path
--probe-lines N         inspect N initial lines for glyph detection
--config FILE           select a configuration file
--help                  show help
```

`/usr/bin/grep` is used by default for pass-through mode, so installing or
symlinking `ugrep` as `grep` does not recurse into `ugrep`.

## `.ugreprc`

`ugrep` reads `.ugreprc` in the current directory, then `~/.ugreprc`. Use
`--config FILE` to select another file. Current-directory configuration wins.

```ini
[ugrep]
probe_lines = 100
grep = /usr/bin/grep
```

`probe_lines` controls the detection window. `grep` selects the system grep
executable. Command-line options override configuration values.

## Tests

```sh
python3 -m unittest discover -s tests -v
```
