
#ifndef ___IO_UTIL_HPP
#define ___IO_UTIL_HPP

#include <cstdint>
#include <string>
#include <functional>

#if defined _NO_CPP17_FS
#include "ghc/filesystem.hpp"
namespace fs
{
using namespace ghc::filesystem;
using ifstream = ghc::filesystem::ifstream;
using ofstream = ghc::filesystem::ofstream;
using fstream = ghc::filesystem::fstream;
} // namespace fs
#else

#include <experimental/filesystem>
//#include <filesystem>
namespace fs
{
using namespace std::experimental::filesystem;
//using namespace std::filesystem;
using ifstream = std::ifstream;
using ofstream = std::ofstream;
using fstream = std::fstream;
} // namespace fs
#endif

namespace utl
{

typedef struct
{
    std::u32string name;
    bool is_dir;
    uint64_t file_len = 0;
} path_info;

inline bool list_dir(const std::u32string &_pathName, std::function<bool(const path_info &_pathInfo)> _fun, const std::u32string &_extName = U"*.*")
{
    fs::path pRoot(_pathName);
    if (/*!fs::exists(pRoot)||*/ !fs::is_directory(pRoot))
        return false;
    fs::directory_iterator endIter;
    for (fs::directory_iterator dirItr(pRoot); dirItr != endIter; ++dirItr)
    {
        path_info ret;
        ret.name = dirItr->path().u32string();
        ret.is_dir = fs::is_directory(*dirItr);
        //std::u32string child = /*path_ + L"\\" + */dirItr->path().u32string();
        if (!ret.is_dir)
        {
            //fs::ifstream ifs(child, std::ios::binary);
            //ifs.seekg(0, std::ios::end);
            //fsize = ifs.tellg();
            ret.file_len = fs::file_size(dirItr->path());
        }

        if (!_fun(ret))
            return false;
    }
    return true;
}

inline bool list_dir(const std::u32string &root, std::vector<path_info> &dest, bool include_dir = true, const std::u32string &extName = U"*.*")
{
    std::function<bool(const path_info &_pathInfo)> fun = [&](const path_info &_pathInfo) -> bool {
        if (!include_dir && _pathInfo.is_dir)
            return 1;
        dest.push_back(_pathInfo);
        return 1;
    };
    return list_dir(root, fun, extName);
}

} // namespace utl

#endif
