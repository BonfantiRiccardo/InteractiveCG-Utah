# Steps 1 and 2: 3DGS renderer in OpenGL with meshes and lights

Renders a 3D Gaussian Splatting model (a `.ply` file written by the official 3DGS code) with OpenGL 4.6. There are two renderers, which share the projection of the splats and a GPU radix sort:
- the **tile renderer**, written in compute shaders, which follows the 3DGS rasterizer;
- the **quad renderer**, which rasterizes one blended quad per splat and serves as a reference.

Step 2 adds meshes (teapots) and point, spot and directional lights. The meshes are composited with the splats in every pixel, and the lights relight both the splats and the meshes. There are no shadows yet.

## Build and run

From this folder, with the same libraries as the other projects:
```
g++ -std=c++17 -O2 -isystem ../../cyCodeBase -isystem ../../lodepng main.cpp -o main.exe -L../../lodepng -llodepng -lfreeglut -lglew32 -lopengl32 -lglu32
main.exe [model.ply]
```
Without an argument the program loads `../models/garden.ply`. `main.cpp` includes the three headers, so it is the only file to compile.

## Controls

Without Ctrl the mouse moves the camera. With Ctrl it acts on the objects.

| Input | Action |
|---|---|
| Left drag, right drag or wheel | Orbit, zoom |
| W A S D, Q E | Move the orbit center forward, left, back, right, down, up |
| R | Reset the camera |
| Tab | Switch between the tile and the quad renderer |
| B | Bounding box of a splat: tight (where alpha is at least 1/255) or the 3 sigma square of 3DGS |
| V, Shift + V | Next or previous view: image, splats per tile, difference between the two renderers, normals, flatness, light without the captured colors, surface depth. The views of the splats per tile and of the surface depth always use the tile renderer |
| 0, 1, 2, 3 | Render at the window size, or at 720p, 1080p, 1440p scaled to the window |
| P | Print the timings (and, in the difference view, the largest difference) |
| T | Test the GPU prefix sum and sort against the CPU |
| F2, F6 | Save a screenshot, reload the shaders |

| Input | Action on the objects |
|---|---|
| 4, 5, 6, 7 | Add a teapot, a point light, a spot light, a directional light where the mouse points. The new object is selected |
| Ctrl + click | Select the object under the mouse; on the selected object, or on no object, deselect |
| Ctrl + left drag | Move the selected object horizontally |
| Ctrl + right drag | Rotate it around the up axis (horizontal motion) and around the camera's right axis (vertical motion) |
| Ctrl + wheel, Page Up, Page Down | Raise or lower it |
| +, - | Size of a mesh, intensity of a light |
| Z, X | Narrower or wider cone of a spot light |
| C | Next color: albedo of a mesh, tint of a light |
| Delete | Remove it |
| J, K | Lower or raise the ambient scale (the light of the capture). The first light sets it to 0.3 |
| O | Compare the splats with the meshes at their peak along each ray, or at their center |
| G | Hide or show the shapes of the lights |

A new teapot stands on the surface under the mouse, and a new light floats above it. This uses the surface depth of the tile renderer; with the quad renderer, the object appears at the distance of the orbit center.

## Files

| File | Content |
|---|---|
| `main.cpp` | Window, camera, mouse and keys, text |
| `splat_loader.h` | Reads the `.ply`, converts the splats to the GPU layout, estimates the scene center and up direction |
| `scene.h` | Objects (meshes and lights), their shapes, the light data for the shaders |
| `renderer.h` | GPU buffers, shader programs, cameras and render targets, the passes of a frame, the self-test |
| `shaders/common.glsl` | Constants, binding points and the storage buffers, included by every shader except `present.vert`. The driver allows 16 storage blocks per shader, so the buffers come in groups, and a shader defines the groups it uses (`USE_RECORDS`, ...) before the include |
| `shaders/lighting.glsl` | Lights of the scene and their falloff, included by `preprocess.comp` and `mesh.frag` |

## A frame

A *pair* is a (key, value) entry of the sort: the value is a splat index, the key holds a tile index above a depth code. The tile renderer needs one pair for each tile a splat touches, the quad renderer one pair for each visible splat.

