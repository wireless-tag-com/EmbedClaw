# EmbedClaw 组件单测说明

本目录存放 `embed_claw` 组件的板上单元测试。

当前测试体系基于 ESP-IDF 官方 `unit-test-app + Unity`，目标不是替代真机联调，而是把以下几类回归尽量提前拦住：

- tool 的参数校验、格式化和调度回归
- channel provider 的消息编解码和路由回归
- LLM provider 的 JSON 转换和响应生命周期回归
- core 层边界条件、空状态和错误路径回归

## 1. 测试范围

这套测试覆盖的是“组件级单测”，重点放在模块契约和板上可重复验证的纯逻辑。

当前明确覆盖：

- 每个内置 tool 至少有一条独立测试
- 每个已注册 channel provider 至少有一条独立测试
- OpenAI 兼容 provider 的 JSON 转换测试
- registry、dispatch、response free 等基础契约
- 串口自动驱动 Unity 菜单并自动采集结果
- GitHub Actions 对主工程和 `unit-test-app` 的编译检查

当前不覆盖：

- 真实 Tavily / DashScope / Feishu 联网成功路径
- 真实 QQBot token / gateway / websocket 联网成功路径
- WebSocket / Feishu 的 server/client 生命周期和异步任务路径
- `ec_agent.c` 的完整 ReAct loop、tool result 回填和队列任务行为
- `embed_claw.c` 的总启动流程与 outbound dispatch task
- `ec_session_list()` 成功路径，以及 session 损坏文件分支
- `get_current_time` 的 NTP 成功/回退路径，`web_search` / `cron` 的完整成功路径与后台行为
- 完整 agent 多轮 tool loop
- 多设备协同
- AI 回复质量

这类场景仍然应该通过真机联调或手动验证确认。

## 2. 当前覆盖矩阵

### 2.1 tools

- [test_tool_get_time.c](/home/kirto/EmbedClaw1/components/embed_claw/test/tools/test_tool_get_time.c)
  - 时间格式化 helper
  - 非法 epoch / 输出缓冲区边界
- [test_tool_web_search.c](/home/kirto/EmbedClaw1/components/embed_claw/test/tools/test_tool_web_search.c)
  - 非法 JSON
  - 缺少 `query`
  - Tavily 结果格式化
  - 空结果输出
- [test_tool_files.c](/home/kirto/EmbedClaw1/components/embed_claw/test/tools/test_tool_files.c)
  - SPIFFS 路径沙箱校验
  - 文本替换 helper
  - `read_file` / `write_file` / `edit_file` 错误路径
  - `read_file` / `write_file` / `edit_file` / `list_dir` 成功路径
- [test_tool_cron.c](/home/kirto/EmbedClaw1/components/embed_claw/test/tools/test_tool_cron.c)
  - 必填字段校验
  - `at` / `every` 调度参数校验
  - `cron_add` / `cron_list` / `cron_remove`
  - 外部 channel 的 `chat_type + chat_id` 约束

### 2.2 channels

- [test_channel_contract.c](/home/kirto/EmbedClaw1/components/embed_claw/test/channel/test_channel_contract.c)
  - registry 路由和未命中行为
- [test_channel_ws.c](/home/kirto/EmbedClaw1/components/embed_claw/test/channel/test_channel_ws.c)
  - WebSocket 入站 JSON 解析
  - Feishu relay 消息兼容解析
  - 出站 response JSON 编码
- [test_channel_feishu.c](/home/kirto/EmbedClaw1/components/embed_claw/test/channel/test_channel_feishu.c)
  - WebSocket ping frame 编码/解析
  - response frame 编码/解析
- [test_channel_qq.c](/home/kirto/EmbedClaw1/components/embed_claw/test/channel/test_channel_qq.c)
  - 官方 QQBot dispatch event 到 `ec_msg_t` 的映射
  - `chat_type + chat_id` 到官方 REST path / body 的编码

### 2.3 llm

- [test_llm_contract.c](/home/kirto/EmbedClaw1/components/embed_claw/test/llm/test_llm_contract.c)
  - provider 初始化边界
  - provider 未初始化行为
  - `ec_llm_response_free()` 生命周期
- [test_llm_openai_json.c](/home/kirto/EmbedClaw1/components/embed_claw/test/llm/test_llm_openai_json.c)
  - tools schema 转 OpenAI function calling 格式
  - assistant / tool_result message 转换
  - pinned CA 选择逻辑

