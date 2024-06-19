#include <stdint.h>
#include "lowpass.h"

// Integer implementation of low pass filter
// Be carefull with data types!
int16_t intLowPass(int16_t* bias, int16_t raw) {
  *bias = (*bias + ((raw - *bias) / 1024)); // int division!
  return raw - *bias;
}

// Double implementation of low pass filter
// Starts to lose precision after lots of iterations 
double doubleLowPass(double* bias, double raw) {
  *bias = (*bias + ((raw - *bias) / 1024.0));
  return raw - *bias;
}
