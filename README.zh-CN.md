# M5Stack Cardputer SSH

[English](README.md)

这是运行在 **M5Stack Cardputer ADV** 上的通用便携式 SSH 客户端。只要目标设备可以通过
网络访问并支持 SSH 用户名和密码认证，就能直接从 Cardputer 连接；不需要在服务器上安装
配套服务或 Python 程序，也不需要修改固件中的 `secrets.h`。

兼容目标包括 Raspberry Pi OS、Debian、Ubuntu、Fedora、Arch 等 Linux 系统、Windows
OpenSSH Server、macOS Remote Login、NAS、OpenWrt 路由器、虚拟机和云服务器。树莓派和
Ubuntu 都只是可连接的设备或系统示例，并非程序限制。

<table>
  <tr>
    <td width="50%"><img src="docs/images/terminal.jpg" alt="M5Stack Cardputer ADV 正在运行 SSH 终端"></td>
    <td width="50%"><img src="docs/images/settings.jpg" alt="M5Stack Cardputer ADV 的 SSH 设置菜单"></td>
  </tr>
  <tr>
    <td align="center">SSH 终端</td>
    <td align="center">设备端设置</td>
  </tr>
</table>

> 这是面向 M5Stack 硬件的社区项目，与 M5Stack 官方不存在隶属或背书关系。

## 主要功能

- 完全在设备上管理最多 4 个 Wi-Fi 配置和 8 个 SSH 主机
- 扫描 2.4 GHz Wi-Fi 的 1–13 信道，并支持手动添加隐藏网络
- 通过 `_ssh._tcp` mDNS 发现通用 SSH 服务，也可手动填写主机名、IP 和端口
- 认证前显示并验证服务器的 SHA-256 主机密钥指纹
- 使用设备自己生成、永不导出的 Ed25519 密钥登录，或使用保存的/每次输入的密码
- 通过 WireGuard VPN 访问内网主机；只有对端 AllowedIPs 范围内的流量走隧道
- 可选：从电脑通过 USB 配置 Wi-Fi、主机和 VPN
- 开机只进入主机列表，不会自动连接某个 SSH 主机
- 提供绿色 REMOTE 终端和橙色 LOCAL 只读回滚模式
- 支持 Cardputer 的 Aa、Fn、Ctrl、Alt、Opt、方向键、F1–F12、Tab、Escape、
  Backspace 和向前删除
- 在终端功能范围内支持 `vim`、`nano`、`top`、`htop` 和 `tmux`
- 诊断页不会显示密码和配置内容

内置终端是固定 40×13 PTY 的有界 `xterm-256color` 模拟器，支持主屏幕与备用屏幕、
120 行回滚、256 色、UTF-8 解码以及常用 Unicode 箭头、边框、块字符、盲文和 Latin-1。

## 普通用户下载与烧录

