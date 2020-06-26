#ifndef Py_INTERNAL_INTERP_H
#define Py_INTERNAL_INTERP_H
#ifdef __cplusplus
extern "C" {
#endif

#ifndef Py_BUILD_CORE
#  error "this header requires Py_BUILD_CORE define"
#endif

#include "pycore_atomic.h"    /* _Py_atomic_address */
#include "pycore_gil.h"       /* struct _gil_runtime_state  */
#include "pycore_gc.h"        /* struct _gc_runtime_state */
#include "pycore_warnings.h"  /* struct _warnings_runtime_state */

struct _Py_parser_state {
    struct {
        int level;
        int atbol;
    } listnode;
};

struct _pending_calls {
    PyThread_type_lock lock;
    /* Request for running pending calls. */
    _Py_atomic_int calls_to_do;
    /* Request for looking at the `async_exc` field of the current
       thread state.
       Guarded by the GIL. */
    int async_exc;
#define NPENDINGCALLS 32
    struct {
        int (*func)(void *);
        void *arg;
    } calls[NPENDINGCALLS];
    int first;
    int last;
};

struct _ceval_state {
    int recursion_limit;
    /* Records whether tracing is on for any thread.  Counts the number
       of threads for which tstate->c_tracefunc is non-NULL, so if the
       value is 0, we know we don't have to check this thread's
       c_tracefunc.  This speeds up the if statement in
       _PyEval_EvalFrameDefault() after fast_next_opcode. */
    int tracing_possible;
    /* This single variable consolidates all requests to break out of
       the fast path in the eval loop. */
    _Py_atomic_int eval_breaker;
    /* Request for dropping the GIL */
    _Py_atomic_int gil_drop_request;
    struct _pending_calls pending;
#ifdef EXPERIMENTAL_ISOLATED_SUBINTERPRETERS
    struct _gil_runtime_state gil;
#endif
};

/* fs_codec.encoding is initialized to NULL.
   Later, it is set to a non-NULL string by _PyUnicode_InitEncodings(). */
struct _Py_unicode_fs_codec {
    char *encoding;   // Filesystem encoding (encoded to UTF-8)
    int utf8;         // encoding=="utf-8"?
    char *errors;     // Filesystem errors (encoded to UTF-8)
    _Py_error_handler error_handler;
};

struct _Py_bytes_state {
    PyObject *empty_string;
    PyBytesObject *characters[256];
};

struct _Py_unicode_state {
    // The empty Unicode object is a singleton to improve performance.
    PyObject *empty_string;
    /* Single character Unicode strings in the Latin-1 range are being
       shared as well. */
    PyObject *latin1[256];
    struct _Py_unicode_fs_codec fs_codec;
};

struct _Py_float_state {
    /* Special free list
       free_list is a singly-linked list of available PyFloatObjects,
       linked via abuse of their ob_type members. */
    int numfree;
    PyFloatObject *free_list;
};

/* Speed optimization to avoid frequent malloc/free of small tuples */
#ifndef PyTuple_MAXSAVESIZE
   // Largest tuple to save on free list
#  define PyTuple_MAXSAVESIZE 20
#endif
#ifndef PyTuple_MAXFREELIST
   // Maximum number of tuples of each size to save
#  define PyTuple_MAXFREELIST 2000
#endif

struct _Py_tuple_state {
#if PyTuple_MAXSAVESIZE > 0
    /* Entries 1 up to PyTuple_MAXSAVESIZE are free lists,
       entry 0 is the empty tuple () of which at most one instance
       will be allocated. */
    PyTupleObject *free_list[PyTuple_MAXSAVESIZE];
    int numfree[PyTuple_MAXSAVESIZE];
#endif
};

/* Empty list reuse scheme to save calls to malloc and free */
#ifndef PyList_MAXFREELIST
#  define PyList_MAXFREELIST 80
#endif

struct _Py_list_state {
    PyListObject *free_list[PyList_MAXFREELIST];
    int numfree;
};

#ifndef PyDict_MAXFREELIST
#  define PyDict_MAXFREELIST 80
#endif

struct _Py_dict_state {
    /* Dictionary reuse scheme to save calls to malloc and free */
    PyDictObject *free_list[PyDict_MAXFREELIST];
    int numfree;
    PyDictKeysObject *keys_free_list[PyDict_MAXFREELIST];
    int keys_numfree;
};

struct _Py_frame_state {
    PyFrameObject *free_list;
    /* number of frames currently in free_list */
    int numfree;
};

#ifndef _PyAsyncGen_MAXFREELIST
#  define _PyAsyncGen_MAXFREELIST 80
#endif

struct _Py_async_gen_state {
    /* Freelists boost performance 6-10%; they also reduce memory
       fragmentation, as _PyAsyncGenWrappedValue and PyAsyncGenASend
       are short-living objects that are instantiated for every
       __anext__() call. */
    struct _PyAsyncGenWrappedValue* value_freelist[_PyAsyncGen_MAXFREELIST];
    int value_numfree;

    struct PyAsyncGenASend* asend_freelist[_PyAsyncGen_MAXFREELIST];
    int asend_numfree;
};

struct _Py_context_state {
    // List of free PyContext objects
    PyContext *freelist;
    int numfree;
};

struct _Py_exc_state {
    // The dict mapping from errno codes to OSError subclasses
    PyObject *errnomap;
    PyBaseExceptionObject *memerrors_freelist;
    int memerrors_numfree;
};


/* interpreter state */

#define _PY_NSMALLPOSINTS           257
#define _PY_NSMALLNEGINTS           5

// The PyInterpreterState typedef is in Include/pystate.h.
struct _is {

    struct _is *next;
    struct _ts *tstate_head;

    /* Reference to the _PyRuntime global variable. This field exists
       to not have to pass runtime in addition to tstate to a function.
       Get runtime from tstate: tstate->interp->runtime. */
    struct pyruntimestate *runtime;

    int64_t id;
    int64_t id_refcount;
    int requires_idref;
    PyThread_type_lock id_mutex;

    int finalizing;

    struct _ceval_state ceval;
    struct _gc_runtime_state gc;

    PyObject *modules;
    PyObject *modules_by_index;
    PyObject *sysdict;
    PyObject *builtins;
    PyObject *importlib;

    /* Used in Modules/_threadmodule.c. */
    long num_threads;
    /* Support for runtime thread stack size tuning.
       A value of 0 means using the platform's default stack size
       or the size specified by the THREAD_STACK_SIZE macro. */
    /* Used in Python/thread.c. */
    size_t pythread_stacksize;

    PyObject *codec_search_path;
    PyObject *codec_search_cache;
    PyObject *codec_error_registry;
    int codecs_initialized;

    PyConfig config;
#ifdef HAVE_DLOPEN
    int dlopenflags;
#endif

    PyObject *dict;  /* Stores per-interpreter state */

    PyObject *builtins_copy;
    PyObject *import_func;
    /* Initialized to PyEval_EvalFrameDefault(). */
    _PyFrameEvalFunction eval_frame;

    Py_ssize_t co_extra_user_count;
    freefunc co_extra_freefuncs[MAX_CO_EXTRA_USERS];

#ifdef HAVE_FORK
    PyObject *before_forkers;
    PyObject *after_forkers_parent;
    PyObject *after_forkers_child;
#endif
    /* AtExit module */
    void (*pyexitfunc)(PyObject *);
    PyObject *pyexitmodule;

    uint64_t tstate_next_unique_id;

    struct _warnings_runtime_state warnings;

    PyObject *audit_hooks;

    struct _Py_parser_state parser;

#if _PY_NSMALLNEGINTS + _PY_NSMALLPOSINTS > 0
    /* Small integers are preallocated in this array so that they
       can be shared.
       The integers that are preallocated are those in the range
       -_PY_NSMALLNEGINTS (inclusive) to _PY_NSMALLPOSINTS (not inclusive).
    */
    PyLongObject* small_ints[_PY_NSMALLNEGINTS + _PY_NSMALLPOSINTS];
#endif
    struct _Py_bytes_state bytes;
    struct _Py_unicode_state unicode;
    struct _Py_float_state float_state;
    /* Using a cache is very effective since typically only a single slice is
       created and then deleted again. */
    PySliceObject *slice_cache;

    struct _Py_tuple_state tuple;
    struct _Py_list_state list;
    struct _Py_dict_state dict_state;
    struct _Py_frame_state frame;
    struct _Py_async_gen_state async_gen;
    struct _Py_context_state context;
    struct _Py_exc_state exc_state;
};

/* Used by _PyImport_Cleanup() */
extern void _PyInterpreterState_ClearModules(PyInterpreterState *interp);

extern PyStatus _PyInterpreterState_SetConfig(
    PyInterpreterState *interp,
    const PyConfig *config);



/* cross-interpreter data registry */

/* For now we use a global registry of shareable classes.  An
   alternative would be to add a tp_* slot for a class's
   crossinterpdatafunc. It would be simpler and more efficient. */

struct _xidregitem;

struct _xidregitem {
    PyTypeObject *cls;
    crossinterpdatafunc getdata;
    struct _xidregitem *next;
};

PyAPI_FUNC(struct _is*) _PyInterpreterState_LookUpID(int64_t);

PyAPI_FUNC(int) _PyInterpreterState_IDInitref(struct _is *);
PyAPI_FUNC(void) _PyInterpreterState_IDIncref(struct _is *);
PyAPI_FUNC(void) _PyInterpreterState_IDDecref(struct _is *);

#ifdef __cplusplus
}
#endif
#endif /* !Py_INTERNAL_INTERP_H */

