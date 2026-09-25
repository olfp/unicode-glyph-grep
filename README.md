# ugrep

`ugrep` is a grep-like command for source files containing Unicode mathematical
alphanumeric glyphs, such as ALGOL 68 publishing notation:

```text
𝐩𝐫𝐨𝐜 𝑚𝑎𝑖𝑛 = ...
```

It lets an ordinary pattern match the glyph form:

```sh
./ugrep -n proc program.u68
```

Matching output remains the original source text. The executable is intentionally
named `ugrep`, so it can be installed separately from the system `grep`:

```sh
install -m 755 ugrep "$HOME/bin/ugrep"
```

## Automatic mode

For regular files, `ugrep` examines the first 100 lines (or the configured probe
size). If mathematical glyphs are found, it normalizes text with Unicode NFKC
before matching. Otherwise it delegates the search to the real system grep,
which preserves normal grep behavior for ordinary files.

A pipe is buffered because stdin cannot be rewound:

```sh
some-command | ugrep -i error
```

Use `-u` to force Unicode mode, or `-N` to force the system grep path:

```sh
ugrep -u proc program.u68
ugrep -N error ordinary.log
```

`/usr/bin/grep` is preferred for pass-through mode. It can be changed in the
configuration file.

## Options

```text
-h, --no-filename       suppress filename prefixes
-H, --with-filename     always print filename prefixes
-n, --line-number       print line numbers
-i, --ignore-case       ignore case
-v, --invert-match      select non-matching lines
-c, --count             print matching-line counts
-m N, --max-count N     stop after N matches per input
-r, --recursive         search directories recursively
-u, --unicode           force Unicode normalization mode
-N, --no-unicode        force the real system grep
--probe-lines N         inspect N initial lines for glyph detection
--config FILE           use a specific configuration file
--help                  show help
```

Unknown grep options are passed through when automatic mode delegates to the
system grep. In forced Unicode mode, only the options listed above are accepted.

## `.ugreprc`

`ugrep` looks for `.ugreprc` in the current directory, then in your home
directory (`~/.ugreprc`). A file in the current directory takes precedence.
Use `--config FILE` to select another file explicitly.

The supported configuration is an optional `[ugrep]` section with:

```ini
[ugrep]
probe_lines = 100
grep = /usr/bin/grep
```

`probe_lines` controls how many initial lines are inspected. `grep` selects the
system grep executable used for ordinary-text pass-through. Command-line options
such as `--probe-lines` override the configuration value.

## Tests

Run the test suite with:

```sh
python3 -m unittest discover -s tests -v
```
