# zroot

zroot generates high-resolution Julia set fractal images and videos, but it doesn't do so quickly.

## Building

- Build and install phosg (https://github.com/fuzziqersoftware/phosg).
- Run `make`.

## Running

Run zroot without any arguments for usage information. Try generating the z^3-1 set first by running `zroot --coefficients=1,0,0,-1 --output-filename=c.bmp`. Then try other values and other numbers of coefficients (up to 18 of them) for more complex images.

zroot can also generate videos by linearly interpolating equations' coefficients into other equations' coefficients over a number of images. [Here's an example](https://www.youtube.com/watch?v=x7NPltLwWM4) of transitioning from z^2-1 to z^3-1 to z^4-1, etc. (each transition takes ten seconds).

The above video took just over 6.5 hours to render in 8K resolution on a 2019 MacBook Pro using 12 threads. 8K is a ridiculously large resolution though, and zroot is much faster at smaller resolutions. The same video can be rendered at 1080p resolution in about 15 minutes, or at 720p in 6.5 minutes.