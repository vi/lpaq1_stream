#include <stdio.h>
#include <stdlib.h>

#include "bit_predictor.h"


void quit(char const* m) {
    fprintf(stderr, "%s\n", m);
    _Exit(1);
}

int main(int argc, char* argv[]) {
    
    BitPredictor p(1<<20);
    
    fprintf(stdout, "%d ",p.p());
    while(!feof(stdin)) {
        int bit = getc(stdin);
        if      (bit == '0') { p.update(0); }
        else if (bit == '1') { p.update(1); }
        else continue;
        
        fprintf(stdout, "%d ",p.p());
    }
    
    return 0;
}
