#include "asprintf.h"

#include "gams_nlcode.h"
#include "gams_nlcode_priv.h"
#include "instr.h"
#include "macros.h"
#include "mdl.h"
#include "nltree_style.h"
#include "pool.h"
#include "printout.h"
#include "win-compat.h"


#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>
#include <fileapi.h>

#define fileno(X) (X)
#define fsync(X) !FlushFileBuffers((HANDLE)(X))

#else

#include <unistd.h>

#endif


#define  NLCODE_DEG_DEBUG(instr, args, degree, stack, stack_arr) { \
  printf("degree: %3u\tinstr: %10s\targs: [%5u]\tstack:", degree, nlinstr2str(instr), args); \
  for (u8 i42 = 0, len42 = (u8)(stack-stack_arr); i42 < len42; ++i42) { \
      (void)printf(" %u: %u,", i42, stack_arr[i42]); \
   } \
   (void)putchar('\n'); \
}

#define NLCODE_DEBUG(...) printf(__VA_ARGS__);


tlsvar char varName[3*sizeof(unsigned)*CHAR_BIT/8+5];
//char cstString[3*sizeof(unsigned)*CHAR_BIT/8+5];

static const char * opcode2name(int opcode, int arg)
{
   switch (opcode) {
      case nlPushV:
      case nlPushI:
         return "";
      case nlAdd:
      case nlAddV:
      case nlAddI:
      case nlMulIAdd:
         return "ADD";
      case nlSub:
      case nlSubV:
      case nlSubI:
         return "SUB";
      case nlMul:
      case nlMulV:
      case nlMulI:
         return "MUL";
      case nlDiv:
      case nlDivV:
      case nlDivI:
         return "DIV";
      case nlUMin:
      case nlUMinV:
         return "UMIN";
      case nlCallArg1:
      case nlCallArg2:
      case nlCallArgN:
         return nlopcode_argfunc2str(arg);
      case nlFuncArgN:
         return "FUNC N";
      default:
         return "ERROR";
   }
}

static void opcode2arg(int opcode, int arg, u32 sz, char oparg[VMT(static sz)],
                       char nodestyle[VMT(static sz)], double *pool, Model *mdl)
{
   const char *varname;

   switch (opcode) {
      case nlPushV:
         if (mdl) {
            varname = mdl_printvarname(mdl, arg-1);
         } else {
            (void)snprintf(varName, sizeof(varName), "x%d", arg);
            varname = varName;
         }

         (void)snprintf(oparg, sz, "%u\\n %s", arg, varname);
         (void)snprintf(nodestyle, sz, ",color=%s", nlTreeStyle_VarNode);
         break;

      case nlAddV:
      case nlSubV:
      case nlMulV:
      case nlDivV:
      case nlUMinV:
         oparg[0] = '\0';
         (void)snprintf(nodestyle, sz, ",color=%s", nlTreeStyle_HasVarArg);
         break;

      case nlPushI:
         if (pool) {
            (void)snprintf(oparg, sz, "%d\\n%.2g", arg, pool[arg-1]);
         } else {
            (void)snprintf(oparg, sz, "%d\\npool %d", arg, arg);
         }
         (void)snprintf(nodestyle, sz, ",color=%s", nlTreeStyle_CstNode);
         break;

      case nlAddI:
      case nlSubI:
      case nlMulI:
      case nlDivI:
         oparg[0] = '\0';
         (void)snprintf(nodestyle, sz, ",color=%s", nlTreeStyle_HasCstArg);
         break;

      case nlAdd:
      case nlSub:
      case nlMul:
      case nlDiv:
      case nlMulIAdd:
      case nlUMin:
      default:
         oparg[0] = '\0';
         nodestyle[0] = '\0';

   }
}

typedef struct {
   u32 idx_sz;      /**< Size of the index vector                    */
   u32 stack_maxlen;    /**< Size of the stack for processing the opcode */
} OpCodeTreeSizes;

/**
 * @brief Return the size of the idx field for the tree
 *
 * @param len    the lengths
 * @param instr  the intruction array
 * @param args   the args array
 *
 * @return       if there was an error, UINT_MAX; otherwise the size of idx field
 */
