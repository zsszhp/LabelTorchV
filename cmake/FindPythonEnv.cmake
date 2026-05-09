# FindPythonEnv.cmake
# 查找 labeltorch conda 环境中的 Python 解释器

set(LABELTORCH_PYTHON_ENV "F:/A/anaconda/envs/labeltorch")

find_program(LABELTORCH_PYTHON_EXECUTABLE
    NAMES python python3
    PATHS "${LABELTORCH_PYTHON_ENV}" "${LABELTORCH_PYTHON_ENV}/Scripts"
    NO_DEFAULT_PATH
)

if(LABELTORCH_PYTHON_EXECUTABLE)
    message(STATUS "Found LabelTorch Python: ${LABELTORCH_PYTHON_EXECUTABLE}")
else()
    message(WARNING "LabelTorch Python environment not found at ${LABELTORCH_PYTHON_ENV}")
endif()

mark_as_advanced(LABELTORCH_PYTHON_EXECUTABLE)
