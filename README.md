# ch347_run_st7789

使用 CH347 驱动 ST7789 显示屏的 C 语言项目。

## 项目简介

本项目搭建了一套基于 clangd 的最小 C 语言开发环境，目标是通过 CH347 USB 转接芯片驱动 ST7789 TFT 显示屏。

## 依赖

| 工具 | 版本要求 |
|------|----------|
| clang | >= 14 |
| clangd | >= 14 |
| cmake | >= 3.15 |

## 目录结构

```
.
├── CMakeLists.txt          # CMake 构建配置
├── Makefile                # 快捷构建脚本
├── .clangd                 # clangd 编译选项
├── compile_commands.json   # clangd 索引（软链接到 build/）
└── src/
    ├── config.h            # 项目配置宏
    └── main.c              # 入口文件
```

## 构建

```bash
# 使用 cmake（推荐）
mkdir -p build
cmake -S . -B build -DCMAKE_C_COMPILER=clang
cmake --build build

# 使用 make
make

# 编译并运行
make run
```

## clangd 配置

cmake 配置时会在 `build/` 下自动生成 `compile_commands.json`，
根目录的软链接让 clangd 能直接找到它：

```bash
compile_commands.json -> build/compile_commands.json
```

每次修改 `CMakeLists.txt` 后重新运行 `cmake -S . -B build` 即可更新。

## License

MIT
