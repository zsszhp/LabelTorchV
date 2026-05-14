#!/bin/bash
# LabelTorch Linux 打包脚本
# 创建 Linux AppImage 和 tar.gz 绿色包

set -e

echo "=== LabelTorch Linux Package Build ==="

# 配置
PACKAGE_NAME="LabelTorch-Linux-v1.0.0"
BUILD_DIR="build-linux"
DEPLOY_DIR="deploy/${PACKAGE_NAME}"
APPDIR="${DEPLOY_DIR}/AppDir"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 检查依赖
check_dependencies() {
    log_info "检查构建依赖..."

    if ! command -v cmake &> /dev/null; then
        log_error "cmake 未安装，请先运行: sudo apt install cmake 或 sudo yum install cmake"
        exit 1
    fi

    if ! command -v ninja-build &> /dev/null && ! command -v ninja &> /dev/null; then
        log_warn "ninja 未安装，使用 make 替代"
        USE_NINJA=false
    else
        USE_NINJA=true
    fi

    if ! command -v linuxdeployqt &> /dev/null && ! command -v linuxdeploy &> /dev/null; then
        log_warn "linuxdeployqt/linuxdeploy 未安装，将使用手动部署方式"
        USE_LINUXDEPLOY=false
    else
        USE_LINUXDEPLOY=true
    fi

    log_info "依赖检查通过"
}

# 构建项目
build_project() {
    log_info "开始构建 Linux 版本..."

    # 清理旧构建
    if [ -d "${BUILD_DIR}" ]; then
        rm -rf "${BUILD_DIR}"
    fi
    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"

    # 配置
    if [ "$USE_NINJA" = true ]; then
        cmake -G Ninja .. \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_INSTALL_PREFIX=/usr
    else
        cmake .. \
            -G "Unix Makefiles" \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_INSTALL_PREFIX=/usr
    fi

    # 编译
    if [ "$USE_NINJA" = true ]; then
        ninja
    else
        make -j$(nproc)
    fi

    cd ..
    log_info "构建完成"
}

