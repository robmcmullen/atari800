import sys
from setuptools import setup, find_packages, Extension

if sys.platform.startswith("win"):
    extra_compile_args = ["-DMSVC", "-D_CRT_SECURE_NO_WARNINGS"]
    config_include = "include/win"
else:
    extra_compile_args = ["-g"]
    config_include = "include/linux"

extensions = [
  Extension("pyatari800.pyatari800",
    sources = ["pyatari800/pyatari800.c",
    "src/shmem/main.c",
    "src/shmem/input.c",
    "src/shmem/video.c",
    "src/shmem/init.c",
    "src/shmem/sound.c",
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
    "src/emuos.c",
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
    "src/ide.c",
              ],
    extra_compile_args = extra_compile_args,
    include_dirs = [config_include, "src", "src/shmem"],
    )
]

cmdclass = dict()

# Cython is only used when creating a source distribution. Users don't need
# to install Cython unless they are modifying the .pyx files themselves.
if "sdist" in sys.argv:
    from distutils.command.sdist import sdist as _sdist

    class sdist(_sdist):
        def run(self):
            from Cython.Build import cythonize
            cythonize(["pyatari800/pyatari800.pyx"], gdb_debug=True)
            _sdist.run(self)
    cmdclass["sdist"] = sdist

execfile('pyatari800/_metadata.py')

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
)
