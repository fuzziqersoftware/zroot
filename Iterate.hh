#pragma once

#include <vector>

#include "Complex.hh"


complex root(const std::vector<complex>& coeffs, const complex& guess,
    double precision, size_t* max);

// On amd64 there's an optimized assembly version of this code that's a bit
// faster
#ifdef AMD64

extern "C" {

void root_iterate_asm(const complex* coeffs, size_t coeffs_count,
    const complex* guess, complex* result);

}

#endif
