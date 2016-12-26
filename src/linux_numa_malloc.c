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
#include <stdlib.h>
#include <sys/mman.h>
#include <syscall.h>
#include <linux/mempolicy.h>

#include <ulibc.h>
#include <common.h>

extern long int syscall(long int __sysno, ...);

long set_mempolicy(int mode, const unsigned long *nmask, unsigned long maxnode) {
  return syscall(SYS_set_mempolicy, mode, nmask, maxnode);
}

int mbind(void *addr, unsigned long len, int mode,
	  unsigned long *nodemask, unsigned long maxnode, unsigned flags) {
  return syscall(SYS_mbind, addr, len, mode, nodemask, maxnode, flags);
}

static const char *get_mempol_mode_name(int mode) {
  switch (mode) {
  case MPOL_DEFAULT:    return "MPOL_DEFAULT";
  case MPOL_PREFERRED:  return "MPOL_PREFERRED";
  case MPOL_BIND:       return "MPOL_BIND";
  case MPOL_INTERLEAVE: return "MPOL_INTERLEAVE";
#ifdef MPOL_LOCAL
  case MPOL_LOCAL:      return "MPOL_LOCAL";
#endif
  default:              return "Unknown";
  }
}

static int get_mempol_mode(int mode) {
  switch (mode) {
  case ULIBC_MPOL_BIND:       return MPOL_BIND;
  case ULIBC_MPOL_INTERLEAVE: return MPOL_INTERLEAVE;
  case ULIBC_MPOL_DEFAULT:
  default:                    return MPOL_DEFAULT;
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
  mpol = get_mempol_mode(mpol);
  
  void *p = mmap(0, size, (PROT_READ | PROT_WRITE), (MAP_PRIVATE | MAP_ANONYMOUS), 0, 0);
  mbind(p, size, mpol | MPOL_F_STATIC_NODES, nodemask, maxnode, MPOL_MF_MOVE);
  
  struct mattr_node_t *m = insert_mattr( &__mattr_tree_root, size, p );
  m->touched = 0;
  m->routine = ULIBC_MMAP;
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
  SET_BITMAP( nodemask, ULIBC_get_online_nodeidx(node) );
  size = ROUNDUP2M( size );
  void *p = ULIBC_malloc_explict(size, ULIBC_MPOL_BIND, nodemask, MAX_NODES);
  return p;
}

void *ULIBC_malloc_interleave(size_t size) {
  unsigned long nodemask[MAX_NODES/sizeof(unsigned long)/8] = {0};
  make_nodemask_online(MAX_NODES, nodemask);
  size = ROUNDUP2M( size );
  void *p = ULIBC_malloc_explict(size, ULIBC_MPOL_INTERLEAVE, nodemask, MAX_NODES);
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
 * legacy interfaces
 * ------------------------------------------------------------ */
char *ULIBC_get_memory_name(void) {
  return "mmap-mbind";
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
