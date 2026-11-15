# Step 1: 3DGS renderer in OpenGL

Renders a 3D Gaussian Splatting model (a `.ply` file written by the official 3DGS code) with OpenGL 4.6. There are two renderers, which share the projection of the splats and a GPU radix sort:
- the **tile renderer**, written in compute shaders, which follows the 3DGS rasterizer;
- the **quad renderer**, which rasterizes one blended quad per splat and serves as a reference.

## Build and run

From this folder, with the same libraries as the other projects:
```
g++ -std=c++17 -O2 -isystem ../../cyCodeBase -isystem ../../lodepng main.cpp -o main.exe -L../../lodepng -llodepng -lfreeglut -lglew32 -lopengl32 -lglu32
main.exe [model.ply]
```
Without an argument the program loads `../models/garden.ply`. `main.cpp` includes the two headers, so it is the only file to compile.

## Controls

| Input | Action |
|---|---|
| Left drag, right drag | Orbit, zoom |
| W A S D, Q E | Move the orbit center forward, left, back, right, down, up |
| R | Reset the camera |
| Tab | Switch between the tile and the quad renderer |
| B | Bounding box of a splat: tight (where alpha is at least 1/255) or the 3 sigma square of 3DGS |
| V | View: image, splats per tile, difference between the two renderers |
| 0, 1, 2, 3 | Render at the window size, or at 720p, 1080p, 1440p scaled to the window |
| P | Print the timings (and, in the difference view, the largest difference) |
| T | Test the GPU prefix sum and sort against the CPU |
| F2, F6 | Save a screenshot, reload the shaders |

## Files

| File | Content |
|---|---|
| `main.cpp` | Window, camera, keys, text |
| `splat_loader.h` | Reads the `.ply`, converts the splats to the GPU layout, estimates the scene center and up direction |
| `renderer.h` | GPU buffers, shader programs, the passes of a frame, the self-test |
| `shaders/common.glsl` | Constants and buffers, included by every shader |

## A frame

A *pair* is a (key, value) entry of the sort: the value is a splat index, the key holds a tile index above a depth code. The tile renderer needs one pair for each tile a splat touches, the quad renderer one pair for each visible splat.

| Pass | Shader | Work |
|---|---|---|
| preprocess | `preprocess.comp` | Projects each splat (mean, conic, bounding box, color from SH) and counts its pairs |
| scan | `scan_reduce.comp`, `scan_down.comp`, `setup.comp` | Prefix sum of the counts: where the pairs of each splat go, and their total; the total sets the size of the later passes |
| duplicate | `duplicate.comp` | Writes the pairs |
| sort | `sort_histogram.comp`, `sort_scatter.comp` | Radix sort of the pairs by key: 4 passes of 8 bits |
| ranges | `tile_ranges.comp` | First and last pair of each tile (tile renderer only) |
| render | `tile_render.comp`, or `quad.vert` and `quad.frag` | Blends the splats: per tile, front to back, with early termination; or as quads, back to front |
| | `present.vert`, `present.frag` | Debug view, then copy to the window |

The CPU never sorts. It reads back only the timers and the pair count of the previous frame, for the text and to grow the sort buffers when the pairs did not fit.

The two renderers give the same image with tight bounds, up to rounding (1 or 2 levels of 255). A few pixels differ by up to 5 levels, because the tile renderer, like 3DGS, drops the splat that would bring the transmittance below 0.0001.

## References

### Gaussian splatting

