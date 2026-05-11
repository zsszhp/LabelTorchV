import sys
import os

# 自动将 backend/ 目录添加到 sys.path，使测试可直接 import labeltorch_backend
backend_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if backend_dir not in sys.path:
    sys.path.insert(0, backend_dir)
