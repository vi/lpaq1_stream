#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <limits.h>

#include "bit_predictor.h"

void quit(char const* m) {
    fprintf(stderr, "%s\n", m);
    _Exit(1);
}

int measure_entropy(const char* buf, int l, BitPredictor& p) {
  int s = 0;
  
  for (int j=0; j<l; ++j) {
    for (int i=7; i>=0; --i) {
      int bit = (buf[j]>>i)&1;
      int prediction = p.p();
      
      if (bit) {
         s += 4096-prediction;
      } else {
         s += prediction;
      }
    
      p.update(bit);
    }
  }
  return s;
}

int main(int argc, char* argv[]) {
    if (argc==1 || !strcmp(argv[1], "--help")) {
        fprintf(stdout, "Usage: classify class1.lpaq1state class2.lpaq1state ... < input.txt > classified.txt\n");
        fprintf(stdout, "    This tool loads lpaq1_stream savestates and classifies input lines (checks in which class it is more compressible)\n");
        return 1;
    }
    
    struct {
        BitPredictor *template_;
        BitPredictor *active_;
        const char* name;
        int s;
    } a[argc-1];
    
    for (int i=1; i<argc; ++i) {
        auto & c = a[i-1];
        c.name = argv[i];
        
        FILE* f = fopen(argv[i], "rb");
        assert(f);
        
        c.template_ = new BitPredictor(f);
        c.active_ = new BitPredictor(c.template_->MEM());
    }
    
    while(!feof(stdin)) { 
        char line[655360]; 
        if(!fgets(line, sizeof line-1, stdin)) break; 
        line[sizeof(line)-1]=0; 
        int l = strlen(line);
        
        int minimum_entropy = INT_MAX;
        const char* best_name = "unknown";
        
        for (int i=1; i<argc; ++i) {
            auto & c = a[i-1];
            *c.active_ = *c.template_;
            c.s = measure_entropy(line, l, *c.active_);
            
            if (minimum_entropy > c.s) {
                minimum_entropy = c.s;
                best_name = c.name;
            }
        }
        
        fprintf(stdout, "%s ", best_name);
        fwrite(line, 1, l, stdout);
        fflush(stdout);
    }
    return 0;
}
