COLMAP Build Instructions
=========================

These notes describe one way to install COLMAP from source for use with this
project. The steps were tested with Ubuntu 22.04 and Qt6.

1. **Install system packages**
   ```bash
   sudo apt update
   sudo apt install git cmake build-essential libboost-all-dev \
        libglew-dev qtbase5-dev libqt5opengl5-dev libcgal-dev \
        libeigen3-dev libsuitesparse-dev libfreeimage-dev
   ```

2. **Clone COLMAP**
   ```bash
   git clone https://github.com/colmap/colmap.git
   cd colmap
   ```

3. **Configure and build**
   ```bash
   mkdir build && cd build
   cmake .. -DCMAKE_BUILD_TYPE=Release
   make -j$(nproc)
   sudo make install
   ```

   The `colmap` executable will be installed to `/usr/local/bin` by default.

4. **Verify installation**
   ```bash
   colmap --help
   ```

If you wish to embed COLMAP as a C++ library, enable `COLMAP_BUILD_CUDA` and
`COLMAP_BUILD_GUI` options in CMake if your system provides CUDA and Qt.
Refer to COLMAP documentation for additional details.

