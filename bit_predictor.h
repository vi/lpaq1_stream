#pragma once

class Predictor;

class BitPredictor {
public:  
  int p() const; // probability that next bit will be 1, from 0 to 4095
  void update(int y); // feed the next bit; y is one bit - 0 or 1
  
  int MEM() const;
  
  BitPredictor(int MEM); // 2*(20+n) bytes
  BitPredictor(const BitPredictor& p);
  BitPredictor& operator= (const BitPredictor& p);
  ~BitPredictor();
private:
  Predictor* impl;
};
