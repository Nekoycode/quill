# zest

[English](README.md) · [简体中文](README.zh-CN.md)

一个轻量、高度可定制、线程安全的 C++20 日志库。

`zest` 将 **spdlog 式的 API**(logger → sinks → pattern formatter → level →
registry)与**前端/后端异步引擎**结合:热路径只捕获参数(零分配),由后台线程池完成
格式化与 I/O。这种「延迟格式化」让异步路径比 spdlog 的前端格式化异步日志器快约 3 倍。

---

## 特性

- **头文件库、零强制依赖** —— 仅基于 C++20 标准库(`std::format`、`std::chrono`、
  `std::source_location`、`std::jthread`、`std::stop_token`)。
- **spdlog 式 API** —— 熟悉的 `logger`、`sinks`、pattern formatter、级别过滤与全局注册表。
- **可靠并发** —— 有界无锁 MPMC 队列 + 可配置后端线程池;优雅关闭会排空所有在途记录。
- **异步热路径零分配** —— 参数被捕获进小缓冲优化的容器(常见情况无堆分配)。
- **编译期安全** —— 格式串在编译期校验;`ZEST_ACTIVE_LEVEL` 会彻底移除被禁用级别的语句。
- **结构化日志** —— `json_sink` 每条记录输出一个 JSON 对象。
- **回溯(backtrace)** —— 保留最近 N 条记录,出错后可回放。
- **现代 CMake** —— 目标导向构建、CMake presets、安装导出(`find_package(zest CONFIG)`)。

## 环境要求

