#ifndef __BITLAB_H
#define __BITLAB_H

typedef enum
{
    BITLAB_RESULT_SUCCESS,
    BITLAB_RESULT_FAILURE
} bitlab_result;

/**
 * Run the BitLab program.
 *
 * @param argc The number of arguments.
 * @param argv The arguments.
 * @return The result of the BitLab program.
 */
bitlab_result run_bitlab(int argc, char* argv[]);

#endif // __BITLAB_H
