# #319 virglrenderer 改为只读上游 + 独立构建工作区纯 Patch 模式

## 一、问题

与 `cuttlefish/`（只读上游 `source/` + `patch/` → 构建副本 `out/cuttlefish/workspace/`）和 `devices/kernels/`（只读上游 + `patches/` → 构建卷内打补丁）不同，`qemu/external/virglrenderer` 在构建时仍被就地打补丁，导致：
1. `qemu/external/virglrenderer` 工作区长期残留 28 个已修改文件，违背外部上游只读约束，影响 `repo sync`；
2. 更新已有补丁或增删 `qemu/patch/virglrenderer/*.patch` 时，原地 `git apply` 易冲突，且 `build-qemu-darwin` / `build-qemu-docker.sh` 缺少补丁摘要校验，改动补丁不会自动触发重编。

## 二、根因

1. `qemu/patch/apply-virglrenderer-patches.sh` 仅支持对目标 git 目录原地 `git apply`，缺少从只读上游导出纯净工作区再打补丁的模式；
2. `build/qemu/build-qemu-darwin` 的 `build_virglrenderer` 与 `build/qemu/build-qemu-docker.sh` 的 `build_virglrenderer`/`build_gpu` 均直接传入 `${EXT}/virglrenderer` 原地打补丁，且未把 `qemu/patch/virglrenderer/*.patch` 计入缓存摘要；
3. `build-qemu-docker.sh` 的 `build_gpu` 重复编写了 virglrenderer 构建流程，且第 516 行在摘要变化后仍被 `keep` 拦截导致跳过发布新版产物。

## 三、证据

- 改造前 `git -C qemu/external/virglrenderer status` 有 28 个脏文件，经脚本逐字节校验 `git diff == patches 01..29`（`EXACT_MATCH`），详见同目录 `log-fail.txt`；
- 改造后纯 patch 模式导出、29/29 补丁应用、macOS 完整编译、二次复跑摘要命中跳过、`doctor` 41/41 通过及上游工作区 100% 干净核验详见同目录 `log-pass.txt`。

## 四、改动

1. **`qemu/patch/apply-virglrenderer-patches.sh`**：
   - 新增 `--src <只读上游 git 目录> --out <可变构建工作区>` 模式：校验上游基线 commit 后通过 `git archive HEAD` 导出纯净源码到 `--out`，在隔离目录按序严格应用全部补丁并移除临时 `.git`，全程零写入上游源码树；同时向后兼容原单参数原地幂等模式。
2. **`build/qemu/build-qemu-darwin`**：
   - 新增 `virglrenderer_digest()`（纳入上游 `HEAD`、`apply-virglrenderer-patches.sh` 及全部 `virglrenderer/*.patch` 的 SHA-256）；
   - `build_virglrenderer()` 改为 `--src "${EXT}/virglrenderer" --out "${BUILD}/virglrenderer-src"` 并从 `${BUILD}/virglrenderer-src` 编译，缓存校验比对 `.virglrenderer-digest`；
   - `doctor` 增加 `qemu/patch/virglrenderer（补丁集）` 检查项。
3. **`build/qemu/build-qemu-docker.sh`**：
   - 新增 `virglrenderer_digest()` 并改造 `build_virglrenderer()` 为 `--src`/`--out` 独立构建工作区模式；
   - `build_gpu()` 统一复用 `build_virglrenderer()`，将 `input_digest` 改为按上游 `HEAD` + `--tree "${PATCHES}/virglrenderer"` + `--file "${PATCHES}/apply-virglrenderer-patches.sh"` 计算，并移除摘要变化后被 `keep` 拦截跳过发布的缺陷；
   - `doctor` 增加 `qemu/patch/virglrenderer（补丁集）` 检查项。
4. **`manifest/qemu.xml` & `qemu/external/virglrenderer`**：
   - 更新 `manifest/qemu.xml` 注释明确 29 个补丁与只读上游导出口径；
   - 还原 `qemu/external/virglrenderer` 工作区（`git checkout -- .`），恢复 100% 纯净上游。

## 五、验证

- 语法检查：`bash -n qemu/patch/apply-virglrenderer-patches.sh build/qemu/build-qemu-darwin build/qemu/build-qemu-docker.sh` 通过；
- 首跑 `build/qemu/build-qemu-darwin deps`：成功从纯净上游导出至 `out/qemu/darwin-arm64/build/virglrenderer-src`，`应用29 失败0`，完成 `libvirglrenderer.a` 与 `virgl_render_server` 编译安装；
- 二跑 `build/qemu/build-qemu-darwin deps`：精确命中摘要缓存输出 `保留静态库 virglrenderer（上游与补丁未变）`；
- `build/qemu/build-qemu-darwin doctor`：`41 项就绪，0 项待补`；
- `git -C qemu/external/virglrenderer status --short` 为空（`qemu/external/` 全部 8 个上游仓库均保持干净）。

## 六、遗留

无。后续只需在 `qemu/patch`、`build`、`manifest` 子仓库按需提交本次补丁与脚本变更（含此前未跟踪的 `qemu/patch/virglrenderer/29-macos-disable-separable-shaders.patch`）。
