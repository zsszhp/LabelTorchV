#!/bin/bash
# Qt IFW 安装包构建脚本
# 支持 Windows/macOS/Linux 三平台

set -e

echo "=== LabelTorch Qt IFW Installer Build ==="

# 配置
INSTALLER_NAME="LabelTorch-Installer-v1.0.0"
BUILD_DIR="build-ifw"
OUTPUT_DIR="deploy/installers"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 检查 Qt IFW 是否安装
check_ifw() {
    log_info "检查 Qt IFW..."

    if command -v binarycreator &> /dev/null; then
        BINARYCREATOR=$(which binarycreator)
        log_info "找到 binarycreator: ${BINARYCREATOR}"
    else
        # 尝试常见路径
        if [ -f "/opt/Qt/Tools/QtInstallerFramework/*/bin/binarycreator" ]; then
            BINARYCREATOR=$(ls /opt/Qt/Tools/QtInstallerFramework/*/bin/binarycreator | head -n 1)
        elif [ -d "$HOME/Qt/Tools/QtInstallerFramework" ]; then
            BINARYCREATOR=$(ls $HOME/Qt/Tools/QtInstallerFramework/*/bin/binarycreator 2>/dev/null | head -n 1)
        else
            log_error "Qt IFW 未安装或未在 PATH 中"
            log_info "请安装 Qt Installer Framework: https://download.qt.io/official_releases/qt-installer-framework/"
            exit 1
        fi
    fi

    log_info "Qt IFW 版本: $(${BINARYCREATOR} --version)"
}

# 准备安装包数据
prepare_package() {
    local platform=$1
    log_info "准备 ${platform} 安装包数据..."

    local data_dir="installer/packages/com.labeltorch.main/data"

    # 清理旧数据
    rm -rf "${data_dir}"/*
    mkdir -p "${data_dir}"

    case ${platform} in
        windows)
            # 复制 Windows 构建产物
            if [ -d "out/build/msvc2022-debug" ]; then
                cp -R out/build/msvc2022-debug/labeltorch.exe "${data_dir}/" || log_warn "labeltorch.exe 未找到"
                cp -R out/build/msvc2022-debug/*.dll "${data_dir}/" 2>/dev/null || log_warn "DLL 文件未找到"
                cp -R out/build/msvc2022-debug/LabelTorch "${data_dir}/" 2>/dev/null || log_warn "QML 模块未找到"
            else
                log_warn "Windows 构建目录未找到，请先构建项目"
            fi

            # 复制后端
            mkdir -p "${data_dir}/backend"
            cp -R backend/labeltorch_backend "${data_dir}/backend/"
            cp backend/requirements.txt "${data_dir}/backend/"

            # 复制许可证
            cp LICENSE "${data_dir}/LICENSE.txt" 2>/dev/null || echo "MIT License - See source code" > "${data_dir}/LICENSE.txt"
            ;;

        macos)
            # 复制 macOS 构建产物
            if [ -d "build-macos/src/标炬.app" ]; then
                cp -R build-macos/src/标炬.app "${data_dir}/"
            else
                log_warn "macOS 构建目录未找到"
            fi

            # 复制后端
            mkdir -p "${data_dir}/标炬.app/Contents/MacOS/backend"
            cp -R backend/labeltorch_backend "${data_dir}/标炬.app/Contents/MacOS/backend/"
            cp backend/requirements.txt "${data_dir}/标炬.app/Contents/MacOS/backend/"

            cp LICENSE "${data_dir}/LICENSE.txt" 2>/dev/null || echo "MIT License" > "${data_dir}/LICENSE.txt"
            ;;

        linux)
            # 复制 Linux 构建产物
            if [ -d "build-linux/src/labeltorch" ]; then
                cp build-linux/src/labeltorch "${data_dir}/"
                chmod +x "${data_dir}/labeltorch"
            else
                log_warn "Linux 构建目录未找到"
            fi

            # 复制 QML 模块
            if [ -d "build-linux/LabelTorch" ]; then
                cp -R build-linux/LabelTorch "${data_dir}/"
            fi

            # 复制后端
            mkdir -p "${data_dir}/backend"
            cp -R backend/labeltorch_backend "${data_dir}/backend/"
            cp backend/requirements.txt "${data_dir}/backend/"

            # 复制启动脚本
            cp scripts/启动标炬.sh "${data_dir}/" 2>/dev/null || echo "#!/bin/bash" > "${data_dir}/启动标炬.sh"
            chmod +x "${data_dir}/启动标炬.sh"

            cp LICENSE "${data_dir}/LICENSE.txt" 2>/dev/null || echo "MIT License" > "${data_dir}/LICENSE.txt"
            ;;

        *)
            log_error "不支持的平台: ${platform}"
            exit 1
            ;;
    esac
}

# 构建安装包
build_installer() {
    local platform=$1
    log_info "构建 ${platform} 安装包..."

    local config_file="installer/config/config.xml"
    local packages_dir="installer/packages"
    local output_file="${OUTPUT_DIR}/${INSTALLER_NAME}-${platform}"

    # 创建输出目录
    mkdir -p "${OUTPUT_DIR}"

    # 根据平台设置输出文件名
    case ${platform} in
        windows)
            output_file="${output_file}.exe"
            ${BINARYCREATOR} -c "${config_file}" -p "${packages_dir}" "${output_file}" -v
            ;;
        macos)
            output_file="${output_file}.app"
            ${BINARYCREATOR} -c "${config_file}" -p "${packages_dir}" "${output_file}" -v
            ;;
        linux)
            output_file="${output_file}.run"
            ${BINARYCREATOR} -c "${config_file}" -p "${packages_dir}" "${output_file}" -v
            chmod +x "${output_file}"
            ;;
    esac

    log_info "安装包创建成功: ${output_file}"
}

# 主流程
main() {
    local platform=${1:-"all"}

    log_info "开始 LabelTorch Qt IFW 安装包构建"

    check_ifw

    if [ "${platform}" = "all" ]; then
        for plat in windows macos linux; do
            prepare_package ${plat}
            build_installer ${plat}
        done
    else
        prepare_package ${platform}
        build_installer ${platform}
    fi

    log_info "=== Qt IFW 安装包构建完成 ==="
    log_info "输出目录: ${OUTPUT_DIR}/"
}

# 执行
main "$@"
