/* ---------------------------------------------------------------------- *
 *
 * Copyright (C) 2013-2016 Yuichiro Yasui < yuichiro.yasui@gmail.com >
 *
 * This file is part of ULIBC.
 *
 * ULIBC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ULIBC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with ULIBC.  If not, see <http://www.gnu.org/licenses/>.
 * ---------------------------------------------------------------------- */
#include <stdio.h>
#include <sys/mman.h>

#include <ulibc.h>
#include <common.h>

#ifndef ULIBC_MPOL_DEFAULT
#define ULIBC_MPOL_DEFAULT 0
#endif

static const char *get_mempol_mode_name(int mode) {
  switch (mode) {
  case ULIBC_MPOL_DEFAULT: return "ULIBC_MPOL_DEFAULT";
  default:                 return "Unknown";
  }
}

#include "numa_malloc.c"

/* ------------------------------------------------------------
 * ULIBC_malloc
 * ------------------------------------------------------------ */
#ifndef ROUNDUP2M
#  define ROUNDUP2M(x) ROUNDUP(x,1UL<<21)
#endif

void *ULIBC_malloc_explict(size_t size, int mpol, unsigned long *nodemask, unsigned long maxnode) {
  void *p;
#ifdef USE_MALLOC
  p = malloc(size);
#else
  posix_memalign((void *)&p, ULIBC_align_size(), size);
#endif
  
  struct mattr_node_t *m = insert_mattr( &__mattr_tree_root, size, p );
  m->touched = 0;
#ifdef USE_MALLOC
  m->routine = ULIBC_MALLOC;
#else
  m->routine = ULIBC_POSIX_MEMALIGN;
#endif
  m->mpol    = mpol;
  m->maxnode = maxnode;
  memcpy( m->nodemask, nodemask, maxnode/sizeof(unsigned long) );
  
  if ( ULIBC_verbose() > 1 ) {
    printf("ULIBC: allocate ");
    print_mattr_node( m );
    printf("\n");
  }
  
  return p;
}

void *ULIBC_malloc_mempol(size_t size, int mpol) {
  unsigned long nodemask[MAX_NODES/sizeof(unsigned long)/8] = {0};
  const char *descnode = getenv("ULIBC_MEMBIND");
  if ( descnode ) {
    make_nodemask_sscanf(descnode, MAX_NODES, nodemask);
  } else {
    make_nodemask_online(MAX_NODES, nodemask);
  }
  size = ROUNDUP2M( size );
  void *p = ULIBC_malloc_explict(size, mpol, nodemask, MAX_NODES);
  return p;
}

void *ULIBC_malloc_bind(size_t size, int node) {
  unsigned long nodemask[MAX_NODES/sizeof(unsigned long)/8] = {0};
  SET_BITMAP( (uint64_t *)nodemask, ULIBC_get_online_nodeidx(node) );
  size = ROUNDUP2M( size );
  void *p = ULIBC_malloc_explict(size, ULIBC_MPOL_DEFAULT, nodemask, MAX_NODES);
  return p;
}

void *ULIBC_malloc_interleave(size_t size) {
  unsigned long nodemask[MAX_NODES/sizeof(unsigned long)/8] = {0};
  make_nodemask_online(MAX_NODES, nodemask);
  size = ROUNDUP2M( size );
  void *p = ULIBC_malloc_explict(size, ULIBC_MPOL_DEFAULT, nodemask, MAX_NODES);
  return p;
}


/* ------------------------------------------------------------
 * ULIBC_free
 * ------------------------------------------------------------ */
void ULIBC_free(void *ptr) {
  if ( ! ptr ) return;

  struct mattr_node_t *res = delete_mattr( &__mattr_tree_root, ptr );
  if ( !res ) return;
  
  if ( res->routine == ULIBC_MMAP ) {
    munmap( res->addr, res->bytes );
  } else {
    free( res->addr );
  }
  
  if ( ULIBC_verbose() > 1 ) {
    printf("ULIBC: free ");
    print_mattr_node( res );
    printf("\n");
  }
  
  free(res);
}

void ULIBC_all_free(void) {
  struct mattr_node_t *res = NULL;
  while ( ( res = pop_mattr(&__mattr_tree_root) ) ) {
    if ( res->routine == ULIBC_MMAP ) {
      munmap( res->addr, res->bytes );
    } else {
      free( res->addr );
    }

    if ( ULIBC_verbose() > 1 ) {
      printf("ULIBC: free ");
      print_mattr_node( res );
      printf("\n");
    }
    
    free(res);
  }
}




/* ------------------------------------------------------------
 * legacy routines
 * ------------------------------------------------------------ */
char *ULIBC_get_memory_name(void) {
#if defined(USE_MALLOC)
  return "malloc";
#else
  return "posix_memalign";
#endif
}

void *NUMA_malloc(size_t size, const int onnode) {
  return ULIBC_malloc_bind(size, onnode);
}


void *NUMA_touched_malloc(size_t size, int onnode) {
  void *p = ULIBC_malloc_bind(size, onnode);
  
  struct mattr_node_t *res = find_mattr(&__mattr_tree_root, p);
  if ( res )
    res->touched = 1;
  
  return touch_seq(p, size);
}

void ULIBC_touch_memories(size_t size[MAX_NODES], void *pool[MAX_NODES]) {
  OMP("omp parallel") {
    struct numainfo_t ni = ULIBC_get_current_numainfo();
    unsigned char *addr = pool[ni.node];
    const size_t sz = size[ni.node];
    
    struct mattr_node_t *res = find_mattr(&__mattr_tree_root, addr);
    if ( res )
      res->touched = 1;
    
    if ( addr )
      touch_seq(addr, sz);
  }
}

void NUMA_free(void *p) {
  ULIBC_free(p);
}