static OpCodeTreeSizes compute_nlcode_tree_sizes(int len, const int instr[VMT(restrict len)],
                                                 const int args[VMT(restrict len)])
{
   OpCodeTreeSizes sizes = {0};
   u8 stack_depth = 0;

   for (int i = 0; i < len; ++i, instr++, args++) {
      switch (*instr) {
      case nlNoOp:
      case nlHeader:
      case nlStore:
      case nlEnd:
         break; /* Do nothing */
      case nlUMinV:
      case nlPushV:
      case nlPushI:
      case nlPushZero:
         stack_depth++;

         if (stack_depth > sizes.stack_maxlen) {
               sizes.stack_maxlen = stack_depth;
         }

         break;
      case nlAddV:
      case nlAddI:
      case nlSubV:
      case nlSubI:
      case nlMulV:
      case nlMulI:
      case nlDivV:
      case nlDivI:
      case nlUMin:
      case nlCallArg1:
         sizes.idx_sz += 1;
         break;
      case nlMulIAdd:
      case nlAdd:
      case nlSub:
      case nlMul:
      case nlDiv:
      case nlCallArg2:
         assert(stack_depth >= 2);
         sizes.idx_sz += 2;
         stack_depth -= 1;
         break;
      case nlCallArgN:
      case nlFuncArgN:
         assert(stack_depth >= *args);
         sizes.idx_sz += *args;
         stack_depth -= (*args-1);
         break;
      case nlChk:
      case nlAddO:
      case nlPushO:
      case nlInvoc:
      case nlStackIn:
      case nlOpcode_size:
      default:
         error("[GAMS nlnode] ERROR: unsupported opcode #%d", *instr);
         return (OpCodeTreeSizes){UINT_MAX, UINT_MAX};
      }
   }

   if (stack_depth != 1) {
      error("[nlopcode] ERROR: stack depth at the end should be 1, got %d",
            stack_depth);
   }

   return sizes;
}

static inline u8 u8lifo_pop(u8 **lifo) { 
   (*lifo)--;
   u8 res = **lifo;
   NLCODE_DEBUG("STACK_POP: %u\n", res);
   return res;
}

static inline u8 u8lifo_popN(u8 **lifo, int nargs, const u8 *lifo_top) {
   if (lifo_top + nargs > *lifo) {
      return UINT8_MAX;
   }

   *lifo -= nargs;
   return 0;
}

static inline void u8lifo_push(u8 **lifo, u8 elt) {
   NLCODE_DEBUG("STACK_PUSH: %u\n", elt);
   **lifo = elt; (*lifo)++;
}

static inline u32 u32lifo_pop(u32 **lifo) { 
   (*lifo)--;
   u32 res = **lifo;
   NLCODE_DEBUG("STACK_POP: %u\n", res);
   return res;
}

static inline void u32lifo_push(u32 **lifo, u32 elt) {
   NLCODE_DEBUG("STACK_PUSH: %u\n", elt);
   **lifo = elt; (*lifo)++;
}

#ifdef UNUSED_2025_05_16
static inline void u32lifo_pushN(u32 **lifo, u32 elt, u8 n) {
   NLCODE_DEBUG("STACK_PUSH: %u; %u times\n", elt, n);
   for (u8 i = 0; i < n; ++i) {
      **lifo = elt; (*lifo)++;
   }
}

static inline u8 degree_inc(u8 deg)
{
   if (deg >= DefMaxPoly) { return deg; }

   return deg+1;
}
#endif

static inline u8 degree_add(u8 degL, u8 degR)
{
   if (degL >= DefMaxPoly || degR >= DefMaxPoly) {
      return (degL >= DefMaxPoly) ? degL : degR;
   }

   return MAX(degL, degR);
}

static inline u8 degree_mul(u8 degL, u8 degR)
{
   if (degL >= DefMaxPoly || degR >= DefMaxPoly) {
      return (degL >= DefMaxPoly) ? degL : degR;
   }

   u16 degmul = ((u16)degL) + ((u16)degR);

   return (degmul >= DefMaxPoly) ? DefMaxPoly : (u8)degmul;
}

#define get_numer(x)  ((x) & 0x7f) >> 3

static inline u8 degree_div(u8 degNumer, u8 degDenom)
{
   if (degDenom == 0) { return degNumer; }

   if (degNumer <= DefMaxPoly && degDenom <= DefMaxPoly) {

      /* We cap the tracking of the degrees at 7*/
      if (degNumer > 1 << 3) { degNumer = 0xF; }
      if (degDenom > 1 << 3) { degDenom = 0xF; }

      return deg_mkdiv(degNumer, degDenom);
   }

   return DegFullyNl;


//   if ((degNumer & DegDiv)) {
//      u16 newNumer = get_numer()
//
//   } || (degDenom & DegDiv)) {
//   }
}

