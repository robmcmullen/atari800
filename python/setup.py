import os
import sys
from setuptools import setup, find_packages, Extension
import numpy as np

if sys.platform.startswith("win"):
    extra_compile_args = ["-DMSVC", "-D_CRT_SECURE_NO_WARNINGS"]
    config_include = "include/win"
else:
    #extra_compile_args = ["-g", "-O3"]
    extra_compile_args = ["-DGENERIC", "-g"]
    config_include = "include/linux"

extensions = [
  Extension("pyatari800.pyatari800",
    sources = ["pyatari800/pyatari800.c",
    "src/generic/main.c",
    "src/generic/input.c",
    "src/generic/video.c",
    "src/generic/init.c",
    "src/generic/sound.c",
    "src/generic/statesav.c",
    "src/afile.c",
    "src/antic.c",
    "src/atari.c",
    "src/binload.c",
    "src/cartridge.c",
    "src/cassette.c",
    "src/compfile.c",
    "src/cfg.c",
    "src/cpu.c",
    "src/crc32.c",
    "src/devices.c",
    "src/emuos_altirra.c",
    "src/esc.c",
    "src/gtia.c",
    "src/img_tape.c",
    "src/log.c",
    "src/memory.c",
    "src/monitor.c",
    "src/pbi.c",
    "src/pia.c",
    "src/pokey.c",
    "src/rtime.c",
    "src/sio.c",
    "src/sysrom.c",
    "src/util.c",
    "src/input.c",
    "src/statesav.c",
    "src/ui_basic.c",
    "src/ui.c",
    "src/artifact.c",
    "src/colours.c",
    "src/colours_ntsc.c",
    "src/colours_pal.c",
    "src/colours_external.c",
    "src/screen.c",
    "src/cycle_map.c",
    "src/pbi_mio.c",
    "src/pbi_bb.c",
    "src/pbi_scsi.c",
    "src/pokeysnd.c",
    "src/mzpokeysnd.c",
    "src/remez.c",
    "src/sndsave.c",
    "src/sound.c",
    "src/pbi_xld.c",
    "src/voicebox.c",
    "src/votrax.c",
    "src/votraxsnd.c",
              ],
    extra_compile_args = extra_compile_args,
    include_dirs = [config_include, "src", "src/generic", np.get_include()],
    )
]

cmdclass = dict()

# Cython is only used when creating a source distribution. Users don't need
# to install Cython unless they are modifying the .pyx files themselves.
if "sdist" in sys.argv:
    try:
        from Cython.Build import cythonize
        from distutils.command.sdist import sdist as _sdist

        class sdist(_sdist):
            def run(self):
                cythonize(["pyatari800/pyatari800.pyx"], gdb_debug=True)
                _sdist.run(self)
        cmdclass["sdist"] = sdist
    except ImportError:
        # assume the user doesn't have Cython and hope that the C file
        # is included in the source distribution.
        pass

execfile('pyatari800/_metadata.py')

# Temporarily move the config.h in the main src directory out of the way
# because it will be found before the pyatari800 platform-specific include file

src_config = "src/config.h"
moved_config = None
if os.path.exists(src_config):
    moved_config = "src/configPY.hPY"
    os.rename(src_config, moved_config)

try:
    setup(
  name = "pyatari800",
  version = __version__,
  author = __author__,
  author_email = __author_email__,
  url = __url__,
  classifiers = [c.strip() for c in """\
    Development Status :: 5 - Production/Stable
    Intended Audience :: Developers
    License :: OSI Approved :: GNU General Public License (GPL)
    Operating System :: MacOS
    Operating System :: Microsoft :: Windows
    Operating System :: OS Independent
    Operating System :: POSIX
    Operating System :: Unix
    Programming Language :: Python
    Topic :: Utilities
    Topic :: Software Development :: Assemblers
    """.splitlines() if len(c.strip()) > 0],
  description = "Python wrapper for atari800, the cross-platform Atari 8-bit computer emulator",
  long_description = open('README.rst').read(),
  cmdclass = cmdclass,
  ext_modules = extensions,
  packages = ["pyatari800"],
  install_requires = [
  'numpy',
  'pyopengl',
  'pyopengl_accelerate',
  'pillow',
  'construct',
  ],
)
finally:
    if moved_config:
        os.rename(moved_config, src_config)
