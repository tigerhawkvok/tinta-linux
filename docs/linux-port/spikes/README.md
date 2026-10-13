# Spikes behind the measurements in ../PLAN.md section 1

These are throwaway programs, not part of the build.

```sh
cc -O2 startup.c -o startup $(pkg-config --cflags --libs gtk4)        # main() to first painted frame
cc -O2 init.c    -o init    $(pkg-config --cflags --libs gtk4)        # gtk_init cost alone
cc -O2 layout.c  -o layout  $(pkg-config --cflags --libs pangocairo)  # one wrapped layout per block
cc -O2 seg.c     -o seg     $(pkg-config --cflags --libs pangocairo)  # Tinta-style measure + per-line segments

xvfb-run -a env GDK_BACKEND=x11 GSK_RENDERER=cairo ./startup   # also: ngl, gl, vulkan
./seg ../../../CHANGELOG.md
```

The numbers in the plan were taken on 2026-10-13 under Xvfb with software GL
(Ubuntu 25.10, GTK 4.22.4, Pango 1.58.2). The runs used Xvfb so that no window
would open on the live desktop; timings on real hardware have not been taken
yet.
