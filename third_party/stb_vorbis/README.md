# stb_vorbis

`stb_vorbis.c` is Sean Barrett's Ogg Vorbis decoder, v1.22, taken
unchanged on 2026-09-29 from https://github.com/nothings/stb, commit
1ee679ca2ef753a528db5ba6801e1067b40481b8 (the last one to touch the
file), https://raw.githubusercontent.com/nothings/stb/1ee679ca2ef753a528db5ba6801e1067b40481b8/stb_vorbis.c.
SHA-256 4c7cb2ff1f7011e9d67950446b7eb9ca044f2e464d76bfbb0b84dd2e23e65636.

Licence: public domain (the Unlicense) or MIT, at the user's choice; the
text is at the end of the file. It is a generic decoder of a published
audio format, no game's code.

Used by the runner (tools/run/mscdex.c, `-cdwav`: the audio tracks of
a cue sheet, which GOG ships as Ogg Vorbis). It is compiled as its own
file; its users include it with `STB_VORBIS_HEADER_ONLY` defined. Keep it
unchanged, so that it can be replaced by a newer version as a whole.
