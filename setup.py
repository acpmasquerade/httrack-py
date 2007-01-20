"""httrack-py library: wrapper to use httrack website copier
"""

classifiers = """\
Development Status :: 3 - Alpha
Intended Audience :: Developers
License :: OSI Approved :: GNU Library or Lesser General Public License (LGPL)
Programming Language :: Python
Topic :: Software Development :: Libraries :: Python Modules
Topic :: Software Development :: Libraries
Topic :: Communications :: Email
Operating System :: Unix
Operating System :: POSIX
"""

HTTRACK_SRC_DIR = "../httrack"

import sys, os
from distutils.core import setup, Extension

if os.name == "posix":
   PLATFORM_INCLUDES = ['/usr/include','/usr/include/python%i.%i' % sys.version_info[:2]]

if sys.version_info < (2, 3):
    _setup = setup
    def setup(**kwargs):
        if kwargs.has_key("classifiers"):
            del kwargs["classifiers"]
        _setup(**kwargs)

doclines = __doc__.split("\n")

setup(name="httrack-py",
      version="0.6",
      maintainer="Abel Deuring",
      maintainer_email="adeuring@gmx.net",
      license = "http://www.fsf.org/licensing/licenses/lgpl.txt",
      platforms = ["unix","win32"],
      packages = ["httrack"],
      package_dir = {"httrack": "lib"},
      description = doclines[0],
      classifiers = filter(None, classifiers.split("\n")),
      long_description = "\n".join(doclines[2:]),
      ext_modules=[Extension(
         "httracklib",
         [os.path.join("src","httrack-py.c")],
         include_dirs=[HTTRACK_SRC_DIR, os.sep.join((HTTRACK_SRC_DIR, "src"))] + PLATFORM_INCLUDES
      )],

)
