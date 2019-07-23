#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <phosg/Image.hh>
#include <phosg/Strings.hh>
#include <set>

#include "Complex.hh"
#include "Iterate.hh"
#include "JuliaSet.hh"

using namespace std;


struct Color {
  uint8_t r, g, b;
};

vector<Color> colors({
  {0xFF, 0x00, 0x00}, // red
  {0xFF, 0x80, 0x00}, // orange
  {0xFF, 0xFF, 0x00}, // yellow
  {0x00, 0xFF, 0x00}, // green
  {0x00, 0xFF, 0xFF}, // cyan
  {0x00, 0x00, 0xFF}, // blue
  {0xFF, 0x00, 0xFF}, // magenta
  {0xFF, 0x80, 0x80}, // light red
  {0xFF, 0xC0, 0x80}, // light orange
  {0xFF, 0xFF, 0x80}, // light yellow
  {0x80, 0xFF, 0x80}, // light green
  {0x80, 0xFF, 0xFF}, // light cyan
  {0x80, 0x80, 0xFF}, // light blue
  {0xFF, 0x80, 0xFF}, // light magenta
  {0x80, 0x00, 0x00}, // dark red
  {0x80, 0x40, 0x00}, // dark orange
  {0x80, 0x80, 0x00}, // dark yellow
  {0x00, 0x80, 0x00}, // dark green
  {0x00, 0x80, 0x80}, // dark cyan
  {0x00, 0x00, 0x80}, // dark blue
  {0x80, 0x00, 0x80}, // dark purple
});

Image color_fractal(const Image& data, int64_t min_intensity = -1, int64_t max_intensity = -1) {
  bool compute_min_intensity = false, compute_max_intensity = false;

  if (min_intensity < 0) {
    if (max_intensity >= 0) {
      min_intensity = max_intensity;
    } else {
      data.read_pixel(0, 0, reinterpret_cast<uint64_t*>(&min_intensity), NULL, NULL);
    }
    compute_min_intensity = true;
  }
  if (max_intensity < 0) {
    max_intensity = min_intensity;
    compute_max_intensity = true;
  }

  // compute the min and max intensities in the entire data image
  if (compute_max_intensity || compute_max_intensity) {
    for (size_t y = 0; y < data.get_height(); y++) {
      for (size_t x = 0; x < data.get_width(); x++) {
        uint64_t depth, root_index, error;
        data.read_pixel(x, y, &depth, &root_index, &error);
        if (error) {
          continue;
        }

        if (compute_max_intensity && (depth < min_intensity)) {
          min_intensity = depth;
        }
        if (compute_max_intensity && (depth > max_intensity)) {
          max_intensity = depth;
        }
      }
    }
  }

  // convert the depths and root indexes into colors
  Image result(data.get_width(), data.get_height());
  uint64_t intensity_range = max_intensity - min_intensity;
  for (size_t y = 0; y < data.get_height(); y++) {
    for (size_t x = 0; x < data.get_width(); x++) {
      uint64_t depth, root_index, error;
      data.read_pixel(x, y, &depth, &root_index, &error);
      if (error) {
        result.write_pixel(x, y, 0xFF, 0xFF, 0xFF);
        continue;
      }

      depth = (depth > max_intensity) ? max_intensity : depth;
      depth = (depth < min_intensity) ? min_intensity : depth;

      const Color& color = colors[root_index];
      uint64_t r, g, b;
      if (intensity_range == 0) {
        r = (depth >= max_intensity) ? color.r : 0;
        g = (depth >= max_intensity) ? color.g : 0;
        b = (depth >= max_intensity) ? color.b : 0;
      } else {
        r = (((depth - min_intensity) * color.r) / intensity_range);
        g = (((depth - min_intensity) * color.g) / intensity_range);
        b = (((depth - min_intensity) * color.b) / intensity_range);
      }
      result.write_pixel(x, y, r, g, b);
    }
  }

  return result;
}



