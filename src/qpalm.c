#include "qpalm.h"
#include "global_opts.h"
#include "constants.h"
#include "validate.h"
#include "lin_alg.h"
#include "util.h"
#include "scaling.h"
#include "linesearch.h"
#include "lbfgs.h"
#include "termination.h"
#include "cs.h"
#include <stdio.h>

/**********************
* Main API Functions *
**********************/
void qpalm_set_default_settings(QPALMSettings *settings) {

  settings->max_iter      = MAX_ITER;                /* maximum iterations to take */
  settings->eps_abs       = (c_float)EPS_ABS;        /* absolute convergence tolerance */
  settings->eps_rel       = (c_float)EPS_REL;        /* relative convergence tolerance */
  settings->eps_abs_in    = (c_float)EPS_ABS_IN;     /* intermediate absolute convergence tolerance */
  settings->eps_rel_in    = (c_float)EPS_REL_IN;     /* intermediate relative convergence tolerance */
  settings->rho           = (c_float)RHO;            /* tolerance scaling factor */
  settings->eps_prim_inf  = (c_float)EPS_PRIM_INF;   /* primal infeasibility tolerance */
  settings->eps_dual_inf  = (c_float)EPS_DUAL_INF;   /* dual infeasibility tolerance */
  settings->theta         = (c_float)THETA;          /* penalty update criterion parameter */
  settings->delta         = (c_float)DELTA;          /* penalty update factor */
  settings->tau_init      = (c_float)TAU_INIT;       /* initial stepsize for backtracking */
  settings->memory        = MEMORY;                  /* LBFGS memory */
  settings->proximal      = PROXIMAL;                /* boolean, proximal method of multipliers*/
  settings->gamma         = (c_float)GAMMA;          /* proximal penalty parameter */
  settings->gamma_upd     = (c_float)GAMMA_UPD;      /* proximal penalty update factor*/
  settings->gamma_max     = (c_float)GAMMA_MAX;      /* proximal penalty parameter cap*/
  settings->scaling       = SCALING;                 /* boolean, scaling */
  settings->warm_start    = WARM_START;              /* boolean, warm start solver */
  settings->verbose       = VERBOSE;                 /* boolean, write out progress */
}


