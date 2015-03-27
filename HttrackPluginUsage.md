To use httrack-py as a plugin, the Python module you've written should have the name httrack.py, and it must additionally define a function named register(), which returns an instance of the callback class.

Then, call httrack with the following parameter:

```
--wrapper start=httrack-py.so:hts_py_start
```

In the start callback, the httrack-py plugin calls register() to get an instance of a class, whose methods are called in the other callbacks.

If methods with the names mentioned above exist, the start callback registers the corresponding callbacks in the httrack core, and the methods will be called by the callback functions of the httrack-py library.

See the example file httrack.py for details on these methods.

NOTE: The automatic registration of other callbacks by the start callback works only in httrack 3.33-beta3 or newer! For older httrack versions, you'll need to specify each callback as an httrack command line parameter.

  * By default, the httrack-py plugin tries to load a Python module stored in a file named httrack.py in the directories listed in sys.path and in the current directory. The name and location of the module can be changed by setting the environment variable HTTRACK\_PYTHON to another filename, optionally with a path prefix.

  * httrack-py as httrack's default plugin: the --wrapper option can be omitted, if the filename of the module is libhttrack-plugin.so instead of httrack-py.so, and if it is located somewhere in the library search path. If libhttrack-plugin.so is stored in another directory, set the enviroment variable LD\_LIBRARY\_PATH to this directory. Thanks to Xavier Roche for the hint that the module can be automatically loaded this way.

Also see ExceptionHandling.
