# SHMEM input template locations
import numpy as np

INPUT_DTYPE = np.dtype([
    ("frame_count", np.uint32),
    ("main_semaphore", np.uint8),
    ("keychar", np.uint8),
    ("keycode", np.uint8),
    ("special", np.uint8),
    ("shift", np.uint8),
    ("control", np.uint8),
    ("start", np.uint8),
    ("select", np.uint8),
    ("option", np.uint8),
    ("joy0", np.uint8),
    ("trig0", np.uint8),
    ("joy1", np.uint8),
    ("trig1", np.uint8),
    ("joy2", np.uint8),
    ("trig2", np.uint8),
    ("joy3", np.uint8),
    ("trig3", np.uint8),
    ("mousex", np.uint8),
    ("mousey", np.uint8),
    ("mouse_buttons", np.uint8),
    ("mouse_mode", np.uint8),
    ("arg_byte_1", np.uint8),
    ("arg_byte_2", np.uint8),
    ("arg_byte_3", np.uint8),
    ("arg_byte_4", np.uint8),
    ("arg_byte_5", np.uint8),
    ("arg_byte_6", np.uint8),
    ("arg_byte_7", np.uint8),
    ("arg_byte_8", np.uint8),
    ("arg_string", "S256"),
])