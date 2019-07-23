#include "Complex.hh"

#include <math.h>


complex::complex() : complex(0.0, 0.0) { }

complex::complex(double real, double imag) : real(real), imag(imag) { }

complex complex::pow(int p) const {
  complex res = *this;
  for (size_t x = 0; x < p; x++) {
    res = res * (*this);
  }
  return res;
}

bool complex::equal(const complex& b, double precision) const {
  double diff = fabs(this->real - b.real);
  double idiff = fabs(this->imag - b.imag);
  return ((diff < precision) && (idiff < precision));
}
