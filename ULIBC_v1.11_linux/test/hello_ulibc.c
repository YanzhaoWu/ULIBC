#include <ulibc.h>
int main(void) {
  ULIBC_init();
  printf("Hello %s!\n", ULIBC_version());
  return 0;
}