QPALMWorkspace* qpalm_setup(const QPALMData *data, QPALMSettings *settings) {
  QPALMWorkspace *work; // Workspace

  // Validate data
  if (validate_data(data)) {
# ifdef PRINTING
    c_eprint("Data validation returned failure");
# endif /* ifdef PRINTING */
    return QPALM_NULL;
  }

  // Validate settings
  if (validate_settings(settings)) {
# ifdef PRINTING
    c_eprint("Settings validation returned failure");
# endif /* ifdef PRINTING */
    return QPALM_NULL;
  }

  // Allocate empty workspace
  work = c_calloc(1, sizeof(QPALMWorkspace));

  if (!work) {
# ifdef PRINTING
    c_eprint("allocating work failure");
# endif /* ifdef PRINTING */
    return QPALM_NULL;
  }

  // Start and allocate directly timer
  # ifdef PROFILING
    work->timer = c_malloc(sizeof(QPALMTimer));
    qpalm_tic(work->timer);
  # endif /* ifdef PROFILING */

  // Copy problem data into workspace
  work->data       = c_malloc(sizeof(QPALMData));
  work->data->n    = data->n;                             // Number of variables
  work->data->m    = data->m;                             // Number of linear constraints
  work->data->Q    = csc_to_triu(data->Q);                // Cost function matrix
  work->data->q    = vec_copy(data->q, data->n);          // Linear part of cost function
  work->data->A    = copy_csc_mat(data->A);               // Linear constraints matrix
  work->data->bmin = vec_copy(data->bmin, data->m);       // Lower bounds on constraints
  work->data->bmax = vec_copy(data->bmax, data->m);       // Upper bounds on constraints

  // Allocate internal solver variables 
  work->x        = c_calloc(work->data->n, sizeof(c_float));
  work->y        = c_calloc(work->data->m, sizeof(c_float));
  work->Ax       = c_calloc(work->data->m, sizeof(c_float));
  work->Qx       = c_calloc(work->data->n, sizeof(c_float));
  work->x_prev   = c_calloc(work->data->n, sizeof(c_float));
  work->Aty      = c_calloc(work->data->n, sizeof(c_float));

  // Workspace variables
  work->temp_m   = c_calloc(work->data->m, sizeof(c_float));
  work->temp_n   = c_calloc(work->data->n, sizeof(c_float));
  work->sigma = c_calloc(work->data->m, sizeof(c_float));
  initialize_sigma(work);

  work->z  = c_calloc(work->data->m, sizeof(c_float));
  work->Axys = c_calloc(work->data->m, sizeof(c_float));
  work->pri_res = c_calloc(work->data->m, sizeof(c_float));
  work->pri_res_in = c_calloc(work->data->m, sizeof(c_float));
  work->yh = c_calloc(work->data->m, sizeof(c_float));
  work->Atyh = c_calloc(work->data->n, sizeof(c_float));
  work->df = c_calloc(work->data->n, sizeof(c_float));
  work->x0 = c_calloc(work->data->n, sizeof(c_float));
  work->xx0 = c_calloc(work->data->n, sizeof(c_float));
  work->dphi = c_calloc(work->data->n, sizeof(c_float));
  work->dphi_prev = c_calloc(work->data->n, sizeof(c_float));
  work->d = c_calloc(work->data->n, sizeof(c_float));

  // Initialize variables x, x_prev, y, Qx and Ax
  cold_start(work);

  // Linesearch variables
  work->Qd          = c_calloc(work->data->n, sizeof(c_float));
  work->Ad          = c_calloc(work->data->m, sizeof(c_float));
  work->sigma_sqrt  = c_calloc(work->data->m, sizeof(c_float));
  work->delta       = c_calloc(work->data->m*2, sizeof(c_float));
  work->alpha       = c_calloc(work->data->m*2, sizeof(c_float));
  work->delta2      = c_calloc(work->data->m*2, sizeof(c_float));
  work->delta_alpha = c_calloc(work->data->m*2, sizeof(c_float));
  work->temp_2m     = c_calloc(work->data->m*2, sizeof(c_float));
  work->s           = c_calloc(work->data->m*2, sizeof(array_element));
  work->index_L     = c_calloc(work->data->m*2, sizeof(c_int));
  work->index_P     = c_calloc(work->data->m*2, sizeof(c_int));
  work->index_J     = c_calloc(work->data->m*2, sizeof(c_int));

  // Primal infeasibility variables
  work->delta_y   = c_calloc(work->data->m, sizeof(c_float));
  work->Atdelta_y = c_calloc(work->data->n, sizeof(c_float));

  // Dual infeasibility variables
  work->delta_x  = c_calloc(work->data->n, sizeof(c_float));
  work->Qdelta_x = c_calloc(work->data->n, sizeof(c_float));
  work->Adelta_x = c_calloc(work->data->m, sizeof(c_float));

  // Copy settings
  work->settings = copy_settings(settings);

  // Perform scaling
  if (settings->scaling) {
    // Allocate scaling structure
    work->scaling       = c_malloc(sizeof(QPALMScaling));
    work->scaling->D    = c_calloc(work->data->n, sizeof(c_float));
    work->scaling->Dinv = c_calloc(work->data->n, sizeof(c_float));
    work->scaling->E    = c_calloc(work->data->m, sizeof(c_float));
    work->scaling->Einv = c_calloc(work->data->m, sizeof(c_float));

    // Allocate workspace variables used in scaling
    work->D_temp   = c_malloc(work->data->n * sizeof(c_float));
    work->E_temp   = c_malloc(work->data->m * sizeof(c_float));

    // Scale data
    scale_data(work);
    
  }
  else {
    work->scaling = QPALM_NULL;
  }

  // LBFGS variables
  work->lbfgs              = c_calloc(1, sizeof(QPALMLbfgs));
  work->lbfgs->curridx     = 0;
  work->lbfgs->currmem     = 0;
  work->lbfgs->reset_lbfgs = 1;
  work->lbfgs->s           = c_calloc(work->data->n, sizeof(c_float));
  work->lbfgs->y           = c_calloc(work->data->n, sizeof(c_float));
  work->lbfgs->Sbuffer     = c_calloc(work->data->n*work->settings->memory, sizeof(c_float));
  work->lbfgs->Ybuffer     = c_calloc(work->data->n*work->settings->memory, sizeof(c_float));
  work->lbfgs->YSbuffer    = c_calloc(work->settings->memory, sizeof(c_float));
  work->lbfgs->H           = 1.0;
  work->lbfgs->alpha       = c_calloc(work->settings->memory, sizeof(c_float));
  work->lbfgs->q           = c_calloc(work->data->n, sizeof(c_float));

  // Allocate solution
  work->solution    = c_calloc(1, sizeof(QPALMSolution));
  work->solution->x = c_calloc(1, work->data->n * sizeof(c_float));
  work->solution->y = c_calloc(1, work->data->m * sizeof(c_float));

  // Allocate and initialize information
  work->info                = c_calloc(1, sizeof(QPALMInfo));
  update_status(work->info, QPALM_UNSOLVED);
  # ifdef PROFILING
  work->info->solve_time  = 0.0;                    // Solve time to zero
  work->info->run_time    = 0.0;                    // Total run time to zero
  work->info->setup_time  = qpalm_toc(work->timer); // Update timer information
  # endif /* ifdef PROFILING */

  // Return workspace structure
  return work;
}


