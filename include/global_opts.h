#ifndef GLOBAL_OPTS_H
# define GLOBAL_OPTS_H

# ifdef __cplusplus
extern "C" {
# endif /* ifdef __cplusplus */

typedef double c_float; /* doubles for numerical values  */
typedef long c_int; /* for indices */

/* DATA CUSTOMIZATIONS (depending on memory manager)-----------------------   */
#  include <stdlib.h>

/* define custom printfs and memory allocation (e.g. matlab/python) */
#  ifdef MATLAB
    #   include "mex.h"
static void* c_calloc(size_t num, size_t size) {
  void *m = mxCalloc(num, size);
  mexMakeMemoryPersistent(m);
  return m;
}

static void* c_malloc(size_t size) {
  void *m = mxMalloc(size);

  mexMakeMemoryPersistent(m);
  return m;
}

static void* c_realloc(void *ptr, size_t size) {
  void *m = mxRealloc(ptr, size);

  mexMakeMemoryPersistent(m);
  return m;
}

    #   define c_free mxFree
#  elif defined PYTHON

// Define memory allocation for python. Note that in Python 2 memory manager
// Calloc is not implemented
    #   include <Python.h>
    #   define c_malloc PyMem_Malloc
    #   if PY_MAJOR_VERSION >= 3
    #    define c_calloc PyMem_Calloc
    #   else  /* if PY_MAJOR_VERSION >= 3 */
static void* c_calloc(size_t num, size_t size) {
  void *m = PyMem_Malloc(num * size);

  memset(m, 0, num * size);
  return m;
}

    #   endif /* if PY_MAJOR_VERSION >= 3 */

// #define c_calloc(n,s) ({
//         void * p_calloc = c_malloc((n)*(s));
//         memset(p_calloc, 0, (n)*(s));
//         p_calloc;
//     })
    #   define c_free PyMem_Free
    #   define c_realloc PyMem_Realloc
#  else  /* ifdef MATLAB */
    #   define c_malloc malloc
    #   define c_calloc calloc
    #   define c_free free
    #   define c_realloc realloc

#  endif /* ifdef MATLAB */

/* PRINTING */
# ifdef PRINTING
#  include <stdio.h>
#  include <string.h>

#  ifdef MATLAB
#   define c_print mexPrintf

// The following trick slows down the performance a lot. Since many solvers
// actually
// call mexPrintf and immediately force print buffer flush
// otherwise messages don't appear until solver termination
// ugly because matlab does not provide a vprintf mex interface
// #include <stdarg.h>
// static int c_print(char *msg, ...)
// {
//   va_list argList;
//   va_start(argList, msg);
//   //message buffer
//   int bufferSize = 256;
//   char buffer[bufferSize];
//   vsnprintf(buffer,bufferSize-1, msg, argList);
//   va_end(argList);
//   int out = mexPrintf(buffer); //print to matlab display
//   mexEvalString("drawnow;");   // flush matlab print buffer
//   return out;
// }
#  elif defined PYTHON
#   include <Python.h>
#   define c_print PySys_WriteStdout
#  elif defined R_LANG
#   include <R_ext/Print.h>
#   define c_print Rprintf
#  else  /* ifdef MATLAB */
#   define c_print printf
#  endif /* ifdef MATLAB */

// Print error macro
// #define c_eprint(desc...) (c_print("ERROR in %s: ", __FUNCTION__); c_print
// (stderr, desc); c_print("\n");)
#  define c_eprint(...) c_print("ERROR in %s: ", __FUNCTION__); c_print( \
    __VA_ARGS__); c_print("\n");

# endif /* ifdef PRINTING */


/* CUSTOMIZED OPERATIONS */
# ifndef c_absval
#  define c_absval(x) (((x) < 0) ? -(x) : (x))
# endif /* ifndef c_absval */

# ifndef c_max
#  define c_max(a, b) (((a) > (b)) ? (a) : (b))
# endif /* ifndef c_max */

# ifndef c_min
#  define c_min(a, b) (((a) < (b)) ? (a) : (b))
# endif /* ifndef c_min */

# ifndef mod
#  define mod(a,b) ((((a)%(b))+(b))%(b))
#endif

#include <math.h>
#  define c_sqrt sqrt

# ifdef __cplusplus
}
# endif /* ifdef __cplusplus */

#endif /* ifndef GLOBAL_OPTS_H */