DBGUSED static inline bool valid_stack(u8 *stack, u8 *stack_top, u8 sz) {
   return (stack >= stack_top && stack - stack_top < sz);
}

DBGUSED static inline bool nonempty_stack(u8 *stack, u8 *stack_top) {
   return (stack > stack_top);
}

//static OpCodeTreeSizes compute_optree_nodal_degrees(int len, int instr[VMT(restrict len)],
//                                                    int args[VMT(restrict len)],
//                                                    u8 degrees[VMT(restrict len)])
//{
//}

static int create_dotfile(Model *mdl, int idx, char **fname_dot)
{
   char *export_dir;
   bool free_export_dir;
   int status = OK;
   if (mdl) {
 
      S_CHECK(mdl_ensure_exportdir(mdl));
      export_dir = mdl->commondata.exports_dir;
      free_export_dir = false;

   } else {

      free_export_dir = true;
#if defined(__APPLE__) || (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200809L)

      char *exports_dir_template;

      A_CHECK(exports_dir_template, strdup("/tmp/reshop_exports_XXXXXX"));
      export_dir = mkdtemp(exports_dir_template);

      if (!export_dir) {
         perror("mkdtemp");
         free(exports_dir_template);
         return Error_SystemError;
      }

#elif defined(_WIN32)

   A_CHECK(export_dir, win_gettmpdir());
   if (mkdir(export_dir, S_IRWXU)) {
      perror("mkdir");
      error("\n[system] ERROR: Could not create directory '%s'\n", export_dir);
      status = Error_SystemError;
      goto _exit;
   }

#else

      fputs("\nNo model provided and unsupported platform regarding temporary directory\n", stderr)
      goto _exit;

#endif

   }

   IO_CALL_EXIT(asprintf(fname_dot, "%s" DIRSEP "nlcode%d.dot", export_dir, idx));

   if (free_export_dir) { free(export_dir); }

   return OK;

_exit:
   if (free_export_dir) { free(export_dir); }

   return status;
}

