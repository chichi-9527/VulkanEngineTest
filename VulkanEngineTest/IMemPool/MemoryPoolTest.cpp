#include "MemoryPoolTest.h"
#include "IMemPool.h"
#include <iostream>
#include <cassert>
#include <thread>
#include <vector>

// 辅助测试类 1：观察小内存和析构
struct SmallObject {
    int data[2]; // 8 字节
    static int destructor_count;
    ~SmallObject() { destructor_count++; }
};
int SmallObject::destructor_count = 0;

// 辅助测试类 2：测试大内存分配（触发通用池）
struct BigObject {
    double data[1000]; // 8000 字节 > 1024
};

// 辅助测试类 3：测试大内存分配（触发通用池）
struct BigObject2 {
    double data[1000]; // 8000 字节 > 1024
    std::vector<int> array;
};

// 1. 测试固定大小内存池（小对象）的分配与释放
void TestFixedSizePool()
{
    std::cout << "[Test] Running TestFixedSizePool..." << std::endl;
    IMemPool::MemClassCounts counts;
    counts.BlockCountByte8 = 10; // 设置较小的初始块数以便测试边界
    IMemPool* pool = IMemPool::CreatePool(counts);
    assert(pool != nullptr);

    SmallObject::destructor_count = 0;

    // 连续分配，验证是否会因为索引初始化错误而崩溃
    std::vector<IUserPtr<SmallObject>> ptrs;
    for (int i = 0; i < 7; ++i)
    {
        auto p = pool->New<SmallObject>(1);
        assert(p);
        ptrs.push_back(p);
    }

    // 验证释放
    for (auto& p : ptrs)
    {
        pool->Delete(p, 1);
        assert(!p); // Delete 后指针应该被置空
    }

    assert(SmallObject::destructor_count == 7);
    IMemPool::DestroyPool(pool);
    std::cout << "[Test] TestFixedSizePool Passed!" << std::endl;
}

// 2. 测试通用内存池扩容与断言（大对象）
void TestGeneralPoolExpansion()
{
    std::cout << "[Test] Running TestGeneralPoolExpansion..." << std::endl;
    IMemPool::MemClassCounts counts;
    counts.BlockCountGeneral = 4; // 只有4个4KB块
    IMemPool* pool = IMemPool::CreatePool(counts);

    // 分配一个需要3个块的大对象
    auto p1 = pool->New<BigObject>(1); // sizeof(BigObject) = 8000 字节 -> 需要 2 个 4KB 块
    assert(p1);

    // 再分配一个需要3个块的大对象，此时必然触发自动扩容
    // 如果没有修复扩容中的 assert(hdr->FreeBlockCount < need_blocks)，这里在Debug下会直接崩
    auto p2 = pool->New<BigObject>(1);
    assert(p2);

    // 验证大内存句柄解析是否正常
    assert(p1.get() != nullptr);
    assert(p2.get() != nullptr);

    pool->Delete(p1, 1);
    pool->Delete(p2, 1);

    IMemPool::DestroyPool(pool);
    std::cout << "[Test] TestGeneralPoolExpansion Passed!" << std::endl;
}

// 3. 测试碎片整理（Defragmentation）后句柄是否依然有效
void TestDefragmentation()
{
    std::cout << "[Test] Running TestDefragmentation..." << std::endl;
    IMemPool::MemClassCounts counts;
    counts.BlockCountGeneral = 10;
    IMemPool* pool = IMemPool::CreatePool(counts);

    // 交替分配大内存以制造空隙
    auto p1 = pool->Allocate<char>(4096);  // 块 0
    auto p2 = pool->Allocate<char>(4096);  // 块 1
    auto p3 = pool->Allocate<char>(4096);  // 块 2

    // 写入标志数据
    *p1.get() = 'A';
    *p2.get() = 'B';
    *p3.get() = 'C';

    // 释放中间的 p2，制造内存空洞
    pool->Deallocate(p2, 4096);

    // 显式触发碎片整理
    pool->DefragmentGeneralPool();

    // 核心断言：由于 IUserPtr 内部是通过 Node 索引解析的
    // 即使 p3 的实际物理内存地址在碎片整理中被 memmove 移动了，它依然可以通过句柄正确访问！
    assert(*p1.get() == 'A');
    assert(*p3.get() == 'C');

    pool->Deallocate(p1, 4096);
    pool->Deallocate(p3, 4096);

    IMemPool::DestroyPool(pool);
    std::cout << "[Test] TestDefragmentation Passed!" << std::endl;
}

// 3. 测试碎片整理（Defragmentation）后句柄是否依然有效
void TestDefragmentation2()
{
    std::cout << "[Test] Running TestDefragmentation2..." << std::endl;
    IMemPool::MemClassCounts counts;
    counts.BlockCountGeneral = 10;
    IMemPool* pool = IMemPool::CreatePool(counts);

    // 交替分配大内存以制造空隙
    auto p1 = pool->New<BigObject2>(1);  // 块 0
    auto p2 = pool->New<BigObject2>(1);  // 块 1
    auto p3 = pool->New<BigObject2>(1);  // 块 2

    // 写入标志数据
    p1.get()->array = { 1,2,3 };
    p2.get()->array = { 1,2,3 };
    p3.get()->array = { 4,5,6 };

    // 释放中间的 p2，制造内存空洞
    pool->Deallocate(p2, 4096);

    // 显式触发碎片整理
    pool->DefragmentGeneralPool();

    // 核心断言：由于 IUserPtr 内部是通过 Node 索引解析的
    // 即使 p3 的实际物理内存地址在碎片整理中被 memmove 移动了，它依然可以通过句柄正确访问！
    assert(p1.get()->array[0] == 1);
    assert(p3.get()->array[1] == 5);

    pool->Deallocate(p1, 4096);
    pool->Deallocate(p3, 4096);

    IMemPool::DestroyPool(pool);
    std::cout << "[Test] TestDefragmentation2 Passed!" << std::endl;
}

// 4. 多线程并发分配安全性测试
void TestMultiThreading()
{
    std::cout << "[Test] Running TestMultiThreading..." << std::endl;
    IMemPool* pool = IMemPool::CreatePool();

    auto worker = [](IMemPool* pool) {
        for (int i = 0; i < 1000; ++i)
        {
            auto p = pool->Allocate<int>(1);
            if (p)
            {
                *p.get() = 42;
                pool->Deallocate(p, 1);
            }
        }
        };

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i)
    {
        threads.emplace_back(worker, pool);
    }

    for (auto& t : threads)
    {
        t.join();
    }

    IMemPool::DestroyPool(pool);
    std::cout << "[Test] TestMultiThreading Passed!" << std::endl;
}

int MemoryPoolTest()
{
    std::cout << "=== Starting IMemPool Verification ===" << std::endl;

    TestFixedSizePool();
    TestGeneralPoolExpansion();
    TestDefragmentation();
    TestDefragmentation2();
    TestMultiThreading();

    std::cout << "=== All Tests Passed Successfully! ===" << std::endl;
    return 0;
}