
### 2D Fluid Simulation (C++/OpenGL)

![demo](docs/assets/demo.gif) 
___
### Description
This is a small real-time 2D fluid simulation I made to practice C++, learn a bit about graphics programming and also practice profiling and optimizing code.

Currently the simulation runs entirely on the CPU and uses spatial hashing to keep the performance at approximately $O(kN)$ time, where $N$ is the number of particles in the simulation and $k$ is the average number of neighbouring particles to a particle, $i$ that lie within the defined smoothing kernel radius, as opposed to $O(N^2)$ time if spatial hashing was not used. I verified these timings through benchmarks, the results of whiched are plotted in `benchmarks/benchmark_plotting.ipynb`.

This project is still very much a work in progress as currently the simulation performs quite poorly after ~2000 particles. The plan is to squeeze out as much performance from the CPU model as possible, before moving to a GPU implementation.

___
### Features
|Key |	Action|
|--------|----------------|
| `SPACE`| Pause / Resume |
|` →	`| Step one frame |
| `R`    |Reset simulation|

___
### Structure
```
src/
 ├── main.cpp              # OpenGL app + render loop
 ├── particles.*           # SPH implementation
 ├── linear_algebra.*      # Vec3 / Mat4 math
 ├── triangle_mesh.*       # Rendered quad mesh
 ├── colors.h              # Particle color stops
 ├── shaders/
 │    ├── vertex.txt
 │    └── fragment.txt
 └── benchmark.cpp         # Google Benchmark tests
 ```