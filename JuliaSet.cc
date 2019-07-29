#include "JuliaSet.hh"

#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <phosg/Image.hh>
#include <stdexcept>

#include "Complex.hh"
#include "Iterate.hh"

using namespace std;


FractalResult julia_fractal(const vector<complex>& coeffs, size_t w, size_t h,
    double xmin, double xmax, double ymin, double ymax, double precision,
    double detect_precision, size_t max_depth, size_t result_bit_width,
    size_t* progress) {

  size_t degree = coeffs.size() - 1;
  double xs = (xmax - xmin) / w, ys = (ymax - ymin) / h, xp, yp = ymin;
  FractalResult result = {vector<complex>(), Image(w, h, false, result_bit_width)};

  for (size_t y = 0; y < h; y++) {
    xp = xmin;

    for (size_t x = 0; x < w; x++) {
      complex this_root(xp, yp);
      size_t this_depth = max_depth;
      this_root = root(coeffs, this_root, precision, &this_depth);
      this_depth = max_depth - this_depth;

      if ((this_root.real == 0) && (this_root.imag == 0)) {
        result.data.write_pixel(x, y, 0, 0, 1);

      } else {
        size_t root_index;
        for (root_index = 0; root_index < result.roots.size(); root_index++) {
          if (result.roots[root_index].equal(this_root, detect_precision)) {
            break;
          }
        }

        if (root_index == result.roots.size()) {
          if (result.roots.size() > degree) {
            throw runtime_error("too many roots");
          }
          result.roots.push_back(this_root);
        }
        result.data.write_pixel(x, y, this_depth, root_index, 0);
      }

      xp += xs;
    }
    yp += ys;
    *progress = y;
  }

  return result;
}
