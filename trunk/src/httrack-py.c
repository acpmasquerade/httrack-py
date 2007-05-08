/*
    "Python glue layer" for HTTrack callbacks. Allows to use a Python
    class for the callback. See httrack.py for a simple iexample

    (c) 2004 Abel Deuring <adeuring@gmx.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.

    How to use:
    - compile this file as a module (httrack-py.so or httrack-py.dll)
      example:
      (with gcc)
      gcc -O -g3 -Wall -D_REENTRANT -DINET6 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -i python2.3 -shared -o callback.so callbacks-example.c
      (you'll probably have to add -I /path/to/python_header

    - use the --wrapper option in httrack:

      httrack --wrapper init=httrack-py.so:hts_py_init 


xxx (possible) Problems:
1. Python needs proper "thread separation". Hence, if the callbacks
   are called by different httrack threads, they need to manage Python's
   Global Interpreter Lock.
2. add a test, if the pCallbackClass object is a class instance
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* #include <pthread.h> */

#include "httrack-library.h"
/* needed for lien_back definition: */
#include "htscore.h"
#ifdef HTS_INTERNAL_BYTECODE
  #include "htsbauth.h"
#endif
#include <Python.h>

/* "External" */
#ifdef _WIN32
#define EXTERNAL_FUNCTION __declspec(dllexport)
#else
#define EXTERNAL_FUNCTION 
#endif

/* #define DEBUG */

/* xxx replace the fprintf with a Python exception!!!
   at least for the extension class
*/
#if 0
 for proper error handling, we need now every callback. Well, not
 every callback, but at elast those, that can abort a mirror.
 Registering the other callbacks too without any condition has the additional
 benefit that the methods can now be added later to the callback class
