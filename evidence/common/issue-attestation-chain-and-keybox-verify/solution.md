# issue-attestation-chain-and-keybox-verify：TEE 证书证明链四阶段改造（`local_keybox` / `remote` RKP V3 / `unprovisioned` 严格隔离 + 真实 AVB 信任根绑定 + `daemon attestation verify`）

- **设备/分组**：`common`（跨全机型宿主 `daemon`、`secure_env` KeyMint TEE 与发布/集群防泄漏链路；真机起机验收覆盖 `vivo/PD2454` `15-AP3A.240905.015.A2-20250416`、`google/oriole` `15-BP1A.250505.005-20250326` 及 `redmi/gold` `15-OS3.0.11.0.VNQCNXM` 负向闸门对账）
- **日期**：2026-11-25
- **状态**：**已闭环**（阶段 1～4 全部代码、Cuttlefish `0007..0011` 补丁集、C++/Go 全量单测、防泄漏加固与真机 `KeyAttestation` 双向实测闭环；在线 Play Integrity 如实标记 `NOT_TESTED`）

---

## 一、问题

改造前云机 KeyMint TEE（`secure_env` + `agent/modules/daemon/backend/tee`）的证明链与启动完整性存在四处结构性缺陷：

1. **每次起机现场捏造临时 CA 且重启即变**：`generateFactoryPKI()` 在进程启动时用随机 RSA-3072 / EC P-256 现签一套名为 `"CDH Hardware Attestation Root"` 的 2 层临时证书链，停机即删；不仅根证书不在 Google Hardware Attestation Root 信任表内，同一实例重启后证明根指纹还会突变。
2. **不支持外部 Android `keybox.xml` 导入与严格验真**：无法解析 OEM / Google 签发的 `<AndroidAttestation>` XML，缺少 PKCS#1/SEC1 → PKCS#8 归一化、逐级签名、`AKI == SKI`、内置信任根白名单（拦截公开泄露的 AOSP 软件测试根）与 Google `status.json` 撤销列表校验，也没有独立错误码（`ERR_KEYBOX_*`）。
3. **Verified Boot 四元组失真且未绑定真实 AVB 公钥**：宿主此前将 `verified-boot-hash` 当作启动公钥摘要回退，或直接随机生成 ES256 公钥充当 `verified_boot_key_sha256`；同时 `unprovisioned` 离线临时根实例仍允许把叶证书签成 `VERIFIED + locked`（「假根配真锁」的自相矛盾中间态），且启动前未校验盘集实际 `vbmeta` 摘要是否与 `device.json` 一致。
4. **缺少 RKP V3 在线供证能力、离线验收 CLI 与私密材料防泄漏护栏**：`cuttlefish/patch/` 仅有旧版 Android 13 兼容补丁，未接入 RKP V3 `rkp_project_identity`（TPM HBK 派生稳定 DICE UDS）、`RemoteAttestationBroker` 与三段式 HTTPS 客户端；仓库 `doc/` 下曾直接存放真实 `gaybox.xml` 样本，`.gitignore`、发布归档与集群 `rsync` 缺少针对 `*keybox*.xml` / `*.pk8` / `factory-pki` / `*rkp*token*` 的硬排除规则。

---

## 二、根因

1. 早期打通 Cuttlefish `secure_env` KeyMint-only 进程时（`0003` / `0004` / `0007` 初版），只验证了「非 AOSP 硬编码测试根即可启动 KeyMint」，未区分 `remote`（RKP 在线供证）、`local_keybox`（本地可信 keybox）与 `unprovisioned`（离线开发态）三种安全模式，导致宿主与 C++ `TpmAttestationRecordContext` 把临时自签 CA 与已供证状态混为一谈。
2. 原 `0003` 硬编码只接受 `RSA-3072` + `EC P-256` 且强制要求 RSA 与 EC 双链同时存在，既无法加载真实 OEM 常见的 `RSA-2048` keybox，也无法加载 RKP 签发的纯 EC（EC-only）证书链；此外 `TpmKeymasterContext::GenerateAttestation` 生成叶证书时未补齐指向签发者 `SubjectKeyIdentifier` 的 `AuthorityKeyIdentifier`（`AKI`）。
3. `13_pack_vda/grub.go` 在所有云机镜像 GRUB cmdline 中固定注入了 `androidboot.verifiedbootstate=green androidboot.vbmeta.device_state=locked androidboot.vbmeta.digest=<hash>`，客机 `RemoteKeymaster::Initialize()` 启动时必然向宿主发送 `ConfigureVerifiedBootInfo("green", "locked", <hash>)`——若宿主在 `unprovisioned` 模式下未锁死 `host_vb_locked_` 降级策略，就会被客机请求提升为 `VERIFIED`；反之若直接返回错误码，又会导致客机 KeyMint HAL 初始化失败崩退。

