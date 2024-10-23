#include "bitlab.h"

#include <stdio.h>

#include "log.h"

bitlab_result run_bitlab(int argc, char* argv[])
{
    // [ ] Utilize the argc and argv parameters.
    if (argc != 1 && argv != NULL) {}

    init_logging(BITLAB_LOG);
    log_message(LOG_INFO, BITLAB_LOG, __FILE__, LOG_BITLAB_STARTED);


    printf("BitLab is running!\n");

    finish_logging();
    return BITLAB_RESULT_SUCCESS;
}
