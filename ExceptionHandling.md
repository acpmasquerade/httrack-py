# Introduction #

Since the httrack plugin has no main Python program, exceptions raised by a method of a callback class instance must be caught within the plugin.

The C extension class providing the Python function httrack has the same behaviour for exceptions raised in callbacks. (The httrack engine is IMO too complex to try to forward a Python exception from a callback up to the caller of the module function httrack.)

While the recommended way to handle execptions is to use try/except/finally statements within a callback method, it can sometimes be difficult to predict all possible exceptions. Moreover, exceptions can also occur in httrack-py itself, e.g. for memory allocation errors, or if a callback method defines the wrong number of arguments.

From version 0.6 on, the "exception behaviour" of httrack-py is configurable.

The main problem with exceptions in Python callback methods is the fact, that not every httrack callback allows to abort the mirror. An example: While I assume that Xavier Roche intended the save-name callback for relatively simple things like directory layout manipulation or for dealing with the 8.3 filename limit under MSDOS, i.e., for code, where errors are highly unlikely (at least after debugging...), Python callbacks make it easy to "abuse" this callback. For example, one can record the relation between original URLs and file paths in an SQL database. If this relation is essential for the usage of the mirrored data, and if the access to the database raises an exception, it does not make sense to continue the mirroring process. In another application, an execption in this callback may be completely irrelevant.

Only the callbacks start, end, change-options, loop, send-header and receive-header can abort the mirror by returning 0; all other callbacks have no way to do this directly. httrack-py offers two ways to deal with this problem:

  1. If the exception is absolutely fatal, httrack-py calls exit(-1); i.e., the program is immediately aborted (IMMEDIATE\_STOP). Be very cautious with this option -- I don't know, if it could for example corrupt httrack's cache data.
  1. Normally, httrack-py sets an internal flag that causes the next call of one of the callbacks start, end, change-options, loop, send-header and receive-header (REGULAR\_STOP). If this flag is set, httrack-py does not try to call the Python callback method, but returns immediately the status 0.

Alternatively, an exception can be completely ignored (IGNORE\_EXCEPTION).

Both the plugin version of httrack-py and the C extension module provide definitions for the constants IMMEDIATE\_STOP, REGULAR\_STOP, IGNORE\_EXCEPTION.

The default behaviour is REGULAR\_STOP. It can be modified as follows:

**1. If the callback class defines a method named `error_handler`**

```
def error_handler(self, cbname, exctype, value, traceback):
```

This method is called, when httrack-py detects an exception in any of the callbacks. cbname is the name of the method raising the exception; exctype and value provide Python's exception type and exception value as described for example in chapter 8 of the Python Tutorial; traceback is the traceback object.

The method must return one of the values IMMEDIATE\_STOP, REGULAR\_STOP and IGNORE\_EXCEPTION.

Python's error information is not automatically printed to stderr, except for uncaught exceptions raised in the error handler itself.

If an exception is raised in the error\_handler, httrack-py uses IMMEDIATE\_STOP to abort the mirror.

**2. If the callback class does not define the method error\_handler**

In this case, httrack-py checks, if the callback instance has an attribute with the name error\_policy. This attribute must be a mapping. Its keys should be the names of the callbacks, or 'default'; the values must be IMMEDIATE\_STOP, REGULAR\_STOP or IGNORE\_EXCEPTION.

If an exception is raised, and if the mapping has an entry with the name of callback that raised the exception, the value of the entry define the error behaviour. Otherwise, the default value is used, if defined.

**3. If the callback object has no method called error\_handler, and if it has either no attribute error\_policy or if error\_policy is defined but has no key default**

In this case, httrack-py tries to read the environment variable HTTRACK\_PY\_ERROR\_POLICY. If defined, this variable must have the value -1 (IMMEDIATE\_STOP), 0 (REGULAR\_STOP) or 1(IGNORE\_EXCEPTION).

**4. otherwise error behaviour REGULAR\_STOP is used.**

If any of the ways to define the error behaviour result in another value than IMMEDIATE\_STOP, REGULAR\_STOP or IGNORE\_EXCEPTION, the value IMMEDIATE\_STOP is assumed.

In the cases 2, 3 and 4, Python's error information is printed to stderr.

Both the plugin version of httrack-py and the C extension module provide definitions for the constants IMMEDIATE\_STOP, REGULAR\_STOP, IGNORE\_EXCEPTION. For the plugin version, integer variables are inserted into the namespace of the Python module during plugin initialization, so you can use them for example in the constructor of the callback class:

```
    def cbClass:
        def __init__(self):
            self.error_policy = {'__default__': IGNORE_EXCEPTION}
```

NOTE: Since the plugin library can create the variables only after the Python module has been loaded, you can't use them for code that is executed at module initialization time. Hence, the following example will lead to an error:

```
    def cbClass:
        error_policy = {'__default__': IGNORE_EXCEPTION}
```

The C extension module provides the variables in its namespace:
```
    import httracklib

    def cbClass:
        error_policy = {'__default__': httracklib.IGNORE_EXCEPTION}
```
Most exceptions raised in httrack-py itself are regular Python exceptions. Additionally, httrack-py can raise execptions of the specific type 'httrack-error':

  * The callbacks preprocess\_html and postprocess\_html raise this exception, if the reallocation of a buffer for HTML data fails.

  * The callbacks start and change\_options raise this exception, if the Python callback method deletes an entry of the options dictionary, or if an entry is set to a value of an invalid type.

# Python exceptions during plugin initialization #

If an error occurs during plugin initialization, the mirror process is aborted in the start callback, except if all of the following conditions are met:

  1. the plugin is the default httrack plugin and if it is loaded as the default plugin, i.e., without specifying a --wrapper option
  1. the environment variable HTTRACK\_PYTHON is not set
  1. an ImportError is raised.

This can mean that the user simply does not need a Python callback module for this mirror, hence the plugin simply prints the Python error message, but continues the mirror.

For all other exceptions (e.g, a syntax error in the Python source code), or if HTTRACK\_PYTHON is set, or if a --wrapper option is specified, the mirror is aborted.












