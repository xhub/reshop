#include "asprintf.h"
#include "reshop_config.h"

#include <string.h>

#include "reshop.h"

#include "test_crm.h"
#include "test_common.h"

/*  For NAN */
#include "asnan.h"

struct tree {
   const int (*arcs)[3];
   unsigned n_nodes;
   unsigned n_nonleaf;
   struct rhp_avar *theta;
   struct rhp_avar *x;
   struct rhp_avar *y;
   struct rhp_avar *h;
   struct rhp_aequ *defState;
   struct rhp_aequ *objequs;
   struct rhp_aequ *objequCRM;
   struct rhp_empdag_arcVF *edgeVF;
   bool *visited;
   const double *prices;
   const double *rain;
   double hinit;
};


/* The EMPDAG are as follows:


            ┌─────┐
            │ MP5 │
            └─────┘
              ▲
              │
              │
┌─────┐     ┌─────┐     ┌─────┐
│ MP1 │ ──▶ │ MP2 │ ──▶ │ MP4 │
└─────┘     └─────┘     └─────┘
  │
  │
  ▼
┌─────┐     ┌─────┐
│ MP3 │ ──▶ │ MP6 │
└─────┘     └─────┘
  │
  │
  ▼
┌─────┐
│ MP7 │
└─────┘




                                                        ┌─────────┐
                                                        │   MP5   │
                                                        └─────────┘
                                                          ▲
                                                          │ y(MP5)
                                                          │
┌─────┐     ┌─────────┐  y(MP2)   ┌─────────┐           ┌─────────┐  y(MP4)   ┌─────┐
│ MP1 │ ──▶ │  CRM1   │ ────────▶ │   MP2   │ ────────▶ │  CRM2   │ ────────▶ │ MP4 │
└─────┘     └─────────┘           └─────────┘           └─────────┘           └─────┘
              │
              │ y(MP3)
              ▼
            ┌─────────┐           ┌─────────┐  y(MP6)   ┌─────────┐
            │   MP3   │ ────────▶ │  CRM3   │ ────────▶ │   MP6   │
            └─────────┘           └─────────┘           └─────────┘
                                    │
                                    │ y(MP7)
                                    ▼
                                  ┌─────────┐
                                  │   MP7   │
                                  └─────────┘


The two above graph are generated by https://dot-to-ascii.ggerganov.com/ using

digraph {
    rankdir="LR";
        
    MP1 -> MP2;
    MP1 -> MP3;
    
    MP2 -> MP4;
    MP2 -> MP5;
    
    MP3 -> MP6;
    MP3 -> MP7;
}

digraph {
    rankdir="LR";
        
    MP1  -> CRM1;
	 CRM1 -> MP2 [label="y(MP2)"];
    CRM1 -> MP3 [label="y(MP3)"];
    
    MP2  -> CRM2;
	 CRM2 -> MP4 [label="y(MP4)"];
	 CRM2 -> MP5 [label="y(MP5)"];
    
    MP3  -> CRM3;
	 CRM3 -> MP6 [label="y(MP6)"];
	 CRM3 -> MP7 [label="y(MP7)"];
}


 */

static const double hinit = 100;
static const double prices[] = { 8., 9., 10 };
static const double rain[] = { NAN, 20., 40., 30., 50., 30., 50.};
static const int arcs[][3] = { {1, 2, -1}, {3, 4, -1}, {5, 6, -1}, {-1}, {-1}, {-1}, {-1} };
static const unsigned n_nodes  = 7;
static const unsigned n_nonleaf = 3;
UNUSED static const unsigned n_stages = 3;
   /* TODO: have probabilities */
//  const double prob[] = { NAN, .7, .3, .4, .6 };

static void _tree_data_init3(struct tree *tree, bool *visited)
{
   tree->arcs = arcs;
   tree->n_nodes = n_nodes;
   tree->n_nonleaf = n_nonleaf;
   tree->visited = visited;
   tree->prices = prices;
   tree->rain = rain;
   tree->hinit = hinit;
}

static int _tree_init(struct rhp_mdl *mdl, struct tree *tree)
{
   int status = 0;
   tree->x = rhp_avar_new();
   tree->h = rhp_avar_new();
   tree->defState = rhp_aequ_new();

   RESHOP_CHECK(rhp_add_posvarsnamed(mdl, tree->n_nodes, tree->x, "x"));
   RESHOP_CHECK(rhp_add_posvarsnamed(mdl, tree->n_nodes, tree->h, "h"));
   RESHOP_CHECK(rhp_add_consnamed(mdl, tree->n_nodes, RHP_CON_EQ, tree->defState, "defState"));

_exit:
   return status;

}

