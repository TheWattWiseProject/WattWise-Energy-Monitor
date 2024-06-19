#ifndef __LOWPASS_H__
#define __LOWPASS_H__

int16_t intLowPass(int16_t* bias, int16_t raw);

double doubleLowPass(double* bias, double raw);

#endif
