# EtherKit（RZ/N2L）RT-Thread BSP + OneNET MQTT 示例

**中文** | [**English**](./README.md)

## 这是什么

本仓库包含瑞萨 **RZ/N2L EtherKit** 开发板的 RT-Thread BSP，并提供一个连接 **OneNET** 的 MQTT 示例应用（基于 `kawaii-mqtt`）。

你可以用它：

- 在 EtherKit 上跑 RT-Thread
- 使用以太网与 MQTT
- 按 OneNET 物模型 Topic 上报/接收属性

## 硬件信息

- MPU：R9A07G084M04GBG，最高 400MHz，Arm Cortex®-R52
- 调试：板载 J-Link

![EtherKit Board](figures/big.png)

## 快速开始

本 BSP 支持 GCC/IAR 工作流，按你现有工具链选择即可。

### 使用 IAR 编译下载

1. 生成 IAR 工程：

   - 在工程根目录打开 ENV
   - 执行：`scons --target=iar`

2. 使用 IAR 打开 `project.eww`，然后执行 **Download and Debug**。

### 使用 GCC 编译

使用 GCC 时，请通过环境变量 `RTT_EXEC_PATH` 配置工具链路径（指向包含 `arm-none-eabi-gcc` 的目录）。

### 串口输出

- 115200-8-1-N
- 在 msh 中输入 `help` 查看命令

## 应用入口

应用入口函数为 `src/hal_entry.c` 内的 `void hal_entry(void)`。

## OneNET 配置（重要：Token 不会提交到 Git）

`src/onenet_config.h` 只保存非敏感配置（host / 产品 ID / 设备名）。

OneNET Token 属于敏感信息，**禁止提交到 GitHub**。请按如下方式本地配置：

1. 将 `src/onenet_secrets_example.h` 复制为 `src/onenet_secrets.h`
2. 使用 `scripts/onenet_token.py` 生成 Token
3. 将 Token 填入 `src/onenet_secrets.h`

`src/onenet_secrets.h` 已在 `.gitignore` 中忽略。

## 目录说明

- `src/`：应用与示例代码
- `board/`：板级与端口配置
- `rzn_cfg/`：FSP 配置头文件
- `rzn_gen/`：生成的 HAL 代码
- `scripts/`：辅助脚本

## 公开备份说明

本仓库会忽略本机/IDE 元数据与本地密钥文件（详见 `.gitignore`），以便安全地公开备份到 GitHub。
