#ifndef UTIL_H
# define UTIL_H

# ifdef __cplusplus
extern "C" {
# endif // ifdef __cplusplus

# include "types.h"
# include "constants.h"


/**********************
* Utility Functions  *
**********************/

/**
 * Copy settings creating a new settings structure (uses MALLOC)
 * @param  settings Settings to be copied
 * @return          New settings structure
 */
QPALMSettings* copy_settings(QPALMSettings *settings);


/**
 * Custom string copy to avoid string.h library
 * @param dest   destination string
 * @param source source string
 */
void c_strcpy(char       dest[],
              const char source[]);


/**
 * Update solver status (value and string)
 * @param info QPALMInfo
 * @param status_val new status value
 */
void update_status(QPALMInfo *info,
                   c_int     status_val);

/**
 * Cold start workspace variables x, x_prev, y, Ax and Qx
 * @param work Workspace
 */
void cold_start(QPALMWorkspace *work);

/**
 * Initialize penalty factors from initial x 
 * @param work Workspace
 */
void initialize_sigma(QPALMWorkspace *work);


/*********************************
* Timer Structs and Functions * *
*********************************/

/*! \cond PRIVATE */

# ifdef PROFILING

// Windows
#  ifdef _WIN32

  // Some R packages clash with elements
  // of the windows.h header, so use a
  // slimmer version for conflict avoidance
  # ifdef R_LANG
    #define NOGDI
  # endif

#   include <windows.h>

struct QPALM_TIMER {
  LARGE_INTEGER tic;
  LARGE_INTEGER toc;
  LARGE_INTEGER freq;
};

// Mac
#  elif defined __APPLE__

#   include <mach/mach_time.h>

/* Use MAC OSX  mach_time for timing */
struct QPALM_TIMER {
  uint64_t                  tic;
  uint64_t                  toc;
  mach_timebase_info_data_t tinfo;
};

// Mac
#  elif defined __MACH__

#   include <mach/mach_time.h>

/* Use MAC OSX  mach_time for timing */
struct QPALM_TIMER {
  uint64_t                  tic;
  uint64_t                  toc;
  mach_timebase_info_data_t tinfo;
};

// Linux
#  elif defined __linux__ // ifdef _WIN32
/* Use POSIX clocl_gettime() for timing on non-Windows machines */
#   include <time.h>
#   include <sys/time.h>


struct QPALM_TIMER {
  struct timespec tic;
  struct timespec toc;
};

#  endif // ifdef _WIN32

/*! \endcond */

/**
 * Timer Methods
 */

/**
 * Start timer
 * @param t Timer object
 */
void    qpalm_tic(QPALMTimer *t);

/**
 * Report time
 * @param  t Timer object
 * @return   Reported time
 */
c_float qpalm_toc(QPALMTimer *t);

# endif /* END #ifdef PROFILING */


# ifdef __cplusplus
}
# endif // ifdef __cplusplus

#endif // ifndef UTIL_H