/* lpaq1.cpp file compressor, July 24, 2007.
(C) 2007, Matt Mahoney, matmahoney@yahoo.com

Modified by _Vi: special "streaming" mode

    LICENSE

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of
    the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details at
    Visit <http://www.gnu.org/copyleft/gpl.html>.

For example:

  lpaq1_stream 9 
  lpaq1_stream -d 


lpaq1 is a "lite" version of PAQ, about 35 times faster than paq8l
at the cost of some compression (but similar to high end BWT and
PPM compressors).  It is a context-mixing compressor combining 7 
contexts: orders 1, 2, 3, 4, 6, a lowercase unigram word context 
(for ASCII text), and a "match" order, which predicts the next
bit in the last matching context.  The independent bit predictions of
the 7 models are combined using one of 80 neural networks (selected by
a small context), then adjusted using 2 SSE stages (order 0 and 1)
and arithmetic coded.

Prediction is bitwise.  This means that an order-n context consists
of the last n whole bytes plus any of the 0 to 7 previously coded
bits of the current byte starting with the most significant bit.
The unigram word context consists of a hash of the last (at most) 11
consecutive letters (A-Z, a-z) folded to lower case.  The context
does not include any nonalphabetic characters nor any characters
preceding the last nonalphabetic character.

The first 6 contexts (orders 1..4, 6, word) are used to index a
hash table to look up a bit-history represented by an 8-bit state.
The representable states are the same as in paq8l.  A state can
either represent all histories up to 4 bits long, or a pair of
0,1 counts plus a flag to indicate the most recent bit.  The counts
are bounded by (41,0), (40,1), (12,2), (5,3), (4,4) and likewise
for 1,0.  When a count is exceeded, the opposite count is reduced to 
approximately preserve the count ratio.  The last bit flag is present
only for states whose total count is less than 16.  There are 253
possible states.

A bit history is mapped to a probability using an adaptive table
(StateMap).  This differs from paq8l in that each table entry includes
a count so that adaptation is rapid at first.  Each table entry
has a 22-bit probability (initially p = 0.5) and 10-bit count (initially 
n = 0) packed into 32 bits.  After bit y is predicted, n is incremented
up to the limit (1023) and the probability is adjusted by 
p := p + (y - p)/(n + 0.5).  This model is stationary: 
p = (n1 + 0.5)/(n + 1), where n1 is the number of times y = 1 out of n.

The "match" model (MatchModel) looks up the current context in a
hash table, first using a longer context, then a shorter one.  If
a match is found, then the following bits are predicted until there is
a misprediction.  The prediction is computed by mapping the predicted
bit, the length of the match (1..15 or quantized by 4 in 16..62, max 62),
and the last whole byte as context into a StateMap.  If no match is found,
then the order 0 context (last 0..7 bits of the current byte) is used
as context to the StateMap.

The 7 predictions are combined using a neural network (Mixer) as in
paq8l, except it is a single level network without MMX code.  The
inputs p_i, i=0..6 are first stretched: t_i = log(p_i/(1 - p_i)), 
then the output is computed: p = squash(SUM_i t_i * w_i), where
squash(x) = 1/(1 + exp(-x)) is the inverse of stretch().  The weights
are adjusted to reduce the error: w_i := w_i + L * t_i * (y - p) where
(y - p) is the prediction error and L ~ 0.002 is the learning rate.
This is a standard single layer backpropagation network modified to
minimize coding cost rather than RMS prediction error (thus dropping
the factors p * (1 - p) from learning).

One of 80 neural networks are selected by a context that depends on
the 3 high order bits of the last whole byte plus the context order
(quantized to 0, 1, 2, 3, 4, 6, 8, 12, 16, 32).  The order is
determined by the number of nonzero bit histories and the length of
the match from MatchModel.

The Mixer output is adjusted by 2 SSE stages (called APM for adaptive
probability map).  An APM is a StateMap that accepts both a discrte
context and an input probability, pr.  pr is stetched and quantized
to 24 levels.  The output is interpolated between the 2 nearest
table entries, and then only the nearest entry is updated.  The entries
are initialized to p = pr and n = 6 (to slow down initial adaptation)
with a limit n <= 255.  The APM differs from paq8l in that it uses
the new StateMap rapid initial adaptation, does not update both
adjacent table entries, and uses 24 levels instead of 33.  The two
stages use a discrete order 0 context (last 0..7 bits) and a hashed order-1
context (14 bits).  Each output is averaged with its input weighted
by 1/4.

The output is arithmetic coded.  The code for a string s with probability
p(s) is a number between Q and Q+p(x) where Q is the total probability
of all strings lexicographically preceding s.  The number is coded as
a big-endian base-256 fraction.  A header is prepended as follows:

- "pQ" 2 bytes must be present or decompression gives an error.
- 1 (0x01) version number (other values give an error).
- memory option N as one byte '0'..'9' (0x30..0x39).
- file size as a 4 byte big-endian number.
- arithmetic coded data.

Two thirds of the memory (2 * 2^N MB) is used for a hash table mapping
the 6 regular contexts (orders 1-4, 6, word) to bit histories.  A lookup
occurs every 4 bits.  The input is a byte-oriented context plus possibly
the first nibble of the next byte.  The output is an array of 15 bit
histories (1 byte each) for all possible contexts formed by appending
0..3 more bits.  The table entries have the format:

 {checksum, "", 0, 1, 00, 10, 01, 11, 000, 100, 010, 110, 001, 101, 011, 111}

The second byte is the bit history for the context ending on a nibble
boundary.  It also serves as a priority for replacement.  The states
are ordered by increasing total count, where state 0 represents the
initial state (no history).  When a context is looked up, the 8 bit
checksum (part of the hash) is compared with 3 adjacent entries, and
if there is no match, the entry with the lowest priority is cleared
and the new checksum is stored.

The hash table is aligned on 64 byte cache lines.  A table lookup never
crosses a 64 byte address boundary.  Given a 32-bit hash h of the context,
8 bits are used for the checksum and 17 + N bits are used for the
index i.  Then the entries i, i XOR 1, and i XOR 2 are tried.  The hash h
is actually a collision-free permuation, consisting of multiplying the
input by a large odd number mod 2^32, a 16-bit rotate, and another multiply.

The order-1 context is mapped to a bit history using a 64K direct
lookup table, not a hash table.

One third of memory is used by MatchModel, divided equally between 
a rotating input buffer of 2^(N+19) bytes and an index (hash table)
with 2^(N+17) entries.  Two context hashes are maintained, a long one,
h1, of length ceil((N+17)/3) bytes and a shorter one, h2, of length 
ceil((N+17)/5) bytes, where ceil() is the ceiling function.  The index
does not use collision detection.  At each byte boundary, if there is 
not currently a match, then the bytes before the current byte are
compared with the location indexed by h1.  If less than 2 bytes match,
then h2 is tried.  If a match of length 1 or more is found, the
match is maintained until the next bit mismatches the predicted bit.
The table is updated at h1 and h2 after every byte.

To compile (g++ 3.4.5, upx 3.00w):
  g++ -Wall lpaq1.cpp -O2 -Os -march=pentiumpro -fomit-frame-pointer 
      -s -o lpaq1.exe
  upx -qqq lpaq1.exe

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <ctype.h>
//#define NDEBUG  // remove for debugging
#include <assert.h>
#include <unistd.h>
#include <errno.h>

#include "bit_predictor.h"

// 8, 16, 32 bit unsigned types (adjust as appropriate)
typedef unsigned char  U8;
typedef unsigned short U16;
typedef unsigned int   U32;

// Error handler: print message if any, and exit
void quit(const char* message=0) {
  if (message) fprintf(stderr, "%s\n", message);
  exit(1);
}

// Create an array p of n elements of type T
template <class T> void alloc(T*&p, int n) {
  p=(T*)calloc(n, sizeof(T));
  if (!p) quit("out of memory");
}


//////////////////////////// Encoder ////////////////////////////

// An Encoder does arithmetic encoding.  Methods:
// Encoder(COMPRESS, f) creates encoder for compression to archive f, which
//     must be open past any header for writing in binary mode.
// Encoder(DECOMPRESS, f) creates encoder for decompression from archive f,
//     which must be open past any header for reading in binary mode.
// code(i) in COMPRESS mode compresses bit i (0 or 1) to file f.
// code() in DECOMPRESS mode returns the next decompressed bit from file f.
// compress(c) in COMPRESS mode compresses one byte.
// decompress() in DECOMPRESS mode decompresses and returns one byte.
// flush() should be called exactly once after compression is done and
//     before closing f.  It does nothing in DECOMPRESS mode.


typedef enum {COMPRESS, DECOMPRESS} Mode;
class Encoder {
public:
  bool stopflag;
  bool ffff_attention;
private:
  BitPredictor &predictor;
  const Mode mode;       // Compress or decompress?
  FILE* archive;         // Compressed data file
  U32 x1, x2;            // Range, initially [0, 1), scaled by 2^32
  U32 x;                 // Decompress mode: last 4 input bytes of archive

  unsigned char getchar() {
    if (this->stopflag) return 255;
      
    unsigned char c = getc(archive);
    
    if (c<0xFE) {
      ffff_attention=false;
      return c;
    }
    
    if (ffff_attention) {
      if (c==0xFE) {
        unsigned char d = getc(archive);
        ffff_attention = false;
        return d+1;
      } else {
        this->stopflag = true;
        return 255;
      }
    } else {
      if (c==0xFF) {
        ffff_attention = true;
      }
      return c;
    }
    
    /*if (c==0xFF) {
      this->stopflag=true;
      return 255;
    }else
    if (c==0xFE) {
      unsigned char d = getc(archive);
      if (d!=0xFF) {
        ungetc(d, archive);
        this->stopflag=true;
      }
    }
    return c;*/
  }
  
  void putchar(unsigned char c) {
    if (c==0xFF && !ffff_attention) {
      putc(0xFF, archive);
      ffff_attention=true;
    }
    else if (c==0xFF && ffff_attention) {
      putc(0xFE, archive);
      putc(0xFE, archive);
      ffff_attention=false;
    } else if (c==0xFE && ffff_attention) {
      putc(0xFE, archive);
      putc(0xFD, archive);
      ffff_attention=false;
    } else {
      putc(c, archive);      
      ffff_attention=false;
    }
  }

  // Compress bit y or return decompressed bit
  int code(int y=0) {
    int p=predictor.p();
    assert(p>=0 && p<4096);
    p+=p<2048;
    U32 xmid=x1 + (x2-x1>>12)*p + ((x2-x1&0xfff)*p>>12);
    assert(xmid>=x1 && xmid<x2);
    if (mode==DECOMPRESS) y=x<=xmid;
    y ? (x2=xmid) : (x1=xmid+1);
    predictor.update(y);
    while (((x1^x2)&0xff000000)==0) {  // pass equal leading bytes of range
      if (mode==COMPRESS) {
        unsigned char c = (x2>>24);
        this->putchar(c);
      }
      x1<<=8;
      x2=(x2<<8)+255;
      if (mode==DECOMPRESS) {
        unsigned char c = this->getchar();
        x=(x<<8)+(c&255);  // EOF is OK
      }
    }
    return y;
  }

