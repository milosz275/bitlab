#include <stdio.h>
#include <stdlib.h>

#include "bitlab.h"

int main(int argc, char* argv[])
{
    setbuf(stdout, NULL);
    int result = run_bitlab(argc, argv);
    if (result != BITLAB_RESULT_SUCCESS)
    {
        fprintf(stderr, "BitLab failed to run with error code: %d\n", result);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
