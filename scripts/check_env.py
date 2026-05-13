"""
LabelTorch Environment Self-Check

Run this on first launch to verify the runtime environment is complete.
"""
import sys
import os
import json
import platform

def check_environment():
    """Check if the runtime environment is complete."""
    result = {
        "python": platform.python_version(),
        "torch": None,
        "torch_cuda": None,
        "cuda_available": False,
        "gpu_name": "N/A",
        "gpu_count": 0,
        "ultralytics": None,
        "onnxruntime": None,
        "platform": platform.platform(),
        "errors": []
    }

    if sys.version_info < (3, 11):
        result["errors"].append(f"Python 3.11+ required, found {platform.python_version()}")

    try:
        import torch
        result["torch"] = torch.__version__
        result["cuda_available"] = torch.cuda.is_available()
        result["torch_cuda"] = torch.version.cuda or "N/A"
        if torch.cuda.is_available():
            result["gpu_name"] = torch.cuda.get_device_name(0)
            result["gpu_count"] = torch.cuda.device_count()
    except ImportError:
        result["errors"].append("PyTorch not installed")

    try:
        import ultralytics
        result["ultralytics"] = ultralytics.__version__
    except ImportError:
        result["errors"].append("ultralytics not installed")
        
    try:
        import onnxruntime
        result["onnxruntime"] = onnxruntime.__version__
    except ImportError:
        pass # Optional but good to have

    print(json.dumps(result, indent=4))

if __name__ == "__main__":
    check_environment()
