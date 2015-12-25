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
#include <assert.h>
#include <ulibc.h>
#include <common.h>

/* # of prosessor cores */
int __max_online_procs;

/* online processor indices */
int __online_proclist[MAX_CPUS];

int __enable_online_procs;

static int get_string_procs(char *string, int *procs);

int ULIBC_init_online_topology(void) {
  /* double t; */
  char *proclist_env = getenv("ULIBC_PROCLIST");
  if ( !proclist_env ) {
    /* PROFILED( t, __max_online_procs = get_online_hwloc_pu(__online_proclist) ); */
    __max_online_procs = 0;
  } else {
    if (ULIBC_verbose()) 
      printf("ULIBC: ULIBC_PROCLIST=\"%s\" (ignored)\n", proclist_env);
    /* PROFILED( t, __max_online_procs = get_string_procs(proclist_env, __online_proclist) ); */
    __max_online_procs = 0;
  }
  
  if (__max_online_procs == 0) {
    __enable_online_procs = 0;
    char proclist[10]="";
    sprintf(proclist, "0-%d", ULIBC_get_num_procs()-1);
    __max_online_procs = get_string_procs(proclist, __online_proclist);
  } else {
    __enable_online_procs = 1;
  }
  
  if (ULIBC_verbose() >= 2) ULIBC_print_online_topology(stdout);
  
  return 0;
}

/* get functions */
int ULIBC_enable_online_procs(void) { return __enable_online_procs; }

int ULIBC_get_max_online_procs(void) { return __max_online_procs; }
int ULIBC_get_online_procidx(unsigned idx) {
  if ((int)idx > ULIBC_get_max_online_procs())
    idx %= ULIBC_get_max_online_procs();
  return __online_proclist[idx];
}

/* print online topology */
void ULIBC_print_online_topology(FILE *fp) {
  if ( !fp ) return;
  fprintf(fp, "ULIBC: #online-processors is %d\n",  ULIBC_get_max_online_procs());
  for (int k = 0; k < ULIBC_get_num_nodes(); ++k) {
    fprintf(fp, "ULIBC: Online processors on NUMA %d:\t", k);
    for (int i = 0; i < ULIBC_get_max_online_procs(); ++i) {
      struct cpuinfo_t ci = ULIBC_get_cpuinfo( ULIBC_get_online_procidx(i) );
      if (ci.node == k) 
	fprintf(fp, "%d, ", ci.id);
    }
    fprintf(fp, "\n");
  }
}

/* generates online processor list from string */
static int cmpr_int(const void *a, const void *b) {
  const int _a = *(int *)a;
  const int _b = *(int *)b;
  if ( _a < _b) return -1;
  if ( _a > _b) return  1;
  return 0;
}

static int get_string_procs(char *string, int *procs) {
  int online = 0;
  const char *sep = ",: ";
  char *comma = NULL;
  for (char *p = strtok_r(string, sep, &comma); p; p = strtok_r(NULL, sep, &comma)) {
    if (ULIBC_verbose() >= 3) printf("%s\t", p);
    const char *rangesep = "-";
    char *pp = NULL;
    int start = 0, stop = 0;
    if ( (pp = strtok(p, rangesep)) )
      start = stop = atoi(pp);
    else
      goto done;
    if ( (pp = strtok(NULL, rangesep)) )
      stop = atoi(pp);
    else 
      goto done;
    if ( (pp = strtok(NULL, rangesep)) ) {
      if (ULIBC_verbose() >= 3) printf("[error] wrong processor list\n");
      exit(1);
    }
    if (start > stop) {
      int t = start;
      start = stop, stop = t;
    }
  done:
    if (ULIBC_verbose() >= 3) printf("\t");
    for (int i = start; i <= stop; ++i) {
      procs[online++] = i;
      if (ULIBC_verbose() >= 3) printf("%d,", i);
    }
    if (ULIBC_verbose() >= 3) printf("\n");
  }
  online = uniq(procs, online, sizeof(int), qsort, cmpr_int);
  return online;
}