u32 compute_nlcode_degree(int len, const int instr[VMT(restrict len)],
                          const int args[VMT(restrict len)])
{
   u8 stack_arr[25];
   u8 *stack_top = stack_arr, *stack = stack_arr;
   u8 cur_degree = 0, Lchild_degree = 0;

   for (int i = 0; i < len; ++i, instr++, args++) {
      NLCODE_DEG_DEBUG(*instr, *args, cur_degree, stack, stack_arr);

      switch (*instr) {
      case nlNoOp:
      case nlHeader:
      case nlStore:
      case nlEnd:
         break; /* Do nothing */
      case nlUMin:
         break; /* Do nothing */
      case nlAddI:
      case nlSubI:
      case nlMulI:
      case nlDivI:
         break; /* Constant-based op: changes nothing */
      case nlPushI:
      case nlPushZero:
         u8lifo_push(&stack, cur_degree); assert(valid_stack(stack, stack_top, sizeof(stack)));
         cur_degree = 0; /* Reset degree */
      break;
      case nlUMinV:
      case nlPushV:
         u8lifo_push(&stack, cur_degree); assert(valid_stack(stack, stack_top, sizeof(stack)));
         cur_degree = 1; /* Reset degree */
      break;
      case nlAddV:
      case nlSubV:
         cur_degree++; break;
      case nlMulV:
         cur_degree = degree_mul(cur_degree, 1); break;
      case nlDivV:
         cur_degree = degree_div(cur_degree, 1); break;

      case nlMulIAdd:
      case nlAdd:
      case nlSub:
         assert(nonempty_stack(stack, stack_top));
         Lchild_degree = u8lifo_pop(&stack);
         cur_degree = degree_add(Lchild_degree, cur_degree);
         break;

      case nlMul:
         assert(nonempty_stack(stack, stack_top));
         Lchild_degree = u8lifo_pop(&stack);
         cur_degree = degree_mul(Lchild_degree, cur_degree);
         break;
      case nlDiv:
         assert(nonempty_stack(stack, stack_top));
         Lchild_degree = u8lifo_pop(&stack);
         cur_degree = degree_div(Lchild_degree, cur_degree);
         break;

      case nlCallArg1:
         if (*args == fnsqr) {
            cur_degree = degree_mul(cur_degree, cur_degree);
         } else {
            /* constant case */
            cur_degree = cur_degree == 0 ? 0 : DegFullyNl;
         }
         break;

      case nlCallArg2: {
         u8 degarg1 = u8lifo_pop(&stack); assert(valid_stack(stack, stack_top, sizeof(stack)));
         switch (*args) {
            /* power cases: X **Y where X is stack[-1], Y is stack[0].
             * The idea is to get an accurate degree computation only for a few basic cases  */
            case fnpower:
            case fnrpower:
            case fncvpower:
            case fnvcpower: {

               int Yinstr = instr[-1];
               int poolIdx = args[-1];
               bool arg2IsCst = (Yinstr == nlPushI);

               /* Y = 0; remember that pool indices are 1-based */
               if ((arg2IsCst && (poolIdx == 8)) || (Yinstr == nlPushZero)) {

                  cur_degree = 0;

               } else if (arg2IsCst) {

                  switch (poolIdx) {
                  case 1:  cur_degree = degarg1; break;                /* X^1 */
                  case 6:  cur_degree = degree_mul(degarg1, degarg1); break; /* X^2 */
                  case 7:  cur_degree = degree_mul(degree_mul(degarg1, degarg1), degree_mul(degarg1, degarg1)); break; /* X^3 */
                  case 15: cur_degree = degree_mul(degarg1, degree_mul(degarg1, degarg1)); break; /* X^4 */
                  default: cur_degree = DegFullyNl;
                  };
               }
            }
         break;
         default:
            /* constant case: degree remains constant */
            cur_degree = cur_degree == 0 ? 0 : DegFullyNl;
         }
         break;
      }
      case nlCallArgN:
      case nlFuncArgN: {
         int nargs = *args; assert(stack-stack_top >= nargs);
         int rc = u8lifo_popN(&stack, nargs, stack_top);
         if (rc) {
            error("[nlopcode] ERROR: stack depth should be at least %d, got %d",
                  nargs, (int)(stack-stack_top));
            return UINT32_MAX;
         }

         /* constant case remains constant */
         cur_degree = cur_degree == 0 ? 0 : DegFullyNl;
         break;
      }

      case nlChk:
      case nlAddO:
      case nlPushO:
      case nlInvoc:
      case nlStackIn:
      case nlOpcode_size:
      default:
         error("[GAMS nlnode] ERROR: unsupported opcode #%d", *instr);
         return UINT32_MAX;
      }
   }

   if (stack - stack_top != 1) {
      error("[nlopcode] ERROR: stack depth at the end should be 1, got %d",
            (int)(stack - stack_top));
      return UINT32_MAX;
   }

   return cur_degree;
}