# 部署应用程序
deploy_app() {
    log_info "部署应用程序..."

    # 清理旧部署
    if [ -d "${DEPLOY_DIR}" ]; then
        rm -rf "${DEPLOY_DIR}"
    fi
    mkdir -p "${DEPLOY_DIR}"
    mkdir -p "${APPDIR}"

    # 复制可执行文件
    if [ -f "${BUILD_DIR}/src/labeltorch" ]; then
        cp "${BUILD_DIR}/src/labeltorch" "${APPDIR}/"
        chmod +x "${APPDIR}/labeltorch"
    else
        log_error "可执行文件未找到: ${BUILD_DIR}/src/labeltorch"
        exit 1
    fi

    # 复制 QML 和资源文件
    if [ -d "${BUILD_DIR}/LabelTorch" ]; then
        cp -R "${BUILD_DIR}/LabelTorch" "${APPDIR}/"
    fi

    # 使用 linuxdeploy 打包依赖（如果可用）
    if [ "$USE_LINUXDEPLOY" = true ]; then
        log_info "使用 linuxdeploy 打包依赖..."

        # 创建桌面文件
        cat > "${APPDIR}/labeltorch.desktop" << EOF
[Desktop Entry]
Type=Application
Name=标炬
Comment=工业缺陷检测桌面软件
Exec=labeltorch
Icon=labeltorch
Categories=Development;
Terminal=false
EOF

        # 创建图标（占位符）
        cat > "${APPDIR}/labeltorch.svg" << 'EOF'
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <rect width="64" height="64" fill="#2196F3"/>
  <text x="32" y="40" font-size="28" fill="white" text-anchor="middle" font-family="Arial">LT</text>
</svg>
EOF

        # 运行 linuxdeploy
        if command -v linuxdeploy &> /dev/null; then
            linuxdeploy --appdir "${APPDIR}" \
                --output appimage \
                --desktop-file "${APPDIR}/labeltorch.desktop" \
                --icon-file "${APPDIR}/labeltorch.svg"
        elif command -v linuxdeployqt &> /dev/null; then
            export QMAKE=$(which qmake6 || which qmake)
            linuxdeployqt --appdir="${APPDIR}" \
                --bundle-non-qt-libs
        fi
    else
        log_warn "手动部署模式，可能需要手动处理依赖"

        # 创建基础目录结构
        mkdir -p "${APPDIR}/usr/bin"
        mkdir -p "${APPDIR}/usr/lib"
        mkdir -p "${APPDIR}/usr/share/applications"
        mkdir -p "${APPDIR}/usr/share/icons/hicolor/64x64/apps"

        # 复制可执行文件
        cp "${APPDIR}/labeltorch" "${APPDIR}/usr/bin/"

        # 创建桌面文件
        cat > "${APPDIR}/usr/share/applications/labeltorch.desktop" << EOF
[Desktop Entry]
Type=Application
Name=标炬 (LabelTorch)
Comment=工业缺陷检测桌面软件 - 标注、训练、推理、导出一体化平台
Exec=usr/bin/labeltorch
Icon=labeltorch
Categories=Development;Graphics;
Terminal=false
Keywords=defect;detection;annotation;training;YOLO;
EOF

        # 创建图标
        mkdir -p "${APPDIR}/usr/share/icons/hicolor/64x64/apps"
        cat > "${APPDIR}/usr/share/icons/hicolor/64x64/apps/labeltorch.svg" << 'EOF'
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <rect width="64" height="64" fill="#2196F3"/>
  <text x="32" y="40" font-size="28" fill="white" text-anchor="middle" font-family="Arial">LT</text>
</svg>
EOF
    fi

    # 打包 Python 后端
    log_info "打包 Python 后端..."
    mkdir -p "${APPDIR}/usr/share/labeltorch/backend"

    # 复制后端文件
    cp -R backend/labeltorch_backend "${APPDIR}/usr/share/labeltorch/backend/"
    cp backend/requirements.txt "${APPDIR}/usr/share/labeltorch/backend/"

    # 创建后端安装脚本
    cat > "${APPDIR}/usr/share/labeltorch/backend/setup_backend.sh" << 'EOF'
#!/bin/bash
echo "设置 Python 后端环境..."

# 检查 Python 版本
if ! command -v python3 &> /dev/null; then
    echo "错误: Python3 未安装"
    exit 1
fi

PYTHON_VERSION=$(python3 -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")')
if (( $(echo "$PYTHON_VERSION < 3.11" | bc -l) )); then
    echo "错误: 需要 Python 3.11+，当前版本: $PYTHON_VERSION"
    exit 1
fi

# 创建虚拟环境
python3 -m venv venv
source venv/bin/activate

# 安装依赖
pip install --upgrade pip
pip install -r requirements.txt

echo "后端环境设置完成"
EOF
    chmod +x "${APPDIR}/usr/share/labeltorch/backend/setup_backend.sh"

    log_info "应用程序部署完成"
}

# 创建启动脚本
create_launcher() {
    log_info "创建启动脚本..."

    # 创建主启动脚本
    cat > "${DEPLOY_DIR}/启动标炬.sh" << 'EOF'
#!/bin/bash
cd "$(dirname "$0")"

# 检查后端环境
if [ ! -f "AppDir/usr/share/labeltorch/backend/venv/bin/activate" ]; then
    echo "首次使用，请先设置 Python 后端环境..."
    cd AppDir/usr/share/labeltorch/backend
    bash setup_backend.sh
    cd -
fi

# 启动应用程序
echo "启动标炬..."
./AppDir/AppRun
EOF
    chmod +x "${DEPLOY_DIR}/启动标炬.sh"

    # 创建 README
    cat > "${DEPLOY_DIR}/README.md" << EOF
# 标炬 (LabelTorch) v1.0.0 - Linux 版

工业缺陷检测桌面软件 - 标注、训练、推理、导出一体化平台

## 快速开始

### 方式一：使用启动脚本（推荐）
\`\`\`bash
chmod +x 启动标炬.sh
./启动标炬.sh
\`\`\`

### 方式二：直接运行 AppImage（如果已生成）
\`\`\`bash
chmod +x LabelTorch-x86_64.AppImage
./LabelTorch-x86_64.AppImage
\`\`\`

### 方式三：手动启动
\`\`\`bash
cd AppDir
./AppRun
\`\`\`

## 首次使用

1. 运行启动脚本会自动检测并设置 Python 后端环境
2. 如果需要手动设置：
   \`\`\`bash
   cd AppDir/usr/share/labeltorch/backend
   bash setup_backend.sh
   \`\`\`

## 系统要求

- Linux 发行版：Ubuntu 20.04+ / Debian 11+ / Fedora 35+ / Arch Linux 等
- Python 3.11+（用于训练后端）
- Qt 6.11+ 运行时库（已打包）
- 4GB+ RAM
- 10GB+ 可用磁盘空间
- 可选：NVIDIA GPU + CUDA 12.1（用于 GPU 加速训练）

## 功能特性

- 项目管理：创建、打开、切换项目
- 数据导入：YOLO txt 格式数据集导入
- 标注引擎：HBB/OBB 高性能标注
- 训练工作台：YOLO 多模型家族支持（v5/v8/v10/v11）
- 模型管理：版本追踪、指标对比、谱系链
- 辅助标注：推理候选框审核、批量确认
- 主动学习：低置信样本回流、难例优先审核
- 模型导出：pt/onnx 格式导出
- 数据治理：快照管理、类别映射、标签修订追踪

## 安装到系统（可选）

\`\`\`bash
# 复制到 /opt
sudo cp -r ${PACKAGE_NAME} /opt/labeltorch

# 创建符号链接
sudo ln -s /opt/labeltorch/AppDir/usr/bin/labeltorch /usr/local/bin/labeltorch

# 复制桌面文件
sudo cp AppDir/usr/share/applications/labeltorch.desktop /usr/share/applications/

# 复制图标
sudo cp AppDir/usr/share/icons/hicolor/64x64/apps/labeltorch.svg /usr/share/icons/hicolor/64x64/apps/
\`\`\`

## 故障排除

### 问题：启动后闪退
- 确保 Python 3.11+ 已安装
- 检查后端环境是否正确设置
- 查看日志：\`./AppDir/AppRun --log-level debug\`

### 问题：训练时找不到 GPU
- 确保已安装 CUDA 12.1 和对应驱动
- 在训练设置中选择正确的设备

### 问题：缺少 Qt 库
- 安装系统 Qt 包：\`sudo apt install qt6-base-dev qt6-declarative-dev\`

## 技术支持

- 项目主页：https://github.com/zzsszhp/LabelTorchV
- 问题反馈：https://github.com/zzsszhp/LabelTorchV/issues
- 开发文档：.monkeycode/docs/

## 许可证

MIT License

## 贡献

欢迎提交 Issue 和 Pull Request！
详见 CONTRIBUTING.md
EOF

    log_info "启动脚本创建完成"
}

# 创建 AppImage
create_appimage() {
    log_info "创建 AppImage..."

    APPIMAGE_NAME="LabelTorch-x86_64.AppImage"

    # 下载 linuxdeploy 插件（如果需要）
    if [ ! -f "linuxdeploy-x86_64.AppImage" ] && [ "$USE_LINUXDEPLOY" = false ]; then
        log_info "下载 linuxdeploy..."
        wget -q "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage" \
            -O linuxdeploy-x86_64.AppImage || {
            log_warn "下载 linuxdeploy 失败，使用手动方式"
            return
        }
        chmod +x linuxdeploy-x86_64.AppImage
    fi

    # 使用 linuxdeploy 创建 AppImage
    if [ -f "linuxdeploy-x86_64.AppImage" ]; then
        export OUTPUT="${DEPLOY_DIR}/${APPIMAGE_NAME}"
        ./linuxdeploy-x86_64.AppImage \
            --appdir "${APPDIR}" \
            --output appimage \
            --desktop-file "${APPDIR}/usr/share/applications/labeltorch.desktop" \
            --icon-file "${APPDIR}/usr/share/icons/hicolor/64x64/apps/labeltorch.svg" || {
            log_warn "linuxdeploy 创建 AppImage 失败"
        }
    fi

    # 如果 AppImage 创建失败，创建手动 AppRun
    if [ ! -f "${DEPLOY_DIR}/${APPIMAGE_NAME}" ]; then
        log_info "创建手动 AppRun 脚本..."

        cat > "${APPDIR}/AppRun" << 'EOF'
#!/bin/bash
SELF=$(readlink -f "$0")
HERE="${SELF%/*}"

# 设置库路径
export LD_LIBRARY_PATH="${HERE}/usr/lib:${LD_LIBRARY_PATH}"

# 设置 QML 路径
export QML2_IMPORT_PATH="${HERE}/usr/qml:${QML2_IMPORT_PATH}"

# 设置 Qt 插件路径
export QT_PLUGIN_PATH="${HERE}/usr/plugins:${QT_PLUGIN_PATH}"

# 执行主程序
exec "${HERE}/usr/bin/labeltorch" "$@"
EOF
        chmod +x "${APPDIR}/AppRun"

        # 创建简易 AppImage（自解压包）
        cat > "${DEPLOY_DIR}/mkappimage.sh" << EOF
#!/bin/bash
# 创建简易 AppImage
ARCHIVE=LabelTorch-x86_64.AppImage

# 创建启动脚本
cat > run.sh << 'INNER_EOF'
#!/bin/bash
TMPDIR=\$(mktemp -d)
tar -xzf "\$0.tar.gz" -C "\$TMPDIR"
\$TMPDIR/AppDir/AppRun
rm -rf "\$TMPDIR"
INNER_EOF
        chmod +x run.sh

        # 打包
        tar -czf "\${ARCHIVE}.tar.gz" AppDir/
        cat run.sh "\${ARCHIVE}.tar.gz" > "\${ARCHIVE}"
        chmod +x "\${ARCHIVE}"
        rm -f run.sh "\${ARCHIVE}.tar.gz"

        echo "AppImage 创建完成: \${ARCHIVE}"
EOF
        chmod +x "${DEPLOY_DIR}/mkappimage.sh"
    fi

    log_info "AppImage 处理完成"
}

# 创建 tar.gz 压缩包
create_tarball() {
    log_info "创建 tar.gz 压缩包..."

    cd deploy
    tar -czf "${PACKAGE_NAME}.tar.gz" "${PACKAGE_NAME}/"
    cd ..

    log_info "tar.gz 压缩包创建完成: deploy/${PACKAGE_NAME}.tar.gz"
}

# 创建 deb 包（Debian/Ubuntu）
create_deb() {
    log_info "创建 deb 包..."

    DEB_DIR="deploy/labeltorch-deb"
    DEB_NAME="labeltorch_1.0.0_amd64.deb"

    # 清理
    rm -rf "${DEB_DIR}"
    mkdir -p "${DEB_DIR}/DEBIAN"
    mkdir -p "${DEB_DIR}/opt/labeltorch"

    # 复制文件
    cp -R "${APPDIR}/usr" "${DEB_DIR}/opt/labeltorch/"

    # 创建控制文件
    cat > "${DEB_DIR}/DEBIAN/control" << EOF
Package: labeltorch
Version: 1.0.0
Section: devel
Priority: optional
Architecture: amd64
Depends: python3 (>= 3.11), libqt6core6, libqt6quick6
Maintainer: LabelTorch Team
Description: 工业缺陷检测桌面软件
 LabelTorch 是一款专业的工业缺陷检测桌面软件，支持标注、训练、推理、导出一体化工作流。
EOF

    # 创建 postinst 脚本
    cat > "${DEB_DIR}/DEBIAN/postinst" << 'EOF'
#!/bin/bash
# 设置后端环境
cd /opt/labeltorch/usr/share/labeltorch/backend
if [ -f "setup_backend.sh" ]; then
    bash setup_backend.sh
fi

# 更新桌面数据库
update-desktop-database /usr/share/applications || true
EOF
    chmod +x "${DEB_DIR}/DEBIAN/postinst"

    # 构建 deb 包
    dpkg-deb --build "${DEB_DIR}" "deploy/${DEB_NAME}"

    log_info "deb 包创建完成: deploy/${DEB_NAME}"
}

# 主流程
main() {
    log_info "开始 LabelTorch Linux 打包流程"

    check_dependencies
    build_project
    deploy_app
    create_launcher
    create_appimage
    create_tarball

    # 如果支持 dpkg，创建 deb 包
    if command -v dpkg-deb &> /dev/null; then
        create_deb
    fi

    log_info "=== Linux 打包完成 ==="
    log_info "输出目录: deploy/"
    log_info "  - ${PACKAGE_NAME}/ (应用程序目录)"
    log_info "  - ${PACKAGE_NAME}.tar.gz (压缩包)"
    if [ -f "deploy/LabelTorch-x86_64.AppImage" ]; then
        log_info "  - LabelTorch-x86_64.AppImage (AppImage)"
    fi
    if [ -f "deploy/labeltorch_1.0.0_amd64.deb" ]; then
        log_info "  - labeltorch_1.0.0_amd64.deb (Debian 包)"
    fi
}

# 执行
main "$@"
