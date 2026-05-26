"""
训练适配器包
"""
from .base import TrainingAdapter as TrainingAdapter
from .base import StopTrainingException as StopTrainingException
from .ultralytics_adapter import UltralyticsAdapter as UltralyticsAdapter
from .registry import TrainingAdapterRegistry as TrainingAdapterRegistry
from .registry import register_builtin_adapters as register_builtin_adapters
