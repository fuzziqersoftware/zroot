#pragma once

#include <vector>

#include "Complex.hh"


complex root(const std::vector<complex>& coeffs, const complex& guess,
    double precision, size_t* max);