### 2.4 core

- [test_agent_contract.c](/home/kirto/EmbedClaw1/components/embed_claw/test/core/test_agent_contract.c)
  - `cron_add` 上下文补丁
  - system prompt 拼装
  - 当前 turn context 追加
- [test_memory_contract.c](/home/kirto/EmbedClaw1/components/embed_claw/test/core/test_memory_contract.c)
  - 长期记忆读写
  - daily note 追加
  - recent notes 聚合
- [test_skill_loader_contract.c](/home/kirto/EmbedClaw1/components/embed_claw/test/core/test_skill_loader_contract.c)
  - 内置 skill 安装
  - skill summary 构建
- [test_session_contract.c](/home/kirto/EmbedClaw1/components/embed_claw/test/core/test_session_contract.c)
  - session history 的空状态
  - 非法输出缓冲区参数
  - `max_msgs <= 0` 的 clamp 行为
  - session append / clear 成功路径
  - history limit ring buffer 行为

当前 core 层仍未直接覆盖：

- [ec_agent.c](/home/kirto/EmbedClaw1/components/embed_claw/core/ec_agent.c)
  - agent task 启动
  - inbound / outbound queue 行为
  - 完整 tool loop 与出站错误路径
- [embed_claw.c](/home/kirto/EmbedClaw1/components/embed_claw/embed_claw.c)
  - 总启动流程
  - outbound dispatch task
- [ec_session.c](/home/kirto/EmbedClaw1/components/embed_claw/core/ec_session.c)
  - session file 枚举
  - 损坏记录跳过分支

## 3. 目录结构

```text
components/embed_claw/test/
├── CMakeLists.txt
├── partitions.csv
├── README.md
├── sdkconfig.defaults
├── support/
│   ├── ec_test_channel_feishu.c
│   ├── ec_test_channel_qq.c
│   ├── ec_test_channel_ws.c
│   ├── ec_test_hooks.h
│   ├── ec_test_llm_openai.c
│   ├── ec_test_tools_files.c
│   ├── ec_test_tools_get_time.c
│   └── ec_test_tools_web_search.c
├── channel/
│   ├── test_channel_contract.c
│   ├── test_channel_feishu.c
│   ├── test_channel_qq.c
│   └── test_channel_ws.c
├── core/
│   ├── test_agent_contract.c
│   ├── test_memory_contract.c
│   ├── test_skill_loader_contract.c
│   └── test_session_contract.c
├── llm/
│   ├── test_llm_contract.c
│   └── test_llm_openai_json.c
└── tools/
    ├── test_tool_cron.c
    ├── test_tool_files.c
    ├── test_tool_get_time.c
    ├── test_tool_web_search.c
    └── test_tools_contract.c
```

## 4. 测试 hook

部分生产模块存在静态 registry、静态状态或内部 helper。为了让每条测试可独立运行，这里保留了少量 reset/configure hook，并通过 test-only shim 暴露需要单测的协议编解码逻辑：

- [ec_test_hooks.h](/home/kirto/EmbedClaw1/components/embed_claw/test/support/ec_test_hooks.h)
- [ec_channel.c](/home/kirto/EmbedClaw1/components/embed_claw/core/ec_channel.c)
- [ec_tools.c](/home/kirto/EmbedClaw1/components/embed_claw/core/ec_tools.c)
- [ec_llm.c](/home/kirto/EmbedClaw1/components/embed_claw/llm/ec_llm.c)
- [ec_test_channel_qq.c](/home/kirto/EmbedClaw1/components/embed_claw/test/support/ec_test_channel_qq.c)
- [ec_test_channel_ws.c](/home/kirto/EmbedClaw1/components/embed_claw/test/support/ec_test_channel_ws.c)
- [ec_test_llm_openai.c](/home/kirto/EmbedClaw1/components/embed_claw/test/support/ec_test_llm_openai.c)

其中：

- `reset/configure` 类 hook 用来隔离静态状态和副作用
- test-only shim 用来复用生产代码里的纯逻辑实现，而不是在测试里复制一份逻辑副本

这些接口只服务测试，不属于生产 API。

## 5. 本地运行方式

### 5.0 CI 行为

当前 GitHub Actions workflow 是：

- [embed_claw-unit-tests.yml](/home/kirto/EmbedClaw1/.github/workflows/embed_claw-unit-tests.yml)

