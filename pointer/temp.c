#include <stdio.h>

int main(void)
{

      // int a=100;

      void *p;
      int a = 100;
      printf("Printing value of a: %c\n", a);

      p = &a;
      char *p1;
      p1 = p;
      printf("Character: %c\n", *p1);

      printf("Integer: %d\n", *p1);
      /*
            int *p1;
            p1=p;

           printf("\nThe address of p is %d\n",p);

         printf("*p1=%d ",*p1);

           p=p+1;
           printf("\nThe address of p is %d\n",p);
        */
}