int gams_nlcode2dot(Model *mdl, const int * restrict instr,
                    const int * restrict args, char **fname_dot)
{
   M_ArenaTempStamp arena_stamp;

   if (mdl) {
      arena_stamp = mdl_memtmp_init(mdl);
   }

   FILE* f = NULL;
   char *fname = NULL;
   u32 *idx_lifo_stack = NULL;
   u8  *ind_start  = NULL;
   *fname_dot = NULL;

  /* ----------------------------------------------------------------------
   * The output operate in two phases:
   * - define all the nodes
   * - define all the arcs
   *
   * During the arc definition, we need a node stack to print both children
   * as we only explore the left one when we follow the nodes
   *
   * We support the case where model is NULL to allow the usage of this function
   * in unit tests
   * ---------------------------------------------------------------------- */
 
   int len = args[0], ei = args[len-1], status = OK;

   S_CHECK_EXIT(create_dotfile(mdl, ei, &fname));
   *fname_dot = fname;

   f = fopen(fname, "w");

   if (!f) {
      error("[system] ERROR: Could not create file named '%s'\n", fname);
      status = Error_SystemError;
      goto _exit;
   }

   IO_CALL_EXIT(fputs("digraph structs {\n node [shape=\"plaintext\", style=\"filled, rounded\", margin=0.2];\n", f));

   if (mdl) {
      IO_CALL_EXIT(fprintf(f, "label=\"%s model '%.*s' #%u: equation '%s'\";", mdl_fmtargs(mdl), mdl_printequname(mdl, ei)));
   }

   OpCodeTreeSizes tree_sizes = compute_nlcode_tree_sizes(len, instr, args);
   u32 idx_lifo_stacklen = MAX(tree_sizes.stack_maxlen, 1);


   if (mdl) {
      idx_lifo_stack = mdl_memtmp_get(mdl, idx_lifo_stacklen*sizeof(u32));
      ind_start = mdl_memtmp_get(mdl, idx_lifo_stacklen);
   } else {
      MALLOC_(idx_lifo_stack, u32, idx_lifo_stacklen);
      MALLOC_(ind_start, u8, idx_lifo_stacklen);
   }

   double *pool = NULL;

   if (mdl) {
      NlPool *nlpool = mdl_getpool(mdl);
      if (nlpool) {
         pool = nlpool->data;
      }
   }

   char oparg[512];
   char style[512];

  /* ----------------------------------------------------------------------
   * In a single pass, create nodes and arcs.
   * We create the nodes from leafs to root. Graphviz seems to order the
   * children according to their creation (?)
   * ---------------------------------------------------------------------- */

   u32 cidx = 0, sidx;
   u32 *idx_lifo = idx_lifo_stack;

   u32 nidx = 1;

   while (nidx < len-1) {

      int opcode = instr[nidx], arg = args[nidx];

      const char *opname = opcode2name(opcode, arg);
      opcode2arg(opcode, arg, sizeof oparg, oparg, style, pool, mdl);

     /* ----------------------------------------------------------------------
      * Print name
      * ---------------------------------------------------------------------- */
 
      (void)fprintf(f, " A%d [label=\"%s %s\" %s];\n", nidx, opname, oparg, style);


      /* Create arc to parent */
 
      switch (opcode) {
         case nlUMinV:
            opname = opcode2name(nlPushV, arg);
            opcode2arg(nlPushV, arg, sizeof oparg, oparg, style, pool, mdl);
            (void)fprintf(f, " AR%d [label=\"%s %s\" %s];\n", nidx, opname, oparg, style);
            (void)fprintf(f, "A%d -> AR%d;\n", nidx, nidx);
            FALLTHRU
         case nlPushV:
         case nlPushI:
         case nlPushZero:
            u32lifo_push(&idx_lifo, cidx);
            break;

         case nlAddV:
         case nlSubV:
         case nlMulV:
         case nlDivV:
            opname = opcode2name(nlPushV, arg);
            opcode2arg(nlPushV, arg, sizeof oparg, oparg, style, pool, mdl);
            (void)fprintf(f, " AR%d [label=\"%s %s\" %s];\n", nidx, opname, oparg, style);
            (void)fprintf(f, "A%d -> A%d;\n", nidx, cidx);
            (void)fprintf(f, "A%d -> AR%d;\n", nidx, nidx);
            break;
         case nlAddI:
         case nlSubI:
         case nlMulI:
         case nlDivI:
            opname = opcode2name(nlPushI, arg);
            opcode2arg(nlPushI, arg, sizeof oparg, oparg, style, pool, mdl);
            (void)fprintf(f, " AR%d [label=\"%s %s\" %s];\n", nidx, opname, oparg, style);
            (void)fprintf(f, "A%d -> A%d;\n", nidx, cidx);
            (void)fprintf(f, "A%d -> AR%d;\n", nidx, nidx);
         break;
 
         case nlMulIAdd:
            opname = opcode2name(nlMul, arg);
            (void)snprintf(style, sizeof style, ",color=%s", nlTreeStyle_MulIAdd);
            (void)fprintf(f, " AR_MUL%d [label=\"%s %s\" %s];\n", nidx, opname, oparg, style);
 
            opname = opcode2name(nlPushI, arg);
            opcode2arg(nlPushI, arg, sizeof oparg, oparg, style, pool, mdl);
            (void)fprintf(f, " AR_MUL_R%d [label=\"%s %s\" %s];\n", nidx, opname, oparg, style);

            assert(idx_lifo > idx_lifo_stack);
            sidx = u32lifo_pop(&idx_lifo);
            (void)fprintf(f, "A%d -> A%d;\n", nidx, sidx);
            (void)fprintf(f, "A%d -> AR_MUL%d;\n", nidx, nidx);
            (void)fprintf(f, "AR_MUL%d -> AR_MUL_R%d;\n", nidx, nidx);
            (void)fprintf(f, "AR_MUL%d -> A%d;\n", nidx, cidx);
            break;
 
         case nlAdd:
         case nlSub:
         case nlMul:
         case nlDiv:
            assert(idx_lifo > idx_lifo_stack);
            sidx = u32lifo_pop(&idx_lifo);
            (void)fprintf(f, "A%d -> A%d;\n", nidx, sidx);
            (void)fprintf(f, "A%d -> A%d;\n", nidx, cidx);
            break;

         case nlUMin:
         case nlCallArg1:
            (void)fprintf(f, "A%d -> A%d;\n", nidx, cidx);
         break;

         case nlCallArg2:
            assert(idx_lifo > idx_lifo_stack);
            // FIXME: clarify if need to pop once or twice
            sidx = u32lifo_pop(&idx_lifo);
            (void)fprintf(f, "A%d -> A%d;\n", nidx, sidx);
            (void)fprintf(f, "A%d -> A%d;\n", nidx, cidx);
            break;

         case nlCallArgN:
         case nlFuncArgN:
         assert(idx_lifo + args[nidx] > idx_lifo_stack);
         // FIXME: clarify how many times we want to pop
            for (u8 j = 0, jlen = args[nidx] - 1; j < jlen; j++) {
               sidx = u32lifo_pop(&idx_lifo);
               (void)fprintf(f, "A%d -> A%d;\n", nidx, sidx);
            }
            (void)fprintf(f, "A%d -> A%d;\n", nidx, cidx);
         break;

         default: ;
 
      }

      /* Save current opcode as parent; maybe overriden later*/
      cidx = nidx;
      nidx++;
 
      assert(idx_lifo >= idx_lifo_stack);
   }

   if (idx_lifo != idx_lifo_stack + 1) {

      error("[nlcode] ERROR: expecting 1 value left in the stack, got %zd\n", idx_lifo - idx_lifo_stack);
      status = Error_RuntimeError;

   } else if (idx_lifo_stack[0] != 0) {

      error("[nlcode] ERROR: expecting left-over stack to be 0, got %u\n", idx_lifo_stack[0]);
      status = Error_RuntimeError;

   }

   IO_CALL_EXIT(fputs("\n}\n", f));

_exit:

   if (mdl) {
      mdl_memtmp_fini(arena_stamp);
   } else {
      free(idx_lifo_stack);
      free(ind_start);
   }

   if (status != OK) {
      free(fname);
      *fname_dot = NULL;
   } 

   if (f) {
      SYS_CALL(fflush(f));
      SYS_CALL(fsync(fileno(f)));
      SYS_CALL(fclose(f));
   }

   return status;
}