---

## 三、证据

- **负向安全闸门实测（详见同目录 [log-fail.txt](log-fail.txt)）**：
  - **Gate A（损坏或非受信根 keybox 拦截）**：`local_keybox` 模式下传入损坏 PEM 或自签伪造根 XML，`daemon` 在拉起 QEMU 前直接报错退出（`[ERR_KEYBOX_CHAIN_INCOMPLETE]` / `[ERR_KEYBOX_UNTRUSTED_ROOT]`），绝不静默降级为临时 CA。
  - **Gate B（陈旧/篡改 `vbmeta.img` 拦截）**：在 `vivo/PD2454` 盘集目录放入摘要不匹配的 `vbmeta.img`（`16c4972c...` vs `device.json` `10bfc120...`），启动前立即被 `VerifyVBMetaForLaunch` 拒绝启动。
  - **Gate C（已供证模式禁止随机启动公钥）**：对 `vda.img#vbmeta_a` 为 `--flags 2` 无内嵌公钥且未配置 `factory-tee-verified-boot-key` 的 `redmi/gold` 盘集启用 `local_keybox`，启动前立即报错 `镜像未包含可提取 AVB 公钥的 vbmeta 且未配置 factory-tee-verified-boot-key，已供证模式禁止生成随机启动公钥`。
- **正向双模式真机起机与 `KeyAttestation 2.0.2-debug` 实测（详见同目录 [log-pass.txt](log-pass.txt) 与四张实机截图）**：
  - **`unprovisioned` 模式（`keyattestation-pd2454-unprovisioned-top.png`）**：即使客机 `ro.boot.verifiedbootstate=green`、`ro.boot.vbmeta.device_state=locked`，KeyAttestation 仍准确展示橙色警示**「未知根证书」**与**「引导加载程序已解锁」**；实例经 `stop -> start` 重启后，`manifest.json` 与 `daemon attestation verify --kv` 的 `root_fingerprint`（`eafdb700e04da351eed0f6f5fe38f8e29fde695678f93e5cb3aebdb64512b778`）及 `input_sha256` 100% 稳定一致。
  - **`local_keybox` 模式 + 真实 AVB 公钥（`keyattestation-pd2454-keybox-{top,mid,rot}.png` 与 `oriole-local-keybox-{keyattestation,rot}.png`）**：`vivo/PD2454`（44s 冷启）与 `google/oriole`（42.6s 冷启）均一次点亮；离线态下 `PD2454` 顶部显示绿盾**「Google 硬件认证根证书」**与绿锁**「引导加载程序已锁定」**，底部硬件强制 `信任根 (Root of Trust [HW])` 显示 `verifiedBootKey = 80d1432f22356a2bb5b2968fa276b38f157fba6b201df69e45c49d0157018cc9`（与宿主从 `vda.img#vbmeta_a` 解析出的真实 AVB RSA-2048 公钥 SHA-256 逐字节一致）、`deviceLocked: true`、`verifiedBootState: Verified`、`verifiedBootHash: 10bfc120dc76ce817ab8fb073ecff28726b692591564926a91d1ae7bc658da39`；`oriole` 联网拉取 `Nov 24, 2026` 在线吊销表时同样完整展示 `Bootloader is locked`、`KeyMint 3.0 / TrustedEnvironment` 与 4 层硬件链（并如实标出历史测试样本 `gaybox.xml` 中间证已被 Google 在线吊销）。

---

## 四、改动

