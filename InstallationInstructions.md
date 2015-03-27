The extension consists of some C code and a Python wrapper to it.

To build the extension, you need the httrack sources. Versions from 3.33-beta3 up to 3.4.0-2 should work (tested with 3.40.4-3.1). However the extension probably cannot be compiled with newer versions due to the changed API/ABI in those versions.

Note that you also need to run the configure script in the httrack source package to generate config.h - if you don't, the compile will not succeed.

On unix, the package can be built & installed like any other python package. You have to first set HTTRACK\_SRC\_DIR in setup.py to point to the location of the httrack sources, then run:

```
python setup.py build
```

and

```
python setup.py install
```


These two steps result in the extension being compiled and installed.

To compile & install the code manually instead, first run the compiler as:

```
gcc -g -I <path-to-Python-header-files> -I <path-to-httrack-source-dir> \
       -I <httrack-source-dir>/src -lhttrack \
       -l python<version-number> -O -g3 -Wall -D_REENTRANT -DINET6 \
       -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -DPLUGIN \
       -shared -pthread -o httrack-py.so httrack-py.c
```

This results in a HTTrack plug-in being built. Then copy the resulting httrack-py.so to /usr/local/lib or to some other place in the library search path. To compile as Python extension instead, leave off the "-DPLUGIN" parameter to gcc, and copy the resulting httracklib.so to Python site-packages directory.

Notes: for running the extension, the shared httrack library (libhttrack) must be installed & available also at runtime. On Debian Linux, it is available as libhttrack1, or you can build it from the same httrack sources needed to compile the extension. Make sure the version of the source & the library are the same if you're not building the library yourself. At least on Debian, it may happen that libhttrack is not found because of missing libhttrack.so link that has to point to libhttrack.so.1 (both in /usr/lib). Adding the link manually solves the issue.

For introduction to the usage of the library, see [Introduction](Introduction.md)