void align_roots(FractalResult& current, const FractalResult& prev) {
  multimap<double, pair<size_t, size_t>> distance_pairs;
  for (size_t x = 0; x < prev.roots.size(); x++) {
    for (size_t y = 0; y < current.roots.size(); y++) {
      complex diff = prev.roots[x] - current.roots[y];
      distance_pairs.emplace(diff.abs2(), make_pair(x, y));
    }
  }

  set<ssize_t> unused_root_indices;
  for (ssize_t x = 0; x < current.roots.size(); x++) {
    unused_root_indices.emplace(x);
  }

  vector<ssize_t> replacement_map(current.roots.size(), -1);
  for (const auto& it : distance_pairs) {
    const auto& rep = it.second;
    if ((replacement_map[rep.second] < 0) && unused_root_indices.erase(rep.first)) {
      replacement_map[rep.second] = rep.first;
    }
  }

  // map any unassigned roots to free values
  for (size_t x = 0; x < replacement_map.size(); x++) {
    if (replacement_map[x] >= 0) {
      continue;
    }

    auto it = unused_root_indices.begin();
    replacement_map[x] = *it;
    unused_root_indices.erase(it);
  }

  for (size_t y = 0; y < current.data.get_height(); y++) {
    for (size_t x = 0; x < current.data.get_width(); x++) {
      uint64_t depth, root_index, error;
      current.data.read_pixel(x, y, &depth, &root_index, &error);
      if (error) {
        continue;
      }
      current.data.write_pixel(x, y, depth, replacement_map[root_index], 0);
    }
  }

  vector<complex> new_roots(current.roots.size());
  for (size_t x = 0; x < replacement_map.size(); x++) {
    new_roots[replacement_map[x]] = current.roots[x];
  }
  current.roots = move(new_roots);
}



void print_usage(const char* argv0) {
  fprintf(stderr, "\
usage: %s [options]\n\
\n\
options:\n\
  --width=X and --height=X: specify the size of the output image(s) in pixels.\n\
      default 2048x1536\n\
  --window-width=X and --window-height=X: specify the coordinate boundaries of\n\
      the rendered image. default 4x3 (left edge of image is -4, right edge is\n\
      +4; top edge is +3, bottom edge is -3)\n\
  --min-depth=X and --max-depth=X: specify minimum and maximum intensities for\n\
      iteration counts\n\
  --coefficients=X1,X2,X3[@KF]: specify the equation to iterate. may be given\n\
      multiple times to produce a linearly-interpolated video; in this case,\n\
      all instances of this option should have a keyframe number at the end\n\
  --output-filename=NAME: write output to this file (windows bmp format). if\n\
      generating a video, the sequence number is appended to the output\n\
      filename. if not given, all images are written in sequence to stdout\n\
\n\
examples:\n\
  render julia set for x^3 - i:\n\
    %s --coefficients=1,0,0,-i --output-filename=cube.bmp\n\
  animate transition from x^3 - i to x^4 - i to x^5 - i (60 frames each):\n\
    %s --coefficients=1,0,0,-i@0 --coefficients=1,0,0,0,-i@60 \\\n\
        --coefficients=1,0,0,0,0,-i@120 --output-filename=cube.bmp\n\
  animate transition from x^3 - i to x^4 - i and directly encode into a video:\n\
    %s --coefficients=1,0,0,-i@0 --coefficients=1,0,0,0,-i@60 \\\n\
        | ffmpeg -r 30 -f bmp_pipe -i - -c:v libx264 -crf 0 -r 30 output.avi\n\
", argv0, argv0, argv0, argv0);
}

