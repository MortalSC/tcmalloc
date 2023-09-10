# 高并发内存池的设计与实现（Mini TCmalloc）


## 项目背景简介
- TCMalloc 是 Google 对 C 的 malloc() 和 C++ 的 operator new 的自定义实现，用于在 C 和 C++ 代码中进行内存分配。它是一种快速的多线程 malloc 实现。

- 本项目实现的是一个高并发的内存池，它的原型是 Google 的一个开源项目tcmalloc，tcmalloc全称Thread-Caching Malloc，即线程缓存的malloc，实现了高效的多线程内存管理，用于替换系统的内存分配相关函数malloc和free。
- Goolge开源地址：https://gitee.com/mirrors/tcmalloc


>---

## 项目（三层）架构说明

-- ThreadCache 
-- CentralCache
-- PageCache
