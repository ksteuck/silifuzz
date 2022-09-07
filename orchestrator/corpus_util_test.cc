// Copyright 2022 The SiliFuzz Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "./orchestrator/corpus_util.h"

#include <fcntl.h>
#include <stddef.h>
#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "./util/byte_io.h"
#include "./util/owned_file_descriptor.h"
#include "./util/testing/status_matchers.h"

namespace silifuzz {
namespace {

using silifuzz::testing::IsOk;
using silifuzz::testing::IsOkAndHolds;
using silifuzz::testing::StatusIs;
using ::testing::HasSubstr;

TEST(CorpusUtil, ReadXZipFile) {
  // What we expect to get from decompression.
  const std::string kContents = "Hello World.\n";

  // Test data generated by echo "Hello World." | xz -9 - | xxd -i
  constexpr uint8_t kCompressedContents[] = {
      0xfd, 0x37, 0x7a, 0x58, 0x5a, 0x00, 0x00, 0x04, 0xe6, 0xd6, 0xb4, 0x46,
      0x02, 0x00, 0x21, 0x01, 0x1c, 0x00, 0x00, 0x00, 0x10, 0xcf, 0x58, 0xcc,
      0x01, 0x00, 0x0c, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x57, 0x6f, 0x72,
      0x6c, 0x64, 0x2e, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x60, 0x4f, 0x3b, 0x6e,
      0x3d, 0xe2, 0xf0, 0x92, 0x00, 0x01, 0x25, 0x0d, 0x71, 0x19, 0xc4, 0xb6,
      0x1f, 0xb6, 0xf3, 0x7d, 0x01, 0x00, 0x00, 0x00, 0x00, 0x04, 0x59, 0x5a,
  };

  const std::string temp_filename =
      absl::StrCat(std::string_view{getenv("TEST_TMPDIR")}, "/ReadXZipFileTest.xz");
  const int temp_fd = open(temp_filename.c_str(), O_CREAT | O_WRONLY | O_TRUNC,
                           S_IRUSR | S_IWUSR);
  ASSERT_GE(temp_fd, 0);

  const ssize_t bytes_written =
      Write(temp_fd, kCompressedContents, sizeof(kCompressedContents));
  ASSERT_EQ(bytes_written, sizeof(kCompressedContents));
  close(temp_fd);

  absl::StatusOr<absl::Cord> cord_or = ReadXzipFile(temp_filename);
  EXPECT_THAT(cord_or, IsOkAndHolds(kContents));

  // Truncate file and read it again.
  struct stat stat_buffer;
  ASSERT_EQ(stat(temp_filename.c_str(), &stat_buffer), 0);
  ASSERT_EQ(truncate(temp_filename.c_str(), stat_buffer.st_size / 2), 0);

  cord_or = ReadXzipFile(temp_filename);
  EXPECT_THAT(cord_or, StatusIs(absl::StatusCode::kInternal,
                                HasSubstr("Failed to decompress")));

  // Invalid filename.
  cord_or = ReadXzipFile("/this does not exist");
  EXPECT_THAT(cord_or, StatusIs(absl::StatusCode::kInternal,
                                HasSubstr("Failed to open")));
}

TEST(CorpusUtil, WriteSharedMemoryFile) {
  std::string big_string_1(1 << 20, 0);
  for (size_t i = 0; i < big_string_1.size(); ++i) {
    big_string_1.data()[i] = static_cast<char>(i);
  }
  const std::string big_string_2(1 << 20, 42);

  absl::Cord contents("This is a test.");
  contents.Append(big_string_1);
  contents.Append(big_string_2);

  auto handle_or = WriteSharedMemoryFile(contents);
  ASSERT_THAT(handle_or, IsOk());
  OwnedFileDescriptor owned_fd = std::move(handle_or.value());
  struct stat stat_buf;
  ASSERT_EQ(fstat(*owned_fd, &stat_buf), 0);
  EXPECT_EQ(stat_buf.st_size, contents.size());

  // Check that file is correctly sealed.
  int seals = fcntl(*owned_fd, F_GET_SEALS);
  EXPECT_NE(seals, -1);
  constexpr int kExpectedSeals =
      F_SEAL_SEAL | F_SEAL_WRITE | F_SEAL_SHRINK | F_SEAL_GROW;
  EXPECT_EQ(seals & kExpectedSeals, kExpectedSeals);

  EXPECT_EQ(ftruncate(*owned_fd, 0), -1);
  EXPECT_EQ(ftruncate(*owned_fd, stat_buf.st_size + 1), -1);

  // Check that contents are correctly written.
  std::string buffer(stat_buf.st_size, 0);
  EXPECT_EQ(lseek(*owned_fd, 0, SEEK_SET), 0);
  EXPECT_EQ(Read(*owned_fd, buffer.data(), buffer.size()), buffer.size());
  EXPECT_EQ(buffer, contents);
}

TEST(CorpusUtil, LoadCorpora) {
  constexpr size_t kCorporaSize = 3;
  const std::array<std::string, kCorporaSize> corpus_contents{"one\n", "two\n",
                                                              "three\n"};

  // generated by echo <corpus_contents[i]> | xz -9 - | xxd -i
  using Bytes = std::vector<uint8_t>;
  const std::array<Bytes, kCorporaSize> compressed_corpus_contents{
      Bytes{0xfd, 0x37, 0x7a, 0x58, 0x5a, 0x00, 0x00, 0x04, 0xe6, 0xd6,
            0xb4, 0x46, 0x02, 0x00, 0x21, 0x01, 0x1c, 0x00, 0x00, 0x00,
            0x10, 0xcf, 0x58, 0xcc, 0x01, 0x00, 0x03, 0x6f, 0x6e, 0x65,
            0x0a, 0x00, 0xf5, 0x5c, 0xbb, 0x56, 0x1f, 0xc6, 0xfd, 0x86,
            0x00, 0x01, 0x1c, 0x04, 0x6f, 0x2c, 0x9c, 0xc1, 0x1f, 0xb6,
            0xf3, 0x7d, 0x01, 0x00, 0x00, 0x00, 0x00, 0x04, 0x59, 0x5a},
      Bytes{0xfd, 0x37, 0x7a, 0x58, 0x5a, 0x00, 0x00, 0x04, 0xe6, 0xd6,
            0xb4, 0x46, 0x02, 0x00, 0x21, 0x01, 0x1c, 0x00, 0x00, 0x00,
            0x10, 0xcf, 0x58, 0xcc, 0x01, 0x00, 0x03, 0x74, 0x77, 0x6f,
            0x0a, 0x00, 0x65, 0x06, 0xd6, 0xff, 0xc4, 0xc0, 0xf9, 0x3a,
            0x00, 0x01, 0x1c, 0x04, 0x6f, 0x2c, 0x9c, 0xc1, 0x1f, 0xb6,
            0xf3, 0x7d, 0x01, 0x00, 0x00, 0x00, 0x00, 0x04, 0x59, 0x5a},
      Bytes{0xfd, 0x37, 0x7a, 0x58, 0x5a, 0x00, 0x00, 0x04, 0xe6, 0xd6, 0xb4,
            0x46, 0x02, 0x00, 0x21, 0x01, 0x1c, 0x00, 0x00, 0x00, 0x10, 0xcf,
            0x58, 0xcc, 0x01, 0x00, 0x05, 0x74, 0x68, 0x72, 0x65, 0x65, 0x0a,
            0x00, 0x00, 0x00, 0x2c, 0xe7, 0xe1, 0xb2, 0x82, 0xb1, 0x93, 0x65,
            0x00, 0x01, 0x1e, 0x06, 0xc1, 0x2f, 0xa4, 0x1d, 0x1f, 0xb6, 0xf3,
            0x7d, 0x01, 0x00, 0x00, 0x00, 0x00, 0x04, 0x59, 0x5a},
  };

  std::vector<std::string> corpus_paths;
  for (size_t i = 0; i < kCorporaSize; ++i) {
    corpus_paths.push_back(absl::StrCat(std::string_view{getenv("TEST_TMPDIR")},
                                        "/LoadCoporaTest_", i, ".xz"));
    const int fd = open(corpus_paths[i].c_str(), O_CREAT | O_TRUNC | O_WRONLY,
                        S_IRUSR | S_IWUSR);
    ASSERT_NE(fd, -1);
    const size_t compressed_size = compressed_corpus_contents[i].size();
    const char* compressed_data =
        reinterpret_cast<const char*>(compressed_corpus_contents[i].data());
    ASSERT_EQ(Write(fd, compressed_data, compressed_size), compressed_size);
    ASSERT_EQ(close(fd), 0);
  }

  absl::StatusOr<LoadCorporaResult> load_corpora_result_or =
      LoadCorpora(corpus_paths);
  ASSERT_THAT(load_corpora_result_or, IsOk());

  const std::vector<OwnedFileDescriptor>& owned_fds =
      load_corpora_result_or.value().file_descriptors;
  const std::vector<std::string>& fd_paths =
      load_corpora_result_or.value().file_descriptor_paths;

  EXPECT_EQ(owned_fds.size(), kCorporaSize);
  EXPECT_EQ(fd_paths.size(), kCorporaSize);

  auto check_file_contents = [](int fd, const std::string& expected_contents) {
    struct stat stat_buf {};
    EXPECT_EQ(fstat(fd, &stat_buf), 0);
    EXPECT_EQ(stat_buf.st_size, expected_contents.size());
    std::string buffer(stat_buf.st_size, 0);
    ASSERT_EQ(Read(fd, buffer.data(), buffer.size()), buffer.size());
    EXPECT_EQ(buffer, expected_contents);
  };

  for (size_t i = 0; i < kCorporaSize; ++i) {
    const int fd = *(owned_fds[i].get());
    check_file_contents(fd, corpus_contents[i]);

    // Check again using file descriptor path.
    const int fd2 = open(fd_paths[i].c_str(), O_RDONLY);
    ASSERT_GE(fd2, 0);
    check_file_contents(fd2, corpus_contents[i]);
    EXPECT_EQ(close(fd2), 0);
  }
}

TEST(CorpusUtil, LoadCorporaFileNotFound) {
  std::vector<std::string> corpus_paths{"This does not exist.xz"};
  absl::StatusOr<LoadCorporaResult> load_corpora_result_or =
      LoadCorpora(corpus_paths);
  EXPECT_THAT(
      load_corpora_result_or.status(),
      StatusIs(absl::StatusCode::kInternal, HasSubstr("Failed to open")));
}

}  // namespace
}  // namespace silifuzz
