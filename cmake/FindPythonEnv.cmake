# FindPythonEnv.cmake
# 查找标炬 Python 后端所依赖的 Python 解释器环境
#
# 路径解析优先级（支持环境变量 / 缓存变量覆盖，不写死盘符）：
#   1. 缓存变量 LABELTORCH_PYTHON_ENV（cmake -DLABELTORCH_PYTHON_ENV=...）
#   2. 环境变量 LABELTORCH_PYTHON_ENV
#   3. 环境变量 PYTHON_ENV
#   4. 本机开发默认路径（仅作兜底，可被上述任一方式覆盖）

# 缓存变量入口：允许通过 -D 覆盖
set(LABELTORCH_PYTHON_ENV "" CACHE PATH "标炬 Python 环境根目录（含 python.exe）")

if(NOT LABELTORCH_PYTHON_ENV)
    if(DEFINED ENV{LABELTORCH_PYTHON_ENV} AND NOT "$ENV{LABELTORCH_PYTHON_ENV}" STREQUAL "")
        file(TO_CMAKE_PATH "$ENV{LABELTORCH_PYTHON_ENV}" _labeltorch_py_env)
    elseif(DEFINED ENV{PYTHON_ENV} AND NOT "$ENV{PYTHON_ENV}" STREQUAL "")
        file(TO_CMAKE_PATH "$ENV{PYTHON_ENV}" _labeltorch_py_env)
    else()
        # 开发机兜底路径；打包机 / 他人机器请通过上述变量覆盖
        set(_labeltorch_py_env "C:/A/anaconda/envs/labeltorch")
    endif()
    set(LABELTORCH_PYTHON_ENV "${_labeltorch_py_env}" CACHE PATH "标炬 Python 环境根目录（含 python.exe）" FORCE)
    unset(_labeltorch_py_env)
endif()

find_program(LABELTORCH_PYTHON_EXECUTABLE
    NAMES python python3
    PATHS "${LABELTORCH_PYTHON_ENV}" "${LABELTORCH_PYTHON_ENV}/Scripts"
    NO_DEFAULT_PATH
)

if(LABELTORCH_PYTHON_EXECUTABLE)
    message(STATUS "Found LabelTorch Python: ${LABELTORCH_PYTHON_EXECUTABLE}")
else()
    message(WARNING "LabelTorch Python environment not found at ${LABELTORCH_PYTHON_ENV}"
        " （可通过 -DLABELTORCH_PYTHON_ENV= 或环境变量 LABELTORCH_PYTHON_ENV 覆盖）")
endif()

mark_as_advanced(LABELTORCH_PYTHON_EXECUTABLE)
