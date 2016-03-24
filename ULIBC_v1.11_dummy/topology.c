/* ---------------------------------------------------------------------- *
 *
 * Copyright (C) 2014 Yuichiro Yasui < yuichiro.yasui@gmail.com >
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
#include <ulibc.h>
#include <common.h>

static int hwloc_topology_init (hwloc_topology_t *topologyp) {
  (void)topologyp;
  return 0;
};

static int hwloc_topology_load(hwloc_topology_t topology) {
  (void)topology;
  return 0;
};

static hwloc_topology_t __hwloc_topology;
hwloc_topology_t ULIBC_get_hwloc_topology(void) { return __hwloc_topology; }

static hwloc_obj_t __node_obj[MAX_NODES];
hwloc_obj_t ULIBC_get_node_hwloc_obj(int node) { return __node_obj[node]; }

static size_t __pagesize[MAX_NODES] = {0};
static size_t __memorysize[MAX_NODES] = {0};
static size_t __alignsize = 0;
static int __cpuinfo_count = 0;
static int __num_procs;
static int __num_nodes;
static int __num_cores;
static int __num_smts;
static struct cpuinfo_t __cpuinfo[MAX_CPUS] = { {0,0,0,0,NULL} };

/* initialize_topology */
static void hwloc_topology_traversal(void);

int ULIBC_init_topology(void) {
  double t;
  
  /* get topology using HWLOC */
  if ( ULIBC_verbose() )
    printf("HWLOC: API version: [unused]\n");
  PROFILED( t, hwloc_topology_init(&__hwloc_topology) );
  PROFILED( t, hwloc_topology_load(__hwloc_topology)  );
  __num_procs = omp_get_num_procs();
  __num_cores = omp_get_num_procs();
  __num_nodes = 1;
  __num_smts  = omp_get_num_procs();
  PROFILED( t, hwloc_topology_traversal() );
  if ( ULIBC_verbose() ) {
    if ( __num_procs != __cpuinfo_count ) {
      printf("ULIBC: cannot cpuinfos\n");
      printf("ULIBC: # CPUs is %d, # CPUinfos is %d\n", __num_procs, __cpuinfo_count);
      exit(1);
    }
  }
  __alignsize = getenvi("ULIBC_ALIGNSIZE", ULIBC_page_size(0));

  /* Check minimum node index  */
  int min_node = MAX_NODES;
  for (int i = 0; i < __cpuinfo_count; ++i) {
    min_node = MIN(min_node, __cpuinfo[i].node);
  }
  if ( ULIBC_verbose() )
    printf("ULIBC: min. node index is %d\n", min_node);
  assert( min_node >= 0 );
  
  if (__num_nodes == 0) __num_nodes = 1;
  if (__alignsize == 0) __alignsize = 4096;
  
  if ( ULIBC_verbose() ) {
    printf("ULIBC: # of Processor cores is %4d\n", ULIBC_get_num_procs());
    printf("ULIBC: # of NUMA nodes      is %4d\n", ULIBC_get_num_nodes());
    printf("ULIBC: # of Cores           is %4d\n", ULIBC_get_num_cores());
    printf("ULIBC: # of SMTs            is %4d\n", ULIBC_get_num_smts());
    printf("ULIBC: Alignment size       is %ld bytes\n", ULIBC_align_size());
  }
  
  if ( ULIBC_verbose() > 2 ) {
    ULIBC_print_topology(stdout);
  }
  return 0;
}



/* get number of processors, packages, cores/socket, and threads/core, page size, memory size */
int ULIBC_get_num_procs(void) { return __num_procs; }
int ULIBC_get_num_nodes(void) { return __num_nodes; }
int ULIBC_get_num_cores(void) { return __num_cores; }
int ULIBC_get_num_smts(void) { return __num_smts; }
size_t ULIBC_page_size(unsigned nodeidx) { return __pagesize[nodeidx]; }
size_t ULIBC_memory_size(unsigned nodeidx) { return __memorysize[nodeidx]; }
size_t ULIBC_align_size(void) { return __alignsize; }

size_t ULIBC_total_memory_size(void) {
  static size_t total = 0;
  if (total == 0) {
    for (int i = 0; i < ULIBC_get_num_nodes(); ++i)
      total += ULIBC_memory_size(i);
  }
  return total;
}

struct cpuinfo_t ULIBC_get_cpuinfo(unsigned procidx) {
  if (ULIBC_get_num_procs() <= (int)procidx)
    procidx %= ULIBC_get_num_procs();
  return __cpuinfo[procidx];
}

/* print cpuinfo */
void ULIBC_print_topology(FILE *fp) {
  if (!fp) return;
  for (int i = 0; i < ULIBC_get_num_nodes(); ++i) {
    fprintf(fp, "ULIBC: NUMA %d has %d CPU cores and %.3f GB (%ld bytes) memory (using %ld bytes page size)\n",
	    i,
	    ULIBC_get_num_procs() / ULIBC_get_num_nodes(),
	    1.0*ULIBC_memory_size(i)/(1UL<<30),
	    ULIBC_memory_size(i),
	    ULIBC_page_size(i));
  }
  for (int i = 0; i < ULIBC_get_num_procs(); ++i) {
    struct cpuinfo_t ci = ULIBC_get_cpuinfo(i);
    fprintf(fp, "ULIBC: CPU[%03d] Processor: %2d, Node: %2d, Core: %2d, SMT: %2d\n",
	    i, ci.id, ci.node, ci.core, ci.smt);
  }
}


/* CPU and Memory detection using HWLOC */
/* temporary variables for hwloc_topology_traversal() */

static void hwloc_topology_traversal(void) {
  __memorysize[0] = 0;
  __pagesize[0] = 4096;
  __cpuinfo_count = ULIBC_get_num_procs();
  for (int i = 0; i < ULIBC_get_num_procs(); ++i) {
    __cpuinfo[i] = (struct cpuinfo_t){
      .id = i,
      .node = 0,
      .core = i,
      .smt = 0,
      .obj = NULL,		/* for hwloc */
    };
  }
}