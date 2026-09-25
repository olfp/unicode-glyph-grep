# unicode-glyph-grep

`unicode-glyph-grep` is a tiny grep-like tool for searching Unicode mathematical glyphs using ordinary text patterns.

It is especially useful when source files contain MathBold/MathItalic identifiers such as:

- `𝐩𝐫𝐨𝐜`
- `𝑟𝑒𝑠𝑢𝑙𝑡`
- `𝐡`

and you want to match them with plain ASCII patterns such as:

- `proc`
- `result`
- `h`

## Why it exists

Many tools like `grep` and `ripgrep` search raw Unicode text. A glyph like `𝐩𝐫𝐨𝐜` is not considered equivalent to `proc` by default, so ordinary searches miss matches.

This utility normalizes mathematical alphanumeric characters before matching, while still printing the original source line. That makes it practical for source files written in glyph-based notations.

## Example

```sh
unicode-glyph-grep -nEi 'proc|result|h' demo.u68
```

If `demo.u68` contains:

```text
𝐩𝐫𝐨𝐜 𝑚𝑎𝑘𝑒𝑛𝑜𝑑𝑒 = (𝐢𝐧𝐭 𝑣) 𝐫𝐞𝐟 𝐧𝐨𝐝𝐞:
```

then the output will show the original line, with the glyphs still visible:

```text
demo.u68:1:𝐩𝐫𝐨𝐜 𝑚𝑎𝑘𝑒𝑛𝑜𝑑𝑒 = (𝐢𝐧𝐭 𝑣) 𝐫𝐞𝐟 𝐧𝐨𝐝𝐞:
```

## Usage

```sh
unicode-glyph-grep [OPTIONS] PATTERN [FILE ...]
```

Common options:

- `-n` print line numbers
- `-i` case-insensitive matching
- `-E` extended regex
- `-r` recursive search (planned)

## Current status

This repository currently provides the idea and the initial implementation scaffold. The command-line utility is intentionally simple and focused on the use case above.

## License

MIT
