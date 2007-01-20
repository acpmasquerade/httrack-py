The httrack-py library allows to write HTTrack callbacks in Python, or
to use the httrack engine in a Python program.

It allows to use nearly all features of the httrack callbacks with a
Python module. 

Installation of the httrack plugin

  gcc -g -I <path-to-Python-header-files> \
         -I <path-to-httrack-source-dir> \
         -I <httrack-source-dir>/src \
         -l python<version-number> -O -g3 -Wall -D_REENTRANT -DINET6 \
         -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -DPLUGIN \
         -shared -pthread -o httrack-py.so httrack-py.c

  Next, copy the file httrack-py.so to /usr/local/lib or to some other
  place in the library search path.

Installation of the Python extension

  gcc -g -I <path-to-Python-header-files> \
         -I <path-to-httrack-source-dir> \
         -I <httrack-source-dir>/src \
         -l python<version-number> -O -g3 -Wall -D_REENTRANT -DINET6 \
         -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE \
         -shared -pthread -o httracklib.so httrack-py.c
  
  Next, copy the file httracklib.so to
  <path-to-the-Python-libraries>/site-packages

Usage:

  - httrack version 3.33-beta3 or newer is strongly recommended.
  
  - write a Python module which defines a class with one or more 
    methods with the following names:

    start, end, pause, query2, query3, change_options, check_html, 
    preprocess_html, postprocess_html, loop, check_link, save_file,
    link_detected, link_detected2 (*), save_name, send_header, 
    receive_header. For details on the method arguments, see the 
    example file httrack.py
    
    (*) for httrack version 3.33-beta4 and newer
    
  - Usage of the plugin for httrack:

    o The Python module mentioned above should have the name
      httrack.py, and it must additionally define a function named 
      register(), which returns an instance of the callback class. 
    
    o call httrack with the following parameter:

        --wrapper start=httrack-py.so:hts_py_start

      In the start callback, the httrack-py library calls register() to get
      an instance of a class, whose methods are called in the other callbacks.

      If methods with the names mentioned above exist, the start callback 
      registers the corresponding callbacks in the httrack core, and the 
      methods will be called by the callback functions of the httrack-py 
      library.
      
      See the example file httrack.py for details on these methods.
      
      NOTE: The automatic registration of other callbacks by the start
      callback works only in httrack 3.33-beta3 or newer! For older
      httrack versions, you'll need to specify each callback as an 
      httrack command line parameter.

    o By default, the httrack-py library tries to load a Python module
      stored in a file named httrack.py in the directories listed in 
      sys.path and in the current directory. The name and location of
      the module can be changed by setting the environment variable 
      HTTRACK_PYTHON to another filename, optionally with a path prefix.

    o httrack-py as httrack's default plugin:
      the --wrapper option can be omitted, if the filename of the module
      is libhttrack-plugin.so instead of httrack-py.so, and if it is located
      somewhere in the library search path. If libhttrack-plugin.so is stored
      in another directory, set the enviroment variable LD_LIBRARY_PATH to 
      this directory. Thanks to Xavier Roche for the hint that the module
      can be automatically loaded this way.

  - Usage of the C extension module:

    The extension module defines only one function, httrack, which
    invokes the httrack engine. The function needs two parameters; 
    the first is an instance of a callback class; the second parameter 
    must be a sequence. The sequence should contain strings, which are
    valid httrack command line arguments. The first element is interpreted 
    as the program name, which must _always_ specified, i.e., passing a 
    sequence of length 0 raises an execption.
    
    A trivial example::
    
      import sys, httracklib
      
      class CbClass:
          # we don't want to do anything fancy, but we simply need a
          # class definition
          pass
      
      callback = CbClass()
      
      # simply pass the command line parameters to the httrack engine
      httracklib.httrack(callback, sys.argv)
      
    This example should work exactly like the "regular" httrack command
    line program.