它只做两件事：

- `idf.py build` 编主工程
- `./scripts/run_unit_tests.sh build` 编 `unit-test-app`

它不会在 GitHub 上执行真实硬件测试，也不会自动烧录板子。

### 5.1 前置条件

运行前需要满足：

- 已安装并配置 ESP-IDF
- 已执行 `source "$IDF_PATH/export.sh"`
- 当前开发板可正常烧录
- 首次构建如果缺少 managed components，本机可以访问 Espressif Component Registry

测试工程会复用主工程的关键默认配置，并在
[sdkconfig.defaults](/home/kirto/EmbedClaw1/components/embed_claw/test/sdkconfig.defaults)
和
[partitions.csv](/home/kirto/EmbedClaw1/components/embed_claw/test/partitions.csv)
中保留一份测试专用镜像配置。

需要注意：

- `sdkconfig.defaults`
  - 保留测试镜像自己的关键配置，当前显式关闭了 `PSRAM`，避免 `unit-test-app` 在 heap poisoning 下把临时分配落到 PSRAM，导致无关的堆损坏噪声
- `partitions.csv`
  - 除了主工程需要的 `spiffs` 分区外，还包含 ESP-IDF `unit-test-app` 必需的 `flash_test` 分区
- [run_unit_tests.sh](/home/kirto/EmbedClaw1/scripts/run_unit_tests.sh)
  - 会强制把测试工程指向这两份配置，而不是根目录的默认分区表

### 5.2 仅构建测试镜像

```bash
./scripts/run_unit_tests.sh build
```

构建产物会写到仓库内的：

```text
build/unit-test-app-<target>/
```

例如当前默认目标为 `esp32s3` 时，会生成到：

```text
build/unit-test-app-esp32s3/
```

### 5.3 完整烧录测试镜像

```bash
./scripts/run_unit_tests.sh -p /dev/ttyUSB0 flash
```

这个命令会烧录当前测试产物对应的：

- bootloader
- partition table
- `unit-test-app.bin`

如果刚改过 `sdkconfig.defaults` 或 `partitions.csv`，不要只刷 app，必须整套重刷。

测试产物位于：

```text
build/unit-test-app-<target>/
```

例如 `esp32s3` 默认会使用：

```text
build/unit-test-app-esp32s3/
```

不要误用仓库里其他旧的 `build/unit-test-app/` 产物。

### 5.4 手动进入 Unity 菜单

```bash
./scripts/run_unit_tests.sh flash monitor
```

进入菜单后，常用输入如下：

- `*`
  - 运行镜像中的全部测试
- `[embed_claw]`
  - 运行全部 EmbedClaw 组件测试
- `[embed_claw][tools]`
  - 只运行 tools
- `[embed_claw][channel]`
  - 只运行 channels
- `[embed_claw][llm]`
  - 只运行 llm
- `[embed_claw][core]`
  - 只运行 core
- `"完整测试名"`
  - 只运行单条测试

### 5.5 自动串口结果采集

如果不想手动操作 Unity 菜单，可以直接自动驱动串口。

先烧录：

```bash
./scripts/run_unit_tests.sh -p /dev/ttyUSB0 flash
```

再采集结果：

```bash
python3 -m pip install pyserial
python3 scripts/collect_unity_results.py \
  --port /dev/ttyUSB0 \
  --selector "[embed_claw]"
```

也可以一次跑多个 selector：

```bash
python3 scripts/collect_unity_results.py \
  --port /dev/ttyUSB0 \
  --selector "[embed_claw][tools]" \
  --selector "[embed_claw][channel]"
```

默认结果会写到：

```text
build/unit-test-results/
```

包含：

- `unity.log`
  - 原始串口日志
- `summary.json`
  - 机器可读汇总
- `junit.xml`
  - 供 CI 消费的 JUnit 结果
- `summary.md`
  - 适合直接贴到 GitHub Step Summary 的 Markdown 汇总

### 5.6 本地一键构建 + 烧录 + 采集

```bash
python3 -m pip install pyserial
ESPPORT=/dev/ttyUSB0 ./scripts/run_unit_tests_ci.sh
```

可选环境变量：

- `ESPPORT`
  - 串口设备，例如 `/dev/ttyUSB0`
- `ESPBAUD`
  - 串口波特率，默认 `115200`
