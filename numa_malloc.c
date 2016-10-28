#include <inttypes.h>
#include <pthread.h>
#include <assert.h>

/* ------------------------------------------------------------
 * mattr_tree
 * ------------------------------------------------------------ */
#include "mattr_tsearch.h"
static void *__mattr_tree_root = NULL;

/* --------------------
 * print routines
 * -------------------- */
static void print_mattr_node(const struct mattr_node_t *m) {
  printf("{ addr: %12p, bytes: %12ld (%7.3f GB), touched: %d, routine: %s",
	 m->addr,
	 m->bytes, (double)m->bytes/(1UL<<30),
	 m->touched, routine_name(m->routine));
  if ( m->routine == ULIBC_MMAP ) {
    printf(", mpol: %25s, ", get_mempol_mode_name(m->mpol));
    printf("nodemask: "); show_bitmap( ULIBC_get_num_nodes(), m->nodemask );
  }
  printf(" }");
}

static void twalk_print_mattr_node(const void *nodep, const VISIT order, const int level) {
  if ( order != postorder && order != leaf )
    return;
  
  printf("ULIBC: show ");
  struct mattr_node_t *m = *(struct mattr_node_t **)nodep;
  print_mattr_node(m);
  printf(" in depth %d\n", level);
}

void ULIBC_print_memory_pool(void) {
  if ( ULIBC_verbose() > 1 )
    printf("ULIBC: ULIBC_print_memory_pool()\n");
  twalk( __mattr_tree_root, twalk_print_mattr_node );
  if ( ULIBC_verbose() > 1 )
    printf("\n");
}


/* --------------------
 * touch routines
 * -------------------- */
void *touch_seq(void *p, size_t length) {
  unsigned char *x = p;
  const size_t stride = 1UL << 12;
  for (size_t k = 0; k < length; k += stride)
    x[k] = (unsigned char)(-1);
  return p;
}

void *touch_flat_omp(void *p, size_t length) {
  unsigned char *x = p;
  const size_t stride = 1UL << 12;
  OMP("omp parallel for")
    for (size_t k = 0; k < length; k += stride)
      x[k] = (unsigned char)(-1);
  return p;
}

static void twalk_touch_mattr_node(const void *nodep, const VISIT order, const int level) {
  if ( order != postorder && order != leaf )
    return;
  
  struct mattr_node_t *m = *(struct mattr_node_t **)nodep;
  if ( m->touched )
    return;
  
  touch_flat_omp( m->addr, m->bytes );
  m->touched = 1;
  
  if ( ULIBC_verbose() > 1 ) {
    printf("ULIBC: [%2d] touched ", ULIBC_get_thread_num());
    print_mattr_node( m );
    printf(" in depth %d\n", level);
  }
}

void ULIBC_touch_memory_pool_naive(void) {
  if ( ULIBC_verbose() > 1 )
    printf("ULIBC: ULIBC_touch_memory_pool()\n");
  twalk( __mattr_tree_root, twalk_touch_mattr_node );
  if ( ULIBC_verbose() > 1 )
    printf("\n");
}

/* --------------------
 * fast touch routines
 * -------------------- */
static int64_t untouched_count;
static int64_t nnodes;
static struct mattr_node_t **addr_nodes;
static int64_t touched_nthrs = 0;
static pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

static void twalk_count_untouched(const void *nodep, const VISIT order, const int level) {
  (void)level;
  if ( order != postorder && order != leaf )
    return;
  struct mattr_node_t *m = *(struct mattr_node_t **)nodep;
  if ( ! m->touched )
    ++untouched_count;
}

int64_t count_untouched_mattr_node(void) {
  untouched_count = 0;
  twalk( __mattr_tree_root, twalk_count_untouched );
  return untouched_count;
}

int get_nthrs_from_mattr_node(struct mattr_node_t *m) {
  if ( !m ) return 0;
  
  int nthr = 0;
  for (unsigned long i = 0; i < m->maxnode; ++i)
    if ( ISSET_BITMAP( (uint64_t *)m->nodemask, i ) )
      for (int j = 0; j < ULIBC_get_online_procs(); ++j) {
      	struct numainfo_t loc = ULIBC_get_numainfo(j);
      	if ( i == (unsigned long)ULIBC_get_cpuinfo( loc.proc ).node )
      	  ++nthr;
      }
  return nthr;
}

static void twalk_count_untouched_numa(const void *nodep, const VISIT order, const int level) {
  (void)level;
  if ( order == postorder || order == leaf ) {
    struct mattr_node_t *m = *(struct mattr_node_t **)nodep;
    if ( ! m->touched )
      addr_nodes[ nnodes++ ] = m;
  }
}