void qpalm_solve(QPALMWorkspace *work) {
  
  #ifdef PROFILING
  qpalm_tic(work->timer); // Start timer
  #endif /* ifdef PROFILING */

  c_int iter;
  c_int iter_out = 0;
  c_float tau;

  for (iter = 0; iter < work->settings->max_iter; iter++) {

    //Axys = Ax + y./sigma
    vec_ew_div(work->y, work->sigma, work->temp_m, work->data->m);
    vec_add_scaled(work->Ax, work->temp_m, work->Axys, 1, work->data->m);
    //z = min(max(Axys,bmin),bmax)
    vec_ew_mid_vec(work->Axys, work->data->bmin, work->data->bmax, work->z, work->data->m);
    //pri_res = Ax-z
    vec_add_scaled(work->Ax, work->z, work->pri_res, -1, work->data->m);
    //yh = y + pri_res.*sigma
    vec_ew_prod(work->pri_res, work->sigma, work->temp_m, work->data->m);
    vec_add_scaled(work->y, work->temp_m, work->yh, 1, work->data->m);

    //df = Qx + q
    vec_add_scaled(work->Qx, work->data->q, work->df, 1, work->data->n);
    if (work->settings->proximal) {
      //df = Qx + q +1/gamma*(x-x0)
      vec_add_scaled(work->x, work->x0, work->xx0, -1, work->data->n);
      vec_add_scaled(work->df, work->xx0, work->df, -1/work->settings->gamma, work->data->n);
    }
    // Atyh = A'*yh
    mat_tpose_vec(work->data->A, work->yh, work->Atyh);
    //dphi = df+Atyh
    vec_add_scaled(work->df, work->Atyh, work->dphi, 1, work->data->n);

    if (check_termination(work)) {
      work->info->iter = iter;
      work->info->iter_out = iter_out;
      /* Update solve time and run time */
      #ifdef PROFILING
        work->info->solve_time = qpalm_toc(work->timer);
        work->info->run_time = work->info->setup_time +
                           work->info->solve_time;
      #endif /* ifdef PROFILING */
      return; 
    } else if (check_subproblem_termination(work)) {
      prea_vec_copy(work->yh, work->y, work->data->m);
      prea_vec_copy(work->Atyh, work->Aty, work->data->n);
      work->settings->eps_abs_in = c_max(work->settings->eps_abs,
                                        work->settings->rho*work->settings->eps_abs_in);
      work->settings->eps_rel_in = c_max(work->settings->eps_rel,
                                        work->settings->rho*work->settings->eps_rel_in);
      if (iter_out > 0 && work->info->pri_res_norm > work->eps_pri) {
        for (c_int k = 0; k < work->data->m; k++) {
          if (c_absval(work->pri_res[k]) > work->settings->theta*c_absval(work->pri_res_in[k])) {
            work->sigma[k] *= work->settings->delta;
          }
        }
        work->lbfgs->reset_lbfgs = 1;
      }
      prea_vec_copy(work->pri_res, work->pri_res_in, work->data->m);

      if(work->settings->proximal) {
        work->settings->gamma = c_min(work->settings->gamma*work->settings->gamma_upd, work->settings->gamma_max);
        prea_vec_copy(work->x, work->x0, work->data->n);
      }

      iter_out++;
    } else {
      // d = lbfgs()
      lbfgs_set_direction(work);

      // tau = exact_linesearch(work);
      // tau = armijo_linesearch(work);
      tau = exact_linesearch_newton(work);
      //x_prev = x
      prea_vec_copy(work->x, work->x_prev, work->data->n);
      //dphi_prev = dphi 
      prea_vec_copy(work->dphi, work->dphi_prev, work->data->n);
      //x = x+tau*d
      vec_add_scaled(work->x, work->d, work->x, tau, work->data->n);
      vec_mult_scalar(work->Qd, tau, work->data->n); //Qdx used in dua_infeas check
      vec_mult_scalar(work->Ad, tau, work->data->m); //Adx used in dua_infeas check
      vec_add_scaled(work->Qx, work->Qd, work->Qx, 1, work->data->n);
      vec_add_scaled(work->Ax, work->Ad, work->Ax, 1, work->data->m);
    }
  }

  // maxiter reached
  update_status(work->info, QPALM_MAX_ITER_REACHED);
  work->info->iter = iter;
  work->info->iter_out = iter_out;
  store_solution(work);
  #ifdef PROFILING
    work->info->solve_time = qpalm_toc(work->timer);
    work->info->run_time = work->info->setup_time +
                           work->info->solve_time;
  #endif /* ifdef PROFILING */
}


