#include <stdio.h>
#include <assert.h>
#include <ulibc.h>

int main(void) {
  ULIBC_init();
  
  printf("#procs   is %d\n", ULIBC_get_num_procs());
  printf("#nodes   is %d\n", ULIBC_get_num_nodes());
  printf("#cores   is %d\n", ULIBC_get_num_cores());
  printf("#smt     is %d\n", ULIBC_get_num_smts());

  printf("default affinity policy is %s-%s\n",
	 ULIBC_get_current_mapping_name(), ULIBC_get_current_binding_name());

  printf("\n");
  const int a_policy[] = { SCATTER_MAPPING, COMPACT_MAPPING, -1 };
  const int b_policy[] = { THREAD_TO_CORE, THREAD_TO_PHYSICAL_CORE, THREAD_TO_SOCKET, -1 };
  void test(void);
  for (int i = 0; a_policy[i] >= 0; ++i) {
    for (int j = 0; b_policy[j] >= 0; ++j) {
      ULIBC_set_affinity_policy(ULIBC_get_num_procs(), a_policy[i], b_policy[j]);
      printf("----------------------------------------\n");
      printf("%s-%s\n",
	     ULIBC_get_current_mapping_name(),
	     ULIBC_get_current_binding_name());
      printf("----------------------------------------\n");
      test();
    }
  }
  return 0;
}


#ifdef __linux__
#include <sched.h>

int is_bind_linux(int procid) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  assert( !sched_getaffinity((pid_t)0, sizeof(cpu_set_t), &cpuset) );
  return CPU_ISSET(procid, &cpuset);
}

#endif


void test(void) {
  
#define MAX_CPUS 4096
  char *pinned[MAX_CPUS];
  for (int i = 0; i < MAX_CPUS; ++i) {
    pinned[i] = (char *)calloc(MAX_CPUS, 1);
  }

  const int cores_per_node = ULIBC_get_num_procs() / ULIBC_get_num_nodes();
  
  OMP("omp parallel") {
    struct numainfo_t ni = ULIBC_get_current_numainfo();
    
    for (int k = 0; k < ULIBC_get_num_nodes(); ++k)
      for (int proc = 0; proc < ULIBC_get_num_procs(); ++proc)
	pinned[ ni.id ][ k*cores_per_node+proc ] = '-';
    
    for (int j = 0; j < ULIBC_get_online_procs(); ++j) {
      const int proc = ULIBC_get_numainfo(j).proc;
      const int node = ULIBC_get_numainfo(j).node;
      const int core = ULIBC_get_numainfo(j).core;
      struct cpuinfo_t cj = ULIBC_get_cpuinfo( proc );
      pinned[ ni.id ][ node*cores_per_node+core ]
	= ULIBC_is_bind_thread(ni.id, cj.id) ? 'x' : '-';
    }
  }
  
  for (int i = 0; i < ULIBC_get_online_procs(); ++i) {
    struct numainfo_t ni = ULIBC_get_numainfo(i);
    printf("Thread: %3d of %d, NUMA: %2d-%02d (core=%2d)\t",
  	   ni.id, ULIBC_get_online_procs(), ni.node, ni.core, ni.lnp);
    
    for (int i = 0; i < ULIBC_get_num_nodes(); ++i) {
      printf(ni.node == i ? "x" : "-");
    }
    printf("\t");
    
    for (int j = 0; j < ULIBC_get_num_procs(); ++j) {
      printf("%c", pinned[i][j]);
      if ((j+1) % cores_per_node == 0) printf(" ");
    }
    printf("\n");
  }
  
  for (int i = 0; i < MAX_CPUS; ++i) {
    free(pinned[i]);
  }
}
