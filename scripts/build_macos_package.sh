#!/bin/bash
# LabelTorch macOS 打包脚本
# 创建 macOS 应用程序包（.app）和 DMG 安装包

set -e

echo "=== LabelTorch macOS Package Build ==="

# 配置
PACKAGE_NAME="LabelTorch-macOS-v1.0.0"
BUILD_DIR="build-macos"
DEPLOY_DIR="deploy/${PACKAGE_NAME}"
APP_NAME="标炬.app"

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
        log_error "cmake 未安装，请先运行: brew install cmake"
        exit 1
    fi

    if ! command -v ninja &> /dev/null; then
        log_warn "ninja 未安装，使用 make 替代"
        USE_NINJA=false
    else
        USE_NINJA=true
    fi

    if ! command -v macdeployqt &> /dev/null; then
        log_error "macdeployqt 未找到，请确保 Qt 6.11+ 已正确安装并在 PATH 中"
        exit 1
    fi

    log_info "依赖检查通过"
}

# 构建项目
build_project() {
    log_info "开始构建 macOS 版本..."

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
            -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
    else
        cmake .. \
            -G "Unix Makefiles" \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
    fi

    # 编译
    if [ "$USE_NINJA" = true ]; then
        ninja
    else
        make -j$(sysctl -n hw.ncpu)
    fi

    cd ..
    log_info "构建完成"
}

# 部署应用程序
deploy_app() {
    log_info "部署应用程序包..."

    # 清理旧部署
    if [ -d "${DEPLOY_DIR}" ]; then
        rm -rf "${DEPLOY_DIR}"
    fi
    mkdir -p "${DEPLOY_DIR}"

    # 复制应用程序包
    if [ -d "${BUILD_DIR}/src/${APP_NAME}" ]; then
        cp -R "${BUILD_DIR}/src/${APP_NAME}" "${DEPLOY_DIR}/"
    else
        log_error "应用程序包未找到: ${BUILD_DIR}/src/${APP_NAME}"
        exit 1
    fi

    # 使用 macdeployqt 打包依赖
    macdeployqt "${DEPLOY_DIR}/${APP_NAME}" -verbose=2 -dmg

    # 打包 Python 后端
    log_info "打包 Python 后端..."
    mkdir -p "${DEPLOY_DIR}/${APP_NAME}/Contents/MacOS/backend"

    # 复制后端文件
    cp -R backend/labeltorch_backend "${DEPLOY_DIR}/${APP_NAME}/Contents/MacOS/backend/"
    cp backend/requirements.txt "${DEPLOY_DIR}/${APP_NAME}/Contents/MacOS/backend/"

    # 创建 Python 虚拟环境（可选，如果用户系统有 Python 3.11+）
    cat > "${DEPLOY_DIR}/${APP_NAME}/Contents/MacOS/backend/setup_backend.sh" << 'EOF'
#!/bin/bash
echo "设置 Python 后端环境..."
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
echo "后端环境设置完成"
EOF
    chmod +x "${DEPLOY_DIR}/${APP_NAME}/Contents/MacOS/backend/setup_backend.sh"

    log_info "应用程序包部署完成"
}