- `UNITY_SELECTORS`
  - 要运行的 selector，默认 `[embed_claw]`
- `UNITY_RESULTS_DIR`
  - 结果输出目录，默认 `build/unit-test-results`

例如：

```bash
ESPPORT=/dev/ttyUSB0 \
UNITY_SELECTORS="[embed_claw][tools] [embed_claw][llm]" \
./scripts/run_unit_tests_ci.sh
```

## 6. 预期结果

成功时应满足：

- 所选测试均为 `PASS`
- Unity summary 中 `Failures = 0`
- 没有异常重启
- 没有 `Guru Meditation`
- 没有明显的堆损坏或非法访问

典型成功 summary：

```text
n Tests 0 Failures 0 Ignored
OK
```

失败时通常有两类：

- 普通断言失败
  - 说明模块契约或行为回归
- 崩溃 / abort / Guru Meditation
  - 说明存在更严重的内存、生命周期或状态污染问题

另外，Unity 的 leak checker 可能打印 warn 级别的内存波动日志。
如果单条测试仍然显示 `PASS`，且最终 summary 为 `0 Failures`，说明这条日志没有被判为阻塞失败；但如果出现稳定的 `critical leak`、heap corruption 或崩溃，仍然需要继续处理。

## 7. 常见问题

### 7.1 `SPIFFS: spiffs partition could not be found`

通常说明板子上跑的不是最新测试分区表，或者只刷了 app 没刷新的 partition table。

处理方式：

- 执行 `./scripts/run_unit_tests.sh -p /dev/ttyUSB0 flash`
- 确认烧录产物来自 `build/unit-test-app-<target>/`
- 如果仍异常，先 `erase-flash` 再重新完整烧录

### 7.2 所有测试一开始都 `Expected Non-NULL`

如果出现类似：

```text
37 Tests 37 Failures 0 Ignored
```

并且每条都在 very early stage 统一失败，优先怀疑公共 `setUp()` 没过，而不是 37 个模块同时回归。

本测试工程里最常见的原因是：

- `flash_test` 分区没有随最新 partition table 一起烧进去
- 跑的仍然是旧的测试镜像

### 7.3 `Stack canary watchpoint triggered (unityTask)`

这通常不是 Unity 自己坏了，而是测试代码或被测函数在 `unityTask` 上分配了过大的栈缓冲。

建议：

- 避免在测试函数里直接放几 KB 到十几 KB 的局部数组
- 需要大临时缓冲时优先使用 heap，或者改成流式拼接
- 如果是 prompt / JSON / 文件聚合逻辑，优先检查 helper 是否叠了多个大局部数组

## 8. 如何为新增模块补测试

### 8.1 新增 tool

新增一个 tool 时，至少补一份独立测试文件：

```text
components/embed_claw/test/tools/test_tool_<name>.c
```

建议最少覆盖：

- 非法 JSON
- 缺少必填字段
- 成功路径或 helper 逻辑
- 格式化输出
- 外部依赖不可达时的稳定错误

如果 tool 强依赖外部服务，不要在单测里真的发网。
优先把可测部分下沉为 helper，然后测：

- 参数解析
- 请求体组装
- 响应格式化
- 边界错误处理

建议 tag：

```text
[embed_claw][tools][<name>]
```

### 8.2 新增 channel provider

新增一个 channel provider 时，建议新增：

```text
components/embed_claw/test/channel/test_channel_<name>.c
```

建议覆盖：

- provider 自身消息解析
- provider 出站编码
- 必填字段缺失
- registry 路由是否仍然成立

建议 tag：

```text
[embed_claw][channel][<name>]
```

例如 QQ channel 当前使用：

```text
[embed_claw][channel][qq]
```

### 8.3 新增 LLM provider

新增 LLM provider 时，建议拆成两层：

- contract 测试
  - 初始化边界
  - 未初始化行为
  - response 生命周期
- provider JSON 测试
  - tools schema 转换
  - messages 转换
  - error body / response body 解析

命名建议：

```text
components/embed_claw/test/llm/test_llm_<provider>_json.c
```

建议 tag：

```text
[embed_claw][llm][<provider>]
```

### 8.4 新增 core 逻辑

适合下沉到 `core/` 单测的内容包括：

- session/history
- 路由状态机
- JSON 边界
- 纯逻辑 helper
- 生命周期和资源释放