static void *pth_touch(void *arg) {
  struct numainfo_t loc = ULIBC_get_numainfo( ULIBC_get_thread_num() );
  struct cpuinfo_t topo = ULIBC_get_cpuinfo( loc.proc );

  for (int i = 0; i < untouched_count; ++i) {
    struct mattr_node_t *m = addr_nodes[i];
    
    if ( ULIBC_get_thread_num() == 0 )
      touched_nthrs = 0;
    
    ULIBC_hierarchical_barrier();
    
    if ( ISSET_BITMAP(m->nodemask, topo.node) ) {
      const int corr_thrid = fetch_and_add_int64(&touched_nthrs, 1);
      const int corr_nthrs = get_nthrs_from_mattr_node( m );
      
      if ( ULIBC_verbose() > 2 ) {
	pthread_mutex_lock(&print_mutex);
	printf("[%02d] [ Thread: %3d of %d (Proc: %3d, Socket: %2d) ] ",
	       i, ULIBC_get_thread_num(), ULIBC_get_num_threads(), topo.id, topo.node);
	printf("addr: %p, size: %ld bytes, ", m->addr, m->bytes);
	printf("nodemask: "); show_bitmap( ULIBC_get_num_nodes(), m->nodemask );
	printf(", Lthr: %3d of %3d \n", corr_thrid, corr_nthrs);
	pthread_mutex_unlock(&print_mutex);
      }
      
      unsigned char *p = m->addr;
      long ls, le;
      prange(m->bytes, 0, corr_nthrs, corr_thrid, &ls, &le);
      touch_seq( &p[ls], le-ls );
      
      if ( corr_thrid == 0 )
	m->touched = 1;
    }
    ULIBC_hierarchical_barrier();
    
    if ( ULIBC_verbose() > 1 ) {
      if ( m->touched && ULIBC_get_thread_num() == 0 ) {
	printf("ULIBC: [%2d] touched ", ULIBC_get_thread_num());
	print_mattr_node( m );
	printf("\n");
      }
    }
    
    ULIBC_hierarchical_barrier();
  }
  return arg;
}

void ULIBC_touch_memory_pool(void) {
  const int64_t untouched_count = count_untouched_mattr_node();
  
  addr_nodes = malloc( sizeof(struct mattr_node_t *) * untouched_count );
  /* printf("ULIBC: Number of untouched entries: %" PRId64 "\n", untouched_count); */
  nnodes = 0;
  twalk( __mattr_tree_root, twalk_count_untouched_numa );
  assert( nnodes == untouched_count );
  
  printf("\n");
  
  pthread_t pth[MAX_CPUS];
  if ( ULIBC_verbose() > 1 )
    printf("ULIBC: ULIBC_touch_memory_pool() with %d posix threads\n",
	   ULIBC_get_online_procs());
  
  for (int i = 0; i < ULIBC_get_online_procs(); ++i) {
    pthread_create( &pth[i], NULL, pth_touch, NULL );
  }
  for (int i = 0; i < ULIBC_get_online_procs(); ++i) {
    pthread_join( pth[i], NULL );
  }
  ULIBC_clear_thread_num();
  
  /* naive */
  ULIBC_touch_memory_pool_naive();
  free(addr_nodes);
}


/* --------------------
 * memory usages
 * -------------------- */
size_t __usage[MAX_NODES];

static void twalk_memory_usage(const void *nodep, const VISIT order, const int level) {
  (void)level;
  if ( order != postorder && order != leaf )
    return;
  
  struct mattr_node_t *m = *(struct mattr_node_t **)nodep;
  
  int ell = 0;
  for (unsigned long i = 0; i < m->maxnode; ++i) {
    if ( ISSET_BITMAP( (uint64_t *)m->nodemask, i ) )
      ++ell;
  }
  for (unsigned long i = 0; i < m->maxnode; ++i) {
    if ( ISSET_BITMAP( (uint64_t *)m->nodemask, i ) )
      __usage[i] += m->bytes / ell + 1;
  }
}

size_t ULIBC_memory_usage_node(unsigned long maxnode, size_t *usage) {
  for (unsigned long i = 0; i < MAX_NODES; ++i) {
    __usage[i] = 0;
  }
  twalk( __mattr_tree_root, twalk_memory_usage );
  if (usage) {
    for (unsigned long i = 0; i < maxnode; ++i)
      usage[i] = __usage[i];
  }
  size_t total = 0;
  for (unsigned long i = 0; i < MAX_NODES; ++i) {
    total += __usage[i];
  }
  return total;
}

size_t ULIBC_memory_usage(void) {
  return ULIBC_memory_usage_node(0, NULL);
}


/* ------------------------------------------------------------
 * NUMA_finalize
 * ------------------------------------------------------------ */
void ULIBC_finalize(void) {
  ULIBC_all_free();
#if __gnu_linux__
  tdestroy( __mattr_tree_root, free );
#endif
}
