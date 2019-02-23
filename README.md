# Acid Font
A application that demos Vulkan TTF font rendering using bezier curves in [Acid](https://github.com/Equilibrium-Games/Acid). This project is C++17 adaptation of https://github.com/kocsis1david/font-demo.

## Screenshots
<img src="https://raw.githubusercontent.com/mattparks/Acid-Font/master/Documents/image1.png" alt="Image1" width="600px">

# Compiling
Once cloned run `git submodule update --init --recursive` in the directory to update the submodules. All platforms depend on [CMake](https://cmake.org/download) to generate IDE/make files.

[Vulkan SDK](https://www.lunarg.com/vulkan-sdk/) is required to develop and run Acid.

Make sure you have the environment variable `VULKAN_SDK` set to the paths you have Vulkan installed into.

On Linux ensure `xorg-dev`, and `libvulkan1` are installed. Read about how to setup [Vulkan on Linux](https://vulkan.lunarg.com/doc/sdk/latest/linux/getting_started.html) so a Vulkan SDK is found.

Setup on MacOS is similar to the setup on Linux, a compiler that supports C++17 is required, such as XCode 10.0.
