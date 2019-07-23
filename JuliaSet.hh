#pragma once

#include <phosg/Image.hh>
#include <vector>

#include "Complex.hh"


struct FractalResult {
  std::vector<complex> roots;
  Image data;
};

FractalResult julia_fractal(const std::vector<complex>& coeffs, size_t w,
    size_t h, double xmin, double xmax, double ymin, double ymax,
    double precision, double detect_precision, size_t max_depth);
