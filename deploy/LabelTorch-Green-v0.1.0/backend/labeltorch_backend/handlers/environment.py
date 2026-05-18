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
        "python": platform.python_version(),
        "python_executable": sys.executable,
        "platform": platform.platform(),
    }

    try:
        torch = importlib.import_module("torch")
        result["torch"] = torch.__version__
        result["cuda_available"] = torch.cuda.is_available()
        if torch.cuda.is_available():
            result["torch_cuda"] = torch.version.cuda or "N/A"
            result["gpu_name"] = torch.cuda.get_device_name(0)
            result["gpu_count"] = torch.cuda.device_count()
            result["gpu_memory_total_mb"] = torch.cuda.get_device_properties(0).total_mem // (1024 * 1024)
        else:
            result["torch_cuda"] = "N/A"
            result["gpu_name"] = "N/A"
            result["gpu_count"] = 0
            result["gpu_memory_total_mb"] = 0
    except ImportError:
        result["torch"] = None
        result["cuda_available"] = False
        result["torch_cuda"] = "N/A"
        result["gpu_name"] = "N/A"
        result["gpu_count"] = 0
        result["gpu_memory_total_mb"] = 0

    try:
        ultralytics = importlib.import_module("ultralytics")
        result["ultralytics"] = ultralytics.__version__
    except ImportError:
        result["ultralytics"] = None

    try:
        onnxruntime = importlib.import_module("onnxruntime")
        result["onnxruntime"] = onnxruntime.__version__
        providers = onnxruntime.get_available_providers()
        result["onnxruntime_providers"] = providers
    except ImportError:
        result["onnxruntime"] = None
        result["onnxruntime_providers"] = []

    try:
        onnx = importlib.import_module("onnx")
        result["onnx_version"] = onnx.__version__
    except ImportError:
        result["onnx_version"] = None

    return result