Exception Handling

  Since the httrack plugin has no main Python program, exceptions raised
  by a method of a callback class instance must be caught within the plugin.
 
  The C extension class providing the Python function httrack has the same 
  behaviour for exceptions raised in callbacks. (The httrack engine is IMO
  too complex to try to forward a Python exception from a callback up to
  the caller of the module function httrack.)
  
  While the recommended way to handle execptions is to use try/except/finally
  statements within a callback method, it can sometimes be difficult to
  predict all possible exceptions. Moreover, exceptions can also occur
  in httrack-py itself, e.g. for memory allocation errors, or if a callback 
  method defines the wrong number of arguments.
  
  From version 0.6 on, the "exception behaviour" of httrack-py is configurable.
  
  The main problem with exceptions in Python callback methods is the fact, 
  that not every httrack callback allows to abort the mirror. An example: 
  While  I assume that Xavier Roche intended the save-name callback for 
  relatively simple things like directory layout manipulation or for dealing 
  with the 8.3 filename limit under MSDOS, i.e., for code, where errors are 
  highly unlikely (at least after debugging...), Python callbacks make it 
  easy to "abuse" this callback. For example, one can record the relation 
  between original URLs and file paths in an SQL database. If this relation is 
  essential for the usage of the mirrored data, and if the access to the 
  database raises an exception, it does not make sense to continue the 
  mirroring process. In another application, an execption in this callback 
  may be completely irrelevant.
  
  Only the callbacks start, end, change-options, loop, send-header and
  receive-header can abort the mirror by returning 0; all other callbacks
  have no way to do this directly. httrack-py offers two ways to deal with
  this problem: 
  
    1. If the exception is absolutely fatal, httrack-py calls exit(-1);
       i.e., the program is immediately aborted (IMMEDIATE_STOP). Be very 
       cautious with this option -- I don't know, if it could for example 
       corrupt httrack's cache data.

    2. Normally, httrack-py sets an internal flag that causes the next
       call of one of the callbacks start, end, change-options, loop, 
       send-header and receive-header (REGULAR_STOP). If this flag is
       set, httrack-py does not try to call the Python callback method,
       but returns immediately the status 0.
  
  Alternatively, an exception can be completely ignored (IGNORE_EXCEPTION).
  
  Both the plugin version of httrack-py and the C extension module provide
  definitions for the constants IMMEDIATE_STOP, REGULAR_STOP,
  IGNORE_EXCEPTION. 
  
  The default behaviour is REGULAR_STOP. It can be modified as follows:

  1. If the callback class defines a method named 'error_handler'::
  
       def error_handler(self, cbname, exctype, value, traceback):
     
     this method is called, when httrack-py detects an exception in any
     of the callbacks. cbname is the name of the method raising the 
     exception; exctype and value provide Python's exception type and
     exception value as described for example in chapter 8 of the Python 
     Tutorial; traceback is the traceback object.
     
     The method must return one of the values IMMEDIATE_STOP, REGULAR_STOP
     and IGNORE_EXCEPTION. 
     
     Python's error information is not automatically printed to stderr, except
     for uncaught exceptions raised in the error handler itself.

     If an exception is raised in the error_handler, httrack-py uses
     IMMEDIATE_STOP to abort the mirror. 
     
  2. If the callback class does not define the method error_handler,
     httrack-py checks, if the callback instance has an attribute with
     the name 'error_policy'. This attribute must be a mapping. Its keys
     should be the names of the callbacks, or '__default__'; the values
     must be IMMEDIATE_STOP, REGULAR_STOP or IGNORE_EXCEPTION. 
     
     If an exception is raised, and if the mapping has an entry 
     with the name of callback that raised the exception, the value
     of the entry define the error behaviour. Otherwise, the 
     __default__ value is used, if defined.
  
  3. If the callback object has no method called error_handler, and if
     it has either no attribute error_policy or if error_policy is defined
     but has no key '__default__', httrack-py tries to read the environment 
     variable HTTRACK_PY_ERROR_POLICY. If defined, this variable must have 
     the value -1 (IMMEDIATE_STOP), 0 (REGULAR_STOP) or 1(IGNORE_EXCEPTION).
  
  4. otherwise error behaviour REGULAR_STOP is used.
  
  If any of the ways to define the error behaviour result in another
  value than IMMEDIATE_STOP, REGULAR_STOP or IGNORE_EXCEPTION, the value
  IMMEDIATE_STOP is assumed.
  
  In the cases 2, 3 and 4, Python's error information is printed to stderr.
  
  Both the plugin version of httrack-py and the C extension module provide
  definitions for the constants IMMEDIATE_STOP, REGULAR_STOP,
  IGNORE_EXCEPTION. For the plugin version, integer variables are inserted
  into the namespace of the Python module during plugin initialization, 
  so you can use them for example in the constructor of the callback class::
  
    def cbClass:
        def __init__(self):
            self.error_policy = {'__default__': IGNORE_EXCEPTION}
  
  NOTE: Since the plugin library can create the variables only after the 
  Python module has been loaded, you can't use them for code that is executed
  at module initialization time. Hence, the following example will lead
  to an error::
  
    def cbClass:
        error_policy = {'__default__': IGNORE_EXCEPTION}

  The C extension module provides the variables in its namespace::
  
    import httracklib
    
    def cbClass:
        error_policy = {'__default__': httracklib.IGNORE_EXCEPTION}
    

  Most exceptions raised in httrack-py itself are regular Python
  exceptions. Additionally, httrack-py can raise execptions of the
  specific type 'httrack-error':

  - The callbacks preprocess_html and postprocess_html raise this exception,
    if the reallocation of a buffer for HTML data fails.
  
  - The callbacks start and change_options raise this exception, if the
    Python callback method deletes an entry of the options dictionary,
    or if an entry is set to a value of an invalid type.

  Python exceptions during plugin initialization
  
    If an error occurs during plugin initialization, the mirror process
    is aborted in the start callback, except if all of the following
    conditions are met:
    
      1) the plugin is the default httrack plugin and if it is loaded as
         the default plugin, i.e., without specifying a --wrapper option

      2) the environment variable HTTRACK_PYTHON is not set
      
      3) an ImportError is raised.
    
    This _can_ mean that the user simply does not need a Python callback
    module for this mirror, hence the plugin simply prints the Python
    error message, but continues the mirror.
    
    For all other exceptions (e.g, a syntax error in the Python source code),
    or if HTTRACK_PYTHON is set, or if a --wrapper option is specified,
    the mirror is aborted.