static void _tree_free(struct tree *tree)
{
   rhp_avar_free(tree->x);
   rhp_avar_free(tree->h);
   rhp_avar_free(tree->theta);
   rhp_aequ_free(tree->defState);
   rhp_arcVF_free(tree->edgeVF);
}

static int _tree_init_ovf(struct rhp_mdl *mdl, struct tree *tree)
{
   int status = 0;
   tree->theta = rhp_avar_new();
   RESHOP_CHECK(rhp_add_varsnamed(mdl, tree->n_nonleaf, tree->theta, "theta"));

_exit:
   return status;
}

static int _tree_init_dag(struct rhp_mdl *mdl, struct tree *tree)
{
   int status = 0;

   tree->y = rhp_avar_new();
   RESHOP_CHECK(rhp_add_varsnamed(mdl, tree->n_nodes, tree->y, "y"));

   tree->objequs = rhp_aequ_new();
   RESHOP_CHECK(rhp_add_consnamed(mdl, tree->n_nodes, RHP_CON_EQ, tree->objequs, "objequ"));
   tree->objequCRM = rhp_aequ_new();
   RESHOP_CHECK(rhp_add_consnamed(mdl, tree->n_nonleaf, RHP_CON_EQ, tree->objequCRM, "objequCRM"));

   tree->edgeVF = rhp_arcVF_new();

_exit:
   return status;
}

static int _add_cons(struct rhp_mdl *mdl, struct tree *tree, unsigned child, unsigned parent)
{
   int status = 0;
   rhp_idx h_idx, x_child, defState, h_child;
   /* ---------------------------------------------------------------------
    * Define the equation of state: h(child) =E= h(n) - x(child) + rain(child)
    * --------------------------------------------------------------------- */

   RESHOP_CHECK(rhp_avar_get(tree->h, child, &h_child))
   RESHOP_CHECK(rhp_avar_get(tree->h, parent, &h_idx));
   RESHOP_CHECK(rhp_avar_get(tree->x, child, &x_child))
   RESHOP_CHECK(rhp_aequ_get(tree->defState, child, &defState));
   RESHOP_CHECK(rhp_equ_addnewlvar(mdl, defState, h_child, 1));
   RESHOP_CHECK(rhp_equ_addnewlvar(mdl, defState, h_idx, -1));
   RESHOP_CHECK(rhp_equ_addnewlvar(mdl, defState, x_child, 1));
   RESHOP_CHECK(rhp_mdl_setequrhs(mdl, defState, tree->rain[child]));

_exit:
   return status;
}

/**
 * @brief Explore a tree in a DFS fashion
 *
 * @param mdl     the model
 * @param tree    the scenario tree
 * @param depth   the depth of the scenario tree
 * @param nodeid  the node ID 
 *
 * @return  the error code
 */
static int DFS_ovf(struct rhp_mdl *mdl, struct tree *tree, unsigned depth, unsigned nodeid)
{
   int status = 0;
   char *defargname = NULL, *argtheta_name = NULL;
   struct rhp_avar *argTheta = NULL;
   struct rhp_aequ *defArgTheta = NULL;

   tree->visited[nodeid] = true;

   const int *children = tree->arcs[nodeid];

   unsigned n_children = 0;
   while (*children >= 0) {
      n_children++;
      children++;
   }

   if (n_children == 0) {
      return 0;
   }


   rhp_idx theta_idx;
   RESHOP_CHECK(rhp_avar_get(tree->theta, nodeid, &theta_idx));

   argTheta = rhp_avar_new();
   argtheta_name = strdup(rhp_mdl_printvarname(mdl, theta_idx));
   RESHOP_CHECK(rhp_add_varsnamed(mdl, n_children, argTheta, argtheta_name));

   if (asprintf(&defargname, "arg%s", argtheta_name) < 0) {
      (void)fprintf(stderr, "asprintf() failed in %s\n", __func__);
      return 1;
   }

   defArgTheta = rhp_aequ_new();
   RESHOP_CHECK(rhp_add_consnamed(mdl, n_children, RHP_CON_EQ, defArgTheta, defargname));

   children = tree->arcs[nodeid];
   int child = *children++;
   unsigned cidx = 0;

   while (child >= 0) {
      rhp_idx defargtheta, argtheta, x_child, theta_child;
      RESHOP_CHECK(rhp_aequ_get(defArgTheta, cidx, &defargtheta));
      RESHOP_CHECK(rhp_avar_get(argTheta, cidx, &argtheta));
      RESHOP_CHECK(rhp_avar_get(tree->x, child, &x_child))

      /* argtheta(child) =E= - price * x(child) + theta[child] */
      /* argtheta(child) + price * x(child) - theta[child] =E= 0*/
      RESHOP_CHECK(rhp_equ_addnewlvar(mdl, defargtheta, argtheta, 1));
      RESHOP_CHECK(rhp_equ_addnewlvar(mdl, defargtheta, x_child, tree->prices[depth+1]));
      if (tree->arcs[child][0] >= 0) {
         RESHOP_CHECK(rhp_avar_get(tree->theta, child, &theta_child));
         RESHOP_CHECK(rhp_equ_addnewlvar(mdl, defargtheta, theta_child, -1));
      }

      RESHOP_CHECK(_add_cons(mdl, tree, child, nodeid));
      RESHOP_CHECK(DFS_ovf(mdl, tree, depth+1, child));

      child = *children++;
      cidx++;
   }

   /* declare the CRM */
  struct rhp_ovfdef *ovf_def;
  RESHOP_CHECK(rhp_ovf_add(mdl, "ecvarup", theta_idx, argTheta, &ovf_def));
  RESHOP_CHECK(rhp_ovf_param_add_scalar(ovf_def, "tail", .2));
  RESHOP_CHECK(rhp_ovf_param_add_scalar(ovf_def, "risk_wt", .7));

_exit:
   free(defargname);
   free(argtheta_name);
   rhp_avar_free(argTheta);
   rhp_aequ_free(defArgTheta);

   return status;
}