# 创建启动脚本
create_launcher() {
    log_info "创建启动脚本..."

    cat > "${DEPLOY_DIR}/启动标炬.command" << EOF
#!/bin/bash
cd "\$(dirname "\$0")"
open "${APP_NAME}"
EOF
    chmod +x "${DEPLOY_DIR}/启动标炬.command"

    # 创建 README
    cat > "${DEPLOY_DIR}/README.md" << EOF
# 标炬 (LabelTorch) v1.0.0 - macOS 版

工业缺陷检测桌面软件 - 标注、训练、推理、导出一体化平台

## 快速开始

### 方式一：双击启动
双击 \`标炬.app\` 即可启动

### 方式二：命令行启动
运行 \`启动标炬.command\`

## 首次使用

1. 打开终端
2. 运行后端环境设置：
   \`\`\`bash
   cd "标炬.app/Contents/MacOS/backend"
   bash setup_backend.sh
   \`\`\`

3. 启动标炬应用程序

## 系统要求

- macOS 12.0 (Monterey) 或更高版本
- Python 3.11+（用于训练后端）
- 4GB+ RAM
- 10GB+ 可用磁盘空间

## 功能特性

- 项目管理：创建、打开、切换项目
- 数据导入：YOLO txt 格式数据集导入
- 标注引擎：HBB/OBB 高性能标注
- 训练工作台：YOLO 多模型家族训练
- 模型管理：版本追踪、指标对比
- 辅助标注：推理候选框审核
- 主动学习：低置信样本回流
- 模型导出：pt/onnx 格式导出

## 技术支持

- 项目主页：https://github.com/zzsszhp/LabelTorchV
- 问题反馈：https://github.com/zzsszhp/LabelTorchV/issues

## 许可证

MIT License
EOF

    log_info "启动脚本创建完成"
}

# 创建 DMG 安装包
create_dmg() {
    log_info "创建 DMG 安装包..."

    DMG_NAME="${PACKAGE_NAME}.dmg"
    VOL_NAME="LabelTorch Installer"

    # 清理旧 DMG
    rm -f "deploy/${DMG_NAME}"

    # 创建临时 DMG
    hdiutil create -srcfolder "${DEPLOY_DIR}" \
        -volname "${VOL_NAME}" \
        -fs HFS+ \
        -fsargs "-c c=64,a=16,e=16" \
        -format UDRW \
        "deploy/${DMG_NAME}.tmp.dmg"

    # 挂载并创建符号链接
    MOUNT_POINT=$(hdiutil attach -readwrite -noverify -noautoopen "deploy/${DMG_NAME}.tmp.dmg" | \
        grep -E "^/dev/" | sed 1q | awk '{print $1}')

    # 创建 Applications 文件夹快捷方式
    ln -s /Applications "/Volumes/${VOL_NAME}/Applications"

    # 设置背景（可选）
    # cp scripts/dmg_background.png "/Volumes/${VOL_NAME}/.background.png"

    # 卸载
    hdiutil detach "${MOUNT_POINT}"

    # 压缩为最终 DMG
    hdiutil convert "deploy/${DMG_NAME}.tmp.dmg" \
        -format UDZO \
        -imagekey zlib-level=9 \
        -o "deploy/${DMG_NAME}"

    # 清理临时文件
    rm -f "deploy/${DMG_NAME}.tmp.dmg"

    log_info "DMG 安装包创建完成: deploy/${DMG_NAME}"
}

# 创建 7z 绿色压缩包
create_7z() {
    log_info "创建 7z 绿色压缩包..."

    if command -v 7z &> /dev/null; then
        cd deploy
        7z a "${PACKAGE_NAME}.7z" "${PACKAGE_NAME}/"
        cd ..
        log_info "7z 压缩包创建完成: deploy/${PACKAGE_NAME}.7z"
    elif command -v tar &> /dev/null; then
        cd deploy
        tar -czf "${PACKAGE_NAME}.tar.gz" "${PACKAGE_NAME}/"
        cd ..
        log_info "tar.gz 压缩包创建完成: deploy/${PACKAGE_NAME}.tar.gz"
    else
        log_warn "7z 和 tar 都未找到，跳过压缩包创建"
    fi
}

# 主流程
main() {
    log_info "开始 LabelTorch macOS 打包流程"

    check_dependencies
    build_project
    deploy_app
    create_launcher
    create_dmg
    create_7z

    log_info "=== macOS 打包完成 ==="
    log_info "输出目录: deploy/"
    log_info "  - ${PACKAGE_NAME}/ (应用程序包)"
    log_info "  - ${PACKAGE_NAME}.dmg (安装包)"
    log_info "  - ${PACKAGE_NAME}.7z 或 ${PACKAGE_NAME}.tar.gz (压缩包)"
}

# 执行
main "$@"
