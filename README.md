lpaq1_stream
============

There [lpaq1](http://encode.ru/threads/874-lpaq1-is-here) - simplified PAQ compressor.

I added some funny modes to it to play with compression:

* lpaq1_stream: Other archive format, suitable for intermediate flushing;
* lpaq1_stream: "Preloading" of other archives to assist compression of little files;
* lpaq1: Removed filesize restriction (now can [de]compress to/from pipe);
* lpaq1: "Stream decompress mode" to extract files with unknown filesize (with some garbade in the end);
* lpaq1: Fuzz decompression (deliverately misdecompress files to see broken content);
* lpaq1: Autonomous mode (after normal decompression, keep decompressing ad infinum from random data);
* lpaq1: Guess mode (output only predicted bits. Lets you to see which parts of file is the most compressible).

Demos
---

```
$ lpaq1_stream 3 | ./lpaq1_stream -d
dsaf
dsaf
123456
123456

$ lpaq1_stream 3 < lpaq1_stream.cpp > lpaq1_stream.cpp.lps
$ lpaq1_stream 3 < lpaq1.cpp > lpaq1.cpp.lps
$ LPAQ_PRELOAD_FILE=lpaq1_stream.cpp.lps lpaq1_stream 3 < lpaq1_stream.cpp > lpaq1_stream.cpp.lps2
$ LPAQ_PRELOAD_FILE=lpaq1_stream.cpp.lps lpaq1_stream 3 < lpaq1.cpp > lpaq1.cpp.lps2
$ du -b lpaq1*.cpp*lps* lpaq1*.cpp
9418  lpaq1.cpp.lps
1130	lpaq1.cpp.lps2
9449	lpaq1_stream.cpp.lps
86	lpaq1_stream.cpp.lps2
32786	lpaq1.cpp
33621	lpaq1_stream.cpp



$ lpaq1 3 lpaq1.cpp lpaq1.cpp.lpq
$ lpaq1 a lpaq1.cpp.lpq /dev/stdout  | uniq | head -n 970 | tail -n 20
    3+(MEM>>20)*3);

  return len;
      }
    }
  }
  if (in), ftell(out), double(clock()-start)/CLOCKS_PER_SEC, 
      }
    }
  }
  if (in), ftell(out), double(clock()-starw#24;
    ftell(in), ftell(out), double(clock()-start)/CLOCKS_PER_SEC, 
      }
    }
  }
  if (in), ftell(out), double(clock()/M@{clock())\ufffd\n");
    }
  }
  if (in), ftell(out), double(clock())\ufffd\n");
    }
    
    
$ lpaq1 g lpaq1.cpp.lpq /dev/stdout | head -n 300 | tail -n 10
ilefine gephstate$?}fe statetable[#tateO;#}du-
m
o/'/////////////////////////(state]ap& 9pM o/////////////////////////-

//(Y`stateMap(~etp`k`bontext(`k q`bbobability$  [apac~c*-
/-
// StateTap(gse~5 qfeate# a`stateMap uith c0aontext3 dsang s=>`ci4es gamory&-
/ Wsh"ap$ ac(`geoit$ contert# gdate`k`(lam&=$s= dk c probability(hq..w !5u&-
// p ` $het dhe gext a! > $rtataog dhe prebious`vbetiction eith c lq..4=$-
//     wemit h1<.17$3< 0evmt~d``243- es dhe ~epimum eoent hor aoo`qta~g {m
```