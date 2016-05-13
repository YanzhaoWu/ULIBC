#include <ulibc.h>
int main(void) {
  ULIBC_init();
  printf("Hello %s!\n", ULIBC_version());
  printf("Hello HWLOC %d!\n", HWLOC_API_VERSION);
  return 0;
}
