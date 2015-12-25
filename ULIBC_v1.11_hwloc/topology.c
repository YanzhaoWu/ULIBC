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
#include <stdio.h>
#include <assert.h>
#include <hwloc.h>
#include <ulibc.h>
#include <common.h>

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

static bitmap_t hwloc_isonline_proc[MAX_CPUS/64] = {0};
static bitmap_t hwloc_isonline_node[MAX_NODES/64] = {0};

/* online_topology.c */
int __max_online_procs = 0;
int __online_proclist[MAX_CPUS];
int __enable_online_procs;
int __max_online_nodes = 0;
int __online_nodelist[MAX_NODES];
int __online_ncores_on_node[MAX_NODES] = { 0 };

#if defined(__linux__) && !defined(__ANDROID__)
#include <sys/sysinfo.h>
#define number_of_procs() get_nprocs()
#else
#include <omp.h>
#define number_of_procs() omp_get_num_procs()
#endif

int __ULIBC_get_online_nodeidx(unsigned idx) {
  if ((int)idx > __max_online_nodes)
    idx %= __max_online_nodes;
  return __online_nodelist[idx];
}

int ULIBC_get_online_cores_on_node(unsigned idx) {
  if ( ISSET_BITMAP(hwloc_isonline_node, idx) )
    return __online_ncores_on_node[idx];
  else
    return 0;
}

/* print online topology */
void ULIBC_print_online_topology(FILE *fp) {
  if ( !fp ) return;
  fprintf(fp, "ULIBC: #online-processors is %d\n",  ULIBC_get_max_online_procs());
  for (int k = 0; k < ULIBC_get_num_nodes(); ++k) {
    const int node = __ULIBC_get_online_nodeidx(k);
    if ( ULIBC_get_online_cores_on_node(node) == 0 ) continue;
    
    fprintf(fp, "ULIBC: Online processors on NUMA %d (Package %d):\t", k, node);
    for (int i = 0; i < ULIBC_get_max_online_procs(); ++i) {
      struct cpuinfo_t ci = ULIBC_get_cpuinfo( ULIBC_get_online_procidx(i) );
      if (ci.node == node) 
	fprintf(fp, "%d, ", ci.id);
    }
    fprintf(fp, "\n");
  }
}

/* initialize_topology */
static void hwloc_topology_traversal(hwloc_topology_t topology, hwloc_obj_t obj, unsigned depth);

int ULIBC_init_topology(void) {
  double t;
  
  /* get topology using HWLOC */
  if ( ULIBC_verbose() )
    printf("ULIBC: HWLOC API version: %u\n", HWLOC_API_VERSION);
  PROFILED( t, hwloc_topology_init(&__hwloc_topology) );
  PROFILED( t, hwloc_topology_load(__hwloc_topology)  );
  __num_procs = hwloc_get_nbobjs_by_type(__hwloc_topology, HWLOC_OBJ_PU);
  __num_nodes = hwloc_get_nbobjs_by_type(__hwloc_topology, HWLOC_OBJ_NODE);
  __num_cores = hwloc_get_nbobjs_by_type(__hwloc_topology, HWLOC_OBJ_CORE);
  __num_smts  = __num_procs;
  
  if ( __num_procs > 0 )
    __enable_online_procs = 1;
  else 
    __enable_online_procs = 0;
  
  if ( ULIBC_verbose() ) {
    printf("ULIBC: hwloc_get_nbobjs_by_type(__hwloc_topology, HWLOC_OBJ_PU):   %d\n", __num_procs);
    printf("ULIBC: hwloc_get_nbobjs_by_type(__hwloc_topology, HWLOC_OBJ_CORE): %d\n", __num_cores);
    printf("ULIBC: hwloc_get_nbobjs_by_type(__hwloc_topology, HWLOC_OBJ_NODE): %d\n", __num_nodes);
  }
  
  PROFILED( t, hwloc_topology_traversal(__hwloc_topology, hwloc_get_root_obj(__hwloc_topology), 0) );
  __num_nodes = __max_online_nodes;
  if ( ULIBC_verbose() ) {
    if ( __num_procs != __cpuinfo_count ) {
      printf("ULIBC: cannot cpuinfos\n");
      printf("ULIBC: # CPUs is %d, # CPUinfos is %d\n", __num_procs, __cpuinfo_count);
      exit(1);
    }
  }
  __alignsize = getenvi("ULIBC_ALIGNSIZE", ULIBC_page_size(0));
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
    ULIBC_print_online_topology(stdout);
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
  if ( ISSET_BITMAP(hwloc_isonline_proc, procidx) )
    return __cpuinfo[procidx];
  else
    return (struct cpuinfo_t){
      .id = -1, .node = -1, .core = -1, .smt = -1, .obj = NULL,
    };
}

