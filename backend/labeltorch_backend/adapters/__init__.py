"""
训练适配器包
"""
from .base import TrainingAdapter
from .ultralytics_adapter import UltralyticsAdapter
from .registry import TrainingAdapterRegistry, register_builtin_adapters
