# Third-party prebuilts

Checked-in third-party libraries live under:

```text
Source/ThirdParty/Prebuilt/<platform-triplet>/<configuration>/lib/
```

Example:

```text
Source/ThirdParty/Prebuilt/windows-msvc-x64/Debug/lib/RMem.lib
Source/ThirdParty/Prebuilt/windows-msvc-x64/Release/lib/RMem.lib
```

By default CMake links these binaries and does not build third-party source.
Use `-DHORIZON_REBUILD_THIRDPARTY=ON` to rebuild from source, then build the
`HorizonPackageThirdParty` target to copy the generated libraries back into this
folder.