- C++20 编译器:GCC ≥ 13、Clang ≥ 15、MSVC ≥ 19.29。
- CMake ≥ 3.25 与 [Ninja](https://ninja-build.org/)(推荐生成器)。

## 快速开始

```cpp
#include <zest/zest.h>

int main() {
  auto logger = zest::stdout_logger("app");
  logger->set_pattern("%^[%H:%M:%S.%e] [%l] [%n] %v%$");

  ZEST_INFO(logger, "hello {}!", "world");
  ZEST_WARN(logger, "the answer is {}", 42);
  return 0;
}
```

```bash
cmake --preset dev
cmake --build --preset dev
./build/dev/examples/zest_basic
```

## 日志记录

### 宏

`ZEST_LOGGER_*` 系列宏接收一个 logger;`ZEST_*` 系列宏使用默认 logger(按需惰性创建):

```cpp
ZEST_LOGGER_TRACE(logger, "trace");      // 指定 logger
ZEST_LOGGER_DEBUG(logger, "x = {}", 1);
ZEST_LOGGER_INFO(logger, "info");
ZEST_LOGGER_WARN(logger, "warn");
ZEST_LOGGER_ERROR(logger, "error");
ZEST_LOGGER_CRITICAL(logger, "critical");

ZEST_INFO("默认 logger");                // 默认 logger
```

源码位置通过 `std::source_location::current()` 在调用点自动捕获。

### 级别与过滤

```cpp
logger->set_level(zest::level::warn);   // 运行时过滤(按 logger)
```

要在编译期彻底移除某个级别,在包含 zest 之前定义 `ZEST_ACTIVE_LEVEL`(例如
`-DZEST_ACTIVE_LEVEL=ZEST_LEVEL_WARN`),低于该级别的语句会在预处理阶段被移除。

### Pattern 格式化

通过 `set_pattern` 配置输出格式。支持的标志:

| 标志 | 含义 | 标志 | 含义 |
|---|---|---|---|
| `%Y` `%m` `%d` | 年 / 月 / 日 | `%H` `%M` `%S` | 时 / 分 / 秒 |
| `%b` `%a` | 月 / 星期缩写 | `%e` `%f` | 毫秒 / 微秒 |
| `%l` `%L` | 级别缩写 / 全称 | `%n` | logger 名 |
| `%t` `%P` | 线程 id / 进程 id | `%v` | 消息文本 |
| `%s` | 源码 `file:line` | `%g` `%#` `%!` | 文件 / 行号 / 函数 |
| `%^` `%$` | 开始 / 结束着色 | `%%` | 字面 `%` |

### 运行时格式串

`log`/`trace`/... 使用编译期校验的格式串。运行时格式串请用 `log_runtime`(底层
`std::vformat`)或 `log_raw`(已格式化好的消息)。

## Sinks

| Sink | 说明 |
|---|---|
| `console_sink` | 标准输出/错误输出,TTY 感知的自动 ANSI 着色 |
| `basic_file_sink` | 追加到单个文件(可选截断) |
| `rolling_file_sink` | 按大小滚动,保留 `max_files` 个备份(`rotating_file_sink` 别名) |
| `daily_file_sink` | 每天一个文件,可在指定时间滚动 |
| `null_sink` | 丢弃所有输出(用于基准测试) |
| `json_sink` | 每条记录一个 JSON 对象 |

```cpp
// 单个 sink
auto logger = zest::file_logger("app", "app.log");

// 多个 sink
auto logger = zest::create_logger(
    "multi", zest::stdout_sink(),
    zest::rolling_file_sink("app.log", /*max_size=*/1024 * 1024,
                             /*max_files=*/5));
```

### 自定义 sink

继承 `zest::sinks::sink`,实现 `flush()` 与受保护的 `write_output(std::string_view)`
(结构化输出则重写 `write()`):

```cpp
class my_sink final : public zest::sinks::sink {
public:
  void flush() override {}

protected:
  void write_output(std::string_view line) override {
    std::fwrite(line.data(), 1, line.size(), stdout);
  }
};
```

## 异步日志

```cpp
// 队列大小 + 后端线程数
auto logger =
    zest::create_async_logger("async", /*queue_size=*/65536,
                               /*backend_threads=*/4, zest::basic_file_sink("app.log"));

ZEST_LOGGER_INFO(logger, "这条会在后台线程格式化并写入");
logger->flush();
```

前端只捕获参数、不格式化(常见情况零分配);后端线程负责格式化与写入。后端线程数
大于 1 时,记录不再严格 FIFO 有序。关闭时会排空所有在途记录。

## 回溯(Backtrace)

```cpp
logger->enable_backtrace(32);
// ... 正常记录日志 ...
logger->dump_backtrace();   // 回放最近 32 条记录
logger->disable_backtrace();
```

## JSON 日志

```cpp
auto logger = zest::create_logger("json", zest::json_sink("app.json"));
ZEST_LOGGER_INFO(logger, "user {} logged in", "alice");
// {"time":"2026-08-16T10:30:00.123","level":"info","logger":"json",...}
```

## 构建

```bash
cmake --preset dev            # 配置(Debug + 测试 + 示例 + -Werror)
cmake --build --preset dev
ctest  --preset dev           # 运行测试
```

configure 预设:`dev`、`debug`、`release`、`relwithdebinfo`、`bench`、`ci`、
`asan`、`tsan`、`coverage`。

## 在其他 CMake 项目中使用

```cmake
find_package(zest CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE zest::zest)
```

也可通过 FetchContent / `add_subdirectory` —— `zest::zest` 是一个 INTERFACE
(头文件库)目标。

### mcpp

仓库根目录的 [`mcpp.toml`](mcpp.toml) 为 [mcpp](https://github.com/mcpp-community/mcpp)
构建工具提供了另一个构建入口,把头文件库编译为传统的 C++20 `lib` 目标:

```bash
mcpp build    # 产物 libzest.a 在 target/ 下
```

**首次** `mcpp build` 会先下载一套私有 gcc 工具链(约 170 MB)以及
ninja/patchelf 引导包,才开始编译,可能要几分钟 —— 进度条停在 `connecting…`
是正常的,不是卡死。

> **网络提示(如国内 / GFW 环境):** gcc 工具链本体走 gitcode 国内 CDN,通常没问题;
> 但 `ninja`/`patchelf` 这两个小的引导包来自 **GitHub release 资源**,可能下不下来。
> 如果 `Bootstrap ninja/patchelf` 这一步失败,要么让 mcpp 走代理(在**同一个** shell 里
> export `https_proxy`/`http_proxy`;注意 WSL NAT 模式下 `127.0.0.1:端口` 指的是 WSL 自己,
> 不是 Windows 宿主),要么切镜像:`mcpp self config --mirror CN`,再 `mcpp self init --force`
> 后重试。

zest 是头文件库,使用时直接 `#include <zest/zest.h>`;`mcpp.toml` 入口只是为了用另一套
module-first 工具链验证公共头文件能编译通过。

## 基准测试

自包含的微基准位于 `benchmarks/`:

```bash
cmake --preset bench
cmake --build --preset bench
./build/bench/benchmarks/bench_logger   # sync / filtered / async / async-mt
./build/bench/benchmarks/bench_queue    # MPMC 队列微基准
```

可选的 spdlog 对比(需 `sudo apt install libspdlog-dev`):

```bash
cmake --preset bench -DZEST_BUILD_SPDLOG_BENCH=ON
cmake --build --preset bench
./build/bench/benchmarks/bench_spdlog
```

代表数据(50 万次迭代,Release,null sink,gcc 15):

| 场景 | zest | spdlog |
|---|---|---|
| sync → null | 69 ns/op | 54 ns/op |
| async(1 生产者)→ null | 101 ns/op | 324 ns/op |
| async(4 生产者)→ null | 183 ns/op | 544 ns/op |

## 开发

- **格式化** —— `cmake --build . --target format`(应用)/ `check-format`(校验),使用
  `.clang-format`。
- **静态分析** —— `-DZEST_ENABLE_CLANG_TIDY=ON`(见 `.clang-tidy`)。
- **覆盖率** —— `cmake --preset coverage && cmake --build --preset coverage &&
  ctest --preset coverage`,再用 `lcov` 采集。
- **API 文档** —— `cmake --build . --target docs`(需要 Doxygen)。

`CI` 工作流(手动触发)会运行三平台构建/测试矩阵、sanitizers、fmt 回退,以及
`check-format`/`clang-tidy`/`coverage` 质量门槛。

## 项目结构

```
include/zest/    公共头文件(头文件库)
tests/            单元测试(doctest,通过 CTest 运行)
examples/         小型使用示例
benchmarks/       微基准(std::chrono)
docs/             架构与设计说明
cmake/            CMake 辅助与包配置模板
.github/workflows CI(GitHub Actions,preset 驱动)
```

## 文档

- [`docs/architecture.md`](docs/architecture.md) —— 分层与线程模型。
- [`docs/design-decisions.md`](docs/design-decisions.md) —— 设计决策与非目标。

## 版本规范

本项目遵循[语义化版本](https://semver.org/lang/zh-CN/)。变更记录在
[CHANGELOG.md](CHANGELOG.md);发布打 `vX.Y.Z` tag。

## 许可证

[MIT](LICENSE)
