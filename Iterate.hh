#pragma once

#include <vector>

#include "Complex.hh"


complex root(const std::vector<complex>& coeffs, const complex& guess,
    double precision, size_t* max);

// on x86_64 theres an optimized assembly version of this code that's a bit
// faster
#ifdef X86_64

extern "C" {

void root_iterate_asm(const complex* coeffs, size_t coeffs_count,
    const complex* guess, complex* result);

}

#endif
