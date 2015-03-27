The extension module defines only one function, httrack, which invokes the httrack engine. The function needs two parameters; the first is an instance of a callback class; the second parameter must be a sequence. The sequence should contain strings, which are valid httrack command line arguments. The first element is interpreted as the program name, which must always specified, i.e., passing a sequence of length 0 raises an execption.

A trivial example of a python module that can be used:

```
import sys, httracklib

class CbClass:
   # we don't want to do anything fancy, but we simply need a
   # class definition
   pass

   callback = CbClass()

   # simply pass the command line parameters to the httrack engine
   httracklib.httrack(callback, sys.argv)
```

This example should work exactly like the "regular" httrack command line program.

Also see ExceptionHandling.