1. **阶段 1：Keybox 严格解析校验、持久化 `factory-pki` 与三态模式配置**
   - 新增 `agent/modules/daemon/backend/tee/{trust_roots.go,tee_attestation_status.json,keybox.go,keybox_test.go}`：内置 Google Hardware Attestation Roots（RSA-4096 Factory Root、EC P-384 Root、RKP Root CA1）与 Google 官方撤销列表（1500+ 条序列号），拦截公开 AOSP 软件测试根；支持 `<AndroidAttestation>` XML 解析、PKCS#1/SEC1/PKCS#8 私钥归一化为标准 PKCS#8 DER、链首公钥匹配、有效期、自签根、逐级签名与 `AKI == SKI` 强校验，定义 12 个独立 `ERR_KEYBOX_*` 错误码。
   - 新增 `agent/modules/daemon/backend/tee/{factory_pki.go,factory_pki_test.go}` 并重构 `secure_env.go`：统一收敛至 `prepareAttestationPKI(stateDir, spec)`，产物以 `0700`/`0600` 权限持久化于 `<instance>/secure-env/state/factory-pki/`，写入脱敏审计元数据 `manifest.json`（仅存公开指纹、`input_sha256` 与启动证据，零私钥）；`local_keybox` 仅在源 XML 的 `sha256` 变化时原子重导，`unprovisioned` 首次生成后跨重启复用。
   - 扩展 `agent/modules/daemon/config/{attestation.go,attestation_test.go,device.go}`：新增 `factory-tee-attestation-mode`（`remote | local_keybox | unprovisioned`，支持环境变量覆盖，缺材料自动降级为 `unprovisioned` 并打告警日志；显式声明 `local_keybox` 却损坏/缺文件则硬报错拒绝启动）、`factory-tee-keybox`、`rkp-endpoint`、`rkp-ca`、`rkp-enrollment-token`（`SecretToken` 封装，`String()`/`GoString()`/`MarshalJSON()` 全脱敏为 `[REDACTED]`）。
2. **阶段 2：真实 AVB 公钥绑定、`vbmeta` 启动前强对账与 `0007` + `0010` 补丁**
   - 新增 `agent/modules/daemon/config/{vbmeta.go,vbmeta_test.go}` 与 `tee/verified_boot_test.go`，更新 `run/{qemu.go,manage.go}`：直接解析盘集 `vbmeta.img` 或 `vda.img` GPT `vbmeta_a/vbmeta` 分区头（`AVB0`），校验内部认证哈希、启动前强比对 `verified-boot-hash`，提取内嵌 AVB 公钥原始字节（或将外部 PEM/DER RSA 2048/4096 公钥编码为 libavb 标准 `avb_rsa_public_key_header || n || r^2` 格式）计算真实 `verified_boot_key_sha256`，彻底移除「拿 `vbmeta` 摘要冒充公钥」的错误回退。
   - 更新 `cuttlefish/patch/0007-bind-factory-verified-boot-state.patch` 并新增 `0010-attestation-mode-unprovisioned-downgrade.patch`：`secure_env` 新增 `--verified_boot_hash_sha256`、`--verified_boot_state`、`--device_locked`、`--attestation_mode`；`TpmKeymasterContext::SetVerifiedBootInfo` 强校验客机上报 `vbmeta_digest == host_vbmeta_digest_`，同时在 `unprovisioned` 模式下无论调用方或客机传入何值均锁死 `KM_VERIFIED_BOOT_UNVERIFIED + unlocked`（并对客机 `ConfigureVerifiedBootInfo` 返回 `KM_ERROR_OK` 保障 HAL 正常就绪）；放宽支持 RSA 2048/3072/4096、EC P-256/P-384 与 EC-only 链，新增 `AddLeafAki` 自动为叶证书注入签发者 SKI 对应的 `AKI`。
3. **阶段 3：RKP V3 项目身份、Broker 通道与三段式 HTTPS 客户端**
   - 新增/更新 `cuttlefish/patch/0008-rkp-v3-project-identity.patch`、`0009-remote-attestation-broker.patch`（原 guest Android 13 兼容补丁顺延为 `0011-device-cuttlefish-keymint-rkp-android13-compat.patch`）并锁定 `cuttlefish/scripts/security-sources.json` SHA-256：实现 `rkp_project_identity.{h,cpp}`（TPM HBK 域隔离派生 Ed25519 DICE BCC 与 V3 `device_info`、CSR_V2）、`tpm_hmac.cpp`、`remote_attestation_broker.{h,cpp}` 与 `--rkp_csr_request_hex` 子进程隔离签名入口。
   - 新增 `agent/modules/daemon/backend/tee/{rkp_client.go,rkp_client_test.go}`：零外部依赖实现 RFC 8949 Canonical CBOR 编解码、`:fetchEekChain` / `:getRandom` / `:signCertificates` 三段式 TLS 1.3 HTTPS 客户端、宿主 CSPRNG + 服务端熵混合生成 P-256 密钥（私钥只留宿主进程，TPM HBK 不出 `secure_env` 子进程）、`ServeRKPBrokerSession` 私密通道服务，并在首次供证成功后格式化落盘 `factory-pki/keybox.xml` 供后续重启离线复用。
