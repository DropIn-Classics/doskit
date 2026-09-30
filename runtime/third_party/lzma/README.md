# lzma

The LZMA and LZMA2 decoders of Igor Pavlov's LZMA SDK, taken unchanged
on 2026-09-30 from 7-Zip's repository, https://github.com/ip7z/7zip,
commit 0766b733fe3e06dd2a7f9a3cfbf2108ac73abd17, directory C/
(https://raw.githubusercontent.com/ip7z/7zip/0766b733fe3e06dd2a7f9a3cfbf2108ac73abd17/C/NAME).

Licence: public domain (each file's head says so). They are a generic
decoder of a published compression format, no game's code.

    a5d03b8cb65cd3a2a56bde46dd68fb356d2ebacd9d70283e7d4d20b4e0a64713  7zTypes.h
    d5e42be77d26beaa8c81d3e62ef99fc5685736cfc079779fdb9154d05fa8bc4a  Compiler.h
    c8903f2e36a771a272d8981e1e9ffed5ba16ca3d2f3dca6d1414d2340a6829d0  Precomp.h
    3aaf07b4ae4173a2d103179455dc7089b5ddbc7fc3db3c0e40964a7499c69266  LzmaDec.h
    4e6ec665a6df01f1722e2b2fc36dc5e3aa7c3890e0f8cb11c6a07c9a01f3309b  LzmaDec.c
    a4b97083c3817d3e1e3049f8b1abc0b4ca3e91192606497c02bb5351b57422c7  Lzma2Dec.h
    3d691d30e4a1f4b661d56fc483db702167ff6a91781d143a017c793c1860476f  Lzma2Dec.c

(SHA-256.) Used by ../../inno.c (GOG's Windows installers, which Inno
Setup packs with LZMA or LZMA2), which includes LzmaDec.c and Lzma2Dec.c
itself, so that a project's build needs no more files than inno.c. Keep
them unchanged, so that they can be replaced by a newer version as a
whole.