static int _DFS_dag(struct rhp_mdl *mdl, struct tree *tree, unsigned depth, unsigned n)
{
   int status = 0;

   tree->visited[n] = true;

   const int *children = tree->arcs[n];

   unsigned n_children = 0;
   while (*children >= 0) {
      n_children++;
      children++;
   }

   rhp_idx defState_n, x_n, h_n, objequ_n;
   RESHOP_CHECK(rhp_avar_get(tree->x, n, &x_n));
   RESHOP_CHECK(rhp_avar_get(tree->h, n, &h_n));
   RESHOP_CHECK(rhp_aequ_get(tree->objequs, n, &objequ_n));
   RESHOP_CHECK(rhp_aequ_get(tree->defState, n, &defState_n));

   struct rhp_mathprgm *mp = rhp_empdag_newmp(mdl, RHP_MIN);

   RESHOP_CHECK(rhp_mp_addvar(mp, x_n));
   RESHOP_CHECK(rhp_mp_addvar(mp, h_n));
   RESHOP_CHECK(rhp_mp_addconstraint(mp, defState_n));
   RESHOP_CHECK(rhp_mp_setobjequ(mp, objequ_n));

    /* local objequ: - prices(n) * x(n) + VF(CRM(n)) */
    RESHOP_CHECK(rhp_equ_addnewlvar(mdl, objequ_n, x_n, -prices[depth]));

   /* if depth = 0, we are at the root and we set it as such in the EMPDAG */
   if (depth == 0) {
      RESHOP_CHECK(rhp_empdag_rootsetmp(mdl, mp));
   }

   if (n_children == 0) {
      return 0;
   }

   children = tree->arcs[n];
   int child = *children++;

   /* ---------------------------------------------------------------------
    * Define the CRM(n) node
    * --------------------------------------------------------------------- */
   rhp_idx objequCRM_n;
   struct rhp_mathprgm *crm = rhp_empdag_newmp(mdl, RHP_MIN);

   RESHOP_CHECK(rhp_aequ_get(tree->objequCRM, n, &objequCRM_n));
   RESHOP_CHECK(rhp_mp_setobjequ(crm, objequCRM_n));

   RESHOP_CHECK(rhp_arcVF_init(tree->edgeVF, objequ_n));
   RESHOP_CHECK(rhp_empdag_mpaddmpVF(mdl, mp, crm, tree->edgeVF));


   while (child >= 0) {
      rhp_idx y_child, x_child;

      RESHOP_CHECK(rhp_avar_get(tree->x, child, &x_child));
      RESHOP_CHECK(rhp_avar_get(tree->y, child, &y_child));

      /* Process the subdag */
      RESHOP_CHECK(_DFS_dag(mdl, tree, depth+1, child));

      /* We add the child defState from the parent, it's easier */
      RESHOP_CHECK(_add_cons(mdl, tree, child, n));

      struct rhp_mathprgm *mp_child = rhp_mdl_getmpforvar(mdl, x_child);
      if (!mp_child) { status = 1; goto _exit; }

      /* Add y(child) * vf(mp(child)) in the objequ of CRM(n) */
      RESHOP_CHECK(rhp_arcVF_init(tree->edgeVF, objequCRM_n));
      RESHOP_CHECK(rhp_arcVF_setvar(tree->edgeVF, y_child));
      RESHOP_CHECK(rhp_empdag_mpaddmpVF(mdl, crm, mp_child, tree->edgeVF));

      /* Add y(child) to the CRM */
      RESHOP_CHECK(rhp_mp_addvar(crm, y_child));

      RESHOP_CHECK(rhp_mdl_setvarbounds(mdl, y_child, 0, 1));

      child = *children++;
   }


_exit:

   return status;
}

