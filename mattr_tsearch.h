#ifndef ADDR_MANAGER_H
#define ADDR_MANAGER_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <search.h>
#include <string.h>

enum mattr_mem_type_t {
  ULIBC_UNKNOWN,
  ULIBC_MALLOC,
  ULIBC_POSIX_MEMALIGN,
  ULIBC_MMAP,
  ULIBC_NROUTINES,
};

struct mattr_node_t {
  /* key */
  void *addr;
  
  /* attributes */
  size_t bytes;
  int touched;
  int routine;
  
  /* for mmap */
  int mpol; 
  unsigned long maxnode;
  unsigned long nodemask[MAX_NODES/sizeof(unsigned long)/8];
};

static const char *routine_name(int routine) {
  const char *name[] = {
    "unknown",
    "malloc",
    "posix_memalign",
    "mmap",
    NULL
  };
  if ( 0 < routine && routine < ULIBC_NROUTINES )
    return name[routine];
  else
    return name[0];
}

static int cmpr_mattr_nodes(const void *pi, const void *pj) {
  const struct mattr_node_t *mi = pi;
  const struct mattr_node_t *mj = pj;
  if ( mi->addr < mj->addr ) return -1;
  if ( mi->addr > mj->addr ) return  1;
  return 0;
}

static int always_match(const void *pi, const void *pj) {
  (void)pi, (void)pj;
  return 0;
}

static struct mattr_node_t *insert_mattr(void **root, size_t bytes, void *addr) {
  struct mattr_node_t *m = calloc( 1, sizeof(struct mattr_node_t) );
  m->bytes = bytes;
  m->addr = addr;
  void *val = tsearch( (void *)m, (void **)root, cmpr_mattr_nodes );
  if ( !val ) {
    return NULL;
  } else if ( cmpr_mattr_nodes(*(struct mattr_node_t **)val, (void *)m) != 0 ) {
    free(m);
    return NULL;
  } else {
    return m;
  }
}

static struct mattr_node_t *find_mattr(void **root, void *addr) {
  struct mattr_node_t target = { .addr = addr };
  void *res = tfind( (void *)&target, root,
		     addr ? cmpr_mattr_nodes : always_match );
  if ( res ) {
    return *(struct mattr_node_t **)res;
  } else {
    return NULL;
  }
}

static struct mattr_node_t *delete_mattr(void **root, void *addr) {
  struct mattr_node_t *m = find_mattr(root, addr);
  if ( m ) {
    tdelete( m, root, cmpr_mattr_nodes );
    return m;
  } else {
    return NULL;
  }
}

static struct mattr_node_t *pop_mattr(void **root) {
  return delete_mattr(root, NULL);
}

#endif /*_MANAGER_H */