#define HOOK(callback, name, py_name) \
  if (PyObject_HasAttrString(pCallbackClass, #py_name)) { \
    PyObject *meth = PyObject_GetAttrString(pCallbackClass, #py_name); \
    if (meth) { \
      if (meth && !PyCallable_Check(meth)) { \
        fprintf(stderr, "httrack-py error: Instance attribute %s is not callable. Not registered\n", #name); \
        res = 0; \
      } \
      else { \
        htswrap_add(#name, callback); \
      } \
      Py_DECREF(meth); \
    } \
    else { \
      PyErr_Print(); \
      res = 0; \
    } \
  }
#else
#define HOOK(callback, name, py_name) htswrap_add(#name, callback);
#endif
  
EXTERNAL_FUNCTION int hts_py_start(httrackp* opt);
EXTERNAL_FUNCTION int hts_py_end(void);
EXTERNAL_FUNCTION int hts_py_change_options(httrackp* opt);
EXTERNAL_FUNCTION void hts_py_pause(char *lockfile);
EXTERNAL_FUNCTION int hts_py_loop(lien_back* back, int back_max,
                                  int back_index,
                                  int lien_tot, int lien_ntot,
                                  int stat_time,
                                  hts_stat_struct* stats);
EXTERNAL_FUNCTION int hts_py_check_html(char* html, int len,
                                        char* url_adresse, char* url_fichier);
EXTERNAL_FUNCTION int hts_py_preprocess_html(char** html, int* len, 
                                        char* url_adresse, char* url_fichier);
EXTERNAL_FUNCTION int hts_py_postprocess_html(char** html, int* len, 
                                        char* url_adresse, char* url_fichier);
EXTERNAL_FUNCTION char* hts_py_query2(char *question);
EXTERNAL_FUNCTION char* hts_py_query3(char *question);
EXTERNAL_FUNCTION void hts_py_save_file(char *file);
EXTERNAL_FUNCTION int hts_py_checklink(char *address, char* fil, int status);
EXTERNAL_FUNCTION int hts_py_link_detected(char *link);
EXTERNAL_FUNCTION int hts_py_link_detected2(char *link, char *start_tag);
EXTERNAL_FUNCTION int hts_py_transfer_status(lien_back *back);
EXTERNAL_FUNCTION int hts_py_save_name(char *adr_complete,
                                       char *fil_complete,
                                       char *referer_adr,
                                       char *referer_fil,
                                       char *save);
EXTERNAL_FUNCTION int hts_py_send_header(char *buf,
                                         char *adr,
                                         char *fil,
                                         char *referer_adr,
                                         char *referer_fil,
                                         htsblk *incoming);
EXTERNAL_FUNCTION int hts_py_receive_header(char *buf,
                                         char *adr,
                                         char *fil,
                                         char *referer_adr,
                                         char *referer_fil,
                                         htsblk *incoming);
static PyObject *pCallbackClass = 0, *pAnswerQuery2 = 0, *pAnswerQuery3 = 0,
                *httrackError;
static int stop_on_next_callback = 0;

#ifdef PLUGIN
  static PyObject *pHttrackModule, *pSysModule;
  static char *default_py_name = "httrack";
#endif

#ifdef PLUGIN
static int is_initialized = 0, abort_in_start_callback = 0;
#endif

#define IMMEDIATE_STOP -1
#define REGULAR_STOP 0
#define IGNORE_EXCEPTION 1

/* return: 1 on success; 0 if an error occured */
#ifdef PLUGIN
static int initialize(int called_from_plugin_init) {
  /* init the Python interpreter
  */
    PyObject *pString, *dict, *syspath, *reg, *v;
    char *modname, *cc = 0;
#else
static int initialize(PyObject* cbInst) {
#endif
  int res = 1;
  stop_on_next_callback = 0;
  #ifdef PLUGIN
    if (is_initialized) {
      return 1;
    }
    is_initialized = 1;
    Py_Initialize();
    abort_in_start_callback = 1;
    /* sys.path contains only the "system library" paths, but not 
       the current directory, which we need
    */
    pString = PyString_FromString("sys");
    if (!pString) {
      PyErr_Print();
      return 0;
    }
    pSysModule = PyImport_Import(pString);
    Py_DECREF(pString);
    if (!pSysModule) {
      PyErr_Print();
      return 0;
    }
    
    dict = PyModule_GetDict(pSysModule);
    syspath = PyDict_GetItemString(dict, "path");
    
    modname = getenv("HTTRACK_PYTHON");
    /* if the module name contains at least one '/', we assume that
       a complete path is specified
    */
    if (modname) {
      cc = rindex(modname, '/');
      if (cc) {
        *cc = 0;
        pString = PyString_FromString(modname);
        *cc = '/';
      }
    }
    if (!cc) {
      /* default path for the httrack module is the current directory */
      pString = PyString_FromString(".");
    }
    if (!pString) {
      PyErr_Print();
      Py_DECREF(pSysModule);
      return 0;
    }

    PyList_Append(syspath, pString);
    Py_DECREF(pString);

    if (!modname) {
      modname = default_py_name;
    }
    else if (cc) {
      modname = cc+1;
    }
    
    /* a possible '.py' suffix should be removed */
    cc = rindex(modname, '.');
    if (cc) {
      if (0 == strcmp(cc, ".py")) {
        *cc = 0;
      }
    }
    pString = PyString_FromString(modname);
    if (!pString) {
      PyErr_Print();
      Py_DECREF(pSysModule);
      return 0;
    }

    pHttrackModule = PyImport_Import(pString);
    Py_DECREF(pString);
    if (!pHttrackModule) {
      int import_error = PyErr_ExceptionMatches(PyExc_ImportError);
      PyErr_Print();
      Py_DECREF(pSysModule);
      /* check the error. If no module is found, we'll get
         'ImportError: No module named httrack'
         This is NOT a reason to abort the mirror, if all of the following
         conditions are met:
         1. this library is httrack's default plugin
         2. the environment variable HTTRACK_PYTHON is not set
         3. this function is called from plugin_init

         All other conditions and all other errors should cause an abortion
         of the mirror. 
      */
      if (   import_error
          && !getenv("HTTRACK_PYTHON")
          && called_from_plugin_init) {
        abort_in_start_callback = 0;
      }
      return 0;
    }
    
    /* now get the Python "callback class" instance */
    dict = PyModule_GetDict(pHttrackModule);
    v = PyInt_FromLong(IMMEDIATE_STOP);
    PyDict_SetItemString(dict, "IMMEDIATE_STOP", v);
    Py_DECREF(v);
    
    v = PyInt_FromLong(REGULAR_STOP);
    PyDict_SetItemString(dict, "REGULAR_STOP", v);
    Py_DECREF(v);
    
    v = PyInt_FromLong(IGNORE_EXCEPTION);
    PyDict_SetItemString(dict, "IGNORE_EXCEPTION", v);
    Py_DECREF(v);
    fprintf(stderr, "initialzes xxxx\n");

    reg = PyDict_GetItemString(dict, "register");
    if (!reg) {
      fprintf(stderr, 
        "htrack-py error: Python module %s does not have a function\n"
        "named register\n", modname);
        Py_DECREF(pSysModule);
        Py_DECREF(pHttrackModule);
        return 0;
    }
    if (!PyCallable_Check(reg)) {
      fprintf(stderr, 
        "httrack-py error: %s.register is not callable\n", modname);
      Py_DECREF(pSysModule);
      Py_DECREF(pHttrackModule);
      return 0;
    }
    pCallbackClass = PyObject_CallObject(reg, 0);

    if (!pCallbackClass) {
      PyErr_Print();
      Py_DECREF(pSysModule);
      Py_DECREF(pHttrackModule);
      return 0;
    }
    htswrap_add("end", hts_py_end);

  #else
    pCallbackClass = cbInst;
    Py_INCREF(pCallbackClass);
    HOOK(hts_py_start, start, start);
  #endif
  httrackError = PyErr_NewException("httrack.error", 0, 0);
  Py_INCREF(httrackError);
  // xxx set pCallbackClass from function param, if this is an exenstion class

  HOOK(hts_py_end, end, end);
  HOOK(hts_py_change_options, change-options, change_options);
  HOOK(hts_py_check_html, check-html, check_html);
  HOOK(hts_py_preprocess_html, preprocess-html, preprocess_html);
  HOOK(hts_py_postprocess_html, postprocess-html, postprocess_html);
  /* xxx not yet implemented HOOK(hts_py_query, query, query); */
  HOOK(hts_py_query2, query2, query2);
  HOOK(hts_py_query3, query3, query3);
  HOOK(hts_py_loop, loop, loop);
  HOOK(hts_py_checklink, check-link, check_link);
  HOOK(hts_py_pause, pause, pause)
  HOOK(hts_py_save_file, save-file, save_file);
  HOOK(hts_py_link_detected, link-detected, link_detected);
  HOOK(hts_py_link_detected2, link-detected2, link_detected2);
  HOOK(hts_py_transfer_status, transfer-status, transfer_status);
  HOOK(hts_py_save_name, save-name, save_name);
  HOOK(hts_py_send_header, send-header, send_header);
  HOOK(hts_py_receive_header, receive-header, receive_header);

  #ifdef PLUGIN
    Py_DECREF(pSysModule);
    Py_DECREF(pHttrackModule);
    abort_in_start_callback = 0;
  #endif

  return res;
}

#ifdef PLUGIN
void plugin_init() {
  int res;
  res = initialize(1);
  /* we need the start callback in any case, because plugin_init
     can't bort the mirror
  */
  htswrap_add("start", hts_py_start);
}
#endif

/* error handler. This handler is called, either if a callback
   class method raises an exception, or if an error occurs in this
   library. 
   
   If the callback class defines a method error_handler
   (def error_handler(self, callback_name, error_object, error_val)), 
   delegate the decision to continue or abort to this method. 
   
   Otherwise:
   
   call PyErr_Print() to provide some information to the user.
 
   If the callback class has an attribute error_policy (must be a 
   dictionary), decide what to do according to that policy
   
   Otherwise:
   check the value of the environment variable HTTRACK_PY_NO_STOP_ON_ERROR. 
   If it is defined, continue the mirror process, else abort.
   
   (Many program check first for an environment variable, and then look
   for some other definitions, e.g. in a config file. If you prefer
   this behaviour, check for an environment variable in the error
   handler of the cllback class.)
   
   Some callbacks do not allow to abort the mirror. If an error occured
   in such a callback, the error handler of the callback class
   can be used to record the occurence of the error in an attribute,
   and the next callback allowing to abort the mirror can decide, what
   to do. This library does not attempt to abort the mirror later.
   
*/

static int process_error(char *cbname) {
  PyObject *pType, *pValue, *pTraceback, *meth, *args, *pCbname, *pRes, *dict;
  int res;
  char *cc, *cc1;

  /* save the error data first; it will be cleared by the 
     PyObject_HasAttrString call below
  */
  PyErr_Fetch(&pType, &pValue, &pTraceback);
  if (pCallbackClass && PyObject_HasAttrString(pCallbackClass, "error_handler")) {
    meth = PyObject_GetAttrString(pCallbackClass, "error_handler");
    if (meth) {
      pCbname = PyString_FromString(cbname);
      if (pCbname) {
        args = PyTuple_New(4);
        if (args) {
          PyTuple_SetItem(args, 0, pCbname);
          PyTuple_SetItem(args, 1, pType);
          if (pValue) {
            PyTuple_SetItem(args, 2, pValue);
          }
          else {
            PyTuple_SetItem(args, 2, Py_None);
            Py_INCREF(Py_None);
          }
          if (pTraceback) {
            PyTuple_SetItem(args, 3, pTraceback);
          }
          else {
            PyTuple_SetItem(args, 3, Py_None);
            Py_INCREF(Py_None);
          }
          pRes = PyObject_CallObject(meth, args);
          if (pRes) {
            if (!PyInt_Check(pRes)) {
              /* consider this a serious error -- an error handler
                 should not produce its own error
              */
              return IMMEDIATE_STOP;
            }
            res = PyInt_AsLong(pRes);
            if (res < IMMEDIATE_STOP || res > IGNORE_EXCEPTION)
              return IMMEDIATE_STOP;
            return res;
          }
        }
      }
    }
    /* if we arrive here, an error occured during error handling.
       Let's stop immediately
    */
    PyErr_Print();
    fprintf(stderr, "this error occurred during error handling. trying to abort\n");
    return IMMEDIATE_STOP;
  }

  PyErr_Restore(pType, pValue, pTraceback);
  PyErr_Print();
  res = REGULAR_STOP;
  cc = getenv("HTTRACK_PY_ERROR_POLICY");
  if (cc) {
    res = strtol(cc, &cc1, 10);
    if (cc == cc1 || -1 > res || res > 1 ) {
      fprintf(stderr, "invalid value for HTTRACK_PY_ERROR_POLICY.\n");
      res = IMMEDIATE_STOP;
    }
  }
  
  /* try to use the error_policy attribute in the callback class */
  if (pCallbackClass && PyObject_HasAttrString(pCallbackClass, "error_policy")) {
    dict = PyObject_GetAttrString(pCallbackClass, "error_policy");
    if (dict) {
      if (!PyMapping_Check(dict)) {
        fprintf(stderr, "error_policy attribute must be a mapping object\n");
        return IMMEDIATE_STOP;
      }
      if (PyMapping_HasKeyString(dict, "__default__")) {
        pRes = PyMapping_GetItemString(dict, "__default__");
        if (pRes) {
          res = PyInt_AsLong(pRes);
          if (-1 > res || res > 1) {
            fprintf(stderr, "invalid value for error_policy['__default__']. ");
            res = IMMEDIATE_STOP;
          }
        }
        else {
          PyErr_Print();
          fprintf(stderr, "this error occurred during error handling. Will probably abort\n");
          res = IMMEDIATE_STOP;
        }
      }
      if (PyMapping_HasKeyString(dict, cbname)) {
        pRes = PyMapping_GetItemString(dict, cbname);
        if (pRes) {
          res = PyInt_AsLong(pRes);
          if (-1 > res || res > 1) {
            fprintf(stderr, "invalid value for error_policy['%s']. ", cbname);
            res = IMMEDIATE_STOP;
          }
        }
        else {
          PyErr_Print();
          fprintf(stderr, "this error occurred during error handling. Will probably abort\n");
          res = IMMEDIATE_STOP;
        }
      }
      return res;
    }
    /* if we arrive here, an error occured during error handling.
       Let's stop immediately
    */
    PyErr_Print();
    fprintf(stderr, "this error occurrd during error handling. Trying to abort\n");
    return IMMEDIATE_STOP;
  }
  return res;
}

/* for callbacks that return 0 for "stop mirror" and 1 for "continue"
*/
static int process_error_direct(char *cbname) {
  int res = process_error(cbname);
  if (res == IMMEDIATE_STOP) 
    return REGULAR_STOP;
  return res;
}

/* for callbacks  that have other return value
*/
static void process_error_indirect(char *cbname) {
  int res = process_error(cbname);
  if (res == IMMEDIATE_STOP) 
    exit(-1);
  if (res == REGULAR_STOP)
    stop_on_next_callback = 1;
}

// xxxxxxxxxxxxxx change error behaviour: raise an exception,
// if the variable type is wrong

#define setAnyItem(pydict, field)   if (!tmp) return 0; \
                        if (PyDict_SetItemString(pydict, #field, tmp) == -1) { \
                          Py_DECREF(tmp); \
                          return 0; \
                        } \
                        Py_DECREF(tmp); 

#define setIntItem(pydict, cstruct, field) { PyObject *tmp; \
                        tmp = PyInt_FromLong(cstruct->field); \
                        setAnyItem(pydict, field) \
                      }

#define setIntItem2(pydict, intname, field) { PyObject *tmp; \
                        tmp = PyInt_FromLong(intname); \
                        setAnyItem(pydict, field) \
                      }

/* xxx LLint may be defined as 'long long int', and for this
   type we don't have a proper conversion function in the Python/C API
   
   We ignore this problem, and use simply PyLong_FromLong
*/
#define setLLIntItem(pydict, cstruct, field) { PyObject *tmp; \
                          tmp = PyLong_FromLong(cstruct->field); \
                          setAnyItem(pydict, field) \
                        }

#define setFloatItem(pydict, cstruct, field) { PyObject *tmp; \
                          tmp = PyFloat_FromDouble(cstruct->field); \
                          setAnyItem(pydict, field) \
                        }

#define setStringItem(pydict, cstruct, field) { PyObject *tmp; \
                           tmp = PyString_FromString(cstruct->field); \
                           setAnyItem(pydict, field) \
                         }

#define getIntItem(pydict, cstruct, field) { PyObject *tmp; \
                        tmp = PyDict_GetItemString(pydict, #field); \
                        if (tmp && PyInt_Check(tmp)) { \
                          cstruct->field = PyInt_AsLong(tmp); \
                        } \
                        else { \
                          if (tmp) { \
                            PyErr_SetString(httrackError, \
                              "wrong type for option dict entry " #field); \
                          } \
                          else { \
                            PyErr_SetString(httrackError, \
                              "value for key " #field " is missing"); \
                          } \
                        } \
                      }

#define getLLIntItem(pydict, cstruct, field) { PyObject *tmp; \
                          tmp = PyDict_GetItemString(pydict, #field); \
                          if (tmp && PyLong_Check(tmp)) { \
                            cstruct->field = PyLong_AsLong(tmp); \
                          } \
                          else { \
                            if (tmp) { \
                              PyErr_SetString(httrackError, \
                                "wrong type for option dict entry " #field); \
                            } \
                            else { \
                              PyErr_SetString(httrackError, \
                                "value for key " #field " is missing"); \
                            } \
                          } \
                        } 

#define getFloatItem(pydict, cstruct, field) { PyObject *tmp; \
                          tmp = PyDict_GetItemString(pydict, #field); \
                          if (tmp && PyFloat_Check(tmp)) { \
                            cstruct->field = PyFloat_AsDouble(tmp); \
                          } \
                          else { \
                            if (tmp) { \
                              PyErr_SetString(httrackError, \
                                "wrong type for option dict entry " #field); \
                            } \
                            else { \
                              PyErr_SetString(httrackError, \
                                "value for key " #field " is missing"); \
                            } \
                          } \
                        }

#define getStringItem(pydict, cstruct, field, maxsize) { PyObject *tmp; \
                      tmp = PyDict_GetItemString(pydict, #field); \
                      if (tmp && PyString_Check(tmp)) { \
                        int size, psize = PyString_Size(tmp); \
                        size = (psize < maxsize) ? psize : maxsize; \
                        strncpy(cstruct->field, PyString_AsString(tmp), size); \
                      } \
                          else { \
                            if (tmp) { \
                              PyErr_SetString(httrackError, \
                                "wrong type for option dict entry " #field); \
                            } \
                            else { \
                              PyErr_SetString(httrackError, \
                                "value for key " #field " is missing"); \
                            } \
                          } \
                    }

#ifdef HTS_INTERNAL_BYTECODE
static PyObject *set_cookies(t_cookie *cookies) {
  /* return: (cookies, auth_data) or a null pointer, if an error occurs
     where: cookies is a list of singlecookies,
              and each cookie is a list of strings
            auth_data is a list of 2-tuples, containing the "prefix" and user:ass
            (see htsbauth.[ch] for details
  */
  PyObject *res, *pAllCookies, *pAuth, *pCookie, *s, *s2, *t;
  char *c1, *c2;
  bauth_chain *auth;
  int i;
  
  pAllCookies = PyList_New(0);
  if (!pAllCookies) {
    return 0;
  }
  
  pAuth = PyList_New(0);
  if (!pAuth) {
    Py_DECREF(pAllCookies);
    return 0;
  }

  res = PyTuple_New(2);
  if (!res) {
    Py_DECREF(pAllCookies);
    Py_DECREF(pAuth);
    return 0;
  }
  
  PyTuple_SetItem(res, 0, pAllCookies);
  PyTuple_SetItem(res, 1, pAuth);
  
  c1 = cookies->data;
  while (*c1) {
    i = 0;
    pCookie = PyList_New(0);
    if (!pCookie) {
      Py_DECREF(res);
      return 0;
    }
    if (PyList_Append(pAllCookies, pCookie)) {
      Py_DECREF(res);
      Py_DECREF(pCookie);
      return 0;
    }
    Py_DECREF(pCookie);

    while (strcmp(c2=cookie_get(c1, i++), "")) {
      s = PyString_FromString(c2);
      if (!s) {
        Py_DECREF(res);
        return 0;
      }
      if (PyList_Append(pCookie, s)) {
        Py_DECREF(res);
        Py_DECREF(s);
        return 0;
      }
      Py_DECREF(s);
    }
    
    c1 = cookie_nextfield(c1);
  }
  
  auth = &cookies->auth;
  while (auth) {
    s = PyString_FromString(auth->prefix);
    if (!s) {
      Py_DECREF(res);
      return 0;
    }
    
    s2 = PyString_FromString(auth->auth);
    if (!s2) {
      Py_DECREF(res);
      Py_DECREF(s);
      return 0;
    }
    
    t = PyTuple_New(2);
    if (!t) {
      Py_DECREF(res);
      Py_DECREF(s);
      Py_DECREF(s2);
      return 0;
    }
    
    PyTuple_SetItem(t, 0, s);
    PyTuple_SetItem(t, 1, s2);
    
    if (PyList_Append(pAuth, t)) {
      Py_DECREF(res);
      Py_DECREF(t);
      return 0;
    }
    auth = auth->next;
  }
  
  return res;
}
#endif

static int set_option_dict(httrackp* opt, PyObject* dict) {
  setIntItem(dict, opt, wizard);
  setIntItem(dict, opt, flush);
  setIntItem(dict, opt, travel);
  setIntItem(dict, opt, seeker);
  setIntItem(dict, opt, depth);
  setIntItem(dict, opt, extdepth);
  setIntItem(dict, opt, urlmode);
  setIntItem(dict, opt, debug);
  setIntItem(dict, opt, getmode);
  /* xxx FILE *log, *errlog are missing. Not sure, if and how to map these 
     to Python types
  */
  setLLIntItem(dict, opt, maxsite);
  setLLIntItem(dict, opt, maxfile_nonhtml);
  setLLIntItem(dict, opt, maxfile_html);
  setIntItem(dict, opt, maxsoc);
  setLLIntItem(dict, opt, fragment);
  setIntItem(dict, opt, nearlink);
  setIntItem(dict, opt, makeindex);
  setIntItem(dict, opt, kindex);
  setIntItem(dict, opt, delete_old);
  setIntItem(dict, opt, timeout);
  setIntItem(dict, opt, rateout);
  setIntItem(dict, opt, maxtime);
  setIntItem(dict, opt, maxrate);
  setFloatItem(dict, opt, maxconn);
  setIntItem(dict, opt, waittime);
  setIntItem(dict, opt, cache);
  setIntItem(dict, opt, shell);
  setIntItem(dict, opt, savename_83);
  setStringItem(dict, opt, savename_userdef);
  setIntItem(dict, opt, mimehtml);
  setIntItem(dict, opt, user_agent_send);
  setStringItem(dict, opt, user_agent);
  setStringItem(dict, opt, referer);
  setStringItem(dict, opt, from);
  setStringItem(dict, opt, path_log);
  setStringItem(dict, opt, path_html);
  setStringItem(dict, opt, path_bin);
  setIntItem(dict, opt, retry);
  setIntItem(dict, opt, makestat);
  setIntItem(dict, opt, maketrack);
  setIntItem(dict, opt, parsejava);
  setIntItem(dict, opt, hostcontrol);
  setIntItem(dict, opt, errpage);
  setIntItem(dict, opt, check_type);
  setIntItem(dict, opt, all_in_cache);
  setIntItem(dict, opt, robots);
  setIntItem(dict, opt, external);
  setIntItem(dict, opt, passprivacy);
  setIntItem(dict, opt, includequery);
  setIntItem(dict, opt, mirror_first_page);
  setStringItem(dict, opt, sys_com);
  setIntItem(dict, opt, sys_com_exec);
  setIntItem(dict, opt, accept_cookie);

#ifdef HTS_INTERNAL_BYTECODE
  if (opt->cookie) {
    PyObject *tmp = set_cookies(opt->cookie);
    if (!tmp) return 0;
    setAnyItem(dict, cookie);
  }
#endif

  setIntItem(dict, opt, http10);
  setIntItem(dict, opt, nokeepalive);
  setIntItem(dict, opt, nocompression);
  setIntItem(dict, opt, sizehack);
  setIntItem(dict, opt, urlhack);
  setIntItem(dict, opt, tolerant);
  setIntItem(dict, opt, parseall);
  setIntItem(dict, opt, parsedebug);
  setIntItem(dict, opt, norecatch);
  setIntItem(dict, opt, verbosedisplay);
  setStringItem(dict, opt, footer);
  setIntItem(dict, opt, maxcache);
  setIntItem(dict, opt, ftp_proxy);
  setStringItem(dict, opt, filelist);
  setStringItem(dict, opt, urllist);
  /* xxx htsfilters filters, void *hash, void *robotsptr missing 
  */
  setStringItem(dict, opt, lang_iso);
  setStringItem(dict, opt, mimedefs);
  setIntItem(dict, opt, maxlink);
  setIntItem(dict, opt, maxfilter);
  setStringItem(dict, opt, exec);
  setIntItem(dict, opt, quiet);
  setIntItem(dict, opt, keyboard);
  setIntItem(dict, opt, is_update);
  setIntItem(dict, opt, dir_topindex);
  /* xxx htsoptstate stats missing */
  return 1;
}

static int get_option_dict(httrackp* opt, PyObject* dict) {
  getIntItem(dict, opt, wizard);
  getIntItem(dict, opt, flush);
  getIntItem(dict, opt, travel);
  getIntItem(dict, opt, seeker);
  getIntItem(dict, opt, depth);
  getIntItem(dict, opt, extdepth);
  getIntItem(dict, opt, urlmode);
  getIntItem(dict, opt, debug);
  getIntItem(dict, opt, getmode);
  /* xxx FILE* are missing. Not sure, if and how to map these 
     to Python types
  */
  getLLIntItem(dict, opt, maxsite);
  getLLIntItem(dict, opt, maxfile_nonhtml);
  getLLIntItem(dict, opt, maxfile_html);
  getIntItem(dict, opt, maxsoc);
  getLLIntItem(dict, opt, fragment);
  getIntItem(dict, opt, nearlink);
  getIntItem(dict, opt, makeindex);
  getIntItem(dict, opt, kindex);
  getIntItem(dict, opt, delete_old);
  getIntItem(dict, opt, timeout);
  getIntItem(dict, opt, rateout);
  getIntItem(dict, opt, maxtime);
  getIntItem(dict, opt, maxrate);
  getFloatItem(dict, opt, maxconn);
  getIntItem(dict, opt, waittime);
  getIntItem(dict, opt, cache);
  getIntItem(dict, opt, shell);
  getIntItem(dict, opt, savename_83);
  getStringItem(dict, opt, savename_userdef, 256);
  getIntItem(dict, opt, mimehtml);
  getIntItem(dict, opt, user_agent_send);
  getStringItem(dict, opt, user_agent, 128);
  getStringItem(dict, opt, referer, 256);
  getStringItem(dict, opt, from, 256);
  getStringItem(dict, opt, path_log, 1024);
  getStringItem(dict, opt, path_html, 1024);
  getStringItem(dict, opt, path_bin, 1024);
  getIntItem(dict, opt, retry);
  getIntItem(dict, opt, makestat);
  getIntItem(dict, opt, maketrack);
  getIntItem(dict, opt, parsejava);
  getIntItem(dict, opt, hostcontrol);
  getIntItem(dict, opt, errpage);
  getIntItem(dict, opt, check_type);
  getIntItem(dict, opt, all_in_cache);
  getIntItem(dict, opt, robots);
  getIntItem(dict, opt, external);
  getIntItem(dict, opt, passprivacy);
  getIntItem(dict, opt, includequery);
  getIntItem(dict, opt, mirror_first_page);
  getStringItem(dict, opt, sys_com, 2048);
  getIntItem(dict, opt, sys_com_exec);
  getIntItem(dict, opt, accept_cookie);
  /* xxx  t_cookie is missing */
  getIntItem(dict, opt, http10);
  getIntItem(dict, opt, nokeepalive);
  getIntItem(dict, opt, nocompression);
  getIntItem(dict, opt, sizehack);
  getIntItem(dict, opt, urlhack);
  getIntItem(dict, opt, tolerant);
  getIntItem(dict, opt, parseall);
  getIntItem(dict, opt, parsedebug);
  getIntItem(dict, opt, norecatch);
  getIntItem(dict, opt, verbosedisplay);
  getStringItem(dict, opt, footer, 256);
  getIntItem(dict, opt, maxcache);
  getIntItem(dict, opt, ftp_proxy);
  getStringItem(dict, opt, filelist, 1024);
  getStringItem(dict, opt, urllist, 1024);
  /* xxx htsfilters, hash, robotsptr missing 
  */
  getStringItem(dict, opt, lang_iso, 64);
  getStringItem(dict, opt, mimedefs, 2048);
  getIntItem(dict, opt, maxlink);
  getIntItem(dict, opt, maxfilter);
  /* xxx hrmm. No size defined for char* exec
     getStringItem(dict, opt, exec);
  */
  getIntItem(dict, opt, quiet);
  getIntItem(dict, opt, keyboard);
  getIntItem(dict, opt, is_update);
  getIntItem(dict, opt, dir_topindex);
  /* xxx htsoptstate stats missing */
  return 1;
}
static int process_options(httrackp* opt, char* method) {
  PyObject *meth, *args, *dict, *pres;
  int res = 0;

  if (stop_on_next_callback)
    return 0;
  if (pCallbackClass && PyObject_HasAttrString(pCallbackClass, method)) {
    meth = PyObject_GetAttrString(pCallbackClass, method);
    if (meth) {
      dict = PyDict_New();
      if (!dict) {
        Py_DECREF(meth);
        return process_error_direct(method);
      }
      if (   !set_option_dict(opt, dict) 
          || !(args = PyTuple_New(1))) {
        Py_DECREF(meth);
        Py_DECREF(dict);
        return process_error_direct(method);
      }
      PyTuple_SetItem(args, 0, dict);
      pres = PyObject_CallObject(meth, args);
      if (pres) {
        res = PyObject_IsTrue(pres);
        if (res) {
          get_option_dict(opt, dict);
        }
        Py_DECREF(pres);
      }
      else {
        return process_error_direct(method);
      }
      Py_DECREF(dict);
      Py_DECREF(args);
      Py_DECREF(meth);
    }
  }
  else
    res = 1;
  return res;
}


EXTERNAL_FUNCTION int hts_py_start(httrackp* opt) {
  /* call the method 'start' of the Python class; pass (almost) all
     option values in a dictionary
  */
  
  /* "delayed error signal" from plugin_init set?
  */
  int res = 1;
#ifdef DEBUG
  fprintf(stderr, "hts_py_start %li\n", pthread_self());
#endif
#ifdef PLUGIN
  if (abort_in_start_callback) return 0;
  res = initialize(0);
#endif
  res = res && process_options(opt, "start");
  return res;
}

static void cleanup() {
  if (pAnswerQuery2) { Py_DECREF(pAnswerQuery2); }
  if (pAnswerQuery3) { Py_DECREF(pAnswerQuery3); }
  
  /* explicitly delete the callback class instance in order to
    allow a possible class destructor to be executed 
  */
  if (pCallbackClass) {
    Py_DECREF(pCallbackClass);
  }

  pCallbackClass = 0;
}

EXTERNAL_FUNCTION int hts_py_end(void) {
  PyObject *meth, *pRes;
#ifdef DEBUG
  fprintf(stderr, "hts_py_end %li\n", pthread_self());
#endif
  if (stop_on_next_callback)
    return 0;
  if (pCallbackClass && PyObject_HasAttrString(pCallbackClass, "end")) {
    meth = PyObject_GetAttrString(pCallbackClass, "end");
    if (meth) {
      pRes =  PyObject_CallObject(meth, 0);
      if (!pRes) {
        /* this is the last of all callbacks, and we can't return
           anything, so let's just see, what the user wants to do
        */
        process_error_direct("end");
      }
      else {
        Py_DECREF(pRes);
      }
      Py_DECREF(meth);
    }
  }
  
#ifdef PLUGIN
  cleanup();
  Py_Finalize();
#endif
  return 1;
}

EXTERNAL_FUNCTION int hts_py_change_options(httrackp* opt) {
#ifdef DEBUG
  fprintf(stderr, "hts_py_change_options %li\n", pthread_self());
#endif
  return process_options(opt, "change_options");
}


static PyObject* process_html(char* html, int len, 
                                   char* url_adresse, char* url_fichier,
                                   char *method) {
  /* allow to change the HTML text
     Python method:
        instance.check_html(html, url_adresse, url_fichier)
        
        If a string is returned, the value is copied into *html (up to len
        bytes); if less bytes are copied, the remaining space up to len
        is filled with 0x20. the return value of this function will be 1.
        
        If some other Python object is returned, its "boolean value" will
        be returned by this function
        
        Python error handling is done in the caller
  */
  PyObject *meth, *pHtml, *pURL_adresse, *pURL_fichier, *pArgs, *pRes;
  
  if (pCallbackClass && PyObject_HasAttrString(pCallbackClass, method)) {
    meth = PyObject_GetAttrString(pCallbackClass, method);
    if (meth) {
      pHtml = PyString_FromStringAndSize(html, len);
      if (!pHtml) {
        Py_DECREF(meth);
        return 0;
      }
      
      pURL_adresse = PyString_FromString(url_adresse);
      if (!pURL_adresse) {
        Py_DECREF(meth);
        Py_DECREF(pHtml);
        return 0;
      }
      
      pURL_fichier = PyString_FromString(url_fichier);
      if (!pURL_fichier) {
        Py_DECREF(meth);
        Py_DECREF(pHtml);
        Py_DECREF(pURL_adresse);
        return 0;
      }
      
      pArgs = PyTuple_New(3);
      if (!pArgs) {
        Py_DECREF(meth);
        Py_DECREF(pHtml);
        Py_DECREF(pURL_adresse);
        Py_DECREF(pURL_fichier);
        return 0;
      }
      
      PyTuple_SetItem(pArgs, 0, pHtml);
      PyTuple_SetItem(pArgs, 1, pURL_adresse);
      PyTuple_SetItem(pArgs, 2, pURL_fichier);
      
      pRes = PyObject_CallObject(meth, pArgs);
      Py_DECREF(meth);
      Py_DECREF(pArgs);
      return pRes;
    }
  }
  /* no callback class or no appropriate method defined: continue */
  pRes = PyInt_FromLong(1);
  return pRes;
}

EXTERNAL_FUNCTION int hts_py_check_html(char* html, int len, 
                                        char* url_adresse, char* url_fichier) {
  PyObject * pRes;
  int res;
#ifdef DEBUG
  fprintf(stderr, "hts_py_check_html %li\n", pthread_self());
#endif
  
  pRes = process_html(html, len, url_adresse, url_fichier, "check_html");
  if (pRes) {
    res = PyObject_IsTrue(pRes);
    Py_DECREF(pRes);
    return res;
  }
  else {
    process_error_indirect("check_html");
    /* return value options 
       0 -> page will not be processed by httrack. If this happens e.g.
            in the start page, not a single file will be saved, and this
            might not be intended.
       1 -> page be processed.
       
       Returning 1 seems to be the better option: If the user wants to
       abort the mirror, s/he can set the desired value in error_policy
       of the Python error handler, and the mirror will be aborted soon
    */
    return 1;
  }
}

static int can_change_html(char** html, int* len, 
                                        char* url_adresse, char* url_fichier,
                                        char *method) {
  PyObject * pRes;
  
  pRes = process_html(*html, *len, url_adresse, url_fichier, method);
  if (pRes) {
    if (PyString_Check(pRes)) {
      int plen = PyString_Size(pRes);
      if (plen > *len) {
        *html = realloc(*html, plen+1);
        if (!(*html)) {
          PyErr_SetString(httrackError, "can't realloc buffer for HTML text\n");
          process_error_indirect(method);
          return 0;
        }
      }
      
      memcpy(*html, PyString_AsString(pRes), plen);
      html[plen] = 0;
      *len = plen;
    }
    Py_DECREF(pRes);
  }
  else {
    process_error_indirect(method);
  }
  return 1;
}

EXTERNAL_FUNCTION int hts_py_preprocess_html(char** html, int* len, 
                                        char* url_adresse, char* url_fichier) {
#ifdef DEBUG
  fprintf(stderr, "hts_py_preprocess_html %li\n", pthread_self());
#endif
  return can_change_html(html, len, url_adresse, url_fichier, "preprocess_html");
}

EXTERNAL_FUNCTION int hts_py_postprocess_html(char** html, int* len, 
                                        char* url_adresse, char* url_fichier) {
#ifdef DEBUG
  fprintf(stderr, "hts_py_postprocess_html %li\n", pthread_self());
#endif
  return can_change_html(html, len, url_adresse, url_fichier, "postprocess_html");
}



#if 0
xxx is this function anywhere used??
EXTERNAL_FUNCTION char* hts_py_query(char *question) {
  return question;
}
#endif


/* the functions query2 and query3 must return a string, and this module
   must implement all memory management for the strings. Since the result
   of these functions is only read, not modified, we simply return
   PyString_AsString of a "static" Python string, which should be
   returned by the Python method. We laeve the final "cleanup" to
   py_Finalize()
*/

static char* query(char *question, char *method, char *default_answer,
                   PyObject **pAnswer) {
  PyObject *pQuestion, *meth, *pArgs, *pRes;
  
  if (pCallbackClass && PyObject_HasAttrString(pCallbackClass, method)) {
    meth = PyObject_GetAttrString(pCallbackClass, method);
    if (meth) {
      pQuestion = PyString_FromString(question);
      if (!pQuestion) {
        process_error_indirect(method);
        return default_answer;
      }
      pArgs = PyTuple_New(1);
      if (!pArgs) {
        Py_DECREF(pQuestion);
        process_error_indirect(method);
        return default_answer;
      }
      
      PyTuple_SetItem(pArgs, 0, pQuestion);
      
      pRes = PyObject_CallObject(meth, pArgs);
      Py_DECREF(pArgs);
      Py_DECREF(meth);
      
      if (!pRes) {
        process_error_indirect(method);
        return default_answer;
      }
      if (PyString_Check(pRes)) {
        if (*pAnswer) {
          Py_DECREF(*pAnswer);
        }
        *pAnswer = pRes;
        return PyString_AsString(pRes);
      }
      else {
        Py_DECREF(pRes);
        return default_answer;
      }
    }
  }
  return default_answer;
}

static char *default_answer_query2 = "y";
EXTERNAL_FUNCTION char* hts_py_query2(char *question) {
#ifdef DEBUG
  fprintf(stderr, "hts_py_query2 %li\n", pthread_self());
#endif
  return query(question, "query2", default_answer_query2, &pAnswerQuery2);
}

static char *default_answer_query3 = "*";
EXTERNAL_FUNCTION char* hts_py_query3(char *question) {
#ifdef DEBUG
  fprintf(stderr, "hts_py_query3 %li\n", pthread_self());
#endif
  return query(question, "query3", default_answer_query3, &pAnswerQuery3);
}


static PyObject *setup_htsblk(PyObject *dict, htsblk* blk) {
  setIntItem(dict, blk, statuscode);
  setIntItem(dict, blk, notmodified);
  setIntItem(dict, blk, is_chunk);
  setIntItem(dict, blk, compressed);
  setIntItem(dict, blk, empty);
  setIntItem(dict, blk, keep_alive);
  setIntItem(dict, blk, keep_alive_trailers);
  setIntItem(dict, blk, keep_alive_t);
  setIntItem(dict, blk, keep_alive_max);
  if (blk->adr) {
    setStringItem(dict, blk, adr);
  }
  if (blk->headers) {
    setStringItem(dict, blk, headers);
  }
  setLLIntItem(dict, blk, size);
  setStringItem(dict, blk, msg);
  setStringItem(dict, blk, contenttype);
  setStringItem(dict, blk, charset);
  setStringItem(dict, blk, contentencoding);
  if (blk->location) {
    setStringItem(dict, blk, location);
  }
  setLLIntItem(dict, blk, totalsize);;
  setIntItem(dict, blk, is_file);
#if HTS_USEOPENSSL
  setIntItem(dict, blk, ssl);
#endif
  setStringItem(dict, blk, lastmodified);
  setStringItem(dict, blk, etag);
  setStringItem(dict, blk, cdispo);
  setLLIntItem(dict, blk, crange);
  /* xxx htsrequest req missing */
  return dict;
}

static PyObject *setup_lien_back(PyObject *dict, lien_back* back) {
  int itmp;
  PyObject *pHtsblk;
  
  /* can happen, that back is null */
  if (!back) return dict;
  setStringItem(dict, back, url_adr);
  setStringItem(dict, back, url_fil);
  setStringItem(dict, back, url_sav);
  setStringItem(dict, back, referer_adr);
  setStringItem(dict, back, location_buffer);
  if (back->tmpfile) {
    setStringItem(dict, back, tmpfile);
  }
  setStringItem(dict, back, tmpfile_buffer);
  setIntItem(dict, back, status);
  setIntItem(dict, back, testmode);
  setIntItem(dict, back, timeout);
  setLLIntItem(dict, back, timeout_refresh);
  setIntItem(dict, back, rateout);
  setLLIntItem(dict, back, rateout_time);
  setLLIntItem(dict, back, maxfile_nonhtml);
  setLLIntItem(dict, back, maxfile_html);
  
  pHtsblk = PyDict_New();
  if (!pHtsblk) {
    return 0;
  }
  PyDict_SetItemString(dict, "r", pHtsblk);
  Py_DECREF(pHtsblk);
  if (!setup_htsblk(pHtsblk, &back->r)) {
    return 0;
  }
  setIntItem(dict, back, is_update);
  setIntItem(dict, back, head_request);
  setLLIntItem(dict, back, range_req_size);
  setLLIntItem(dict, back, ka_time_start);
  setIntItem(dict, back, http11);
  setIntItem(dict, back, is_chunk);
  if (back->chunk_adr) {
    setStringItem(dict, back, chunk_adr);
  }
  setLLIntItem(dict, back, chunk_size);
  setLLIntItem(dict, back, chunk_blocksize);
  setLLIntItem(dict, back, compressed_size);
  if (back->pass2_ptr) {
    itmp = *back->pass2_ptr;
    setIntItem2(dict, itmp, pass2_ptr);
  }
  setStringItem(dict, back, info);
  setIntItem(dict, back, stop_ftp);
  setIntItem(dict, back, finalized);
  
  return dict;
}

EXTERNAL_FUNCTION int hts_py_loop(lien_back* back, int back_max,
                                  int back_index, 
                                  int lien_tot, int lien_ntot,
                                  int stat_time, 
                                  hts_stat_struct* stats) {
  PyObject *pLienback, *pBackMax, *pBackIndex, *pLienTot, *pLienNtot,
          *pStatTime, *meth, *pArgs=0, *pRes;
  int res;
  if (stop_on_next_callback)
    return 0;
#ifdef DEBUG
  fprintf(stderr, "hts_py_loop %li\n", pthread_self());
#endif
  if (pCallbackClass && PyObject_HasAttrString(pCallbackClass, "loop")) {
    meth = PyObject_GetAttrString(pCallbackClass, "loop");
    if (meth) {
      pBackMax = PyInt_FromLong(back_max);
      if (!pBackMax) {
        Py_DECREF(meth);
        return process_error_direct("loop");
      }
      pBackIndex = PyInt_FromLong(back_index);
      if (!pBackIndex) {
        Py_DECREF(meth);
        Py_DECREF(pBackMax);
        return process_error_direct("loop");
      }
      pLienTot = PyInt_FromLong(lien_tot);
      if (!pLienTot) {
        Py_DECREF(meth);
        Py_DECREF(pBackMax);
        Py_DECREF(pBackIndex);
        return process_error_direct("loop");
      }
      pLienNtot = PyInt_FromLong(lien_ntot);
      if (!pLienNtot) {
        Py_DECREF(meth);
        Py_DECREF(pBackMax);
        Py_DECREF(pBackIndex);
        Py_DECREF(pLienTot);
        return process_error_direct("loop");
      }
      pStatTime = PyInt_FromLong(stat_time);
      if (!pStatTime) {
        Py_DECREF(meth);
        Py_DECREF(pBackMax);
        Py_DECREF(pBackIndex);
        Py_DECREF(pLienTot);
        Py_DECREF(pLienNtot);
        return process_error_direct("loop");
      }
      
      pLienback = PyDict_New();
      if (!pLienback) {
        Py_DECREF(meth);
        Py_DECREF(pBackMax);
        Py_DECREF(pBackIndex);
        Py_DECREF(pLienTot);
        Py_DECREF(pLienNtot);
        Py_DECREF(pStatTime);
        return process_error_direct("loop");
      }
      
      if (!setup_lien_back(pLienback, back) || !(pArgs = PyTuple_New(6))) {
        Py_DECREF(pBackMax);
        Py_DECREF(pBackIndex);
        Py_DECREF(pLienTot);
        Py_DECREF(pLienNtot);
        Py_DECREF(pStatTime);
        Py_DECREF(pLienback);
        return process_error_direct("loop");
      }

      PyTuple_SetItem(pArgs, 0, pLienback);
      PyTuple_SetItem(pArgs, 1, pBackMax);
      PyTuple_SetItem(pArgs, 2, pBackIndex);
      PyTuple_SetItem(pArgs, 3, pLienTot);
      PyTuple_SetItem(pArgs, 4, pLienNtot);
      PyTuple_SetItem(pArgs, 5, pStatTime);
      
      pRes = PyObject_CallObject(meth, pArgs);
      Py_DECREF(pArgs);
      Py_DECREF(meth);
      
      if (!pRes) {
        return process_error_direct("loop");
      }
      res = PyObject_IsTrue(pRes);
      Py_DECREF(pRes);
      return res;
    }
  }
  return 1;
}

EXTERNAL_FUNCTION int hts_py_checklink(char *address, char* fil, int status) {
  PyObject *pAddress, *pFil, *pStatus, *meth, *pArgs, *pRes;
  int res;
 
 #ifdef DEBUG
  fprintf(stderr, "hts_py_checklink %li\n", pthread_self());
#endif
 
  if (pCallbackClass && PyObject_HasAttrString(pCallbackClass, "check_link")) {
    meth = PyObject_GetAttrString(pCallbackClass, "check_link");
    if (meth) {
      pAddress = PyString_FromString(address);
      if (!pAddress) {
        process_error_indirect("check_link");
        Py_DECREF(meth);
        return -1;
      }
      pFil = PyString_FromString(fil);
      if (!pFil) {
        process_error_indirect("check_link");
        Py_DECREF(meth);
        Py_DECREF(pAddress);
        return -1;
      }
      pStatus = PyInt_FromLong(status);
      if (!pStatus) {
        process_error_indirect("check_link");
        Py_DECREF(meth);
        Py_DECREF(pAddress);
        Py_DECREF(pFil);
        return -1;
      }
      pArgs = PyTuple_New(3);
      if (!pArgs) {
        process_error_indirect("check_link");
        Py_DECREF(meth);
        Py_DECREF(pAddress);
        Py_DECREF(pFil);
        Py_DECREF(pStatus);
        return -1;
      }
      
      PyTuple_SetItem(pArgs, 0, pAddress);
      PyTuple_SetItem(pArgs, 1, pFil);
      PyTuple_SetItem(pArgs, 2, pStatus);
      
      pRes = PyObject_CallObject(meth, pArgs);
      Py_DECREF(pArgs);
      Py_DECREF(meth);
      
      if (!pRes) {
        process_error_indirect("check_link");
        return -1;
      }
      if (PyInt_Check(pRes)) {
        res = PyInt_AsLong(pRes);
      }
      else {
        res = -1;
      }
      Py_DECREF(pRes);
      return res;
    }
  }
  return -1;
}

#if 0
/* the next two functions are stolen from the httrack sources */
static int fexist(char* s) {
  struct stat st;
  memset(&st, 0, sizeof(st));
  if (stat(s, &st) == 0) {
    if (S_ISREG(st.st_mode)) {
      return 1;
    }
  }
  return 0;
}
#endif

static void default_pause(char* lockfile) {
  while (fexist(lockfile)) {
    sleep(1);
  }
}                        

EXTERNAL_FUNCTION void hts_py_pause(char *lockfile) {
  PyObject *pLockfile, *meth, *pArgs, *pRes;
  
#ifdef DEBUG
  fprintf(stderr, "hts_py_pause %li\n", pthread_self());
#endif
  if (pCallbackClass && PyObject_HasAttrString(pCallbackClass, "pause")) {
    meth = PyObject_GetAttrString(pCallbackClass, "pause");
    if (meth) {
      pLockfile = PyString_FromString(lockfile);
      if (!pLockfile) {
        process_error_indirect("pause");
        default_pause(lockfile);
        return;
      }
      pArgs = PyTuple_New(1);
      if (!pArgs) {
        process_error_indirect("pause");
        Py_DECREF(pLockfile);
        default_pause(lockfile);
        return;
      }
      
      pRes = PyObject_CallObject(meth, pArgs);
      Py_DECREF(pArgs);
      Py_DECREF(meth);
      
      if (!pRes) {
        process_error_indirect("pause");
        /* The Python error can have occured anywehre, and 
           we should be really sure that the lockfile is gone,
           so we'll call the internal test function.
        */
        default_pause(lockfile);
        return;
      }
      
      Py_DECREF(pRes);
      return;
    }
  }
  
  default_pause(lockfile);
}

EXTERNAL_FUNCTION void hts_py_save_file(char *file) {
  PyObject *pFile, *meth, *pArgs, *pRes;
  
#ifdef DEBUG
  fprintf(stderr, "hts_py_save_file %li\n", pthread_self());
#endif
  if (pCallbackClass && PyObject_HasAttrString(pCallbackClass, "save_file")) {
    meth = PyObject_GetAttrString(pCallbackClass, "save_file");
    if (meth) {
      pFile = PyString_FromString(file);
      if (!pFile) {
        process_error_indirect("save_file");
        Py_DECREF(meth);
        return;
      }

      pArgs = PyTuple_New(1);
      if (!pArgs) {
        process_error_indirect("save_file");
        Py_DECREF(meth);
        Py_DECREF(pFile);
        return;
      }
      
      PyTuple_SetItem(pArgs, 0, pFile);
      
      pRes = PyObject_CallObject(meth, pArgs);
      Py_DECREF(pArgs);
      Py_DECREF(meth);
      
      if (!pRes) {
        process_error_indirect("save_file");
        return;
      }
      Py_DECREF(pRes);
    }
  }
  return;
}

EXTERNAL_FUNCTION int hts_py_link_detected(char *link) {
  PyObject *pLink, *meth, *pArgs, *pRes;
  int res;
  
#ifdef DEBUG
  fprintf(stderr, "hts_py_link_detected %li\n", pthread_self());
#endif
  if (pCallbackClass && PyObject_HasAttrString(pCallbackClass, "link_detected")) {
    meth = PyObject_GetAttrString(pCallbackClass, "link_detected");
    if (meth) {
      pLink = PyString_FromString(link);
      if (!pLink) {
        process_error_indirect("link_detected");
        Py_DECREF(meth);
        return 1;
      }

      pArgs = PyTuple_New(1);
      if (!pArgs) {
        process_error_indirect("link_detected");
        Py_DECREF(meth);
        Py_DECREF(pLink);
        return 1;
      }
      
      PyTuple_SetItem(pArgs, 0, pLink);
      
      pRes = PyObject_CallObject(meth, pArgs);
      Py_DECREF(pArgs);
      Py_DECREF(meth);
      
      if (!pRes) {
        process_error_indirect("link_detected");
        return 1;
      }
      
      res = PyObject_IsTrue(pRes);
      Py_DECREF(pRes);
      return res;
    }
  }
  return 1;
}

EXTERNAL_FUNCTION int hts_py_link_detected2(char *link, char* start_tag) {
  PyObject *pLink, *pStartTag, *meth, *pArgs, *pRes;
  int res;
  
#ifdef DEBUG
  fprintf(stderr, "hts_py_link_detected2 %li\n", pthread_self());
#endif
  if (pCallbackClass && PyObject_HasAttrString(pCallbackClass, "link_detected2")) {
    meth = PyObject_GetAttrString(pCallbackClass, "link_detected2");
    if (meth) {
      pLink = PyString_FromString(link);
      if (!pLink) {
        process_error_indirect("link_detected2");
        Py_DECREF(meth);
        return 1;
      }

      pStartTag = PyString_FromString(start_tag);
      if (!pStartTag) {
        process_error_indirect("link_detected2");
        Py_DECREF(meth);
        Py_DECREF(pLink);
        return 1;
      }

      pArgs = PyTuple_New(2);
      if (!pArgs) {
        process_error_indirect("link_detected2");
        Py_DECREF(meth);
        Py_DECREF(pLink);
        Py_DECREF(pStartTag);
        return 1;
      }
      
      PyTuple_SetItem(pArgs, 0, pLink);
      PyTuple_SetItem(pArgs, 1, pStartTag);
      
      pRes = PyObject_CallObject(meth, pArgs);
      Py_DECREF(pArgs);
      Py_DECREF(meth);
      
      if (!pRes) {
        process_error_indirect("link_detected2");
        return 1;
      }
      
      res = PyObject_IsTrue(pRes);
      Py_DECREF(pRes);
      return res;
    }
  }
  return 1;
}

EXTERNAL_FUNCTION int hts_py_transfer_status(lien_back *back) {
  PyObject *pLienback, *meth, *pArgs=0, *pRes;
  
#ifdef DEBUG
  fprintf(stderr, "hts_py_transfer_status %li\n", pthread_self());
#endif
  if (pCallbackClass && PyObject_HasAttrString(pCallbackClass, "transfer_status")) {
    meth = PyObject_GetAttrString(pCallbackClass, "transfer_status");
    if (meth) {
      pLienback = PyDict_New();
      if (!pLienback) {
        process_error_indirect("transfer_status");
        Py_DECREF(meth);
        return 1;
      }
      
      if (!setup_lien_back(pLienback, back) || !(pArgs = PyTuple_New(1))) {
        process_error_indirect("transfer_status");
        Py_DECREF(meth);
        Py_DECREF(pLienback);
        return 1;
      }
      
      PyTuple_SetItem(pArgs, 0, pLienback);
      
      pRes = PyObject_CallObject(meth, pArgs);
      Py_DECREF(pArgs);
      Py_DECREF(meth);
      
      if (!pRes) {
        process_error_indirect("transfer_status");
      }
    }
  }
  return 1;
}

EXTERNAL_FUNCTION int hts_py_save_name(char *adr_complete,
                                       char *fil_complete,
                                       char *referer_adr,
                                       char *referer_fil,
                                       char *save) {
  PyObject *pAddress, *pFil, *pRefererAdr, *pRefererFil, *pSave, 
           *meth, *pArgs, *pRes;
  
#ifdef DEBUG
  fprintf(stderr, "hts_py_save_name %li\n", pthread_self());
#endif
  if (pCallbackClass && PyObject_HasAttrString(pCallbackClass, "save_name")) {
    meth = PyObject_GetAttrString(pCallbackClass, "save_name");
    if (meth) {
      pAddress = PyString_FromString(adr_complete);
      if (!pAddress) {
        process_error_indirect("save_name");
        Py_DECREF(meth);
        return 1;
      }
      pFil = PyString_FromString(fil_complete);
      if (!pFil) {
        process_error_indirect("save_name");
        Py_DECREF(meth);
        Py_DECREF(pAddress);
        return 1;
      }
      pRefererAdr = PyString_FromString(referer_adr);
      if (!pRefererAdr) {
        process_error_indirect("save_name");
        Py_DECREF(meth);
        Py_DECREF(pAddress);
        Py_DECREF(pFil);
        return 1;
      }
      pRefererFil = PyString_FromString(referer_fil);
      if (!pRefererFil) {
        process_error_indirect("save_name");
        Py_DECREF(meth);
        Py_DECREF(pAddress);
        Py_DECREF(pFil);
        Py_DECREF(pRefererAdr);
        return 1;
      }
      pSave = PyString_FromString(save);
      if (!pSave) {
        process_error_indirect("save_name");
        Py_DECREF(meth);
        Py_DECREF(pAddress);
        Py_DECREF(pFil);
        Py_DECREF(pRefererAdr);
        Py_DECREF(pRefererFil);
        return 1;
      }

      pArgs = PyTuple_New(5);
      if (!pArgs) {
        process_error_indirect("save_name");
        Py_DECREF(meth);
        Py_DECREF(pAddress);
        Py_DECREF(pFil);
        Py_DECREF(pRefererAdr);
        Py_DECREF(pRefererFil);
        Py_DECREF(pSave);
        return 1;
      }
      
      PyTuple_SetItem(pArgs, 0, pAddress);
      PyTuple_SetItem(pArgs, 1, pFil);
      PyTuple_SetItem(pArgs, 2, pRefererAdr);
      PyTuple_SetItem(pArgs, 3, pRefererFil);
      PyTuple_SetItem(pArgs, 4, pSave);
      
      pRes = PyObject_CallObject(meth, pArgs);
      Py_DECREF(pArgs);
      Py_DECREF(meth);
      
      if (!pRes) {
        process_error_indirect("save_name");
        return 1;
      }
      
      if (PyString_Check(pRes)) {
        int size = PyString_Size(pRes);
        size = size < 1023 ? size : 1023;
        if (size) {
          memcpy(save, PyString_AsString(pRes), size);
          save[size] = 0;
        }
      }
      
      Py_DECREF(pRes);
      return 1;
    }
  }
  return 1;
}

static int process_header(char *buf,
                          char *adr,
                          char *fil,
                          char *referer_adr,
                          char *referer_fil,
                          htsblk *incoming,
                          char *function) {
  PyObject *pBuf, *pAdr, *pFil, *pRefererAdr, *pRefererFil, *pHtsblk,
           *meth, *pArgs, *pRes;
  int res;
  if (stop_on_next_callback)
    return 0;
  
  if (pCallbackClass && PyObject_HasAttrString(pCallbackClass, function)) {
    meth = PyObject_GetAttrString(pCallbackClass, function);
    if (meth) {
      pBuf = PyString_FromString(buf);
      if (!pBuf) {
        Py_DECREF(meth);
        return process_error_direct(function);
      }
      pAdr = PyString_FromString(adr);
      if (!pAdr) {
        Py_DECREF(meth);
        Py_DECREF(pBuf);
        return process_error_direct(function);
      }
      pFil = PyString_FromString(fil);
      if (!pFil) {
        Py_DECREF(meth);
        Py_DECREF(pBuf);
        Py_DECREF(pAdr);
        return process_error_direct(function);
      }
      pRefererAdr = PyString_FromString(referer_adr);
      if (!pRefererAdr) {
        Py_DECREF(meth);
        Py_DECREF(pBuf);
        Py_DECREF(pAdr);
        Py_DECREF(pFil);
        return process_error_direct(function);
      }
      pRefererFil = PyString_FromString(referer_fil);
      if (!pRefererAdr) {
        Py_DECREF(meth);
        Py_DECREF(pBuf);
        Py_DECREF(pAdr);
        Py_DECREF(pFil);
        Py_DECREF(pRefererAdr);
        return process_error_direct(function);
      }

      pHtsblk = PyDict_New();
      if (!pHtsblk) {
        Py_DECREF(meth);
        Py_DECREF(pBuf);
        Py_DECREF(pAdr);
        Py_DECREF(pFil);
        Py_DECREF(pRefererAdr);
        Py_DECREF(pRefererFil);
        return process_error_direct(function);
      }
      if (!setup_htsblk(pHtsblk, incoming)) {
        Py_DECREF(meth);
        Py_DECREF(pBuf);
        Py_DECREF(pAdr);
        Py_DECREF(pFil);
        Py_DECREF(pRefererAdr);
        Py_DECREF(pRefererFil);
        Py_DECREF(pHtsblk);
        return process_error_direct(function);
      }
      pArgs = PyTuple_New(6);
      if (!pRefererAdr) {
        Py_DECREF(meth);
        Py_DECREF(pBuf);
        Py_DECREF(pAdr);
        Py_DECREF(pFil);
        Py_DECREF(pRefererAdr);
        Py_DECREF(pRefererFil);
        Py_DECREF(pHtsblk);
        return process_error_direct(function);
      }
      
      PyTuple_SetItem(pArgs, 0, pBuf);
      PyTuple_SetItem(pArgs, 1, pAdr);
      PyTuple_SetItem(pArgs, 2, pFil);
      PyTuple_SetItem(pArgs, 3, pRefererAdr);
      PyTuple_SetItem(pArgs, 4, pRefererFil);
      PyTuple_SetItem(pArgs, 5, pHtsblk);
      
      pRes = PyObject_CallObject(meth, pArgs);
      Py_DECREF(pArgs);
      Py_DECREF(meth);
      
      if (!pRes) {
        return process_error_direct(function);
      }
      
      res = PyObject_IsTrue(pRes);
      Py_DECREF(pRes);
      return res;
    }
  }
  return 1;
}

EXTERNAL_FUNCTION int hts_py_send_header(char *buf,
                                         char *adr,
                                         char *fil,
                                         char *referer_adr,
                                         char *referer_fil,
                                         htsblk *incoming) {
#ifdef DEBUG
  fprintf(stderr, "hts_py_send_header %li\n", pthread_self());
#endif
  return process_header(buf, adr, fil, referer_adr, referer_fil, 
                        incoming, "send_header");
}

EXTERNAL_FUNCTION int hts_py_receive_header(char *buf,
                                         char *adr,
                                         char *fil,
                                         char *referer_adr,
                                         char *referer_fil,
                                         htsblk *incoming) {
#ifdef DEBUG
  fprintf(stderr, "hts_py_send_header %li\n", pthread_self());
#endif
  return process_header(buf, adr, fil, referer_adr, referer_fil, 
                        incoming, "receive_header");
}

#ifndef PLUGIN
  /* this is an extension.
     We need a Python wrapper for the hts_main call
  */
  
  static PyObject* hts_py_hts_main(PyObject *self, PyObject *args) {
    PyObject *cbObj, *params, *s, *errmsg, *numres, *result;
    char **hts_main_args;
    int i, argc;
    
    if (!PyArg_ParseTuple(args, "OO", &cbObj, &params))
      return 0;
    
    /* the second argument must be a sequence, but no a string or unicode 
       object 
    */
    argc = PySequence_Size(params);
    if (argc == -1) 
      return 0;
    if (PyString_Check(params) || PyUnicode_Check(params)) {
      PyErr_SetString(PyExc_TypeError, "second parameter must be a sequence");
      return 0;
    }
    
    if (argc == 0) {
      PyErr_SetString(PyExc_TypeError, "the httrack engine needs at least one parameter");
      return 0;
    }
    
    if (argc) {
      hts_main_args = malloc(sizeof(char*) * argc);
      if (!hts_main_args) {
        PyErr_NoMemory();
        return 0;
      }
    }
    else {
      hts_main_args = 0;
    }
    
    for (i = 0; i < argc; i++) {
      s = PySequence_GetItem(params, i);
      if (!s) 
        return 0;
      if (s && !PyString_Check(s)) {
        Py_DECREF(s);
        PyErr_SetString(PyExc_TypeError, "elements of the sequence must be strings");
        free(hts_main_args);
        return 0;
      }
      hts_main_args[i] = PyString_AsString(s);
      Py_DECREF(s);
    }
    
    hts_init();
    initialize(cbObj);
    
    i = hts_main(argc, hts_main_args);
    cleanup();

    if (i) {
      errmsg = PyString_FromString(hts_errmsg());
      if (!errmsg) {
        return 0;
      }
    }
    else {
      errmsg = Py_None;
      Py_INCREF(Py_None);
    }
    numres = PyInt_FromLong(i);
    if (!numres) {
      Py_DECREF(errmsg);
      return 0;
    }
    result = PyTuple_New(2);
    if (!result) {
      Py_DECREF(errmsg);
      Py_DECREF(numres);
      return 0;
    }
    PyTuple_SetItem(result, 0, numres);
    PyTuple_SetItem(result, 1, errmsg);
    
    return result;
  }
  
  static PyMethodDef httrackMethods[] = {
    {"httrack", hts_py_hts_main, METH_VARARGS, 
     "calls the hts_main function\n"
     "usage: httrack(callback_instance, arguments)\n\n"
     "return value: (intres, errmsg)\n\n"
     "where intres is the return value of the httrack engine;\n"
     "errmsg is None, if intres==0, else errmsg contains httrack's\n"
     "error message\n\n"
     "callback_instance must be an intance of class defining at least\n"
     "one of the callbacks start, end, pause, query2, query3,change_options,\n"
     "check_html, preprocess_html, postprocess_html, loop, check_link,\n"
     "save_file, link_detected, save_name, send_header, receive_header\n\n"
     "arguments must be a sequence of strings, where the strings are valid\n"
     "arguments for hts_main, i.e., they must be httrack command line\n"
     "parameters or URLs\n"
    },
    {NULL, NULL, 0, NULL}
  };
  
  PyMODINIT_FUNC inithttracklib() {
    PyObject *m, *d, *v;
    m = Py_InitModule("httracklib", httrackMethods);
    d = PyModule_GetDict(m);

    v = PyInt_FromLong(IMMEDIATE_STOP);
    PyDict_SetItemString(d, "IMMEDIATE_STOP", v);
    Py_DECREF(v);
    
    v = PyInt_FromLong(REGULAR_STOP);
    PyDict_SetItemString(d, "REGULAR_STOP", v);
    Py_DECREF(v);
    
    v = PyInt_FromLong(IGNORE_EXCEPTION);
    PyDict_SetItemString(d, "IGNORE_EXCEPTION", v);
    Py_DECREF(v);
    
    if (PyErr_Occurred())
      Py_FatalError("can't initialize module httracklib");

  }
#endif
