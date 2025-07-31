#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file_process.h"

/* App main method */
/* ==================================================================== */
int main(int argc, char *argv[]) {
    int i, runtime_result, success_count = 0;
    int total_files = argc - 1;

    if (argc < 2) {
        fprintf(stderr, "Expected program call: %s <filename1> [filename2] ... \n", argv[0]);
        return 1;
    }

    for (i = 1; i < argc; i++) {
        if (process_file(argv[i], i, total_files)) 
            success_count++;
    }
    
    printf("\n --- Summary --- \n");
    printf("Successfully processed: %d/%d files \n", success_count, total_files);
    
    runtime_result = (success_count == total_files) ? 0 : 1;
    printf("Assembler completed with exit code %d! \n", runtime_result);

    return runtime_result;
}