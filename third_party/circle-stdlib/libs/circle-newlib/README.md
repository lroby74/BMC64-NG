# Newlib for Circle

This is a fork of [newlib](https://sourceware.org/newlib/) for using it with [Circle](https://github.com/rsta2/circle).

## Project Home

This project moved from GitHub to **Codeberg**.

**Canonical repository:** https://codeberg.org/larchcone/circle-newlib.git

The GitHub repository https://github.com/smuehlst/circle-newlib.git
is a read-only mirror.

## Getting Started

This repository can only be used in the context of the
[circle-stdlib](https://codeberg.org/larchcone/circle-stdlib) project.
[circle-stdlib](https://codeberg.org/larchcone/circle-stdlib) contains
documentation and scripts for building newlib in combination with Circle.

## Modifying Newlib for Circle

The modifications for circle-stdlib are mainly in the subdirectory libgloss/circle.

When files are added, die Newlib build system must be updated. For this autoconf2.69
must be installed.

To update the build system:

```
$ cd libgloss
$ autoreconf2.69
```
