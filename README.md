# Skipper

Windows 桌面程序。选定一个目录后，把该目录第一层的视频当作同一组素材，为每一集另存一份去掉片头和片尾的新文件。原片保持不变，导出使用流复制，不重新编码。

片头和片尾靠这一组视频互相参照来定位：反复出现的音频、文件里的章节名，以及片尾常见的黑场或纯色底。也可以指定其中一集，并给出它的片头、片尾时间，把这段声音当作标准。

## 使用

1. 选择目录。程序只读取该目录第一层，扩展名不区分大小写：`mp4`、`mkv`、`avi`、`mov`、`webm`、`ts`、`m2ts`、`wmv`、`flv`。
2. 可选：指定其中一集作为标准，并填写片头区间、片尾区间，或两者都填。时间可以写成 `90`、`90.5`、`1:30`、`1:30.25`、`01:02:03`、`01:02:03.25`。
3. 开始分析。确认页列出每一集的片头、片尾和来源（标准、章节、音频、黑帧，或黑帧与音频的合并）。可以改时间、清空某一头，或排除某一集。
4. 开始导出。新文件写到所选目录下的 `output/`，文件名在扩展名前插入 `_trimmed`。例如 `ep01.mkv` 写成 `output/ep01_trimmed.mkv`。目标文件已存在时跳过，不覆盖。

分析可以取消。导出取消时保留已经写完的新文件，正在写的那一集会删掉临时文件。

运行时先在程序所在目录查找 `ffmpeg.exe` 和 `ffprobe.exe`，找不到再查 PATH。音频指纹优先使用 FFmpeg 的 chromaprint 复用器，否则使用 `fpcalc.exe`。两者都没有、而这次又要用到音频时，音频比对跳过，其余已勾选的手段继续进行，确认页会说明这一点。

## 环境

| 依赖 | 本机路径 | 说明 |
| --- | --- | --- |
| CMake 3.21 及以上 | `D:\Install\CMake` | 生成器使用 MinGW Makefiles |
| Qt 6 Widgets | `C:\Install\Qt\6.5.3\mingw_64` | 本仓库按 6.5.3 编过 |
| MinGW g++（GCC 11.2） | `C:\Install\Qt\Tools\mingw1120_64` | 必须和上面这套 Qt 是同一套 MinGW |
| FFmpeg | `third_party/ffmpeg-2026-11-09-git-16a345b022-full_build/bin` | gyan.dev full 构建，含 chromaprint。安装包只带 `ffmpeg.exe`、`ffprobe.exe` |

Qt 6.5.3 的 `mingw_64` 库按 GCC 11.2 构建。编译器换成其他 GCC 后无法和这套 Qt 链接。CMake、`g++` 和 Qt 的 `bin` 都要在配置与编译时的 PATH 里。

`third_party/` 已放好 nlohmann/json 3.11.3 和 doctest 2.4.11。配置时不需要联网。

## 编译

在仓库根目录打开 PowerShell：

```powershell
$env:PATH = "C:\Install\Qt\Tools\mingw1120_64\bin;C:\Install\Qt\6.5.3\mingw_64\bin;D:\Install\CMake\bin;" + $env:PATH

cmake -G "MinGW Makefiles" `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH="C:/Install/Qt/6.5.3/mingw_64" `
  -DCMAKE_C_COMPILER="C:/Install/Qt/Tools/mingw1120_64/bin/gcc.exe" `
  -DCMAKE_CXX_COMPILER="C:/Install/Qt/Tools/mingw1120_64/bin/g++.exe" `
  -B build -S .

cmake --build build -j 8
```

产物是 `build\skipper.exe`。它依赖 Qt 和 MinGW 运行库。部署到 `build\` 旁边：

```powershell
& "C:\Install\Qt\6.5.3\mingw_64\bin\windeployqt.exe" --compiler-runtime --no-translations "build\skipper.exe"
```

部署完成后，从资源管理器直接打开 `build\skipper.exe` 即可。编译时会把上述 full 构建的 `ffmpeg.exe` 和 `ffprobe.exe` 复制到 `build\`。

改过源码后，保持同一个 PATH，再执行一次 `cmake --build build -j 8`。

## 测试

领域规则有独立测试，不启动 FFmpeg：

```powershell
.\build\skipper_domain_tests.exe
```

也可以在 `build` 目录执行 `ctest`。测试目标名是 `domain`。

## 目录

```
src/domain           片头片尾的判定，不访问界面、文件和进程
src/application      分析与导出的编排
src/infrastructure   枚举、探测、指纹、黑帧和流复制
src/ui               Qt 界面
tests/domain         领域测试
third_party          nlohmann/json 与 doctest
docs                 需求、设计与界面稿
```

依赖方向是界面 → 应用层 → 领域。基础设施实现应用层的端口。
