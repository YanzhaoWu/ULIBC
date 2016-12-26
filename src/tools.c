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
#include <sys/time.h>
#include <assert.h>

#include <ulibc.h>
#include <common.h>

double get_msecs(void) {
  struct timeval tv;
  assert( !gettimeofday(&tv, NULL) );
  return (double)tv.tv_sec*1e3 + (double)tv.tv_usec*1e-3;
}

unsigned long long get_usecs(void) {
  struct timeval tv;
  assert( !gettimeofday(&tv, NULL) );
  return (unsigned long long)tv.tv_sec*1000000 + tv.tv_usec;
}

long long getenvi(char *env, long long def) {
  if (env && getenv(env)) {
    return atoll(getenv(env));
  } else {
    return def;
  }
}

double getenvf(char *env, double def) {
  if (env && getenv(env)) {
    return atof(getenv(env));
  } else {
    return def;
  }
}

size_t uniq(void *base, size_t nmemb, size_t size,
            void (*sort)(void *base, size_t nmemb, size_t size,
                         int (*compar)(const void *, const void *)),
            int (*compar)(const void *, const void *)) {
  size_t i, crr;
  sort(base, nmemb, size, compar);
  for (i = crr = 1; i < nmemb; i++) {
    if ( compar(base+(i-1)*size, base+i*size) != 0 ) {
      if (base+crr*size != base+i*size ) 
        memcpy(base+crr*size, base+i*size, size);
      crr++;
    }
  }
  return crr;
}

#define PARENT(x) ( ((x)-1) >> 1 )
#define LEFT(x)   ( ((x) << 1) + 1 )

static inline void heapify(void *base, size_t size, size_t i,
			   int (*compar)(const void *, const void *)) {
#ifdef __SUNPRO_C
  void *crr = memcpy(malloc(size), base+i*size, size);
#else
  void *crr = memcpy(alloca(size), base+i*size, size);
#endif
  while ( i > 0 && compar(base+PARENT(i)*size, crr) < 0 ) {
    memcpy(base+i*size, base+PARENT(i)*size, size);
    i = PARENT(i);
  }
  memcpy(base+i*size, crr, size);
#ifdef __SUNPRO_C
  free(crr);
#endif
}

static inline void extractmin(void *base, size_t size, size_t hqsz, void *buf,
			      int (*compar)(const void *, const void *)) {
  size_t i = 0, left = 1;
  void *HQ_hqsz = memcpy(buf, base+hqsz*size, size);
  
  /* left and right */
  size_t next;
  while ( left+1 < hqsz ) {
    if ( compar(base+left*size, base+(left+1)*size) > 0 ) {
      next = left;
    } else {
      next = left+1;
    }
    if ( compar(base+next*size, HQ_hqsz) > 0 ) {
      memcpy(base+i*size, base+next*size, size);
      i = next;
      left = LEFT(i);
    } else {
      break;
    }
  }
  /* left only */
  if ( left+1 == hqsz && compar(base+left*size, HQ_hqsz) > 0 ) {
    memcpy(base+i*size, base+left*size, size);
    i = left;
  }
  if (i != hqsz) {
    memcpy(base+i*size, HQ_hqsz, size);
  }
}

void uheapsort(void *base, size_t nmemb, size_t size,
               int (*compar)(const void *, const void *)) {
  /* heapify */
  for (size_t i = 0; i < nmemb; ++i) {
    heapify(base, size, i, compar);
  }
  
  size_t bufsz = ROUNDUP( size, ULIBC_align_size() );
  void *scratch = malloc(bufsz);
  void *buf = malloc(bufsz);
  memset(scratch, 0x00, bufsz);
  memset(buf, 0x00, bufsz);
  
  /* extractmin */
  for (size_t i = 0; i < nmemb; i++) {
    memcpy(scratch, base, size);
    extractmin(base, size, (nmemb-1)-i, buf, compar);
    memcpy(base+(nmemb-1-i)*size, scratch, size);
  }
  
  free( scratch );
  free( buf );
}

void prange(long len, long off, long np, long id, long *ls, long *le) {
  const long qt = len / np;
  const long rm = len % np;
  *ls = qt * (id+0) + (id+0 < rm ? id+0 : rm) + off;
  *le = qt * (id+1) + (id+1 < rm ? id+1 : rm) + off;
}


/* ------------------------------------------------------------
 * nodemask
 * ------------------------------------------------------------ */
long make_nodemask_sscanf(const char *s, unsigned long maxnode, unsigned long *nodemask) {
  char *string = strdup(s), *comma = NULL;
  const char *sep = ",: ", *rangesep = "-";
  for (char *p = strtok_r(string, sep, &comma); p; p = strtok_r(NULL, sep, &comma)) {
    char *pp = NULL;
    unsigned long start = 0, stop = 0;
    if ( (pp = strtok(p, rangesep)) )
      start = stop = atol(pp);
    else
      goto done;
    if ( (pp = strtok(NULL, rangesep)) )
      stop = atol(pp);
    else
      goto done;
    if ( (pp = strtok(NULL, rangesep)) )
      return 0;
  done:
    {
      const unsigned long l = MIN(MIN(start,stop),maxnode);
      const unsigned long r = MIN(MAX(start,stop),maxnode);
      for (unsigned long i = l; i <= r; ++i)
	SET_BITMAP( (uint64_t *)nodemask, i );
    }
  }
  unsigned long online = 0;
  for (unsigned long i = 0; i < maxnode; ++i) {
    if ( ISSET_BITMAP( (uint64_t *)nodemask, i ) )
      ++online;
  }
  free(string);
  return online;
}

long make_nodemask_online(unsigned long maxnode, unsigned long *nodemask) {
  long online = ULIBC_get_online_nodes();
  for (int i = 0; i < online; ++i) {
    unsigned long node = ULIBC_get_online_nodeidx(i);
    if ( node < maxnode )
      SET_BITMAP( (uint64_t *)nodemask, node );
  }
  return online;
}

char *sprintf_ulong_bits(char *buf, unsigned long mask) {
  const int nbits = sizeof(unsigned long) * 8;
  for (int i = 0; i < nbits; ++i) {
    buf[ i ] = mask & (1UL << ( nbits - (i+1) )) ? '1' : '0';
  }
  buf[ nbits ] = '\0';
  return buf;
}

void show_bitmap(const unsigned long maxnode, const unsigned long *nodemask) {
  char buf[sizeof(unsigned long) * 8 + 1];
  const int nbits = sizeof(unsigned long) * 8;
  unsigned long words = ROUNDUP(maxnode,nbits) / nbits;
  if ( words == 1 ) {
    const unsigned long n = ROUNDUP(maxnode,16);
    sprintf_ulong_bits( buf, nodemask[0] );
    printf("%s", &buf[nbits-n]);
  } else {
    words = ROUNDUP(words,2);
    for (int i = words-1; i >= 0; --i) {
      printf("%s ", sprintf_ulong_bits( buf, nodemask[i] ));
    }
  }
}

char *get_cc_version(void) {
  static char str[256];
#ifdef __INTEL_COMPILER
  sprintf(str, "ICC %.2f (%d)", __INTEL_COMPILER / 100.0, __INTEL_COMPILER_BUILD_DATE);
#elif __GNUC__
  sprintf(str, "GCC %d.%d.%d", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#elif __SUNPRO_C
  sprintf(str, "SunPro %x", __SUNPRO_C);
#elif __IBMC__
  sprintf(str, "IBM XL C %x", __xlC__);
#else
  strcatfmt(str, "unknown");
#endif
  return str;
}
