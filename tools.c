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
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <assert.h>
#include <ulibc.h>

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



/* ------------------------- *
 * Local variables:          *
 * c-basic-offset: 2         *
 * indent-tabs-mode: nil     *
 * End:                      *
 * ------------------------- */
