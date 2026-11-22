# LightWave 3D plug-in examples

Working code from two manuals:

- *C and C++ Plug-in Development for LightWave 3D*
- *Python Scripting and Plugins for LightWave 3D*

Every example builds for LightWave 2025 on Windows (`.p`) and macOS (universal `.plugin`), and CI builds and tests each one on every push.

## Examples

| Folder | Server name | Class | Manual section | Try it in LightWave |
| --- | --- | --- | --- | --- |
| `c/hello` | `LWEx_HelloC` | Layout Generic | C/C++ 12 (Appendix) | Utilities > Additional > Hello C shows a message |
| `c/lift` | `LWEx_Lift` | Item Motion + XPanel | C/C++ 3, 5 | Add to a null as a motion modifier; it rises 1 m; edit Lift in its panel |
| `c/lift_vparm` | `LWEx_LiftVParm` | Item Motion with a VParm | C/C++ 3 | As Lift, then click **E** beside Lift and keyframe it |
| `c/lift_cpp` | `LWEx_LiftCpp` | Item Motion in C++17 | C/C++ 3 | Same behaviour as Lift |
| `c/checker_node` | `LWEx_Checker` | Node | C/C++ 6 | Node Editor > Add Node > LWEx > Textures > Checker; wire Color into a material |
| `c/invert_filter` | `LWEx_Invert` | Image Filter | C/C++ 4 | Add under Processing > Image Filters; F9 renders inverted |
| `c/lift_tool` | `LWEx_LiftTool` | Modeler MeshEditTool | C/C++ 4 | In Modeler, select points, run Lift Points and drag left and right |
| `c/mesh_centroid` | `LWEx_MeshCentroid` | Layout Generic reading a mesh | C/C++ 4 | Select an object, run Mesh Centroid; reports point count, centre and a UV |
| `c/imgui_gain` | `LWEx_GainImGui` | Image Filter with a Dear ImGui window (Windows only) | C/C++ 5 | Add under Image Filters, double-click it: an ImGui window opens; drag or click Gain to type, F9 renders brighter |
| `python/my_bob` | `MY_Bob` | Python Item Motion | Python 7, 11 | Add `plugin/my_bob.py` with Add Plugins; apply Bob to a null |

Server names use an `LWEx_` prefix so they cannot clash with your own plug-ins.

## Building

Dear ImGui 1.91.5 (MIT, `third_party/imgui/LICENSE.txt`) is vendored for the ImGui example.

The LightWave 2025 SDK headers are in `third_party/lwsdk2025.0/include`, unmodified, so no LightWave install is needed to build. They belong to LightWave Digital and are covered by the LightWave EULA. To use your own SDK copy, pass `-DLWSDK=<sdk root>`.

**Windows** (Developer PowerShell for Visual Studio 2022):

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release
```

Plug-ins land in `build\Release\*.p`.

**macOS** (Xcode command-line tools):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build
ctest --test-dir build
codesign --force --sign - build/*.plugin
```

A downloaded bundle is quarantined by macOS; clear it with `xattr -dr com.apple.quarantine <name>.plugin`.

**Python tests** (no LightWave needed; uses a fake `lwsdk`):

```bat
cd python\my_bob
"<LW>\support\modules\MSWIN\x86_64\Python-3.13.0\python.exe" -B -m unittest discover -s tests -v
"<LW>\support\modules\MSWIN\x86_64\Python-2.7.18\python.exe" -c "compile(open('plugin/my_bob.py').read(), 'my_bob.py', 'exec')"
```

CI runs the 3.13 tests; run the 2.7 parse check locally, since current CI images have no Python 2.7.

## Status

| Check | Windows | macOS |
| --- | --- | --- |
| Builds without warnings | Yes | CI |
| Exports `_mod_descrip` | Yes (`dumpbin`) | CI (`nm`) |
| Unit tests | Pass | CI |
| Tried inside LightWave | Not yet | Not yet |

Please report an example that misbehaves in LightWave as an issue, with the LightWave version and what you saw.
