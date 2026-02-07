import sys
if sys.prefix == '/home/nammy/micro_ros_ws/firmware/toolchain/espressif/python_env/idf4.1_py3.10_env':
    sys.real_prefix = sys.prefix
    sys.prefix = sys.exec_prefix = '/home/nammy/micro_ros_ws/firmware/mcu_ws/install'
