"""
environment.check 命令处理器
"""
import torch
import ultralytics


async def handle_check(payload: dict) -> dict:
    """检查Python后端环境状态"""
    cuda_available = torch.cuda.is_available()
    cuda_version = torch.version.cuda if cuda_available else None

    devices = ["cpu"]
    if cuda_available:
        for i in range(torch.cuda.device_count()):
            devices.append(f"cuda:{i}")

    return {
        "python_version": torch.__version__.split("+")[0],
        "torch_version": torch.__version__,
        "ultralytics_version": ultralytics.__version__,
        "cuda_available": cuda_available,
        "cuda_version": cuda_version,
        "devices": devices,
    }