最方便的方式是打开最新的
[GitHub Release](https://github.com/MangoX0567/M5Stack-Cardputer-SSH/releases/latest)。

### 第一次安装

1. 下载 `M5Stack-Cardputer-SSH-full.bin`。
2. 安装 [Python](https://www.python.org/) 和 esptool：

   ```bash
   python -m pip install --upgrade esptool
   ```

3. 使用 USB-C 数据线连接 Cardputer ADV，确认串口后，将完整固件写入 `0x0`：

   ```bash
   python -m esptool --chip esp32s3 --port COM4 --baud 460800 write-flash 0x0 M5Stack-Cardputer-SSH-full.bin
   ```

   请把 `COM4` 换成实际串口，例如 Windows 上的 `COM7` 或 Linux 上的
   `/dev/ttyACM0`。

完整固件适合第一次安装。它会写入完整布局，并清除 NVS 中原来保存的 Wi-Fi 和 SSH
配置。烧录过程中不要断开 USB 或电源。

### 保留配置进行更新

从 Release 下载 `M5Stack-Cardputer-SSH-firmware.bin`，将应用固件写入 `0x10000`：

```bash
python -m esptool --chip esp32s3 --port COM4 --baud 460800 write-flash 0x10000 M5Stack-Cardputer-SSH-firmware.bin
```

这种更新方式保留现有分区表和 NVS 配置，只适合已经安装过本项目固件的设备。Release 中的
`SHA256SUMS.txt` 可用于校验下载文件。

## 第一次使用

首次启动时，在 Cardputer 上添加一个 2.4 GHz Wi-Fi。连接成功后会进入 `SSH HOSTS`。
你可以发现 `_ssh._tcp` 服务，也可以手动添加任意服务器的名称、主机名/IP、端口和用户名。
设备首次启动时还会生成自己的 SSH 密钥和 WireGuard 密钥，详见
[设备密钥与 WireGuard VPN](#设备密钥与-wireguard-vpn)。

首次连接会显示服务器的 SHA-256 主机密钥指纹。请先和服务器上的指纹核对，再选择信任。
只有确认信任后，固件才会使用任何凭据。以后如果主机密钥变化，连接会直接停止，直到你
确认服务器确实更换或重装，并在 Settings 中手动重置信任。

Settings 可以添加、编辑、启用、禁用、删除、调整优先级或重新排列 Wi-Fi 与主机配置。
每个主机可以把密码保存在 ESP32 NVS 中，也可以设置为每次连接时询问。

## 设备密钥与 WireGuard VPN

### 设备密钥

首次启动时，Cardputer 会生成一个 Ed25519 密钥并保存在 NVS 中，只有公钥会离开设备。
要用它登录，请把公钥加入服务器的 `~/.ssh/authorized_keys`。**Settings > Device key**
可以在屏幕上显示公钥，或通过 USB 串口输出。

连接时会先尝试设备密钥。只有服务器拒绝该密钥、且同时允许密码登录时，才会使用保存的
或当场输入的密码。因此只接受密钥的服务器可以直接使用，原有的密码主机也不受影响。
添加只接受密钥的主机时，请选择 **Device key, else prompt**。

在服务器上可以把该密钥限制为只允许来自设备的 VPN 地址，例如
`from="10.0.0.2" ssh-ed25519 AAAA... cardputer-1a2b3c`。在 Settings 中重新生成密钥会
替换旧密钥，需要把新公钥重新加到服务器上。

### WireGuard VPN

客户端可以在任意 2.4 GHz Wi-Fi 下，通过 WireGuard 访问内网主机（例如 VPN 中心节点后面的服务器）。

1. 在 **Settings > WireGuard VPN** 中查看设备的 WireGuard 公钥，或通过 USB 发送 `keys`
   （见下文）。该密钥在首次启动时生成。
2. 在 WireGuard 服务器上把 Cardputer 添加为对端，并分配一个地址：

   ```ini
   [Peer]
   # cardputer
   PublicKey = <Cardputer 公钥>
   AllowedIPs = 10.0.0.2/32
   ```

3. 给 Cardputer 一份标准的 wg-quick 配置。`PrivateKey` 行可以省略，省略时设备继续使用
   自己生成的密钥。

   ```ini
   [Interface]
   Address = 10.0.0.2/32

   [Peer]
   PublicKey = <服务器公钥>
   Endpoint = vpn.example.com:51820
   AllowedIPs = 10.0.0.0/24
   PersistentKeepalive = 25
   ```

   可以通过 **Settings > WireGuard VPN > Import config over USB** 发送，也可以使用下文的
   `vpn-import`。导入后 VPN 会自动开启。

Wi-Fi 连接后，设备会先通过 NTP 校准时钟（WireGuard 握手包含服务器会检查的时间戳），
然后启动隧道；运行后主机列表会显示 `WIFI+VPN`。只有 `AllowedIPs` 范围内的流量走隧道
（`0.0.0.0/0` 表示全部流量），DNS、NTP 和 mDNS 仍直接使用 Wi-Fi。添加 VPN 内的主机时
请使用其隧道地址。

只支持一个对端和 IPv4：`Address` 和 `AllowedIPs` 只使用第一个 IPv4 条目，`Address`
必须位于 `AllowedIPs` 范围内；`DNS`、`MTU`、IPv6 条目和 IPv6 Endpoint 会被忽略或拒绝。

### 通过 USB 配置（可选）

除了在 Cardputer 上输入，也可以从电脑通过 USB 串口发送配置。
[`tools/cardputer_serial.py`](tools/cardputer_serial.py)（需要 `pip install pyserial`）
打开串口时不会重启设备，并且只打印设备的回复：

```bash
python3 tools/cardputer_serial.py keys                 # SSH 和 WireGuard 公钥
python3 tools/cardputer_serial.py vpn-import wg0.conf  # 上文格式的 wg-quick 配置
python3 tools/cardputer_serial.py host-add host.txt
printf 'ssid=%s\npassword=%s\n' "$SSID" "$PASSWORD" | python3 tools/cardputer_serial.py wifi-add -
```

主机文件由 `key=value` 行组成（`port` 默认为 22）：

```text
name=homelab
host=10.0.0.10
port=22
user=me
hostkey=SHA256:...
```

`hostkey` 可以省略。填写后，服务器的 Ed25519 主机密钥会在首次连接前就被固定，而不是
首次连接时才信任。请从可信来源获取指纹，例如在服务器上运行
`ssh-keygen -lf /etc/ssh/ssh_host_ed25519_key.pub`，或对自己的 `known_hosts` 运行
`ssh-keygen -lF <host>`。

每条命令会回复 `<命令>: ok` 或 `<命令>: error: <原因>`；如果当前没有 SSH 会话，设备随后
会重启以应用更改。`wifi-add` 和 `host-add` 会替换 SSID 或名称相同的已有配置。不用脚本时，
先单独发送命令一行，再发送内容各行，最后发送 `END`。内容不会被回显或记录，但与键盘一样，
任何能接触 USB 端口的人都可以发送命令。

## 键盘

按住 `Aa` 可临时使用键帽上印刷的复用层；短按并松开 `Aa` 可锁定或取消锁定。例如：
`Aa+1` → `!`、`Aa+2` → `@`、`Aa+;` → `:`。

| 场景 | 按键 | 功能 |
|---|---|---|
| 菜单 | Fn + 印刷的方向键 | 移动选择 |
| 菜单 | Enter / Del | 确认 / 返回 |
| 文本框 | Del / Fn+Del | 向后删除 / 向前删除 |
| REMOTE | 普通键 / Aa 复用键 | 发送对应字符 |
| REMOTE | Fn+方向、Fn+1–0 | 方向键和 F1–F12 |
| REMOTE | Ctrl / Alt + 字符 | 控制字符 / 带 Escape 前缀的字符 |
| REMOTE | Opt+Space | 切换到 LOCAL，不发送空格 |
| REMOTE/LOCAL | Opt+Enter | 打开 Settings |
| REMOTE/LOCAL | Opt+Del | 打开默认取消的结束会话确认框 |
| LOCAL | Fn+上/下 | 滚动一行 |
| LOCAL | Fn+左/右 | 滚动一页 |
| LOCAL | Del | 跳到最新输出 |

LOCAL 会继续接收 SSH 输出，但不会向服务器发送终端按键。

## 开发者编译与优化

克隆完整 PlatformIO 项目：

```bash
git clone https://github.com/MangoX0567/M5Stack-Cardputer-SSH.git
cd M5Stack-Cardputer-SSH
```

安装 VS Code 和 **PlatformIO IDE** 扩展，打开仓库根目录，然后执行
**PlatformIO: Upload**。使用 PlatformIO Core 时可以运行：

```bash
pio test -e native
pio run -e m5cardputer-adv
pio run -e m5cardputer-adv -t upload
```

普通 PlatformIO Upload 只更新应用程序，不会擦除 NVS。Wi-Fi 和 SSH 信息都通过设备界面
配置，不需要编辑凭据头文件。组件边界、安全行为、终端限制和资源占用说明位于
[docs/architecture.md](docs/architecture.md)。

欢迎提交有针对性的功能改进。请勿提交密码、私钥、编译输出、个人编辑器配置和串口日志。
GitHub Actions 会自动运行原生测试和生产构建。

## 在目标设备上开启 SSH

具体方式取决于远程操作系统。使用 systemd 且服务名为 `ssh` 的 Linux 系统可以执行：

```bash
sudo systemctl enable --now ssh
```

部分系统的服务名是 `sshd`。Windows 需要启用可选的 OpenSSH Server，macOS 可以开启
**Remote Login**。SSH 默认端口是 `22`，Cardputer 也可以保存其他端口。

## 可选的两行 Bash 提示符

如果用户名或主机名太长，导致命令输入空间很少，可以在远程 Bash 用户的 `~/.bashrc`
加入：

```bash
PS1='\u@\h:\w\n\$ '
```

执行 `source ~/.bashrc` 或重新连接。用户名、主机名和路径会显示在第一行，命令从第二行的
`$ ` 或 `# ` 后开始。删除这行即可恢复。固件不会识别或修改提示符，所以不会影响 Vim 等
全屏终端程序。

## 安全与限制

- 建议创建没有免密 `sudo` 权限的专用普通远程账户。
- 保存的密码以及设备的 SSH、WireGuard 私钥都位于普通 ESP32 NVS，而不是安全芯片；
  能够物理读取 Flash 的人可能提取它们。设备丢失时，请从服务器的 `authorized_keys`
  中删除其公钥，并从 WireGuard 服务器上删除其对端。
- 遇到主机密钥变化时，应先确认服务器情况，再重置信任。
- SSH 登录使用设备自己的 Ed25519 密钥或密码；不支持导入其他私钥或使用 SSH agent。
- 终端暂不支持 CJK/双宽排版、组合字符、双向文字、鼠标协议、终端图形、OSC 剪贴板和
  实时 PTY 尺寸调整。

## 许可与参考

本项目采用 [MIT License](LICENSE)。

设备优先的界面思路参考了
[Bruce firmware](https://github.com/BruceDevices/firmware)，PTY 和按键行为也与
[SSHClient-M5Cardputer](https://github.com/aat440hz/SSHClient-M5Cardputer) 做过对照。
本项目没有复制它们的源代码、素材、品牌或 UI 图稿。
