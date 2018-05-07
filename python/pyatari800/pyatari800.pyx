from libc.stdlib cimport malloc, free
from cpython.string cimport PyString_AsString
import numpy as np
cimport numpy as np

cdef extern:
    int start_generic(int, char **)
    void process_frame(void *input, void *output)
    int mount_disk_image(int diskno, const char *filename, int readonly)
    void load_save_state(void *restore)

cdef char ** to_cstring_array(list_str):
    cdef char **ret = <char **>malloc(len(list_str) * sizeof(char *))
    for i in xrange(len(list_str)):
        ret[i] = PyString_AsString(list_str[i])
    return ret

def start_emulator(args):
    cdef char *fake_args[10]
    cdef char **argv = fake_args
    cdef int argc
    cdef char *progname="pyatari800"
    cdef char **c_args = to_cstring_array(args)

    argc = 1
    fake_args[0] = progname
    for i in xrange(len(args)):
        arg = c_args[i]
        fake_args[argc] = arg
        argc += 1

    start_generic(argc, argv)
    free(c_args)

def next_frame(np.ndarray input not None, np.ndarray output not None):
    cdef np.uint8_t[:] ibuf
    cdef np.uint8_t[:] obuf

    ibuf = input.view(np.uint8)
    obuf = output.view(np.uint8)
    process_frame(&ibuf[0], &obuf[0])

def load_disk(int disknum, char *filename, int readonly=0):
    mount_disk_image(disknum, filename, readonly)

def restore_state(np.ndarray[char, ndim=1] save not None):
    load_save_state(&save[0])