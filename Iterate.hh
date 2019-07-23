#pragma once

#include <vector>

#include "Complex.hh"


complex root(const std::vector<double>& coeffs, const complex& guess,
    double precision, size_t* max);
