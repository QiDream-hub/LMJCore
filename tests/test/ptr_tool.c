#include "../../Toolkit/ptr_uuid_gen/include/lmjcore_uuid_gen.h"
#include "../../core/include/lmjcore.h"
#include <stdio.h>

int main() {
  lmjcore_ptr ptr;
  void *ctx = NULL;

  int rc = lmjcore_uuidv4_ptr_gen(ctx, ptr);

  ptr[0] = LMJCORE_OBJ;

  char buff[4096];
  lmjcore_ptr_to_string(ptr, buff, 4096);
  printf("%s", buff);

  return 0;
}