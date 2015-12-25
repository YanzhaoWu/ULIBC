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
#include <math.h>
#include <assert.h>
#include <ulibc.h>
#include <common.h>

int ULIBC_init_numa_threads(void) {
  if ( ULIBC_use_affinity() == NULL_AFFINITY ) return 1;
  return 0;
}

int ULIBC_bind_thread(void) {
  if ( ULIBC_use_affinity() != ULIBC_AFFINITY ) return 0;
  if ( !omp_in_parallel() ) return 0;
  return 1;
}

int ULIBC_unbind_thread(void) {
  if ( ULIBC_use_affinity() != ULIBC_AFFINITY ) return 0;
  if ( !omp_in_parallel() ) return 0;

  return 1;
}

int ULIBC_is_bind_thread(int tid, int procid) {
  (void)tid;
  (void)procid;
  return 0;
}

long ULIBC_get_num_bind_threads(int id) {
  (void)id;
  return 0;
}
