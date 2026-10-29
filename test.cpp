/*
 * Copyright 10/29/2026 https://github.com/su8/0verau
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */
// make check: the parts that read files and network answers
#undef NDEBUG // the checks are asserts: they must run in every build
#define main player_main
#include "main.cpp"
#undef main
#include <cassert>

int main() {
  namespace fs = std::filesystem;
  // The saved library is read back as it was written
  const fs::path dir = fs::temp_directory_path() / "0verau-test";
  fs::create_directories(dir);
  const std::string file = (dir / "library.tsv").string();
  Track a;
  a.path = "/music/a.mp3";
  a.title = "Caf\xC3\xA9\twith tab";
  a.artist = "Artist";
  a.album = "Album";
  a.duration = "03:05";
  a.size = 123;
  a.mtime = 456;
  assert(saveLibraryCache(file, "/music", {a}));
  auto loaded = loadLibraryCache(file, "/music");
  assert(loaded.size() == 1);
  const Track &b = loaded.at("/music/a.mp3");
  assert(b.title == "Caf\xC3\xA9 with tab" && b.album == "Album" && b.duration == "03:05" && b.size == 123 && b.mtime == 456);
  assert(loadLibraryCache(file, "/other").empty()); // the library of another folder is not used
  fs::remove_all(dir);

  // Lyrics: tags are skipped, a line with two times is there twice, the lines are in time order
  auto lines = parseLrc("[ar:Artist]\n[00:03.00]b\n[00:01.50][00:05.00]a\nno time\n");
  assert(lines.size() == 3);
  assert(lines[0].time == 1.5f && lines[0].text == "a" && lines[1].text == "b" && lines[2].time == 5.f);

  // Picture sizes from the headers of a PNG and a JPEG
  int w = 0, h = 0;
  std::vector<unsigned char> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 0, 0, 0, 13, 'I', 'H', 'D', 'R', 0, 0, 1, 44, 0, 0, 0, 200, 8, 2, 0, 0, 0};
  assert(imageSize(png, w, h) && w == 300 && h == 200);
  std::vector<unsigned char> jpeg = {0xFF, 0xD8, 0xFF, 0xE0, 0, 4, 0, 0, 0xFF, 0xC2, 0, 17, 8, 0, 200, 1, 44, 3, 0, 0, 0};
  assert(imageSize(jpeg, w, h) && w == 300 && h == 200);
  assert(!imageSize({'G', 'I', 'F', '8', '9', 'a', 0, 0, 0, 0}, w, h));

  // Sorting ignores case and keeps equal songs in the order of their files
  std::vector<Track> songs(3);
  songs[0].path = "1"; songs[0].title = "b";
  songs[1].path = "2"; songs[1].title = "A";
  songs[2].path = "3"; songs[2].title = "a";
  sortTracks(songs, 1);
  assert(songs[0].path == "2" && songs[1].path == "3" && songs[2].path == "1");

  std::cout << "All tests passed." << std::endl;
  return EXIT_SUCCESS;
}