| Pass | Shader | Work |
|---|---|---|
| meshes | `mesh.vert`, `mesh.frag` | Draws the objects into three images: shaded color, view depth and object id |
| preprocess | `preprocess.comp` | Projects each splat (mean, conic, bounding box, color from SH), relights it and counts its pairs |
| scan | `scan_reduce.comp`, `scan_down.comp`, `setup.comp` | Prefix sum of the counts: where the pairs of each splat go, and their total; the total sets the size of the later passes |
| duplicate | `duplicate.comp` | Writes the pairs |
| sort | `sort_histogram.comp`, `sort_scatter.comp` | Radix sort of the pairs by key: 4 passes of 8 bits |
| ranges | `tile_ranges.comp` | First and last pair of each tile (tile renderer only) |
| render | `tile_render.comp`, or `quad.vert` and `quad.frag` | Blends the splats over the mesh image: per tile, front to back, with early termination; or as quads, back to front. Splats behind a mesh are skipped |
| | `present.vert`, `present.frag` | Debug view and outline of the selected object, then copy to the window |

The CPU never sorts. It reads back only the timers and the pair count of the previous frame, for the text and to grow the sort buffers when the pairs did not fit. A click or a new object also reads one pixel: the object id, or the surface depth.

The two renderers give the same image with tight bounds, up to rounding (1 or 2 levels of 255). A few pixels differ by up to 5 levels, because the tile renderer, like 3DGS, drops the splat that would bring the transmittance below 0.0001. Without objects and with an ambient scale of 1, the image is identical to that of step 1.

## Meshes and lights

**Compositing.** The mesh pass stores the view depth of the meshes in every pixel. A splat is blended in a pixel only if it is in front of the mesh there. Its depth is either that of its center or, by default, the depth at which it peaks along the ray of the pixel. With the EWA approximation that depth changes linearly across the splat: `depth + slope . (pixel - mean)`, where the slope comes from the covariance between the pixel position and the depth. The peak counts as at most 5% of the depth away from the center (`PEAK_RANGE`). This changes fewer than 0.1% of the visible splats, and it lets a pixel stop behind a mesh as soon as a splat's center lies 5% beyond it, because the list is sorted by depth. The tile renderer also writes the surface depth, the depth where the transmittance falls below 0.5, which places new objects.

**Other views.** A `RenderTarget` holds the images and tile ranges of one view, and a `Camera` can be perspective or orthographic. The screen is one target. Steps 3 and 4 will render from the lights into their own targets, with `computeColor` off when only the footprints and depths of the splats are needed.

**Relighting.** The captured color `c` of a splat is taken as albedo times the light of the sky. The ambient scale `a` dims that light, and each light adds its own:

`c' = srgb(lin(c) (a + sum over the lights of k t L max(0, n . l)))`

- `k` and `t` are the intensity and the tint of the light. An intensity of 1 gives as much light as the sky of the capture.
- `L` is the falloff: 1 for a directional light, and `(reach / d)^2` for a point or spot light at distance `d`. The reach is the height at which the light was created. A spot light also fades toward the edge of its cone.
- `n` is the shortest axis of the Gaussian, turned toward the camera, and `l` is the direction toward the light.
- `lin` and `srgb` convert between the display colors of the capture and linear light.

The meshes use Blinn-Phong shading with the same lights, and a sky term scaled by `a` that is brighter from above.

**Limits.** The normals of the splats are noisy where the Gaussians are not flat (grass, bushes); the normals and flatness views show this. Baked shading of the capture stays in the colors. The meshes are not antialiased.

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
- Y. Jiang et al. *GaussianShader: 3D Gaussian Splatting with Shading Functions for Reflective Surfaces.* CVPR 2024. [arXiv](https://arxiv.org/abs/2311.17977).
  The shortest axis of a Gaussian as its approximate normal.
- L. Radl et al. *StopThePop: Sorted Gaussian Splatting for View-Consistent Real-time Rendering.* SIGGRAPH 2024. [Project page](https://r4dl.github.io/StopThePop).
  The depth of a splat in a pixel as the point of its highest contribution along the ray. The slope used here is the linear version of that depth under the EWA approximation.

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