4. **阶段 4：验收工具 `daemon attestation verify` 与全链路防泄漏加固**
   - 新增 `agent/modules/daemon/backend/tee/{attest.go,attest_test.go}` 并在 `run/run.go` 接入 `daemon attestation verify [--kv] <instance-dir>`：离线复核 PKCS#8 私钥、证书链签名、`AKI == SKI`、信任根、撤销状态、供证模式不变量及关联盘集 `vbmeta` 摘要，固定输出 `play_integrity=NOT_TESTED`。
   - 更新根 `.gitignore`、`devices/.gitignore`、`agent/core/.gitignore`、`doc/devices/.gitignore`，并在 `agent/core/internal/release/release.go`、`agent/core/internal/cluster/{cluster.go,distributed.go,sync_sources.go}`、`agent/modules/pipeline/internal/distributed/runtime_server.go` 全量排除 `*keybox*.xml`、`gaybox.xml`、`*.pk8`、`factory-pki`、`*rkp*token*`；将原工作树内 `doc/gaybox.xml` 迁至仓库外本机 `0700`/`0600` 受控目录 `~/.cdh/keybox/gaybox.xml`。

---

## 五、验证

1. **Cuttlefish 编译与 C++/Shell 测试套件**：`build/cuttlefish/build-cuttlefish-darwin build` 零错误重编并安装 `out/qemu/dist/darwin/libexec/secure_env`；`rkp_project_identity_test`、`remote_attestation_broker_test`、`attestation_mode_test` 及 3 个 `secure_env_keymint_only*_test.sh` 共 6 套测试全部 PASS。
2. **全仓 Go 单元测试与竞态检测**：`cuttlefish-security-source`、`agent/modules/daemon/...`（`-race`，含调用真实 C++ `secure_env` 子进程签发 CSR_V2 的 `TestSecureEnvTASignerIntegrationIfBuilt` 与外部样本回归）、`agent/internal/{release,cluster}`、`agent/modules/pipeline/internal/{distributed,halcore/halbuild}` 全部 100% PASS。
3. **真机双模式起机、`daemon attestation verify` 与防泄漏审计**：
   - `vivo/PD2454` 在 `unprovisioned` 与 `local_keybox` 两种模式下均约 44s 冷启至 `sys.boot_completed=1`，KeyAttestation 截图与 `daemon attestation verify` 输出逐项吻合；
   - 对 `manifest.json`、`secure-env.log`、`daemon` 日志、证据目录及全仓库工作树扫描 PEM 私钥头、`*keybox*.xml` 与 `gaybox.xml`，私密材料残留为 **0**。

---

## 六、遗留

1. **在线 Play Integrity 与 Google RKP 生产登记通道**：本机当前无 Google 官方 RKP 合作商登记令牌（`rkp-enrollment-token`）及 GMS Play Integrity 在线测试环境，按计划规范将 `play_integrity` 如实记录为 `NOT_TESTED`；`remote` 模式的三段式协议、TA 实签 CSR_V2、错误脱敏与持久化复用已由本地 TLS 模拟 RKP CA 全链路单测覆盖，后续取得合作登记凭据或部署自建 RKP 服务后可直接配置启用。
2. **Lineage `--flags 2` 无内嵌公钥机型的 `local_keybox` 配置**：像 `vivo/PD2454` 这类 `vda.img#vbmeta_a` 内嵌了真实 AVB RSA 公钥的盘集可直接零配置提取 `verified_boot_key_sha256`；而像 `redmi/gold` 这类底包 `vbmeta_a` 为空公钥（`flags=2`）的盘集，在切换 `local_keybox` / `remote` 模式时需在 `device.json` 显式提供 `factory-tee-verified-boot-key`（原厂 `vbmeta.img` 提取公钥或 PEM 公钥路径），否则会被 Gate C 按设计拒绝启动。
