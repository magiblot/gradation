# Gradation Curves

<div align="center">
<img src="https://user-images.githubusercontent.com/20713561/152564351-a3e2e9c5-8b24-42b6-9034-e348f62292a2.png"/>
</div></br>

An updated version of Alexander Nagiller's Gradation Curves filter for VirtualDub. Additional documentation can be found in the filter's [original homepage](https://neosol.at/vdub/index.html).

This version adds 64-bit support and a cross-platform AviSynth+ plugin.

# Usage

## VirtualDub

See the [original Readme](https://neosol.at/vdub/readme.html).

## AviSynth

AviSynth+ 3.7.1 or newer required.

**Gradation(clip *clip*, string *pmode*, string *drawmode* [, array *points*] [, string *file*, string *ftype*])**

* *clip* **clip** = *(required)*

    Input clip. It must be RGB32.

* *string* **pmode** = *(required)*

    Processing mode. It must be one of:

    * `"rgb"`: RGB only. It processes the image by the common RGB channel. Any edited color channel will be ignored.
    * `"full"`: RGB + R/G/B. It processes the individual color settings first and then applies the RGB settings.
    * `"rgbw"`, `"fullw"`: Weighted modes. They differ from the non-weighted ones in the way RGB is processed. First a Y (gray) value of each pixel is calculated. Then the output value of the RGB curve of this Y (RGB input) value is taken and applied to all three components of the pixel the same way.
    * `"yuv"`, `"cmyk"`, `"hsv"`, `"lab"`: It processes each channel individually.

* *string* **drawmode** = `"spline"`

    Determines how the filter curves are drawn. This parameter is only meaningful when **points** is provided or when the curves file pointed to by **file** does not imply a draw mode itself (as is the case of `.acv` files). It must be one of:

    * `"linear"`: Straight lines will be drawn between the points.
    * `"spline"`: A spline interpolation will be applied for the given coordinates.
    * `"gamma"`: This mode can be used to apply a gamma correction.

* *array* **points** = *Undefined()*

    If provided, it must be an array of lists of points structured in the following way:

    ```c
    points=[
    \   [[0, 0], [3, 10], [255, 255]],  /* points for channel 1 */
    \   [],                             /* points for channel 2 */
    \   [[0, 20], [240, 255]]           /* points for channel 3 */
    \   ]
    ```

    If no list of points is provided for some channel, an empty list is assumed. An empty list of points is treated like `[[0, 0], [255, 255]]`.

    The channels for each processing mode are:

    * `"rgb"`, `"full"`, `"rgbw"`, `"fullw"`: RGB, R, G, B
    * `"yuv"`: Y, U, V
    * `"cmyk"`: C, M, Y, K
    * `"hsv"`: H, S, V
    * `"lab"`: L, A, B

* *string* **file** = *Undefined()*

    If provided, it must be a path to a curves file.

    Either **points** or **file** must be provided.
    
* *string* **ftype** = `"auto"`

    Used along **file**. It specifies the curves file format, It must be one of:

    * `"auto"`: Autodetect according to file extension. Supported formats are `.amp`, `.acv`, `.csv`, `.crv`, `.map`.
    * `"SmartCurve HSV"`. Must be specified manually sice it also uses the `.amp` extension.


# Build

## CMake

CMake can be used to build both the VirtualDub and AviSynth+ filters. The VirtualDub filter is only available on Windows.

On Unix-like systems:

```sh
cmake . -B ./build -DCMAKE_BUILD_TYPE=Release
cmake --build ./build
# Binary:
# ./build/libgradation-avs.so
```

On Windows:

```sh
cmake . -B ./build # Add '-A x64' (64-bit) or '-A Win32' (32-bit) to override the default architecture.
cmake --build ./build --config Release
# Binaries:
# ./build/Release/gradation-avs.dll
# ./build/Release/gradation.vdf
```

## Visual C++ 6.0

The VirtualDub filter can still be built with ancient versions of Visual Studio by loading the `gradation.dsp` file into the IDE.
