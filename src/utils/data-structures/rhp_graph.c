#include "asprintf.h"

#include <assert.h>


#include "macros.h"
#include "mdl.h"
#include "ovfinfo.h"
#include "printout.h"
#include "reshop.h"
#include "reshop_data.h"
#include "rhp_graph.h"
#include "rhp_graph_data.h"
#include "status.h"

struct rhp_graph_gen {
  void *obj;
  enum dfs_state state;
  unsigned len;
  unsigned max;
  struct rhp_graph_child *children;
};

static inline void rhp_graph_gen_dfs_init_node(struct rhp_graph_gen *node)
{
  assert(!(node->state == InProgress));
  node->state = InProgress;
}

static inline void rhp_graph_gen_dfs_fini_node(struct rhp_graph_gen *node)
{
  assert((node->state == InProgress));
  node->state = Processed;
}

struct rhp_graph_gen* rhp_graph_gen_alloc(void* obj)
{
  struct rhp_graph_gen *node;
  CALLOC_NULL(node, struct rhp_graph_gen, 1)
  node->obj = obj;
  node->state = NotExplored;
  return node;

}

void rhp_graph_gen_free(struct rhp_graph_gen* node)
{
  if (node) {
    FREE(node->children);
  }
}

int rhp_graph_gen_set_children(struct rhp_graph_gen *node,
                               struct rhp_graph_child *children,
                               unsigned n_children)
{
  if (node->children) {
    error("%s :: children is already allocated.\n", __func__);
    return Error_UnExpectedData;
  }

  MALLOC_(node->children, struct rhp_graph_child, n_children);

  memcpy(node->children, children, sizeof(*(node->children))*n_children);
  node->len = n_children;
  node->max = n_children;

  return OK;
}

static int rhp_graph_gen_visit(struct rhp_graph_gen * restrict node,
                               ObjArray * restrict dat)
{
  switch (node->state) {
  case InProgress:
    error("%s :: A circular dependency situation has been detected! "
                       "This is not yet supported.\n", __func__);
    return Error_NotImplemented;
  case Processed:
    return OK;
  case NotExplored:

   /* ----------------------------------------------------------------------
    * start visiting this node and its children
    * ---------------------------------------------------------------------- */

    rhp_graph_gen_dfs_init_node(node);

    for (size_t i = 0; i < node->len; ++i) {
      S_CHECK(rhp_graph_gen_visit(node->children[i].child, dat));
    }

    S_CHECK(rhp_obj_add(dat, node->obj));
    rhp_graph_gen_dfs_fini_node(node);

    return OK;

  default:
    error("%s :: node has an non-standard state %d\n",
                       __func__, node->state);
    return Error_RuntimeError;
  }

}

int rhp_graph_gen_dfs(struct rhp_graph_gen ** restrict nodes, unsigned n_nodes,
                      ObjArray * restrict dat)
{
  for (size_t i = 0; i < n_nodes; ++i) {
    struct rhp_graph_gen *node = nodes[i];

    switch (node->state) {
    case NotExplored:
      S_CHECK(rhp_graph_gen_visit(node, dat));
      break;
    case Processed:
      break;
    case InProgress:
      error("%s :: node #%zu is already being processed\n",
                         __func__, i);
      return Error_RuntimeError;
    default:
      error("%s :: node #%zu has an non-standard state %d\n",
                         __func__, i, node->state);
    }

  }

  return OK;
}

const char * const nodestyle_ovf  = "style=filled,color=lightblue1";
const char * const arcstyle_ovf   = "color=red";

static int _print_ovfgraph_nodes(const OvfDef* ovf, FILE* f,
                                 const Container *ctr)
{
   while (ovf) {

      unsigned idx = ovf->vi_ovf;
      IO_CALL(fprintf(f, " OVF%u [label=\"OVF(%s)\\nfn: %s\\n\", %s];\n", idx,
                      ctr_printvarname(ctr, ovf->vi_ovf), ovf_getname(ovf),
                      nodestyle_ovf));
      ovf = ovf->next;
   }

   return OK;
}

static int _print_ovfgraph_edges(struct rhp_graph_gen **nodes,
                                 unsigned n_nodes, FILE* f,
                                 const Container *ctr)
{
   for (unsigned i = 0; i < n_nodes; ++i) {
      const struct rhp_graph_gen *node = nodes[i];
      const OvfDef *ovf = ( const struct rhp_ovfdef *)node->obj;

      unsigned idx = ovf->vi_ovf;

      for (unsigned j = 0, len = node->len; j < len; ++j) {
           rhp_idx cidx = node->children[j].idx;
           unsigned c_n = ((const OvfDef*)node->children[j].child->obj)->vi_ovf;
           IO_CALL(fprintf(f, " OVF%u -> OVF%u [label=\"%s\", %s];\n", idx, c_n,
                           ctr_printvarname(ctr, cidx), arcstyle_ovf));
           }
   }

   return OK;
}

int ovfgraph_dot(const struct ovfinfo *ovf_info, struct rhp_graph_gen **nodes,
                 unsigned n_nodes, const Model *mdl, const char* fname)
{
   if (!ovf_info->ovf_def) return OK;
   int status = OK;

   FILE* f = fopen(fname, "w");

   if (!f) {
      error("%s :: Could not create file named '%s'\n", __func__, fname);
      return OK;
   }

   IO_CALL_EXIT(fputs("digraph structs {\n node [shape=record];\n", f));
   IO_CALL_EXIT(fprintf(f, " label=\"OVFDAG for model %s\"\n", mdl_getname(mdl)));

   IO_CALL_EXIT(_print_ovfgraph_nodes(ovf_info->ovf_def, f, &mdl->ctr));
   IO_CALL_EXIT(_print_ovfgraph_edges(nodes, n_nodes, f, &mdl->ctr));


   IO_CALL_EXIT(fputs("\n}\n", f));

   SYS_CALL(fclose(f));

   char *cmd;
   IO_CALL(asprintf(&cmd, "dot -Tpng -O %s", fname));
   int rc = system(cmd); /* YOLO */
   if (rc) {
      error("[empdag] executing '%s' yielded return code %d\n", cmd, rc);
      FREE(cmd);
   }
   FREE(cmd);

   if (!optvalb(mdl, Options_Display_OvfDag)) { return OK; }

   const char *png_viewer = optvals(mdl, Options_Png_Viewer);
   bool free_png_viewer = false;

  if (!png_viewer) {
#ifdef __APPLE__
      png_viewer = "open "
#elif defined(__linux__)
      png_viewer = "xdg-open";
#else
      png_viewer = NULL;
#endif
   } else {
      free_png_viewer = true;
   }

   if (png_viewer) {
      IO_CALL(asprintf(&cmd, "%s %s.png", png_viewer, fname));
      rc = system(cmd); /* YOLO */
      if (rc) {
         error("[empdag] executing '%s' yielded return code %d\n", cmd, rc);
      }
      FREE(cmd);
   }

   if (free_png_viewer) { FREE(png_viewer); }

   return OK;

_exit:
   SYS_CALL(fclose(f));
   return status;
}
