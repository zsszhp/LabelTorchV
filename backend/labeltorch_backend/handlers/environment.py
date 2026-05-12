"""
environment.check 处理器
"""
import sys
import platform
import importlib
import logging

logger = logging.getLogger(__name__)


async def handle_check(payload: dict) -> dict:
    """
    检测运行环境：Python版本、PyTorch、CUDA、Ultralytics等
    """
    result = {
        "python_version": platform.python_version(),
        "python_executable": sys.executable,
        "platform": platform.platform(),
    }

    try:
        torch = importlib.import_module("torch")
        result["torch_version"] = torch.__version__
        result["cuda_available"] = torch.cuda.is_available()
        if torch.cuda.is_available():
            result["cuda_version"] = torch.version.cuda or "N/A"
            result["gpu_name"] = torch.cuda.get_device_name(0)
            result["gpu_count"] = torch.cuda.device_count()
            result["gpu_memory_total_mb"] = torch.cuda.get_device_properties(0).total_mem // (1024 * 1024)
        else:
            result["cuda_version"] = None
            result["gpu_name"] = None
            result["gpu_count"] = 0
            result["gpu_memory_total_mb"] = 0
    except ImportError:
        result["torch_version"] = None
        result["cuda_available"] = False
        result["cuda_version"] = None
        result["gpu_name"] = None
        result["gpu_count"] = 0
        result["gpu_memory_total_mb"] = 0

    try:
        ultralytics = importlib.import_module("ultralytics")
        result["ultralytics_version"] = ultralytics.__version__
    except ImportError:
        result["ultralytics_version"] = None

    try:
        onnxruntime = importlib.import_module("onnxruntime")
        result["onnxruntime_version"] = onnxruntime.__version__
        providers = onnxruntime.get_available_providers()
        result["onnxruntime_providers"] = providers
    except ImportError:
        result["onnxruntime_version"] = None
        result["onnxruntime_providers"] = []

    try:
        onnx = importlib.import_module("onnx")
        result["onnx_version"] = onnx.__version__
    except ImportError:
        result["onnx_version"] = None

    return result
