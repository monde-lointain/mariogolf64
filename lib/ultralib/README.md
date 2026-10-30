# ultralib

libultra 2.0J, from the decompilation at https://github.com/decompals/ultralib,
commit e24c836 ("Remove usage of `pull_request_target` and GHA secrets from
CI").

This holds all of its public headers, but only the sources that the game
links and the private headers they include. Its own build, tools and
reference archive listings are left out: the game's Makefile builds these
sources with the game's toolchains.

Changes to the sources since then are in this repository's history.
