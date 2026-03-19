#include <stdio.h>
#include <stdlib.h>

int main(void)
{

  int a;
  int *ptr1 = &a;
  float b;
  char c;
  double d;

  printf("%d %d %d %d\n", sizeof(a), sizeof(b), sizeof(c), sizeof(d));

  printf("%d %d", ptr1, ptr1 + 1);
}
