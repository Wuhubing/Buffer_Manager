#include <stdio.h>
 
int main()
{
 
    printf("The loop with break produces output as: \n");
 
    for (int i = 1; i <= 7; i++) {
 
        // Program comes out of loop when
        // i becomes multiple of 3.
        if (i == 3)
            break;
        else
            printf("%d ", i);
    }
 
    printf("\nThe loop with continue produces output as: \n");
    for (int i = 1; i <= 7; i++) {
 
        // The loop prints all values except
        // those that are multiple of 3.
        if (i == 3)
            continue;
        printf("%d ", i);
    }
    return 0;
}