public:
  Encoder(Mode m, FILE* f, BitPredictor& pred);
  void flush();  // call this when compression is finished

  // Compress one byte
  void compress(int c) {
    assert(mode==COMPRESS);
    for (int i=7; i>=0; --i)
      code((c>>i)&1);
  }

  // Decompress and return one byte
  int decompress() {
    int c=0;
    for (int i=0; i<8; ++i)
      c+=c+code();
    return c;
  }
};

Encoder::Encoder(Mode m, FILE* f, BitPredictor& pred):
    stopflag(false), ffff_attention(false), mode(m), archive(f), x1(0), x2(0xffffffff), x(0), predictor(pred) {
  if (mode==DECOMPRESS) {  // x = first 4 bytes of archive
    for (int i=0; i<4; ++i)
      x=(x<<8)+(this->getchar()&255);
  }
}

void Encoder::flush() {
  if (mode==COMPRESS)
    this->putchar(x1>>24);  // Flush first unequal byte of range
}


//////////////////////////// User Interface ////////////////////////////

unsigned char buffer[0x3EFE]; // don't increase the length without thinking

int getmem(unsigned char mem) {
  return 1<<(mem-'0'+20);
}

void do_compress(FILE* in, FILE* out, unsigned char mem, BitPredictor& predictor) {
    fprintf(out, "pQS%c", mem);
    fflush(out);

    for(;;) {
      int ret = fread(&buffer, 1, sizeof buffer, in);
      if (ret==0 || ret==-1) {
        break;
      }
      
      // 0xxxxxxx one plain byte
      // 10xxxxxx len up to 64
      // 11xxxxxx len up to 2^(6+8) == 16384
      
      bool smallthing = false;
      
      if(ret<7) {
        smallthing=true;
        int i;
        for (i=1; i<ret; ++i) {
          if (buffer[i]>=0x80) smallthing=false;
        }
      }
      
      if(smallthing) {
        fwrite(buffer, ret, 1, out);
        fflush(out);
        continue;
      } else
      if(ret<64) {
        unsigned char c = ret | 0x80;
        putc(c, out);
      } else {
        int c = (ret >> 8) | 0xC0;
        int d = ret & 0xFF;
        putc(c, out); // maximum 0xFE
        putc(d, out); // maximum 0xFE
      }
      
      Encoder e(COMPRESS, out, predictor);
      
      int i;
      for (i=0; i<ret; ++i) {
        e.compress(buffer[i]);
      }
      e.flush();
      putc(0xFF, out);
      putc(0xFF, out);
      fflush(out);
      
    }
}

