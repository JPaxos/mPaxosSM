#ifndef NVMHASHMAP_HELPERS_HPP_INCLUDED
#define NVMHASHMAP_HELPERS_HPP_INCLUDED

struct MyHash{
    auto operator()(const std::string & str){
        return std::hash<std::string>()(str);
    }
    auto operator()(const std::string_view & str){
        return std::hash<std::string_view>()(str);
    }
    auto operator()(const pmem::obj::string & str){
        return operator()(std::string_view(str.data(), str.length()));
    }
};

struct MyComparator{
    bool operator()(const pmem::obj::string & a, const std::string& b){
        return std::string_view(a.data(), a.length()) == b;
    }
    bool operator()(const pmem::obj::string & a, const std::string_view& b){
        return std::string_view(a.data(), a.length()) == b;
    }
    bool operator()(const pmem::obj::string & a, const pmem::obj::string & b){
        return a == b;
    }
};

struct MyFactory{
    void operator()(pmem::obj::string & pstr, const std::string& str){
        pstr.assign(str);
    }
    void operator()(pmem::obj::string & pstr, const std::string_view& sv){
        pstr.assign(sv.data(), sv.length());
    }
    std::string_view operator()(const pmem::obj::string& str){
        return std::string_view(str.data(), str.length());
    }
};

#endif // NVMHASHMAP_HELPERS_HPP_INCLUDED
