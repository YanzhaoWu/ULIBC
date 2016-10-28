#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <ulibc.h>
#include <omp_helpers.h>

#ifndef PROFILED
#  define PROFILED(t, X) do { \
    double tt=get_msecs(); \
    X; \
    t=get_msecs()-tt; \
    if ( ULIBC_verbose() ) \
      printf("ULIBC: %s (%f ms)\n", #X, t);	\
  } while (0)
#endif

#define SLEEP() sleep(3)

#include <time.h>

char *now(void) {
  static char s[256];
  time_t timer = time(NULL);
  struct tm *date = localtime(&timer);
  strftime(s, 255, "%Y%m%d %H:%M:%S", date);
  return s;
}

int main(void) {
  ULIBC_init();
  
  printf("RAM size: %ld\n", ULIBC_total_memory_size());
  
  printf("# running node: {\n");
  size_t online_ramsize = 0;
  for (int i = 0; i < ULIBC_get_online_nodes(); ++i) {
    online_ramsize += ULIBC_memory_size(i);
    printf("\t%d (%.3f GB, %.f KB aligned)\n",
	   ULIBC_get_online_nodeidx(i),
	   (double)ULIBC_memory_size(i) / (1UL << 30),
	   (double)ULIBC_page_size(i) / (1UL << 10));
  }
  printf("}\n");
  const char *descnode = getenv("ULIBC_MEMBIND");
  printf("ULIBC_MEMBIND: { %s }\n", descnode);
  printf("\n");
  
  if ( !online_ramsize )
    online_ramsize = 1UL << 30;
  
#define NADDR 8
  const size_t base_sz = online_ramsize / NADDR;
  const size_t presz = base_sz / NADDR;
  void *addr[NADDR] = { NULL };
  
  srand( 0 );
  for (int k = 0; k < NADDR; ++k) {
    const size_t sz = presz + presz * rand()*(k+1)/RAND_MAX;
    printf("# [%d]: %s (%f GB)\n", k, now(), (double)sz / (1UL << 30));
    if ( k % 5 == 1 )
      addr[k] = ULIBC_malloc_interleave( sz );
    else if ( k % 3 == 1 )
      addr[k] = ULIBC_malloc_bind( sz, k % ULIBC_get_online_nodes() );
    else if ( k % 2 == 1 )
      addr[k] = ULIBC_malloc_mempol( sz, ULIBC_MPOL_INTERLEAVE );
    else
      addr[k] = ULIBC_malloc_mempol( sz, ULIBC_MPOL_BIND );
  }
  printf("\n");
  SLEEP();
  
  size_t usage[256], total;
  total = ULIBC_memory_usage_node( ULIBC_get_num_nodes(), usage);
  for (int i = 0; i < ULIBC_get_num_nodes(); ++i) {
    printf("Node: %3d, Usage: %lu (%f GB)\n", i, usage[i], (double)usage[i]/(1UL<<30));
  }
  printf("Node: all, Usage: %lu (%f GB)\n", total, (double)ULIBC_memory_usage()/(1UL<<30));
  printf("\n");
  
  ULIBC_print_memory_pool();
  SLEEP();
  
  double t;
#if 0
  PROFILED( t, ULIBC_touch_memory_pool_naive() );
  SLEEP();
  ULIBC_print_memory_pool();
  SLEEP();
  
  PROFILED( t, ULIBC_touch_memory_pool_naive() );
  SLEEP();
  ULIBC_print_memory_pool();
  SLEEP();
#else
  PROFILED( t, ULIBC_touch_memory_pool() );
  SLEEP();
  ULIBC_print_memory_pool();
  SLEEP();
  
  PROFILED( t, ULIBC_touch_memory_pool() );
  SLEEP();
  ULIBC_print_memory_pool();
  SLEEP();
#endif
  
  for (int k = 0; k < NADDR; ++k) {
    if (k % 2 == 1) {
      printf("free %p\n", addr[k]);
      ULIBC_free( addr[k] );
    }
  }
  printf("\n");
  SLEEP();
  
  ULIBC_print_memory_pool();
  SLEEP();
  
  ULIBC_all_free();
  SLEEP();
  
  ULIBC_print_memory_pool();
  
  return 0;
}
