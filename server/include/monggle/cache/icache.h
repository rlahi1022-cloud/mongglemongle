#pragma once

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>

namespace monggle {

// L1(메모리)/L2(Redis) 공통 인터페이스. 둘 다 동일 시그니처를 가지므로
// LayeredCache가 두 백엔드를 같은 호출로 다룰 수 있다.
class ICache {
public:
    virtual ~ICache() = default;

    virtual std::optional<std::string> get(const std::string& key) = 0;
    virtual void set(const std::string& key, std::string value, std::chrono::seconds ttl) = 0;
    virtual void invalidate(const std::string& key) = 0;
    // prefix scan은 L1은 O(n) 순회, L2는 SCAN+UNLINK. 호출자가 빈도 인지하고 사용해야 한다.
    virtual std::size_t invalidatePrefix(const std::string& prefix) = 0;

    // L2가 살아있는지 확인용. L1은 항상 true.
    virtual bool healthy() const { return true; }
};

} // namespace monggle