int gams_opcodetree2dot(Model *mdl, GamsOpCodeTree *otree, char **fname_dot)
{
   int status = OK;

   char *fname;
   FILE *f = NULL;
   rhp_idx ei = otree->idx;
   S_CHECK(create_dotfile(mdl, ei, &fname));
   *fname_dot = fname;

   f = fopen(fname, "w");

   if (!f) {
      error("[system] ERROR: Could not create file named '%s'\n", fname);
      status = Error_SystemError;
      goto _exit;
   }

   IO_CALL_EXIT(fputs("digraph structs { ordering=out;\n node [shape=\"plaintext\", style=\"filled, rounded\", margin=0.2];\n", f));

   if (mdl) {
      IO_CALL_EXIT(fprintf(f, "label=\"%s model '%.*s' #%u: equation '%s'\";", mdl_fmtargs(mdl), mdl_printequname(mdl, ei)));
   }

   double *pool = NULL;

   if (mdl) {
      NlPool *nlpool = mdl_getpool(mdl);
      if (nlpool) {
         pool = nlpool->data;
      }
   }

   char oparg[512];
   char style[512];

  /* ----------------------------------------------------------------------
   * In a single pass, create nodes and arcs.
   * We create the nodes from leafs to root. Graphviz seems to order the
   * children according to their creation (?)
   * ---------------------------------------------------------------------- */

   u32 *p = otree->p+1, *i = otree->i, cidx = UINT32_MAX;
   const int *instr = otree->instr, *args = otree->args;

   for (u32 nidx = 1, len = otree->len; nidx < len-1; ++nidx) {

      int opcode = instr[nidx], arg = args[nidx];

      const char *opname = opcode2name(opcode, arg);
      opcode2arg(opcode, arg, sizeof oparg, oparg, style, pool, mdl);

      /* Print name */
      (void)fprintf(f, " A%d [label=\"%s %s\" %s];\n", nidx, opname, oparg, style);

      /* Print arcs. nlMulIAdd is very special ... */
      u32 fma_offset = opcode == nlMulIAdd ? 1 : 0;

      for (u32 j = *p, jmax = *++p; j < jmax-fma_offset; ++j) {
         (void)fprintf(f, " A%d -> A%d;\n", nidx, i[j]);
      }

      /* For nlMulIAdd: the right arc is from a "implicit" node to a child */
      if (fma_offset) {
         assert(*p - p[-1] > 1);
         cidx = i[*p-1];
      }

      /* Create special arcs */
 
      switch (opcode) {
         case nlAddV:
         case nlSubV:
         case nlMulV:
         case nlDivV:
         case nlUMinV:
            opname = opcode2name(nlPushV, arg);
            opcode2arg(nlPushV, arg, sizeof oparg, oparg, style, pool, mdl);
            (void)fprintf(f, " AR%d [label=\"%s %s\" %s];\n", nidx, opname, oparg, style);
            (void)fprintf(f, "A%d -> AR%d;\n", nidx, nidx);
         break;

         case nlAddI:
         case nlSubI:
         case nlMulI:
         case nlDivI:
            opname = opcode2name(nlPushI, arg);
            opcode2arg(nlPushI, arg, sizeof oparg, oparg, style, pool, mdl);
            (void)fprintf(f, " AR%d [label=\"%s %s\" %s];\n", nidx, opname, oparg, style);
            (void)fprintf(f, "A%d -> AR%d;\n", nidx, nidx);
         break;
 
         case nlMulIAdd:
            opname = opcode2name(nlMul, arg);
            (void)snprintf(style, sizeof style, ",color=%s", nlTreeStyle_MulIAdd);
            (void)fprintf(f, " AR_MUL%d [label=\"%s %s\" %s];\n", nidx, opname, oparg, style);
 
            opname = opcode2name(nlPushI, arg);
            opcode2arg(nlPushI, arg, sizeof oparg, oparg, style, pool, mdl);
            (void)fprintf(f, " AR_MUL_R%d [label=\"%s %s\" %s];\n", nidx, opname, oparg, style);

            (void)fprintf(f, "A%d -> AR_MUL%d;\n", nidx, nidx);
            (void)fprintf(f, "AR_MUL%d -> A%d;\n", nidx, cidx);
            (void)fprintf(f, "AR_MUL%d -> AR_MUL_R%d;\n", nidx, nidx);
         break;
 
         default: ;
 
      }

   }

   IO_CALL_EXIT(fputs("\n}\n", f));

_exit:

   if (f) {
      SYS_CALL(fflush(f));
      SYS_CALL(fsync(fileno(f)));
      SYS_CALL(fclose(f));
   }

   return status;
}

