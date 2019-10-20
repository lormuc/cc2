#include <cassert>
#include <istream>
#include <experimental/filesystem>

#include "file.hpp"

namespace fs = std::experimental::filesystem;

size_t t_file_manager::read_file(const str& abs_path, const str& rel_path) {
    _ is = std::ifstream();
    is.exceptions(std::ifstream::badbit);
    is.open(abs_path);

    is.seekg(0, std::ios::end);
    _ size = is.tellg();
    _ file_contents = str(size, ' ');
    is.seekg(0);
    is.read(file_contents.data(), size);

    files.push_back({abs_path, rel_path, std::move(file_contents)});
    return files.size() - 1;
}

size_t t_file_manager::read_file(const str& rel_path) {
    return read_file(fs::canonical(rel_path), rel_path);
}

const str& t_file_manager::get_file_contents(size_t idx) const {
    assert(idx < files.size());
    return files[idx].file_contents;
}

const str& t_file_manager::get_path(size_t idx) const {
    assert(idx < files.size());
    return files[idx].path;
}

const str& t_file_manager::get_abs_path(size_t idx) const {
    assert(idx < files.size());
    return files[idx].abs_path;
}

void t_file_manager::clear() {
    files.clear();
}

str get_file_dir(const str& abs_path) {
    return fs::path(abs_path).parent_path().string();
}
