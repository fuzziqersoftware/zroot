#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <deque>
#include <map>
#include <phosg/Filesystem.hh>
#include <phosg/Image.hh>
#include <phosg/Strings.hh>
#include <set>
#include <thread>

#include "Complex.hh"
#include "Iterate.hh"
#include "JuliaSet.hh"

using namespace std;
using namespace std::chrono_literals;


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

Image color_fractal(const Image& data, int64_t min_intensity = -1,
    int64_t max_intensity = -1, vector<ssize_t> replacement_map = vector<ssize_t>()) {
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

      root_index = (root_index < replacement_map.size()) ? replacement_map[root_index] : root_index;

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



vector<ssize_t> align_roots(const FractalResult& current, const FractalResult& prev) {
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

  return replacement_map;
}



class MultiFrameRenderer {
public:
  struct FrameMetadata {
    size_t frame_index;
    vector<complex> frame_coeffs;
    size_t w, h;
    double xmin, xmax, ymin, ymax;
    double precision, detect_precision;
    size_t max_iterations;
    size_t result_bit_width;
  };

private:
  size_t thread_count;
  size_t ready_limit;
  vector<thread> threads;
  vector<size_t> worker_progress;

  mutable mutex lock;
  condition_variable cond;
  deque<FrameMetadata> pending_work;
  map<size_t, FractalResult> results;
  size_t next_result;

public:

  MultiFrameRenderer(size_t thread_count, size_t ready_limit) :
      thread_count(thread_count), ready_limit(ready_limit), next_result(0) { }

  ~MultiFrameRenderer() {
    for (auto& t : this->threads) {
      t.join();
    }
  }

  const vector<size_t>& get_worker_progress() const {
    return this->worker_progress;
  }

  size_t work_queue_length() const {
    unique_lock<mutex> g(this->lock);
    return this->pending_work.size();
  }

  size_t result_queue_length() const {
    unique_lock<mutex> g(this->lock);
    return this->results.size();
  }

  void add(FrameMetadata&& m) {
    unique_lock<mutex> g(this->lock);
    this->pending_work.emplace_back(move(m));
  }

  void start() {
    this->worker_progress.resize(this->thread_count);
    while (this->threads.size() < this->thread_count) {
      this->threads.emplace_back(&MultiFrameRenderer::worker, this, this->threads.size());
    }
  }

  FractalResult get_result() {
    for (;;) {
      unique_lock<mutex> g(this->lock);
      auto it = this->results.find(this->next_result);
      if (it != this->results.end()) {
        this->next_result++;
        FractalResult ret = move(it->second);
        this->results.erase(it);
        return ret;
      }
      this->cond.wait(g);
    }
  }

  void worker(size_t worker_index) {
    for (;;) {
      FrameMetadata fm;
      {
        unique_lock<mutex> g(this->lock);
        if (this->pending_work.empty()) {
          break;
        }
        if (this->results.size() > this->ready_limit) {
          this->worker_progress[worker_index] = 0;
          g.unlock();
          usleep(100000);
          continue;
        }
        fm = move(this->pending_work.front());
        this->pending_work.pop_front();
      }

      FractalResult res = julia_fractal(fm.frame_coeffs, fm.w, fm.h, fm.xmin,
          fm.xmax, fm.ymin, fm.ymax, fm.precision, fm.detect_precision,
          fm.max_iterations, fm.result_bit_width,
          &this->worker_progress[worker_index]);

      {
        unique_lock<mutex> g(this->lock);
        this->results.emplace(fm.frame_index, move(res));
      }
      this->cond.notify_one();
    }
  }
};



void report_status_thread_fn(atomic<bool>* should_exit,
    MultiFrameRenderer* renderer, size_t* compile_thread_frame, size_t height,
    size_t end_frame) {
  while (!should_exit->load()) {
    string status = "workers";
    for (size_t progress : renderer->get_worker_progress()) {
      status += string_printf(" %5zu/%5zu", progress, height);
    }
    status += string_printf(" compiler %zu/%zu queue %zu ready %zu\n",
        *compile_thread_frame, end_frame, renderer->work_queue_length(),
        renderer->result_queue_length());
    fwritex(stderr, status);
    usleep(1000000);
  }
}




void print_usage(const char* argv0) {
  fprintf(stderr, "\
Usage: %s [options]\n\
\n\
Options:\n\
  --width=X and --height=X: specify the size of the output image(s) in pixels.\n\
      Default size is 2048x1536.\n\
  --window-width=X and --window-height=X: specify the coordinate boundaries of\n\
      the rendered image. The default is 4 by 3, which means the left edge of\n\
      the image is x = -4, the right edge is x = +4, the top edge is y = +3,\n\
      and the bottom edge is y = -3.\n\
  --min-depth=X and --max-depth=X: specify minimum and maximum intensities. The\n\
      color of each pixel illustrates which root that position is attracted to,\n\
      and the intensity illustrates how many iterations were required to\n\
      approach that root. By default, the intensities in the image will\n\
      automatically scale based on the minimum and maximum iteration counts\n\
      over the entire image, but this can be overridden with these options.\n\
  --bit-width=X: specify width of integers used when computing the result.\n\
      Values are 8 (default), 16, 32, and 64.\n\
  --coefficients=X1,X2,X3[@KF]: specify the expression to iterate. This option\n\
      may be given multiple times to produce a linearly-interpolated video; in\n\
      this case, all instances of this option should have a keyframe number at\n\
      the end. The examples below illustrate this usage more clearly.\n\
  --output-filename=NAME: write output to this file (in Windows BMP format). If\n\
      generating a video, the sequence number is appended to the output\n\
      filename. If this option is not given, all images are written in sequence\n\
      to stdout, which is appropriate for ffmpeg's bmp_pipe input filter.\n\
  --thread-count=X: use this many threads to render video frames in parallel.\n\
      If not given, use as many threads as there are CPU cores.\n\
  --ready-limit=X: don\'t start new frames if there are this many waiting to be\n\
      written to the output. Useful to control memory pressure.\n\
\n\
Examples:\n\
  Render Julia set for x^3 - i:\n\
    %s --coefficients=1,0,0,-i --output-filename=cube.bmp\n\
  Animate transition from x^3 - i to x^4 - i to x^5 - i (60 frames each):\n\
    %s --coefficients=1,0,0,-i@0 --coefficients=1,0,0,0,-i@60 \\\n\
        --coefficients=1,0,0,0,0,-i@120 --output-filename=output_frame.bmp\n\
  Animate transition from x^3 - i to x^4 - i and directly encode into a video:\n\
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
  size_t thread_count = 0;
  ssize_t ready_limit = -1;
  size_t result_bit_width = 8;
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

    } else if (!strncmp(argv[x], "--thread-count=", 15)) {
      thread_count = atoi(&argv[x][15]);
    } else if (!strncmp(argv[x], "--ready-limit=", 14)) {
      ready_limit = atoi(&argv[x][14]);
    } else if (!strncmp(argv[x], "--bit-width=", 12)) {
      result_bit_width = atoi(&argv[x][12]);

    } else {
      fprintf(stderr, "unknown command-line option: %s\n", argv[x]);
      return 1;
    }
  }

  if (thread_count == 0) {
    thread_count = thread::hardware_concurrency();
  }
  if (ready_limit < 0) {
    ready_limit = 2 * thread_count;
  }

  if (keyframe_to_coeffs.empty()) {
    print_usage(argv[0]);
    return 1;

  } else if (keyframe_to_coeffs.size() == 1) {
    // rendering a single image
    auto it = *keyframe_to_coeffs.begin();
    size_t progress;
    FractalResult result = julia_fractal(it.second, w, h, xmin, xmax, ymin,
        ymax, precision, detect_precision, max_iterations, result_bit_width,
        &progress);
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

    MultiFrameRenderer renderer(thread_count, ready_limit);
    size_t end_frame = keyframe_to_coeffs.rbegin()->first;
    for (size_t frame = 0; frame <= end_frame; frame++) {

      MultiFrameRenderer::FrameMetadata fm;
      fm.frame_index = frame;
      fm.w = w;
      fm.h = h;
      fm.xmin = xmin;
      fm.xmax = xmax;
      fm.ymin = ymin;
      fm.ymax = ymax;
      fm.precision = precision;
      fm.detect_precision = detect_precision;
      fm.max_iterations = max_iterations;
      fm.result_bit_width = result_bit_width;
      if ((next_kf_it != keyframe_to_coeffs.end()) && (frame == next_kf_it->first)) {
        kf_it++;
        next_kf_it++;
        fm.frame_coeffs = kf_it->second;

      } else {
        // linearly interpolate coeffs between the keyframes
        size_t interval_frames = next_kf_it->first - kf_it->first;
        size_t progress = frame - kf_it->first;
        for (size_t x = 0; x < kf_it->second.size(); x++) {
          fm.frame_coeffs.emplace_back(((next_kf_it->second[x] * progress) + (kf_it->second[x] * (interval_frames - progress))) / interval_frames);
        }
      }

      renderer.add(move(fm));
    }

    renderer.start();

    size_t frame = 0;
    atomic<bool> should_exit(false);
    thread status_thread(&report_status_thread_fn, &should_exit, &renderer,
        &frame, h, end_frame);

    FractalResult prev_result = {vector<complex>(), Image(0, 0)};
    for (frame = 0; frame <= end_frame; frame++) {
      FractalResult result = renderer.get_result();

      vector<ssize_t> replacement_map;
      if (!prev_result.roots.empty()) {
        replacement_map = align_roots(result, prev_result);
      }
      Image img = color_fractal(result.data, min_intensity, max_intensity,
          replacement_map);

      // reorder the roots so the next frame will make sense
      vector<complex> new_roots(result.roots.size());
      for (size_t x = 0; x < replacement_map.size(); x++) {
        new_roots[replacement_map[x]] = result.roots[x];
      }
      result.roots = move(new_roots);

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

    should_exit.store(true);
    status_thread.join();
    fputc('\n', stderr);
  }

  return 0;
}