static inline u32 otree_len(GamsOpCodeTree *otree)
{
   return otree->len;
}

int gams_otree2instrs(Model *mdl, GamsOpCodeTree *otree, int **instrs, int **args)
{
   int status = OK;

   rhp_idx ei = otree->idx;
   int *instrs_, *args_, *pinstrs, *pargs;

   /* TODO: do a smart allocation if we have a model */
   u32 len = otree_len(otree);

   int *idat;
   MALLOC_(idat, int, 2*len);
   instrs_ = idat;
   args_ = idat+len;

  /* ----------------------------------------------------------------------
   * Get LIFO for DFS
   * ---------------------------------------------------------------------- */

   u32 * restrict lifo, * restrict lifo_start;
   M_ArenaTempStamp arena_stamp;

   if (mdl) {
      arena_stamp = mdl_memtmp_init(mdl);
      lifo = mdl_memtmp_get(mdl, len*sizeof(u32));
   } else {
      MALLOC_EXIT(lifo, u32, len);
   }

   lifo_start = lifo;
   *lifo++ = UINT32_MAX;

   instrs_[0] = nlHeader;
   args_[0]   = (int)len;

   pinstrs = &instrs_[len-1];
   pargs   = &args_[len-1];

   *pinstrs-- = nlStore;
   *pargs--   = ei;

   u32 * restrict p = otree->p, * restrict i = otree->i;
   const int * restrict tinstrs = otree->instr, * restrict targs = otree->args;

   u32 nidx = otree->root;

  /* ----------------------------------------------------------------------
   * Procedure: we start a DFS from the root, exploring children from last
   * to first
   * ---------------------------------------------------------------------- */

   do {

      /* perform action: copy nlinstr and arg */
      *pinstrs-- = tinstrs[nidx];
      *pargs-- = targs[nidx];

      /* deal with DFS: update nidx and store chidlren in the lifo*/
      u32 j = p[nidx], jmax = p[nidx+1];

      NLCODE_DEBUG("DEBUG: node %5u: nlintr: %10s; arg: %5d; children: [%u,%u)\n",
                   nidx, nlinstr2str(tinstrs[nidx]), targs[nidx], j, jmax);

      if (j == jmax) { /* No child: pop from LIFO */

         nidx = *--lifo;

      } else { /* process children: store in nidx the last one, push any other on stack */

         u32 *imax = &i[jmax-1];
         nidx = *imax;

         /* put on the lifo the excess children */
         for (u32 *iptr = &i[j]; iptr < imax;) {
            *lifo++ = *iptr++;
         }

      }

      assert(pinstrs >= instrs_);
      assert(pargs >= args_);

   } while (lifo > lifo_start);

   assert(nidx == UINT32_MAX);

_exit:
   if (mdl) {
      mdl_memtmp_fini(arena_stamp);
   } else {
      free(lifo);
   }

   if (status != OK) {
      free(idat);
   } else {
      *instrs = instrs_;
      *args   = args_;
   }

   return status;
}