void qpalm_cleanup(QPALMWorkspace *work) {

  if (work) { // If workspace has been allocated
    // Free Data
    if (work->data) {
      if (work->data->Q) csc_spfree(work->data->Q);

      if (work->data->A) csc_spfree(work->data->A);

      if (work->data->q) c_free(work->data->q);

      if (work->data->bmin) c_free(work->data->bmin);

      if (work->data->bmax) c_free(work->data->bmax);
      c_free(work->data);
    }

    // Free scaling
    if (work->settings->scaling) {
      if (work->scaling->D) c_free(work->scaling->D);

      if (work->scaling->Dinv) c_free(work->scaling->Dinv);

      if (work->scaling->E) c_free(work->scaling->E);

      if (work->scaling->Einv) c_free(work->scaling->Einv);

      if (work->D_temp) c_free(work->D_temp);

      if (work->E_temp) c_free(work->E_temp);

      c_free(work->scaling);
    }

    // Free other Variables
    if (work->x) c_free(work->x);
    
    if (work->y) c_free(work->y);

    if (work->Ax) c_free(work->Ax);

    if (work->Qx) c_free(work->Qx);
    
    if (work->x_prev) c_free(work->x_prev);

    if (work->Aty) c_free(work->Aty);

    if (work->temp_m) c_free(work->temp_m);

    if (work->temp_n) c_free(work->temp_n);

    if (work->sigma) c_free(work->sigma);

    if (work->z) c_free(work->z);

    if (work->Axys) c_free(work->Axys);

    if (work->pri_res) c_free(work->pri_res);

    if (work->pri_res_in) c_free(work->pri_res_in);

    if (work->yh) c_free(work->yh);

    if (work->Atyh) c_free(work->Atyh);

    if (work->df) c_free(work->df);

    if (work->x0) c_free(work->x0);

    if (work->xx0) c_free(work->xx0);

    if (work->dphi) c_free(work->dphi);

    if (work->dphi_prev) c_free(work->dphi_prev);

    if (work->d) c_free(work->d);

    if (work->Qd) c_free(work->Qd);

    if (work->Ad) c_free(work->Ad);

    if (work->sigma_sqrt) c_free(work->sigma_sqrt);

    if (work->delta) c_free(work->delta);

    if (work->alpha) c_free(work->alpha);

    if (work->delta2) c_free(work->delta2);

    if (work->delta_alpha) c_free(work->delta_alpha);

    if (work->temp_2m) c_free(work->temp_2m);

    if (work->s) c_free(work->s);

    if (work->index_L) c_free(work->index_L);

    if (work->index_P) c_free(work->index_P);

    if (work->index_J) c_free(work->index_J);

    if (work->delta_y) c_free(work->delta_y);

    if (work->Atdelta_y) c_free(work->Atdelta_y);

    if (work->delta_x) c_free(work->delta_x);

    if (work->Qdelta_x) c_free(work->Qdelta_x);

    if (work->Adelta_x) c_free(work->Adelta_x);

    // Free Settings
    if (work->settings) c_free(work->settings);

    // Free lbfgs struct
    if (work->lbfgs) {
      if (work->lbfgs->s) c_free(work->lbfgs->s);

      if (work->lbfgs->y) c_free(work->lbfgs->y);

      if (work->lbfgs->Sbuffer) c_free(work->lbfgs->Sbuffer);

      if (work->lbfgs->Ybuffer) c_free(work->lbfgs->Ybuffer);

      if (work->lbfgs->YSbuffer) c_free(work->lbfgs->YSbuffer);

      if (work->lbfgs->alpha) c_free(work->lbfgs->alpha);

      if (work->lbfgs->q) c_free(work->lbfgs->q);

      c_free(work->lbfgs);
    }
    
    // Free solution
    if (work->solution) {
      if (work->solution->x) c_free(work->solution->x);

      if (work->solution->y) c_free(work->solution->y);
      c_free(work->solution);
    }

    // Free timer
    # ifdef PROFILING
      if (work->timer) c_free(work->timer);
    # endif /* ifdef PROFILING */

    // Free information
    if (work->info) c_free(work->info);

    // Free work
    c_free(work);
  }

}