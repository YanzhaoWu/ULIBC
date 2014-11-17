/* ---------------------------------------------------------------------- *
 *
 * Copyright (C) 2014 Yuichiro Yasui < yuichiro.yasui@gmail.com >
 *
 * This file is part of ULIBC.
 *
 * ULIBC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ULIBC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ULIBC.  If not, see <http://www.gnu.org/licenses/>.
 * ---------------------------------------------------------------------- */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <ulibc.h>
#include <common.h>

hwloc_cpuset_t __default_cpuset[MAX_CPUS];
hwloc_cpuset_t __bind_cpuset[MAX_CPUS];
hwloc_cpuset_t __curr_cpuset[MAX_CPUS];

static long __num_bind[MAX_CPUS] = {0};
static void bind_thread(void);

int ULIBC_init_numa_threads(void) {
  for (int i = 0; i < MAX_CPUS; ++i) {
    __bind_cpuset[i] = hwloc_bitmap_alloc();
    __curr_cpuset[i] = hwloc_bitmap_alloc();
    __default_cpuset[i] = hwloc_bitmap_alloc();
    hwloc_bitmap_zero( __bind_cpuset[i] );
    hwloc_bitmap_zero( __curr_cpuset[i] );
    hwloc_bitmap_zero( __default_cpuset[i] );
  }
  
  if ( ULIBC_use_affinity() == NULL_AFFINITY ) return 1;

  OMP("omp parallel") {
    /* get default affinity */
    const int id = omp_get_thread_num();
    
    const struct numainfo_t ni = ULIBC_get_numainfo( id );
    assert( ni.lnp == ULIBC_get_online_cores( ni.node ) );
    
    hwloc_get_cpubind(ULIBC_get_hwloc_topology(),
		      __default_cpuset[id], HWLOC_CPUBIND_THREAD);
    bind_thread();
  }
  return 0;
}

static void bind_thread(void) {
  if ( ULIBC_use_affinity() != ULIBC_AFFINITY ) return;
  if ( !omp_in_parallel() ) return;
  
  /* constructs cpuset */
  const int id = omp_get_thread_num();
  struct numainfo_t ni = ULIBC_get_numainfo(id);
  hwloc_cpuset_t cpuset = hwloc_bitmap_alloc();
  hwloc_bitmap_zero(cpuset);
  
  int c = 0;
  switch ( ULIBC_get_current_binding() ) {
  case THREAD_TO_CORE: {
    struct cpuinfo_t ci = ULIBC_get_cpuinfo( ni.proc );
    hwloc_bitmap_or(cpuset, cpuset, ci.obj->cpuset), ++c;
    break;
  }
  case THREAD_TO_PHYSICAL_CORE:
    for (int u = 0; u < ULIBC_get_online_procs(); ++u) {
      struct cpuinfo_t ci = ULIBC_get_cpuinfo( ni.proc );
      struct cpuinfo_t cj = ULIBC_get_cpuinfo( ULIBC_get_numainfo(u).proc );
      if ( ci.node == cj.node && ci.core == cj.core ) {
	hwloc_bitmap_or(cpuset, cpuset, cj.obj->cpuset), ++c;
      }
    }
    break;
  case THREAD_TO_SOCKET:
    for (int u = 0; u < ULIBC_get_online_procs(); ++u) {
      struct cpuinfo_t ci = ULIBC_get_cpuinfo( ni.proc );
      struct cpuinfo_t cj = ULIBC_get_cpuinfo( ULIBC_get_numainfo(u).proc );
      if ( ci.node == cj.node ) {
	hwloc_bitmap_or(cpuset, cpuset, cj.obj->cpuset), ++c;
      }
    }
    break;
  default: break;
  }
  
  /* binds */
  if (c > 0) {
    hwloc_set_cpubind(ULIBC_get_hwloc_topology(), cpuset, HWLOC_CPUBIND_THREAD);
    hwloc_bitmap_copy(__bind_cpuset[id], cpuset);
    ++__num_bind[id];
  }
  hwloc_bitmap_free(cpuset);
}

int ULIBC_bind_thread(void) {
  if ( ULIBC_use_affinity() != ULIBC_AFFINITY ) return 0;
  if ( !omp_in_parallel() ) return 0;
  
  const int id = omp_get_thread_num();
  hwloc_get_cpubind(ULIBC_get_hwloc_topology(), __curr_cpuset[id], HWLOC_CPUBIND_THREAD);
  if ( hwloc_bitmap_isequal(__bind_cpuset[id], __curr_cpuset[id]) ) {
    return 0;
  } else {
    bind_thread();
    return 1;
  }
}

int ULIBC_unbind_thread(void) {
  if ( ULIBC_use_affinity() != ULIBC_AFFINITY ) return 0;
  if ( !omp_in_parallel() ) return 0;
  
  const int id = omp_get_thread_num();
  hwloc_set_cpubind(ULIBC_get_hwloc_topology(), __default_cpuset[id], HWLOC_CPUBIND_THREAD);
  return 1;
}

int ULIBC_is_bind_thread(int tid, int procid) {
  return hwloc_bitmap_isset(__bind_cpuset[tid], procid);
}

long ULIBC_get_num_bind_threads(int id) {
  return __num_bind[id];
}
