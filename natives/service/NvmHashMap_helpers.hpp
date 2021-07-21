#pragma once

#include <string>
#include <string_view>
#include <libpmemobj++/container/string.hpp>

struct MyHash{
    auto operator()(const std::string & str) const {
        return std::hash<std::string>()(str);
    }
    auto operator()(const std::string_view & str) const {
        return std::hash<std::string_view>()(str);
    }
    auto operator()(const pmem::obj::string & str) const {
        return operator()(std::string_view(str.data(), str.length()));
    }
};

struct MyComparator{
    bool operator()(const pmem::obj::string & a, const std::string& b) const {
        return std::string_view(a.data(), a.length()) == b;
    }
    bool operator()(const pmem::obj::string & a, const std::string_view& b) const {
        return std::string_view(a.data(), a.length()) == b;
    }
    bool operator()(const pmem::obj::string & a, const pmem::obj::string & b) const {
        return a == b;
    }
};

struct MyFactory{
    void operator()(pmem::obj::string & pstr, const std::string& str) const {
        pstr.assign(str);
    }
    void operator()(pmem::obj::string & pstr, const std::string_view& sv) const {
        pstr.assign(sv.data(), sv.length());
    }
    std::string_view operator()(const pmem::obj::string& str) const {
        return std::string_view(str.data(), str.length());
    }
};
