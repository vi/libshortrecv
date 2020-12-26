# libshortrecv

Many special stream-oriented file descriptors in UNIX are expected to be allowed to read or write fewer bytes than requested by application.
Primary example can be a TCP socket. One `send` can turn into multiple `recv`s or multiple `send`s can turn into one `recv` at the other end.
In practive however when testing simple cases (e.g issueing small sends to 127.0.0.1) system may avoid short reads and writes, masking bugs in user programs.

This LD_PRELOAD library allows artificially decreasing buffer size parameter, simulating short reads or writes. Well-written programs are expected to stay as is, latently buggy programs can start to actually misbehave.


```
$ cmake .
$ make
$ LD_PRELOAD=$PWD/libshortrecv.so strace -e read,write cat
...
> ABCDEFGHIJKLMNOP
read(0, "ABCDEFGHIJKLMNOP\n", 79760)    = 17
write(1, "ABCDEFGH", 8)                 = 8
write(1, "IJ", 2)                       = 2
write(1, "KLMNOP", 6)                   = 6
write(1, "\n", 1)                       = 1
< ABCDEFGHIJKLMNOP

> kkkkkk
read(0, "kkkkk", 5)                     = 5
write(1, "kk", 2)                       = 2
write(1, "kk", 2)                       = 2
write(1, "k", 1)                        = 1
read(0, "k\n", 23675)                   = 2
write(1, "k", 1)                        = 1
write(1, "\n", 1)                       = 1
< kkkkkk
```


## Features

* Intercepting functions
    * read
    * write
    * send
    * recv
    * sendto
    * recvfrom
    * readv
    * writev
* Environment variable configuration
    * `LIBSHORTRECV_SEED` - set randome number seed. Use value `0` to force 1-byte reads or writes every time.
    * `LIBSHORTRECV_NOREAD` - disable intercepting of reading functions
    * `LIBSHORTRECV_NOWRITE` - disable intercepting of writing functions
    * `LIBSHORTRECV_NOV` - disable intercepting of `readv`/`writev` (which is untested at all)


## Issues/Limitations

* Almost untested (LD_PRELOAD trick seems to fail to work with the program I indended to test this with, so I sort of abandoned the project)
* Not everything is interpeptable with LD_PRELOAD. `strace`-based or other closer-to-kernel solution would likely to work more reliably.
* `readv`/`writev` interception code temporarily mutates iov which is supposed to be const.
