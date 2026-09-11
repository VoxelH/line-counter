# LineCounter

## Summary

A utility program designed to count code lines of the specified files, but was rewritten at least ten times before the dear honorable author wrote the first line of code of code line counting logic.
You can use it to count how much code you've typed in your project.
Besides, it is aimed to help you compare your program works with others', which can effectively boost your pride and happiness.

## Usage

```bash
line-counter [[--follow-symlink=<BOOL>] [--follow-junction-and-mountpoint=<BOOL>] --] <include-pattern> [<include-patterns>] [--exclude <exclude-pattern> [<exclude-patterns>]]
```

1. `--follow-symlink=<BOOL>`: `true` by default
2. `--follow-junction-and-mountpoint=<BOOL>`: `true` by default


## Patterns

A pattern describes what kinds of files should be collected or excluded. Every single pattern is a regular filesystem path combined with the following wildcards.

1. `*` (ASTERISK) wildcard: matches any number of characters, including zero, in a single path segment. 
   Note that `*` sequence is effectively equivalent to a single `*` character, except the `**` GLOBSTAR character sequence
2. `?` (QUESTIONMARK) wildcard: matches a single character in a single path segment.
3. `**` (GLOBSTAR) wildcard: matches any number of path segments. Note that repeating `**` sequence separated by path separator (`/` or `\`) is equivalent to a single `**` sequence

In addition, `?` and `*` wildcards can match volumes.

Examples:

```
D:\Development\**\*.cpp
?:\$RECYCLE.BIN\**\*.*
C:a.txt
E:\?.md
?:\** (YOU WILL NEVER WANNA TRY THIS)
```

## TODO
1. Encoding support. Currently assumes that all files are encoded in UTF-8
2. Better newline character detection
3. Better string matching pattern design
4. Better line counting logic ( How directories should be ignored, better code structure ... )
5. Tidy up main.cpp