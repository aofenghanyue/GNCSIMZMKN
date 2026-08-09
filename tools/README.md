# Repository tools

- `bootstrap.ps1`：配置、构建、运行 CTest 和仓库检查。
- `verify-repository.ps1`：验证目录、JSON、任务依赖、legacy hash、架构边界、UTF-8 和 Markdown 链接。
- `extract-legacy-reference.ps1`：校验并解包只读 legacy archive 到忽略目录。

脚本只操作本仓库中的明确路径。任何生成的研究数据需要经过 fixture/Artifact 晋升流程才能进入版本控制。

Windows 自检入口：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/bootstrap.ps1
```

安装 PowerShell 7 的平台也可以将命令中的 `powershell` 换成 `pwsh`。
