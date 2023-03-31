#if defined(__EMSCRIPTEN__)
#include "../include/zlib.h"
#include <emscripten.h>
#else
#include <zlib.h>
#endif
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <iterator>
#include <random>
#include <stdexcept>
#include <type_traits>
#include <vector>

// static_assert(sizeof(unsigned char) == sizeof(std::byte), "Unsgined char and
// std::bye are not equal in size");

static constexpr size_t DATA_SIZE = 16 * 1024 * 1024;
static constexpr size_t BUFFER_SIZE = 16'384; // 16 * 1024

constexpr auto randChar = []() -> char {
  constexpr char charset[] = "0123456789"
                             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                             "abcdefghijklmnopqrstuvwxyz";
  constexpr size_t max_index = (sizeof(charset) - 1);
  return charset[rand() % max_index];
};

void *custom_malloc(voidpf opaque, uInt items, uInt size) {

  auto add = malloc(items * size);
  std::cout << "zalloc : " << add << " = " << items * size << std::endl;
  return add;
}

void custom_free(voidpf opaque, voidpf address) {
  std::cout << "zfree : " << address << std::endl;
  return free(address);
}

int InitDeflateZStream(z_stream &strm, int level) {
  strm.zalloc = custom_malloc;
  strm.zfree = custom_free;
  strm.opaque = Z_NULL;

  int ret = deflateInit(&strm, level);

  if (ret != Z_OK)
    throw std::runtime_error("'deflateInit' failed!");

  return ret;
}

int InitInflateZStream(z_stream &strm) {
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;

  int ret = inflateInit(&strm);

  if (ret != Z_OK)
    throw std::runtime_error("'inflateInit' failed!");

  return ret;
}

template <typename T>
std::vector<unsigned char> Deflate(const std::vector<T> &source,
                                   int level = -1) {

  int ret, flush;
  z_stream strm;
  ret = InitDeflateZStream(strm, level);
  const std::size_t src_size = source.size() * sizeof(T);
  std::size_t out_buffer_size = src_size / 3 + 16;
  std::vector<unsigned char> out_buffer(out_buffer_size, {});

  strm.avail_in = src_size;
  strm.next_in = reinterpret_cast<unsigned char *>(
      const_cast<std::remove_const_t<T> *>(source.data()));
  strm.avail_out = out_buffer.size();
  strm.next_out = out_buffer.data();

  do {

    if (strm.avail_out == 0) {
      const std::size_t extension_size = src_size / 3 + 16;
      strm.avail_out = extension_size;
      out_buffer.resize(out_buffer_size + extension_size, {});
      strm.next_out = std::next(out_buffer.data(), out_buffer_size);
      out_buffer_size += extension_size;
    }

    ret = deflate(&strm, Z_FINISH);

    if (ret == Z_STREAM_ERROR)
      throw std::runtime_error("Zlib Stream Error!");
  } while (ret != Z_STREAM_END);

  deflateEnd(&strm);
  out_buffer.resize(out_buffer_size - strm.avail_out);

  return out_buffer;
}

template <typename T>
std::vector<T> Inflate(const std::vector<unsigned char> &source) {

  int ret, flush;
  z_stream strm;
  ret = InitInflateZStream(strm);
  const std::size_t src_size = source.size();
  std::size_t out_buffer_size = src_size / 3 + 16;
  std::vector<unsigned char> out_buffer(out_buffer_size, {});

  strm.avail_in = src_size;
  strm.next_in = const_cast<unsigned char *>(source.data());
  strm.avail_out = out_buffer.size();
  strm.next_out = out_buffer.data();

  do {

    if (strm.avail_out == 0) {
      const std::size_t extension_size = src_size / 3 + 16;
      strm.avail_out = extension_size;
      out_buffer.resize(out_buffer_size + extension_size, {});
      strm.next_out = std::next(out_buffer.data(), out_buffer_size);
      out_buffer_size += extension_size;
    }

    ret = inflate(&strm, Z_FINISH);

    if (ret == Z_STREAM_ERROR)
      throw std::runtime_error("Zlib Stream Error!");
  } while (ret != Z_STREAM_END);

  inflateEnd(&strm);
  out_buffer_size -= strm.avail_out;

  std::vector<T> ret_buffer(reinterpret_cast<T *>(out_buffer.data()),
                            std::next(reinterpret_cast<T *>(out_buffer.data()),
                                      (out_buffer_size / sizeof(T))));

  return ret_buffer;
}

boolean test() {
  std::vector<char> data(DATA_SIZE, {});
  std::generate_n(std::begin(data), DATA_SIZE, randChar);

  std::cout << "Compressing Buffer of size : " << DATA_SIZE << "B" << std::endl;
  const auto compressed_buffer = Deflate(data, 6);

  std::cout << "Decompressing Buffer of size : " << compressed_buffer.size()
            << "B" << std::endl;
  const auto decompressed_buffer = Inflate<char>(compressed_buffer);

  auto comp_res = data == decompressed_buffer;

  std::cout << (comp_res ? "Success" : "Fail") << std::endl;

  return comp_res;
}

int main() {

  test();

  return 0;
}

// g++ src/module.cpp -lz -o module && ./module