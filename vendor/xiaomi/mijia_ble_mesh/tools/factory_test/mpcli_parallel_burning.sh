#!/bin/bash
# 将mpcli命令行工具解压到脚本根目录，并将重命名为1、2、3...
# 更改烧录串口号和烧录文件名，脚本烧录结果会输出到对应的日志中

echo "烧录任务开始。" >> flash.log
# 获取脚本的完整路径
SCRIPT_PATH=$(readlink -f "$0")

# 获取脚本所在的文件夹目录
SCRIPT_DIR=$(dirname "$SCRIPT_PATH")

# 输出脚本所在的文件夹目录
echo "脚本所在的文件夹目录是: $SCRIPT_DIR"

MPCLI="$SCRIPT_DIR/mpcli.exe" 
# 创建或清空日志文件

# 定义一个包含所有COM口的列表
COM_PORTS=("COM4" "COM5" "COM6" "COM8" "COM9" "COM10" "COM11" "COM12" "COM14" "COM15") ## 16个COM口
IMG_FILE="$SCRIPT_DIR/1217_ImgPacketFile-f72519dd4562069d9d87cc987558e89c.bin"  # 请替换为你的IMG文件路径

# 并行执行16个进程
counter=1
for COM in "${COM_PORTS[@]}"; do
	{
		LOG_FILE="flash$counter.log"
		> "$LOG_FILE"
		echo "开始烧录 COM口: $COM" >> "$LOG_FILE"
		echo "开始烧录 COM口: $COM"
		"$SCRIPT_DIR/$counter/mpcli.exe" -P "$IMG_FILE" -c "$COM" -r >> "$LOG_FILE" 2>&1
		if [ $? -eq 0 ]; then
			echo "烧录完成: $COM" >> "$LOG_FILE"
		else
			echo "烧录失败: $COM" >> "$LOG_FILE"
		fi
	}&
	((counter++))
done

# 等待所有后台进程完成
wait

echo "所有烧录任务完成。"
echo "所有烧录任务完成。" >> flash.log
