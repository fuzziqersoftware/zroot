#include "Iterate.hh"

#include <math.h>

#include "Complex.hh"

using namespace std;


static const complex zero = {0, 0};

#ifndef X86_64
static complex root_iterate(const vector<complex>& coeffs,
    const complex& guess) {

  complex numer(0, 0);
  complex denom(0, 0);

  complex guess_powers[coeffs.size()];
  guess_powers[0].real = 1;
  guess_powers[0].imag = 0;
  for (size_t x = 1; x < coeffs.size(); x++) {
    guess_powers[x] = guess_powers[x - 1] * guess;
  }

  int64_t degree = static_cast<int64_t>(coeffs.size() - 1);
  for (ssize_t x = 0; x < coeffs.size(); x++) {
    numer = numer + (guess_powers[degree - x] * coeffs[x]) * (degree - x - 1);
    denom = denom + (guess_powers[degree - x - 1] * coeffs[x]) * (degree - x);
  }
  return (numer / denom);
}
#endif


complex root(const vector<complex>& coeffs, const complex& guess,
    double precision, size_t* max) {

  complex this_guess = guess;
  complex last(0, 0);
  do {
    last = this_guess;
#ifdef X86_64
    root_iterate_asm(coeffs.data(), coeffs.size(), &this_guess, &this_guess);
#else
    this_guess = root_iterate(coeffs, this_guess);
#endif
    (*max)--;
  } while (!last.equal(this_guess, precision) && (*max));
  return *max ? this_guess : zero;
}
