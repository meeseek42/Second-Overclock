# 第二超频，肉鸽深岩 | Second Overclock, Roguelike DRG

[English](README.en.md) · [mod.io 主页](https://mod.io/g/drg/m/second-overclock-roguelike-drg) · [已知问题与待实现组合](Docs/KNOWN_ISSUES.zh-CN.md)

本项目是《深岩银河》（Deep Rock Galactic）的机制扩展 Mod，支持在保留原版超频正负属性与特殊机制的前提下，为武器注入并融合**第二枚超频**。目前已适配全部 4 个职业的 24 把主副武器，共计 **453 种可用融合组合**，处理了底层的机制冲突、装备动态加载与多人联机同步。

> **玩家指引**：本仓库仅托管源码、数据配表与构建工具链，直接下载源码 ZIP 无法作为 Mod 安装。**普通玩家请前往 [mod.io 页面](https://mod.io/g/drg/m/second-overclock-roguelike-drg)订阅游玩并查看完整说明**（需配合 [ModHub](https://mod.io/g/drg/m/mod-hub) 使用，兼容游戏内原生订阅与 MINT / mintcat 加载器）。

## 工程文档导航

本仓库不包含游戏原版资产、Unreal Engine、第三方模板及测试存档，外部输入需由开发者在本地自行准备：

1. [贡献与构建流程 (CONTRIBUTOR-WORKFLOW.md)](Engineering/CONTRIBUTOR-WORKFLOW.md)：目录职责、工具安装、稳定身份及分支构建要求。
2. [公开输入准备与构建工具链 (PUBLIC-TOOLCHAIN.md)](Engineering/PUBLIC-TOOLCHAIN.md)：从本机原版资料准备输入，生成资产、Cook、核对并打包。
3. [工具缺口与支持边界 (TOOL-GAPS.md)](Engineering/TOOL-GAPS.md)：当前可修改范围及尚未封装为统一编辑入口的部分。

## 构建环境

- **环境要求**：Windows、PowerShell 7、Node.js 22、.NET 8 SDK、Unreal Engine 4.27.2，以及固定版本的兼容 FSD 模板。
- **构建入口**：新环境请按 `PUBLIC-TOOLCHAIN.md` 的公开准备流程操作；`npm run build:mod` 仅用于旧版输入目录布局，并非新开发者开箱构建命令。
- **版本标识**：面向玩家的已发布版本为 **0.93.14**；源码中的 `alpha170-contributor-reviewed16-nonrelease` 仅为构建工具验证标识。

## 后续开发计划 (TODO)

- 开局随机初始原版超频，以及未装备超频武器的单超频抽卡。
- 统一对外文件及日志命名，将设置、备份等迁移至本 Mod 专属目录并继承现有数据。
- 在保持联机可靠性的前提下，研究跨关卡复用连接并减少首次可见握手信息。
- 失败后自动开启详细诊断（持续到关闭游戏），替代当前的手动开关。
- Debug 批量测试增加“全部”范围。
- 评估将心石聚能炮“自动光控＋聚能过载”的独立装备准备顺序迁入共用路线，保留必要的光束适配。

## 问题反馈与日志提取

当前已确认 Bug、架构边界及 8 组暂未实装的冲突组合详见[已知问题与待实现组合](Docs/KNOWN_ISSUES.zh-CN.md)。

- **GitHub Issues**：[Bug 反馈、功能建议、机制讨论及构建问题](https://github.com/meeseek42/Second-Overclock/issues)
- **mod.io 评论区**：[日常游玩反馈](https://mod.io/g/drg/m/second-overclock-roguelike-drg)
- **邮箱**：`jioujiouningwang@gmail.com`（用于发送日志、崩溃文件或存档）

排查故障或崩溃时可提取以下目录文件（联机异常建议同时提供同局的房主与异常客机日志）：

- **Mod 诊断目录**：`<游戏目录>\FSD\Mods\DualOverclockReward`（可在 `ModHub → Debug` 页面点击“复制日志文件夹路径”后打包为 ZIP）。
- **游戏原生日志与崩溃目录**：`<游戏目录>\FSD\Saved\Logs\FSD.log` 与 `<游戏目录>\FSD\Saved\Crashes`。

## 许可证

本仓库原创源码与自有数据采用 [Mozilla Public License 2.0](LICENSE) 协议开源。游戏、引擎和第三方工具仍受各自许可约束，详见[许可范围与第三方说明 (LICENSE-SCOPE.md)](LICENSE-SCOPE.md)。
