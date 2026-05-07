#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace monggle {

struct ProfileRow {
    std::int64_t id;
    std::string  email;
    std::string  displayName;
    std::string  avatarPath;   // 빈 문자열이면 미설정
};

struct ProfileError {
    enum Code { NotFound, BadRequest, UnsupportedType, InternalError };
    Code        code;
    std::string detail;
};

template <typename T>
using PResult = std::variant<T, ProfileError>;

class ProfileService {
public:
    explicit ProfileService(std::string storageRoot);

    std::optional<ProfileRow> get(std::int64_t userId);

    // 업로드된 바이트를 OpenCV로 정사각형 크롭 + 256x256 JPG로 저장.
    // 성공 시 디스크 저장 경로(상대) 반환.
    PResult<std::string> updateAvatar(std::int64_t userId,
                                       const std::string& mime,
                                       const char* bytes,
                                       std::size_t size);

    // GET /users/{id}/avatar 용 절대 디스크 경로.
    std::optional<std::string> avatarFile(std::int64_t userId);

private:
    std::string storageRoot_;
};

} // namespace monggle
