## Cryptography Extension

Copyright (C) 2012 Saphirion AG
Copyright (C) 2012-2020 Ren-C Open Source Contributors

Distributed under the Apache 2.0 License
mbedTLS is distributed under the Apache 2.0 License

### Purpose

The Cryptography extension currently includes an eclectic set of ciphers, key
generators, and hashes.  All of the crypto math comes from the mbedTLS
project, which is a modular library of routines written in pure C, that can
be chosen from a-la-carte:

https://tls.mbed.org/source-code

The extension does not include mbedTLS's C code for the handshaking and
protocol of TLS itself.  Instead, this is implemented in usermode by a file
called %prot-tls.r - so only the basic crypto primitives are built as C.

Currently there are no options in the extension build process for selectively
choosing which cryptography primitives are included.  Instead, the chosen set
is enough to write a TLS 1.2 module that can speak to the majority of websites
operating circa 2020.

### History

R3-Alpha originally had a few hand-picked routines for hashing picked from
OpenSSL.  Saphirion added support for the AES streaming cipher and Diffie
Hellman keys in order to do Transport Layer Security 1.1 (TLS, e.g. the "S" in
the "Secure" of HTTPS).  Much of the code for this cryptography was extracted
from the "axTLS" library by Cameron Rich.

Ren-C added support for TLS 1.2.  But with sites starting to require newer
cipher suites using ECDHE ("Elliptic-Curve Diffie-Hellman Ephemeral") and CBC
("Cipher Block Chaining"), it became clear that maintaining one's own
granular cryptography would not be easy.  The patchwork method of pulling
non-audited C files off the internet was haphazard and insecure.

Rather than give in to not offering a transparent window into the protocol
itself, all crypto was transitioned to the mbedTLS project.  Its mission
aligned well with Rebol philosophy...having its dependencies factored well
so you could use just those parts of the library you needed.  And it is
vetted for security holes by a number of invested parties:

https://tls.mbed.org/security

### Future Work

The initial goal of changing all key exchanges, hashes, and ciphers to mbedTLS
is complete.  However, the natives which expose this functionality does not
cover the full abilities that the underlying code has.

For instance: it is possible to do "streaming" hashes which do not require the
entire block of data that is to be hashed to be loaded in memory at one time.
One could imagine a hash "PORT!" to which data could be written a piece at
a time, enabling the calculation of SHA hashes on multi-gigabyte files...
digesting the information as it is read in chunks and then discarded.

mbedTLS includes a "BigNum" facility, which does arbitrary-precision math
on integers.  It would be ideal if the INTEGER! datatype could be extended
to apply this code as the means for handling numbers whose range doesn't fit
into a single value cell.  That would be a salient reuse of code, not found
in other languages.  See this forum post for concepts:

https://forum.rebol.info/t/planning-ahead-for-bignum-integer/623

Being able to break the ciphers into smaller modules might be appealing,
where they could be loaded on an as-needed basis.  This could run against
the "single file" aesthetic of historical Rebol, but having many tiny DLLs
may be a benefit as a build option for some.
