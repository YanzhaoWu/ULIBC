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
#include <string.h>
#include <assert.h>
#include <sched.h>
#include <sys/mman.h>
#include <hwloc.h>

#include <ulibc.h>
#include <common.h>

extern hwloc_topology_t ULIBC_get_hwloc_topology(void);
extern hwloc_obj_t ULIBC_get_node_hwloc_obj(int node);

static const char *get_mempol_mode_name(int mode) {
  switch (mode) {
  case HWLOC_MEMBIND_DEFAULT:    return "HWLOC_MEMBIND_DEFAULT";
  case HWLOC_MEMBIND_FIRSTTOUCH: return "HWLOC_MEMBIND_FIRSTTOUCH";
  case HWLOC_MEMBIND_BIND:       return "HWLOC_MEMBIND_BIND";
  case HWLOC_MEMBIND_INTERLEAVE: return "HWLOC_MEMBIND_INTERLEAVE";
  case HWLOC_MEMBIND_REPLICATE:  return "HWLOC_MEMBIND_REPLICATE";
  case HWLOC_MEMBIND_NEXTTOUCH:  return "HWLOC_MEMBIND_NEXTTOUCH";
  default:                       return "Unknown";
  }
}

static int get_mempol_mode(int mode) {
  switch (mode) {
  case ULIBC_MPOL_BIND:       return HWLOC_MEMBIND_BIND;
  case ULIBC_MPOL_INTERLEAVE: return HWLOC_MEMBIND_INTERLEAVE;
  case ULIBC_MPOL_DEFAULT:
  default:                    return HWLOC_MEMBIND_DEFAULT;
  }
}

#include "numa_malloc.c"

/* ------------------------------------------------------------
 * ULIBC_malloc
 * ------------------------------------------------------------ */
#ifndef ROUNDUP2M
#  define ROUNDUP2M(x) ROUNDUP(x,1UL<<21)
#endif

#if defined(__sun) || defined(sun) || defined(__sun__)
/* solaris */
#define USE_HWLOC_ALLOCATOR 1
#else
/* linux */
#define USE_HWLOC_ALLOCATOR 0
#endif

void *ULIBC_malloc_explict(size_t size, int mpol, unsigned long *nodemask, unsigned long maxnode) {
  mpol = get_mempol_mode(mpol);
   
  hwloc_nodeset_t nodeset = hwloc_bitmap_alloc();
  hwloc_bitmap_zero( nodeset );
  for (unsigned long i = 0; i < maxnode; ++i) {
    if ( ISSET_BITMAP( (uint64_t *)nodemask, i ) )
      hwloc_bitmap_or( nodeset, nodeset, ULIBC_get_node_hwloc_obj(i)->nodeset );
  }
  
  /* for (unsigned long i = 0; i < maxnode; ++i) { */
  /*   if ( hwloc_bitmap_isset(nodeset, i) ) */
  /*     printf("%ld,", i); */
  /* } */
  /* printf("\t"); */
  
  void *p = NULL;
  if ( USE_HWLOC_ALLOCATOR ) {
    p = hwloc_alloc_membind_nodeset( ULIBC_get_hwloc_topology(), size, nodeset, mpol, HWLOC_MEMBIND_MIGRATE );
  } else {
    p = mmap(0, size, (PROT_READ | PROT_WRITE), (MAP_PRIVATE | MAP_ANONYMOUS), 0, 0);
    hwloc_set_area_membind_nodeset( ULIBC_get_hwloc_topology(), p, size, nodeset, mpol, HWLOC_MEMBIND_MIGRATE );
  }
  hwloc_bitmap_free(nodeset);
  
  struct mattr_node_t *m = insert_mattr( &__mattr_tree_root, size, p );
  m->touched = 0;
  m->routine = ULIBC_MMAP; /* dummy */
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
  
  if ( USE_HWLOC_ALLOCATOR ) {
    hwloc_free( ULIBC_get_hwloc_topology(), res->addr, res->bytes );
  } else {
    if ( res->routine == ULIBC_MMAP ) {
      munmap( res->addr, res->bytes );
    } else {
      free( res->addr );
    }
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
    if ( USE_HWLOC_ALLOCATOR ) {
      hwloc_free( ULIBC_get_hwloc_topology(), res->addr, res->bytes );
    } else {
      if ( res->routine == ULIBC_MMAP ) {
	munmap( res->addr, res->bytes );
      } else {
	free( res->addr );
      }
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
  if ( USE_HWLOC_ALLOCATOR ) {
    return "hwloc_alloc_membind_nodeset";
  } else {
    return "mmap-hwloc_set_area_membind_nodeset";
  }
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