/* print cpuinfo */
void ULIBC_print_topology(FILE *fp) {
  if (!fp) return;
  for (int i = 0; i < ULIBC_get_num_nodes(); ++i) {
    const int node = __online_nodelist[i];
    if ( __online_ncores_on_node[node] > 0 )
      fprintf(fp,
	      "ULIBC: NUMA %d has %d CPU cores and %.3f GB (%ld bytes) RAM (%ld-bytes page)\n",
	      node,
	      __online_ncores_on_node[node],
	      1.0*ULIBC_memory_size(node)/(1UL<<30),
	      ULIBC_memory_size(node),
	      ULIBC_page_size(node));
  }
  for (int i = 0; i < ULIBC_get_num_procs(); ++i) {
    const int proc = ULIBC_get_online_procidx(i);
    struct cpuinfo_t ci = ULIBC_get_cpuinfo(proc);
    fprintf(fp, "ULIBC: CPU[%03d] Processor: %2d, Node: %2d, Core: %2d, SMT: %2d\n",
	    i, ci.id, ci.node, ci.core, ci.smt);
  }
}


/* CPU and Memory detection using HWLOC */
/* temporary variables for hwloc_topology_traversal() */
unsigned curr_node = 0;
unsigned curr_core = 0;
unsigned curr_smt[MAX_CPUS] = {0};

static void hwloc_topology_traversal(hwloc_topology_t topology, hwloc_obj_t obj, unsigned depth) {
  if (obj->type == HWLOC_OBJ_NODE) {
    if ( ULIBC_verbose() > 3 )
      printf("ULIBC: Type: HWLOC_OBJ_NODE, os_index: %d, logical_index: %d\n",
	     obj->os_index, obj->logical_index);
    
    /* nodes */
    curr_node = obj->os_index;
    assert( !ISSET_BITMAP(hwloc_isonline_node, curr_node) );
    SET_BITMAP(hwloc_isonline_node, curr_node);
    memset(curr_smt, 0x00, sizeof(unsigned)*MAX_CPUS);
    curr_core = -1;
    __memorysize[curr_node] = obj->memory.local_memory;
    for (unsigned i = 0; i < obj->memory.page_types_len; ++i) {
      __pagesize[curr_node] = obj->memory.page_types[i].size;
    }
    __node_obj[curr_node] = obj;

    /* online nodes */
    __online_nodelist[__max_online_nodes++] = curr_node;
  }
  
  if (obj->type == HWLOC_OBJ_CORE) {
    ++curr_core;
  }
  
  if (obj->type == HWLOC_OBJ_PU) {
    if ( ULIBC_verbose() > 3 )
      printf("ULIBC: Type: HWLOC_OBJ_PU, os_index: %d, logical_index: %d\n",
	     obj->os_index, obj->logical_index);
    const unsigned proc = obj->os_index;
/*     printf("proc: %d, node: %d, core: %d, smt: %d\n", */
/* 	   proc, curr_node, curr_core, curr_smt[curr_core]); */
    assert( !ISSET_BITMAP(hwloc_isonline_proc, proc) );
    SET_BITMAP(hwloc_isonline_proc, proc);
    
    /* cpus */
    __cpuinfo_count++;
    __cpuinfo[proc] = (struct cpuinfo_t){
      .id = proc,
      .node = curr_node,
      .core = curr_core,
      .smt = curr_smt[curr_core]++,
      .obj = obj,		/* for hwloc */
    };
    
    /* online cpus */
    __online_proclist[__max_online_procs++] = proc;
    __online_ncores_on_node[curr_node]++;
  }
  for (unsigned i = 0; i < obj->arity; ++i) {
    hwloc_topology_traversal(topology, obj->children[i], depth + 1);
  }
}

int ULIBC_enable_online_procs(void) { return __enable_online_procs; }
int ULIBC_get_max_online_procs(void) { return __max_online_procs; }
int ULIBC_get_online_procidx(unsigned idx) {
  if ((int)idx > ULIBC_get_max_online_procs())
    idx %= ULIBC_get_max_online_procs();
  return __online_proclist[idx];
}

/* generates online processor list from hwloc */
int fill_online_hwloc_pu(int *cpuset) {
  int online = 0;
  const int ncpus = ULIBC_get_num_procs();
  hwloc_cpuset_t online_cpuset = hwloc_bitmap_alloc();
  hwloc_bitmap_zero(online_cpuset);
  hwloc_get_cpubind(ULIBC_get_hwloc_topology(), online_cpuset, HWLOC_CPUBIND_PROCESS);
  /* bitmap_t found[ MAX_CPUS/64 ] = {0}; */
  for (int i = 0; i < ncpus; ++i) {
    struct cpuinfo_t ci = ULIBC_get_cpuinfo(i);
    if ( hwloc_bitmap_isset(online_cpuset, ci.id) )
      cpuset[online++] = ci.id;
  }
  hwloc_bitmap_free(online_cpuset);
  return online;
}