- B. Kerbl, G. Kopanas, T. Leimkühler, G. Drettakis. *3D Gaussian Splatting for Real-Time Radiance Field Rendering.* ACM Transactions on Graphics 42(4), SIGGRAPH 2023. [Project page](https://repo-sam.inria.fr/fungraph/3d-gaussian-splatting/), [arXiv](https://arxiv.org/abs/2308.04079).
  The scene representation and the tile renderer: 16x16 tiles, keys with the tile above the depth, front-to-back blending, the alpha limits (1/255 and 0.99) and the early termination.
- Official 3DGS code: [gaussian-splatting](https://github.com/graphdeco-inria/gaussian-splatting) (the `.ply` format and the pretrained models) and its CUDA rasterizer [diff-gaussian-rasterization](https://github.com/graphdeco-inria/diff-gaussian-rasterization).
  Its `cuda_rasterizer/forward.cu` contains the details that the paper omits: the 1.3 clamp in the Jacobian, the 0.3 pixel dilation, the 3 sigma square, and the SH evaluation with the 0.5 offset.
- M. Zwicker, H. Pfister, J. van Baar, M. Gross. *EWA Volume Splatting.* IEEE Visualization 2001. [PDF](https://www.cs.umd.edu/~zwicker/publications/EWAVolumeSplatting-VIS01.pdf), [DOI](https://doi.org/10.1109/VISUAL.2001.964490).
  The projection of a 3D Gaussian to a 2D Gaussian, Σ' = J W Σ Wᵀ Jᵀ, where J is the Jacobian of the local affine approximation of the perspective projection. 3DGS cites this paper for it.
- M. Zwicker, H. Pfister, J. van Baar, M. Gross. *EWA Splatting.* IEEE Transactions on Visualization and Computer Graphics 8(3), 2002. [PDF](https://www.cs.umd.edu/~zwicker/publications/EWASplatting-TVCG02.pdf), [DOI](https://doi.org/10.1109/TVCG.2002.1021576).
  The journal version, for volume and surface data. It adds a screen-space low-pass filter to the projected Gaussian; the 0.3 pixel dilation of 3DGS is a similar filter that keeps the peak opacity unchanged.
- A. Hanson et al. *Speedy-Splat: Fast 3D Gaussian Splatting with Sparse Pixels and Sparse Primitives.* CVPR 2025. [Paper](https://openaccess.thecvf.com/content/CVPR2025/html/Hanson_Speedy-Splat_Fast_3D_Gaussian_Splatting_with_Sparse_Pixels_and_Sparse_CVPR_2025_paper.html), [arXiv](https://arxiv.org/abs/2412.00578).
  The tight bounds (B key) are its SnugBox: the bounding box of the ellipse where alpha reaches 1/255. They remove pairs without changing the image.
- J. T. Barron, B. Mildenhall, D. Verbin, P. P. Srinivasan, P. Hedman. *Mip-NeRF 360: Unbounded Anti-Aliased Neural Radiance Fields.* CVPR 2022. [Project page](https://jonbarron.info/mipnerf360/).
  The dataset that contains the garden scene.

### Spherical harmonics

- P.-P. Sloan. *Stupid Spherical Harmonics (SH) Tricks.* Game Developers Conference 2008. [PDF](https://www.ppsloan.org/publications/StupidSH36.pdf).
  Appendix A2 lists the real SH basis as polynomials in x, y, z. The constants and signs in `preprocess.comp` are those of bands 0 to 3, rewritten with x² + y² + z² = 1.

### GPU prefix sum and sort

- M. Harris, S. Sengupta, J. D. Owens. *Parallel Prefix Sum (Scan) with CUDA.* GPU Gems 3, Chapter 39, 2007. [Online](https://developer.nvidia.com/gpugems/gpugems3/part-vi-gpu-computing/chapter-39-parallel-prefix-sum-scan-cuda).
  The prefix sum of a large array in blocks, with the block sums scanned at the next level, and its use in a radix sort.
- N. Satish, M. Harris, M. Garland. *Designing Efficient Sorting Algorithms for Manycore GPUs.* IPDPS 2009. [DOI](https://doi.org/10.1109/IPDPS.2009.5161005).
  The structure of the radix sort: one digit histogram per block, stored digit-major, so that a single prefix sum gives each block the output position of each digit; then a scatter.
- [VkRadixSort](https://github.com/MircoWerner/VkRadixSort), a radix sort in Vulkan and GLSL.
  The scatter ranks each key with per-digit bit masks in shared memory and `bitCount`, as its `multi_radixsort.comp` does. There the digit offsets come from subgroup operations; here they come from the prefix sum.

### Libraries

- [cyCodeBase](https://github.com/cemyuksel/cyCodeBase) by Cem Yuksel (cyGL, cyMatrix, cyVector), FreeGLUT, GLEW and [LodePNG](https://github.com/lvandeve/lodepng).
