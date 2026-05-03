# MikuMikuWorld

这是一个用于制作和预览某游戏谱面的编辑器。

本仓库是在原 MikuMikuWorld 项目基础上继续维护的版本。原项目仓库目前已经删除，因此这里保留并继续扩展编辑器功能。

## 主要改动

- 支持读取和导出谱面 Maker 的 JSON 谱面格式。
- 支持单个 note 的 `speedRatio` 数据编辑、保存和导出。
- 支持 macOS 构建，并生成可直接运行的 `.app`。
- 支持 Windows 自动构建，推送到仓库后可在 GitHub Actions 中下载构建产物。

## 功能

- 导入和导出 `sus` 谱面。
- 导入和导出谱面 Maker JSON 谱面。
- 编辑 BPM、拍号和全局 hi-speed。
- 编辑单个 note 的 `speedRatio`，长条只需要设置头部即可作用于整条长条。
- 自定义时间轴分拍，最高支持 1920 分拍。
- 创建和使用自定义 note 预设。
- 自定义快捷键。

## 运行要求

Windows：

- 64 位 Windows 10 或更高版本。
- Visual C++ Redistributable。
- 支持 OpenGL 3.3 的显卡和驱动。

macOS：

- Apple Silicon 或支持当前构建目标的 macOS 设备。
- 如需自行构建，需要安装 Xcode Command Line Tools。

## 构建

Windows：

使用 Visual Studio 打开 `MikuMikuWorld/MikuMikuWorld.vcxproj`，选择 `Release | x64` 构建。

也可以使用 GitHub Actions 自动构建。构建完成后，在 Actions 页面下载 `MikuMikuWorld-windows-x64` 产物。

macOS：

```sh
cmake -S . -B build-macos -DCMAKE_BUILD_TYPE=Release
cmake --build build-macos --config Release
```

构建完成后，应用位于：

```txt
build-macos/MikuMikuWorld.app
```

## 说明

`hi-speed` 和 `speedRatio` 是不同效果，不能混用。`hi-speed` 是谱面级速度变化，`speedRatio` 是 note 自身的速度倍率数据。