void do_decompress(FILE* in, FILE* out, BitPredictor& predictor) {
    // Check header version, get memory option, file size
    if (getc(in)!='p' || getc(in)!='Q' || getc(in)!='S')
      quit("Not a lpaq1_stream file");
    
    {
      int m = getc(in);
      if (m<'0' || m>'9') quit("Bad memory option (not 0..9)");
      int MEM2 = getmem(m);
      assert(MEM2 == predictor.MEM());
    }

    for (;;) {
      int c = getc(in);
      if(c==EOF) break;
      int len;
      if(c==0xFF) continue;
      if (c<0x80) {
        if(out) {
          putc(c, out);
          fflush(out);
        }
        continue;
      }
      if (c<0xC0) {
        len = c&0x3F;
      } else {
        int d = getc(in);
        len = ((c&0x3F) << 8) | d;
      }
      
      Encoder e(DECOMPRESS, in, predictor);
      int i;
      for(i=0; i<len; ++i) {
        unsigned char c = e.decompress();
        if (out) {
          putc(c, out);
        }
      }
      if (out) {
        fflush(out);
      }
    }
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

void do_analyse(FILE* in, FILE* out, const char* modes, BitPredictor& predictor, int filter_mode, int MEM) {
  struct {
    BitPredictor* template_;
    BitPredictor* active;
    int s;
    char mode;
    bool needs_reset;
  } info[16];
  
  memset(info, 0, sizeof(info));
  
  
  #define ITERATE_MODES   for (int i=0; i<sizeof(info)/sizeof(*info) && modes[i]; ++i) 
  ITERATE_MODES {
    auto & in = info[i];
    in.mode = modes[i];
    in.s = 0;
    switch(in.mode) {
      case 'P':
        in.template_ = new BitPredictor(predictor);
        break;
      case 'p':
        in.template_ = &predictor;
        break;
      case 'c':
      case 'C':
        in.template_ = new BitPredictor(MEM);
        break;
      default:
        assert(!"Invalid measure mode");
    }
    switch (in.mode) {
      case 'p':
      case 'c':
        in.active = new BitPredictor(MEM);
        in.needs_reset = true;
        break;
      case 'P':
      case 'C':
        in.active = in.template_;
        in.needs_reset = false;
        break;
    }
  }
  
  bool negative_filter = false;
  if (filter_mode < 0) { filter_mode = -filter_mode; negative_filter = true; }
  
  while(!feof(in)) { 
    char line[65536]; 
    if(!fgets(line, sizeof line-1, in)) break; 
    line[65535]=0; 
    int l = strlen(line);
    
    ITERATE_MODES {
      auto & in = info[i];
      
      if (in.needs_reset) {
        *in.active = *in.template_;
      }      
      
      in.s = measure_entropy(line, l, *in.active);   
      
      if (filter_mode==-1) {
        fprintf(out, "%d ", in.s);
      }
    }
    
    bool do_output = true;
    
    if (filter_mode != -1) {
      if (info[1].s  > ((unsigned long long)filter_mode) * info[0].s / 1000) do_output = false;
      if (negative_filter) do_output = ! do_output;
    }
    
    if (do_output) {
      fwrite(line, 1, l, out);
      fflush(out);
    }
  }
}

void do_fantasy(FILE* in, FILE* out, BitPredictor& predictor, int length, int MEM)
{
  BitPredictor p(MEM);
  
  int gap = 1024;
  if (getenv("GAP")) gap=atoi(getenv("GAP"));
  
  while(!feof(in)) { 
    char line[65536]; 
    if(!fgets(line, sizeof line-1, in)) break; 
    line[65535]=0; 
    int l = strlen(line)-1;
    line[l]=0; // trim '\n'
    
    p = predictor;
    
    for (int j=0; j<l; ++j) {
      for (int i=7; i>=0; --i) {
        int bit = (line[j]>>i)&1;      
        p.update(bit);
      }
    }
    
    fwrite(line, 1, l, out);
    
    for (int j=0; j<length; ++j) {
      unsigned char c = 0;
      for (int i=7; i>=0; --i) {
        
        int prediction = p.p();
        
        int bit;
        if        (prediction > 2048+gap) {
          bit=1;  
        } else if (prediction < 2048-gap) {
          bit=0;
        } else {
          bit = rand() < RAND_MAX/4096*prediction ? 1 : 0;
        }
        
        p.update(bit);
        
        c<<=1;
        c|=bit;
      }
      fputc(c, out);
    }
    
    fprintf(out, "\n");
    fflush(out);
  }
}

int main(int argc, char **argv) {
  // Check arguments
  if (argc<3 || argc > 3  ||  !isdigit(argv[1][0]) || !strcmp(argv[1], "--help")) {
    printf(
      "lpaq1 file compressor (C) 2007, Matt Mahoney\n"
      "Licensed under GPL, http://www.gnu.org/copyleft/gpl.html\n"
      "Stream version by _Vi (http://vi-server.org)\n"
      "\n"
      "To compress:      lpaq1_stream N -c < file > file.lps  (N=0..9, uses 3+3*2^N MB)\n"
      "To decompress:    lpaq1_stream N -d < file.lps > file  (needs same memory)\n"
      "To analyse lines: lpaq1_stream N --analyse=[pPcC] < file.txt > file.txt\n"
      "                      p - prefeeded (see PRELOAD or LOAD); c - clean; P/C - accumulated\n"
      "To filter lines:  lpaq1_stream N --filter=5000 < file.txt > file.txt\n"
      "                      (useless without PRELOAD or LOAD, argument is per millis, negative for inclusive filtering)\n"
      "To 'guess' continuations of lines: lpaq1_stream N --fantasy=length < file.txt > file.txt\n"
      "                      (useless without PRELOAD or LOAD)\n"
      "\n"
      "Each read produces a compressed chunk, \"lpaq1_stream 3 -c | lpaq1_stream 3 -d\" should print your input immediately. \n"
      "\n"
      "Set PRELOAD to initialize predictor with the specified lpaq1_stream-compressed file.\n"
      "Set LOAD to load predictor state before working, SAVE to save it after working.\n");
    return 1;
  }

  // Get start time
  clock_t start=clock();

  // Open input file
  FILE *in = stdin;
  FILE *out = stdout;

  int MEM = getmem(argv[1][0]);

  BitPredictor predictor(MEM);
  
  if (getenv("LOAD")) { FILE* f = fopen(getenv("LOAD"), "rb"); predictor.load(f); }
  
  if (getenv("PRELOAD")) {
    FILE* preload = fopen(getenv("PRELOAD"), "rb");
    if(!preload) quit("Can't open PRELOAD file");
    do_decompress(preload, NULL, predictor);
  }
  
  // Compress
  if (!strcmp(argv[2], "-c")) {
      do_compress(in, out, argv[1][0], predictor);
  } else
  if (!strncmp(argv[2], "--analyse=", strlen("--analyse="))) {
      const char* modes = argv[2] + strlen("--analyse=");
      do_analyse(in, out, modes, predictor, -1, MEM);
  } else
  if (!strncmp(argv[2], "--filter=", strlen("--filter="))) {
      do_analyse(in, out, "pc", predictor, atoi(argv[2] + strlen("--filter=")), MEM);
  } else
  if (!strncmp(argv[2], "--fantasy=", strlen("--fantasy="))) {
      do_fantasy(in, out, predictor, atoi(argv[2]+strlen("--fantasy=")), MEM);
  } else
  if (!strcmp(argv[2], "-d")) {
    do_decompress(in, out, predictor);
  } else {
    fprintf(stderr, "Unknown mode %s\n", argv[2]);
    return 1;  
  }
  
  if (getenv("SAVE")) { FILE* f = fopen(getenv("SAVE"), "wb"); predictor.save(f); }

  return 0;
}