int main(int argc, char* argv[]) {
  double xmin = -4.0, ymin = -3.0, xmax = 4.0, ymax = 3.0;
  double precision = 0.0000001; // calculation precision
  double detect_precision = 0.0001; // detection precision (must be less precise than calc precision)
  size_t max_iterations = 100; // speed for fail points (higher is slower)

  int w = 2048, h = 1536;
  int64_t min_intensity = -1, max_intensity = -1;
  map<size_t, vector<complex>> keyframe_to_coeffs;
  size_t max_coeffs = 0;
  const char* output_filename = NULL;
  for (int x = 1; x < argc; x++) {

    if (!strncmp(argv[x], "--width=", 8)) {
      w = atoi(&argv[x][8]);
    } else if (!strncmp(argv[x], "--height=", 9)) {
      h = atoi(&argv[x][9]);

    } else if (!strncmp(argv[x], "--window-width=", 15)) {
      xmin = atof(&argv[x][15]) / 2;
      xmax = -xmin;
    } else if (!strncmp(argv[x], "--window-height=", 16)) {
      ymin = atof(&argv[x][16]) / 2;
      ymax = -ymin;

    } else if (!strncmp(argv[x], "--min-depth=", 12)) {
      min_intensity = atoi(&argv[x][12]);
    } else if (!strncmp(argv[x], "--max-depth=", 12)) {
      max_intensity = atoi(&argv[x][12]);

    } else if (!strncmp(argv[x], "--coefficients=", 15)) {
      size_t frame;
      auto tokens = split(&argv[x][15], '@');
      if (tokens.size() > 2) {
        fprintf(stderr, "invalid frame specifier: %s\n", argv[x]);
        return 1;
      } else if (tokens.size() == 2) {
        frame = stoi(tokens[1]);
      } else {
        frame = 0;
      }

      tokens = split(tokens[0], ',');
      vector<complex> coeffs;
      for (const string& token : tokens) {
        coeffs.emplace_back(token.c_str());
      }
      if (coeffs.size() > max_coeffs) {
        max_coeffs = coeffs.size();
      }

      keyframe_to_coeffs.emplace(frame, move(coeffs));

    } else if (!strncmp(argv[x], "--output-filename=", 18)) {
      output_filename = &argv[x][18];

    } else {
      fprintf(stderr, "unknown command-line option: %s\n", argv[x]);
      return 1;
    }
  }

  if (keyframe_to_coeffs.empty()) {
    print_usage(argv[0]);
    return 1;

  } else if (keyframe_to_coeffs.size() == 1) {
    // rendering a single image
    auto it = *keyframe_to_coeffs.begin();
    FractalResult result = julia_fractal(it.second, w, h, xmin, xmax, ymin,
        ymax, precision, detect_precision, max_iterations);
    Image img = color_fractal(result.data, min_intensity, max_intensity);
    if (output_filename) {
      img.save(output_filename, Image::ImageFormat::WindowsBitmap);
    } else {
      img.save(stdout, Image::ImageFormat::WindowsBitmap);
    }

  } else {
    // rendering a video (or sequence of images)

    // make sure coeffs are the same length in all keyframes
    for (auto& it : keyframe_to_coeffs) {
      if (it.second.size() < max_coeffs) {
        it.second.insert(it.second.begin(), max_coeffs - it.second.size(), complex());
      }
    }

    auto kf_it = keyframe_to_coeffs.begin();
    auto next_kf_it = kf_it;
    advance(next_kf_it, 1);

    vector<complex> frame_coeffs = kf_it->second;
    FractalResult prev_result = {vector<complex>(), Image(0, 0)};
    size_t end_frame = keyframe_to_coeffs.rbegin()->first;
    for (size_t frame = 0; frame <= end_frame; frame++) {

      if ((next_kf_it != keyframe_to_coeffs.end()) && (frame == next_kf_it->first)) {
        kf_it++;
        next_kf_it++;
        frame_coeffs = kf_it->second;

      } else {
        // linearly interpolate coeffs between the keyframes
        for (size_t x = 0; x < kf_it->second.size(); x++) {
          size_t interval_frames = next_kf_it->first - kf_it->first;
          size_t progress = frame - kf_it->first;
          frame_coeffs[x] = ((next_kf_it->second[x] * progress) + (kf_it->second[x] * (interval_frames - progress))) / interval_frames;
        }
      }

      fprintf(stderr, "... frame %zu of %zu ->", frame, end_frame);
      for (const complex& coeff : frame_coeffs) {
        string s = coeff.str();
        fprintf(stderr, " %s", s.c_str());
      }
      fputc('\r', stderr);

      FractalResult result = julia_fractal(frame_coeffs, w, h, xmin, xmax, ymin,
          ymax, precision, detect_precision, max_iterations);
      if (!prev_result.roots.empty()) {
        align_roots(result, prev_result);
      }
      Image img = color_fractal(result.data, min_intensity, max_intensity);

      if (output_filename) {
        string numbered_filename = output_filename;
        if (ends_with(numbered_filename, ".bmp")) {
          numbered_filename = numbered_filename.substr(0, numbered_filename.size() - 4) + string_printf(".%zu.bmp", frame);
        } else {
          numbered_filename += string_printf(".%zu", frame);
        }
        img.save(numbered_filename.c_str(), Image::ImageFormat::WindowsBitmap);
      } else {
        img.save(stdout, Image::ImageFormat::WindowsBitmap);
      }

      prev_result = move(result);
    }
    fputc('\n', stderr);
  }

  return 0;
}
