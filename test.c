#include <stdio.h>

typedef struct test{
  int t;
  int rr;
}test;

test tt;
tt.t = 0;

int main(int argc, char const *argv[])
{
  printf("%d\n", tt.t);
  return 0;
}