/* Solve a simple hydro problem over a tree */
int test_ecvarup_msp_ovf(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver)
{
   int status = 0;

   bool visited[7] = {false};

   struct tree tree;
   memset(&tree, 0, sizeof(tree));
   _tree_data_init3(&tree, visited);
   RESHOP_CHECK(_tree_init(mdl, &tree));
   RESHOP_CHECK(_tree_init_ovf(mdl, &tree));

   struct rhp_avar *x = tree.x;
   struct rhp_avar *h = tree.h;
   struct rhp_avar *theta = tree.theta;
   struct rhp_aequ *defState = tree.defState;

   rhp_idx x0, theta_idx;
   RESHOP_CHECK(rhp_avar_get(x, 0, &x0));
   RESHOP_CHECK(rhp_avar_get(theta, 0, &theta_idx));

   /* ---------------------------------------------------------------------
    * Define the objective functional (first to avoid issue with checking)
    * objequ: - prices(0) * x(0) + theta(1)
    * --------------------------------------------------------------------- */

   rhp_idx objequ;
   RESHOP_CHECK(rhp_add_func(mdl, &objequ));

   RESHOP_CHECK(rhp_equ_addnewlvar(mdl, objequ, x0, -prices[0]));
   RESHOP_CHECK(rhp_equ_addnewlvar(mdl, objequ, theta_idx, 1));

   RESHOP_CHECK(rhp_mdl_setobjequ(mdl, objequ));
   RESHOP_CHECK(rhp_mdl_setobjsense(mdl, RHP_MIN));
   
   /* ---------------------------------------------------------------------
    * Define the equations of state and add first relation: h(0) =E= h_init - x(0)
    * --------------------------------------------------------------------- */

   rhp_idx state_init, h_0;
   RESHOP_CHECK(rhp_avar_get(h, 0, &h_0))
   RESHOP_CHECK(rhp_aequ_get(defState, 0, &state_init));
   RESHOP_CHECK(rhp_equ_addnewlvar(mdl, state_init, h_0, 1));
   RESHOP_CHECK(rhp_equ_addnewlvar(mdl, state_init, x0, 1))
   RESHOP_CHECK(rhp_mdl_setequrhs(mdl, state_init, hinit));

   RESHOP_CHECK(DFS_ovf(mdl, &tree, 0, 0));

   struct sol_vals solvals;
   sol_vals_init(&solvals);
   double xvals[23] = {
      0., 0., 0., 150, 170, 170, 190,     /* x */
      100, 120, 140, 0., 0., 0., 0.,   /* h */
      -1560, -1530, -1730,                /* theta */
      NAN, NAN, NAN, NAN, NAN, NAN,       /* argTheta */
   };
   double xduals[23] = {
      2, .85, .15, 0, 0, 0, 0,     /* x */
      0, 0, 0, 7.225, 1.275, 1.275, 0.225,   /* h */
      0, 0, 0,                /* theta */
   };
   solvals.xvals = xvals;
   solvals.xduals = xduals;
   solvals.objval = -1560;

   status = test_solve(mdl, mdl_solver, &solvals);
_exit:
   _tree_free(&tree);

   return status;
}

/* Solve a simple hydro problem over a tree */
UNUSED static int test_ecvarup_msp_dag(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver)
{
   int status = 0;

   bool visited[7] = {false};

   struct tree tree;
   memset(&tree, 0, sizeof(tree));
   _tree_data_init3(&tree, visited);
   RESHOP_CHECK(_tree_init(mdl, &tree));
   RESHOP_CHECK(_tree_init_dag(mdl, &tree));

   RESHOP_CHECK(_DFS_dag(mdl, &tree, 0, 0));

   struct sol_vals solvals;
   sol_vals_init(&solvals);
   double xvals[23] = {
      0., 0., 0., 150, 170, 170, 190,     /* x */
      100, 120, 140, 0., 0., 0., 0.,   /* h */
      NAN, NAN, NAN, NAN               /* y */
   };
   double xduals[23] = {
      2, .85, .15, 0, 0, 0, 0,     /* x */
      0, 0, 0, 7.225, 1.275, 1.275, 0.225,   /* h */
      NAN, NAN, NAN, NAN               /* y */
   };

   solvals.xvals = xvals;
   solvals.xduals = xduals;
   solvals.objval = -1560;

   status = test_solve(mdl, mdl_solver, &solvals);
_exit:
   _tree_free(&tree);
   return status;
}
