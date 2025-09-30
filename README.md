### Installation / Usage

[Instructions here](https://github.com/uilchtchuirn/DR2-SmoothCockpitCam/blob/master/Installation.md)

### Credits

Injectable Game Camera System by Frans Bouma 
<br>ghostinthecamera for game-specific stuff ([suppprt him](https://ko-fi.com/M4M0VZFCD))

### Requirements to build the code
To build the code, you need to have VC++ 2015 update 3 or higher, newer cameras need VC++ 2017. 
Additionally you need to have installed the Windows SDK, at least the windows 8 version. The VC++ installer should install this. 
The SDK is needed for DirectXMath.h

### External dependencies
There's an external dependency on [MinHook](https://github.com/TsudaKageyu/minhook) through a git submodule. This should be downloaded
automatically when you clone the repo. The camera uses DirectXMath for the 3D math, which is a self-contained .h file, from the Windows SDK. 

### Acknowledgements
Some camera code uses [MinHook](https://github.com/TsudaKageyu/minhook) by Tsuda Kageyu.