void gams_opcodetree_free(GamsOpCodeTree *otree)
{
   if (otree) {
      free(otree->p);
      free(otree);
   }
}

GamsOpCodeTree* gams_opcodetree_new(int len, const int instr[VMT(static restrict len)],
                                    const int args[VMT(static restrict len)])
{
   OpCodeTreeSizes tree_sizes = compute_nlcode_tree_sizes(len, instr, args);

   GamsOpCodeTree *otree;
   CALLOC_NULL(otree, GamsOpCodeTree, 1);

   u32 *dat, *node_stack;
   MALLOC_EXIT_NULL(dat, u32, tree_sizes.stack_maxlen + tree_sizes.idx_sz + len + 1);
   node_stack = &dat[tree_sizes.idx_sz + len + 1];

   otree->idx = args[len-1];
   otree->len = len;
   otree->idx_sz = tree_sizes.idx_sz;
   otree->instr = instr;
   otree->args = args;
   otree->p = dat;
   otree->i = &dat[len+1];
   otree->node_stack = node_stack;
   otree->root = len-2;

  /* ----------------------------------------------------------------------
   * We need to build the index and pointer arrays. Algorithm:
   * - go from leafs to root
   * - each time a leaf node is encounter, stack the previous index
   * - nodes that consume stack 
   * ---------------------------------------------------------------------- */

   u32 cidx = 0, sidx, nidx = 1, ipos = 0;
   u32 *idx_lifo = node_stack, *p = dat+2, *i = &dat[len+1];
   dat[0] = 0;
   dat[1] = 0;

   while (nidx < len) {

      int opcode = instr[nidx];

      /* Create arc to parent */
 
      switch (opcode) {
         /* leaf node */
         case nlUMinV:
         case nlPushV:
         case nlPushI:
         case nlPushZero:
            u32lifo_push(&idx_lifo, cidx);
            break;

         case nlAddV:
         case nlSubV:
         case nlMulV:
         case nlDivV:
         case nlAddI:
         case nlSubI:
         case nlMulI:
         case nlDivI:
         case nlUMin:
         case nlCallArg1:
            i[ipos++] = cidx;
         break;
 
         case nlMulIAdd:
         case nlAdd:
         case nlSub:
         case nlMul:
         case nlDiv:
         case nlCallArg2:
            sidx = u32lifo_pop(&idx_lifo);
            i[ipos++] = sidx;
            i[ipos++] = cidx;
            break;
 
         case nlCallArgN:
         case nlFuncArgN:
         // FIXME: clarify how many times we want to pop
            for (u8 j = 0, jlen = args[nidx] - 1; j < jlen; j++) {
               sidx = u32lifo_pop(&idx_lifo);
               i[ipos++] = sidx;
            }
            *p++ = ipos;
         break;

         default: ;
 
      }

      *p++ = ipos;

      cidx = nidx;
      nidx++;
 
      assert(idx_lifo >= node_stack);
   }

   if (idx_lifo != node_stack + 1) {

      error("[nlcode] ERROR: expecting 1 value left in the stack, got %zd\n", idx_lifo - node_stack);
      goto _exit;

   } else if (node_stack[0] != 0) {

      error("[nlcode] ERROR: expecting left-over stack to be 0, got %u\n", node_stack[0]);
      goto _exit;

   }

   return otree;

_exit:
   gams_opcodetree_free(otree);
   return NULL;
}
