from libc.stdlib cimport malloc, free
from cpython.string cimport PyString_AsString
import numpy as np
cimport numpy as np

ctypedef void (*monitor_callback_ptr)()

cdef extern:
    int a8_init(int, char **, monitor_callback_ptr)
    void a8_prepare_arrays(void *input, void *output)
    void a8_next_frame(void *input, void *output)
    int a8_mount_disk_image(int diskno, const char *filename, int readonly)
    void a8_restore_state(void *restore)
    int a8_monitor_step(int addr)

cdef char ** to_cstring_array(list_str):
    cdef char **ret = <char **>malloc(len(list_str) * sizeof(char *))
    for i in xrange(len(list_str)):
        ret[i] = PyString_AsString(list_str[i])
    return ret

monitor_callback = None

cdef void callback():
    print "in cython callback"
    monitor_callback()

def start_emulator(args, python_callback_function):
    global monitor_callback
    cdef char *fake_args[10]
    cdef char **argv = fake_args
    cdef int argc
    cdef char *progname="pyatari800"
    cdef char **c_args = to_cstring_array(args)
    monitor_callback = python_callback_function

    argc = 1
    fake_args[0] = progname
    for i in xrange(len(args)):
        arg = c_args[i]
        fake_args[argc] = arg
        argc += 1

    a8_init(argc, argv, &callback)
    free(c_args)

def prepare_arrays(np.ndarray input not None, np.ndarray output not None):
    cdef np.uint8_t[:] ibuf
    cdef np.uint8_t[:] obuf

    ibuf = input.view(np.uint8)
    obuf = output.view(np.uint8)
    a8_prepare_arrays(&ibuf[0], &obuf[0])

def next_frame(np.ndarray input not None, np.ndarray output not None):
    cdef np.uint8_t[:] ibuf
    cdef np.uint8_t[:] obuf

    ibuf = input.view(np.uint8)
    obuf = output.view(np.uint8)
    a8_next_frame(&ibuf[0], &obuf[0])

def load_disk(int disknum, char *filename, int readonly=0):
    a8_mount_disk_image(disknum, filename, readonly)

def restore_state(np.ndarray state not None):
    cdef np.uint8_t[:] sbuf
    sbuf = state.view(np.uint8)
    a8_restore_state(&sbuf[0])

def monitor_step(int addr):
    cdef int resume;
    resume = a8_monitor_step(addr)
    return resume
