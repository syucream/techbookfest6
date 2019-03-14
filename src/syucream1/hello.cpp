#include <cerrno>
#include <fuse.h>
#include <string>

using std::string;

constexpr auto STAT_SIZE = sizeof(struct stat);
const auto HELLO_CONTENT = string("Hello, World!");
const auto HELLO_CONTENT_LEN = HELLO_CONTENT.length();
const auto HELLO_PATH = string("/hello");

static int hello_getattr(const char *path, struct stat *stbuf) {
  const auto path_str = string(path);

  memset(stbuf, 0, STAT_SIZE);

  if (path_str == "/") {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
    return 0;
  } else if (path_str == HELLO_PATH) {
    stbuf->st_mode = S_IFREG | 0444;
    stbuf->st_nlink = 1;
    stbuf->st_size = HELLO_CONTENT_LEN;
    return 0;
  } else {
    return -ENOENT;
  }
}

static int hello_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi) {
  const auto path_str = string(path);

  if (path_str != "/") {
    return -ENOENT;
  }

  filler(buf, ".", nullptr, 0);
  filler(buf, "..", nullptr, 0);
  filler(buf, HELLO_PATH.c_str() + 1, nullptr, 0);

  return 0;
}

static int hello_open(const char *path, struct fuse_file_info *fi) {
  const auto path_str = string(path);

  if (path_str != HELLO_PATH) {
    return -ENOENT;
  }

  return 0;
}

static int hello_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
  const auto path_str = string(path);

  if (path_str != HELLO_PATH) {
    return -ENOENT;
  }

  if (offset < HELLO_CONTENT_LEN) {
    if (offset + size > HELLO_CONTENT_LEN) {
      size = HELLO_CONTENT_LEN - offset;
    }
    std::memcpy(buf, HELLO_CONTENT.c_str() + offset, size);
  } else {
    size = 0;
  }

  return size;
}

static struct fuse_operations hello_oper = {.getattr = hello_getattr,
                                            .readdir = hello_readdir,
                                            .open = hello_open,
                                            .read = hello_read};

int main(int argc, char *argv[]) {
  fuse_main(argc, argv, &hello_oper, nullptr);
}
