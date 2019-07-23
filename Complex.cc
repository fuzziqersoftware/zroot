#include "Complex.hh"

#include <math.h>
#include <string.h>
#include <stdio.h>

#include <phosg/Strings.hh>
#include <stdexcept>

using namespace std;


complex::complex() : complex(0.0, 0.0) { }

complex::complex(double real, double imag) : real(real), imag(imag) { }

complex::complex(const char* text) {
  size_t count = strlen(text);
  size_t used_count;

  // i and -i
  if (!strcmp(text, "i")) {
    this->real = 0.0;
    this->imag = 1.0;
    return;
  }
  if (!strcmp(text, "-i")) {
    this->real = 0.0;
    this->imag = -1.0;
    return;
  }

  // real numbers
  if ((sscanf(text, "%lf%zn", &this->real, &used_count) == 1) && (used_count == count)) {
    this->imag = 0;
    return;
  }

  // imaginary numbers
  if ((sscanf(text, "%lfi%zn", &this->imag, &used_count) == 1) && (used_count == count)) {
    this->real = 0;
    return;
  }

  // if the imaginary part is 1 or -1, the 1 can be omitted
  if ((sscanf(text, "%lf+i%zn", &this->real, &used_count) == 1) && (used_count == count)) {
    this->imag = 1.0;
    return;
  }
  if ((sscanf(text, "%lf-i%zn", &this->real, &used_count) == 1) && (used_count == count)) {
    this->imag = -1.0;
    return;
  }

  // true complex numbers
  if ((sscanf(text, "%lf%lfi%zn", &this->real, &this->imag, &used_count) == 2) && (used_count == count)) {
    return;
  }

  throw invalid_argument("text is not a complex number");
}

string complex::str() const {
  if (this->imag == 0.0) {
    return string_printf("%lf", this->real);
  }
  if (this->real == 0.0) {
    return string_printf("%lfi", this->imag);
  }
  if (this->imag < 0) {
    return string_printf("%lf-%lfi", this->real, -this->imag);
  }
  return string_printf("%lf+%lfi", this->real, this->imag);
}

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
