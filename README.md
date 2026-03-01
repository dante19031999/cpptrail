[![Documentation]()

# CppTrail: High-Performance C++20 Logging 🏔️

![CppTrail Logo](resources/icon.svg)

CppTrail is a lightweight, **header-only** logging framework built for the modern C++20 era.
It is specifically optimized for high-throughput environments—like **wire stuff** network protocol analysis—where low latency and thread safety are non-negotiable.

---

## 🏗️ Architecture

The library is built on a **handle-implementation (pImpl)** pattern. This ensures:

- **Thread Safety**: All backends use internal mutexes or producer-consumer queues.
- **Resource Efficiency**: An internal **Object Pool** recycles log entries to minimize heap pressure during high-frequency captures.
- **Non-blocking I/O**: Asynchronous loggers offload formatting and writing to background worker threads.

---

## 🕒 RFC 3339 Timestamps

CppTrail features a high-performance ISO 8601/RFC 3339 formatter:
- **Zulu Time**: `2026-03-01 15:17:34.672Z`
- **Local Time**: `2026-03-01 15:17:34.672+01:00`

> [!NOTE]
> This feature requires **C++20** `<chrono>` support. On older standards, timestamps are gracefully omitted.

---

## 📦 Integration (FetchContent)

Add the following to your `CMakeLists.txt` to integrate CppTrail seamlessly:

```cmake
include(FetchContent)
FetchContent_Declare(
    cpptrail
    GIT_REPOSITORY [https://github.com/DanteDomenech/cpptrail.git](https://github.com/DanteDomenech/cpptrail.git)
    GIT_TAG main
)
FetchContent_MakeAvailable(cpptrail)

# Link to the interface library
target_link_libraries(my_app PRIVATE CppTrail::cpptrail)
```

---

## 🚀 Quick Start

### Asynchronous Console Logging

```
#include "cpptrail/ostream_logger.h"

int main() {
    // Instantiate logger
    CppTrail::CoutAsyncLogger log;

    // Start logger
    log.start();

    // Log what you wish
    log.log(CppTrail::Level::INFO, "Monitoring the wire...");

    // Log an exception directly
    try {
        // ... network logic ...
    } catch (const std::exception& e) {
        log.log(e);
    }

    // Stop the logger (flushes everything before stopping)
    log.stop(); 

    return 0;
}
```

---

## 🎓 Learning More

To understand how to extend the library for your own needs, check out our internal documentation:

- **How to create custom loggers**: See our [Custom Loggers Tutorial](docs/custom_loggers.md) for a deep dive into implementing your own synchronous and asynchronous sinks.

---

## 👤 About us

**Dante Doménech Martínez** Main Developer & Maintainer

### 📜 License

This project is licensed under the **GPL-3 License**. See the `LICENSE` file for details.

