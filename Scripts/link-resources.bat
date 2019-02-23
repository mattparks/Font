@echo off
cd ../

IF EXIST cmake-build-debug\bin (
mkdir cmake-build-debug\bin\Resources
mklink /J cmake-build-debug\bin\Resources\Engine Libraries\Acid\Resources
mklink /J cmake-build-debug\bin\Resources\Game Resources
)

IF EXIST cmake-build-release\bin (
mkdir cmake-build-release\bin\Resources
mklink /J cmake-build-release\bin\Resources\Engine Libraries\Acid\Resources
mklink /J cmake-build-release\bin\Resources\Game Resources
)

IF EXIST Build\Debug\Bin (
mkdir Build\Debug\Bin\Resources
mklink /J Build\Debug\Bin\Resources\Engine Libraries\Acid\Resources
mklink /J Build\Debug\Bin\Resources\Game Resources
)

IF EXIST Build\Debug\Bin32 (
mkdir Build\Debug\Bin32\Resources
mklink /J Build\Debug\Bin32\Resources\Engine Libraries\Acid\Resources
mklink /J Build\Debug\Bin32\Resources\Game Resources
)

IF EXIST Build\Release\Bin (
mkdir Build\Release\Bin\Resources
mklink /J Build\Release\Bin\Resources\Engine Libraries\Acid\Resources
mklink /J Build\Release\Bin\Resources\Game Resources
)

IF EXIST Build\Release\Bin32 (
mkdir Build\Release\Bin32\Resources
mklink /J Build\Release\Bin32\Resources\Engine Libraries\Acid\Resources
mklink /J Build\Release\Bin32\Resources\Game Resources
)

IF EXIST Build\Tests\TestGUI\Resources\Engine (
mklink /J Build\Tests\TestGUI\Resources\Engine Libraries\Acid\Resources
mklink /J Build\Tests\TestGUI\Resources\Game Resources
)

IF EXIST Build\Tests\TestPBR\Resources\Engine (
mklink /J Build\Tests\TestPBR\Resources\Engine Libraries\Acid\Resources
mklink /J Build\Tests\TestPBR\Resources\Game Resources
)

IF EXIST Build\Tests\TestPhysics\Resources\Engine (
mklink /J Build\Tests\TestPhysics\Resources\Engine Libraries\Acid\Resources
mklink /J Build\Tests\TestPhysics\Resources\Game Resources
)

IF EXIST Build\Tests\TestVoxel\Resources\Engine (
mklink /J Build\Tests\TestVoxel\Resources\Engine Libraries\Acid\Resources
mklink /J Build\Tests\TestVoxel\Resources\Game Resources
)

pause
