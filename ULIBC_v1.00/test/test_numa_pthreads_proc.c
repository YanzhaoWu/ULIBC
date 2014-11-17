#include <stdio.h>
#include <assert.h>
#include <ulibc.h>

int main(void) {
  ULIBC_init();
  
  printf("#procs   is %d\n", ULIBC_get_num_procs());
  printf("#nodes   is %d\n", ULIBC_get_num_nodes());
  printf("#cores   is %d\n", ULIBC_get_num_cores());
  printf("#smt     is %d\n", ULIBC_get_num_smts());
  printf("\n");
  printf("#threads is %d\n", ULIBC_get_num_threads());
  printf("\n");
  printf("default affinity policy is %s-%s\n",
	 ULIBC_get_current_mapping_name(), ULIBC_get_current_binding_name());

  printf("\n");
  const int a_policy[] = { SCATTER_MAPPING, COMPACT_MAPPING, -1 };
  const int b_policy[] = { THREAD_TO_CORE, THREAD_TO_PHYSICAL_CORE, THREAD_TO_SOCKET, -1 };
  void test(void);
  for (int i = 0; a_policy[i] >= 0; ++i) {
    for (int j = 0; b_policy[j] >= 0; ++j) {
      ULIBC_set_affinity_policy(ULIBC_get_num_threads(), a_policy[i], b_policy[j]);
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


struct tharg_t {
  int id;
  char *pinned_id;
};

static void fill_affnity_string(void *_args) {
  struct tharg_t *args = (struct tharg_t *)_args;
  const int id = args->id;
  char *pinned_id = args->pinned_id;
  
  const struct numainfo_t ni = ULIBC_get_numainfo( id );
  for (int proc = 0; proc < ULIBC_get_num_procs(); ++proc) {
    pinned_id[ proc ] = ULIBC_is_bind_thread(ni.id, proc) ? 'x' : '-';
  }
}

void test(void) {
  
#define MAX_CPUS 4096
  char *pinned[MAX_CPUS];
  for (int i = 0; i < MAX_CPUS; ++i) {
    pinned[i] = (char *)calloc(MAX_CPUS, 1);
  }
  
  pthread_t handler[MAX_CPUS];
  struct tharg_t args[MAX_CPUS];
  for (int i = 1; i < ULIBC_get_num_threads(); i++) {
    args[i].id = i;
    args[i].pinned_id = pinned[i];
    assert( !pthread_create( &handler[i], NULL,
			     (void * (*)(void *))(&fill_affnity_string),
			     (void *)(&args[i]) ) );
  }
  args[0].id = 0;
  args[0].pinned_id = pinned[0];
  fill_affnity_string( &args[0] );
  for (int i = 1; i < ULIBC_get_num_threads(); i++) {
    assert( !pthread_join( handler[i], NULL ) );
  }
    
  const int cores_per_node = ULIBC_get_num_procs() / ULIBC_get_num_nodes();

  for (int i = 0; i < ULIBC_get_online_procs(); ++i) {
    struct numainfo_t ni = ULIBC_get_numainfo(i);
    struct cpuinfo_t ci = ULIBC_get_cpuinfo( ni.proc );
    printf("Thread: %3d of %d, NUMA: %2d-%02d (core=%2d)"
  	   ", Proc: %2d, Pkg: %2d, Core: %2d, Smt: %2d\t",
  	   ni.id, ULIBC_get_online_procs(), ni.node, ni.core, ni.lnp,
  	   ci.id, ci.node, ci.core, ci.smt);
    
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
