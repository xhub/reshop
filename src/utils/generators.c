#include "generators.h"
#include "macros.h"
#include "printout.h"
#include "status.h"


static int _alloc_gen_data(struct gen_data *gd)
{
   gd->size = 0;
   gd->max = 3;
   CALLOC_(gd->val, double *, gd->max);

   return OK;
}

static int _add_gen_data(struct gen_data *gd, double* v)
{
   if (gd->size >= gd->max) {
      size_t old_max = gd->max;
      gd->max = MAX(2*gd->max, gd->size+1);
      REALLOC_(gd->val, double *, gd->max);
      for (size_t i = old_max; i < gd->max; i++) {
         gd->val[i] = NULL;
      }
   }
   gd->val[gd->size++] = v;

   return OK;
}

struct generators* generators_alloc(unsigned space_dim)
{
   struct generators* gen;
   CALLOC_NULL(gen, struct generators, 1);

   gen->dim = space_dim;
   if (_alloc_gen_data(&gen->vertices))
      goto _exit;

   if (_alloc_gen_data(&gen->rays))
      goto _exit;

   return gen;

_exit:
   FREE(gen->vertices.val);
   FREE(gen->rays.val);
   FREE(gen);
   return NULL;
}

void generators_dealloc(struct generators *gen)
{
   if (!gen) return;

   for (unsigned i = 0; i < gen->vertices.size; ++i) {
      FREE(gen->vertices.val[i]);
   }
   FREE(gen->vertices.val);

   for (unsigned i = 0; i < gen->rays.size; ++i) {
      FREE(gen->rays.val[i]);
   }
   FREE(gen->rays.val);
   FREE(gen);
}

int generators_add_vertex(struct generators *gen, double *v)
{
   S_CHECK(_add_gen_data(&gen->vertices, v));
   return OK;
}

int generators_add_ray(struct generators *gen, double *r)
{
   S_CHECK(_add_gen_data(&gen->rays, r));
   return OK;
}

int generators_add_line(struct generators *gen, double *l)
{
   S_CHECK(_add_gen_data(&gen->lines, l));
   return OK;
}

static int _trim_mem(struct gen_data *d)
{
   if (d->val && d->size > 0 && d->size < d->max) {
      REALLOC_(d->val, double *, d->size);
      d->max = d->size;
   }
   return OK;
}

int generators_trim_memory(struct generators *gen)
{
   S_CHECK(_trim_mem(&gen->vertices));
   S_CHECK(_trim_mem(&gen->rays));
   S_CHECK(_trim_mem(&gen->lines));
   return OK;
}

static void _display(struct gen_data *dat, const char* name, unsigned dim)
{
   printout(PO_DEBUG, "%d %s\n", dat->size, name);
   for (unsigned i = 0; i < dat->size; ++i) {
      printout(PO_DEBUG, "  [%5d]:", i);
      for (unsigned j = 0; j < dim; ++j) {
          printout(PO_DEBUG, " %e", dat->val[i][j]);
      }
      printout(PO_DEBUG, "\n");
   }
}

static int _savetxt(struct gen_data *dat, const char* name, unsigned dim)
{
   FILE* f = fopen(name, RHP_WRITE_TEXT);

   if (!f) {
      int lerrno = errno;
      perror("fopen");
      error("%s :: could not create file named \"%s\" (error %d)\n",
               __func__, name, lerrno);
      return Error_SystemError;
   }

   for (unsigned i = 0; i < dat->size; ++i) {
      for (unsigned j = 0; j < dim; ++j) {
         IO_CALL(fprintf(f, "%e, ", dat->val[i][j]));
      }
      IO_CALL(fprintf(f, "\n"));
   }

   SYS_CALL(fclose(f));

   return OK;
}

void generators_savetxt(struct generators *gen)
{
   printout(PO_DEBUG, "* saving generators\n");
   _savetxt(&gen->vertices, "vertices", gen->dim);
   _savetxt(&gen->rays, "rays", gen->dim);
   _savetxt(&gen->lines, "lines", gen->dim);
}

void generators_print(struct generators *gen)
{
   printout(PO_DEBUG, "* displaying generator of dimension %d\n", gen->dim);
   _display(&gen->vertices, "Vertices", gen->dim);
   _display(&gen->rays, "Rays", gen->dim);
   _display(&gen->lines, "Lines", gen->dim);
}
