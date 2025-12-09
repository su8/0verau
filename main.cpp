/*
 * Copyright 12/07/2025 https://github.com/su8/0verau
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
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdlib>
#include <unordered_map>
#include <sstream>
#include <SFML/Audio.hpp>
#include <ncurses.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <mpg123.h>

struct Track {
  std::string path;
  std::string name;
  std::string title;
  std::string artist;
  std::string album;
  std::string duration;
};

// Draw function tracks and status lines
void drawStatus(int currentTrack, int rows, int cols, std::vector<Track> playlist, int highlight, int colorPair, std::string status, int offset, bool shuffle, bool repeat, float volume, std::string &searchQuery, std::unordered_map<std::string, int> keys, int showHideAlbum, int showHideArtist);
// Filter playlist by search term
std::vector<Track> filterTracks(const std::vector<Track> &tracks, const std::string &term);
// List audio files in directory
std::vector<Track> listAudioFiles(const std::string &path);
// Function to read metadata using TagLib
Track readMetadata(const std::filesystem::path &filePath);
// Format seconds into mm:ss
std::string formatTime(float duration);
// Draw progress bar with time
void drawProgressBarWithTime(float elapsed, float total, int width, int y, int x);

// Saving and loading music files upon start/end
//void savePlaylistState(const std::vector<Track> &playlist, const std::string &searchTerm, bool shuffle);
//bool loadPlaylistState(std::vector<Track> &playlist, std::string &searchTerm, bool &shuffle);

// Convert string to key code
int keyFromString(const std::string &val);
// Load key bindings from config file
std::unordered_map<std::string, int> loadKeyBindings(const std::string &configPath);

int main(int argc, char *argv[]) {
  if (argc < 2) { std::cerr << "You must provide some folder with music in it." << std::endl; return EXIT_FAILURE; }
  std::string musicDir = argv[1]; // Change to your music folder
  auto playlist = listAudioFiles(musicDir);
  if (playlist.empty()) { std::cerr << "No audio files found in " << musicDir << "\n"; return EXIT_FAILURE; }

  // Load key bindings from config file
  auto keys = loadKeyBindings((getenv("HOME") ? static_cast<std::string>(getenv("HOME")) : static_cast<std::string>(".")) + static_cast<std::string>("/0verau.conf"));

  // ncurses setup
  initscr();
  start_color();
  use_default_colors();
  init_pair(1, COLOR_GREEN, -1); // Playing
  init_pair(2, COLOR_YELLOW, -1); // Paused
  init_pair(3, COLOR_RED, -1); // Stopped
  noecho();
  cbreak();
  keypad(stdscr, TRUE);
  curs_set(0);
  timeout(200); // Non-blocking getch

  int highlight = 0;
  int offset = 0;
  int choice;
  bool shuffle = false;
  bool repeat = false;
  bool running = true;
  int showHideAlbum = 0;
  int showHideArtist = 0;
  float volume = 100.f;
  std::string searchQuery;

  sf::Music music;
  int currentTrack = -1;

  // Try to restore previous session
  //loadPlaylistState(playlist, searchQuery, shuffle);

  while (running) {
    //clear();
    werase(stdscr);
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    // Now Playing section
    std::string status;
    int colorPair = 3; // Default: stopped
    if (music.getStatus() == sf::Music::Playing) {
      status = "Playing";
      colorPair = 1;
    } else if (music.getStatus() == sf::Music::Paused) {
      status = "Paused";
      colorPair = 2;
    } else {
      status = "Stopped";
      colorPair = 3;
    }
    drawStatus(currentTrack, rows, cols, playlist, highlight, colorPair, status, offset, shuffle, repeat, volume, searchQuery, keys, showHideAlbum, showHideArtist);
    // Show progress bar if playing
    if (music.getStatus() != sf::Music::Stopped) {
      float elapsed = music.getPlayingOffset().asSeconds();
      float total = music.getDuration().asSeconds();
      drawProgressBarWithTime(elapsed, total, cols - 20, rows - 2, 0);
    }
    wrefresh(stdscr);
    choice = getch();
    if (choice == keys["UP"]) {
      highlight = (highlight - 1 + playlist.size()) % playlist.size();;
    }
    else if (choice == keys["DOWN"]) {
      highlight = (highlight + 1) % playlist.size();
    }
    else if (choice == keys["PLAY"]) {
      if (!music.openFromFile(playlist[highlight].path)) {
        mvprintw(rows - 1, 0, "Error: Cannot play file.");
      } else {
        music.setVolume(volume);
        music.play();
        currentTrack = highlight;
      }
    }
    else if (choice == keys["PAUSE"]) {
      if (music.getStatus() == sf::Music::Playing) music.pause();
      else if (music.getStatus() == sf::Music::Paused) music.play();
    }
    else if (choice == keys["VOLUMEUP"]) {
      volume = std::min(100.f, volume + 5.f);
      music.setVolume(volume);
    }
    else if (choice == keys["VOLUMEDOWN"]) {
      volume = std::max(0.f, volume - 5.f);
      music.setVolume(volume);
    }
    else if (choice == keys["SHUFFLE"]) {
      shuffle = !shuffle;
    }
    else if (choice == keys["REPEAT"]) {
      repeat = !repeat;
    }
    else if (choice == keys["SHOW_HIDE_ALBUM"]) {
      showHideAlbum = !showHideAlbum;
    }
    else if (choice == keys["SHOW_HIDE_ARTIST"]) {
      showHideArtist = !showHideArtist;
    }
    else if (choice == keys["SEARCH"]) {
      echo();
      curs_set(1);
      char buf[256] = {'\0'};
      mvprintw(rows - 3, 0, "Search: ");
      getnstr(buf, 255);
      searchQuery = buf;
      // Filter playlist
      playlist.clear();
      auto allFiles = listAudioFiles(musicDir);
      /*for (auto &t : allFiles) {
        if (t.name.find(searchQuery) != std::string::npos) {
          playlist.push_back(t);
        }
      }*/
      playlist = filterTracks(allFiles, searchQuery);
      highlight = 0;
      offset = 0;
      noecho();
      curs_set(0);
    }
    else if (choice == keys["SEEKLEFT"]) {
      if (music.getStatus() != sf::Music::Stopped) {
        float newPos = music.getPlayingOffset().asSeconds() - 5.0f;
        if (newPos < 0) newPos = 0;
        music.setPlayingOffset(sf::seconds(newPos));
      }
    }
    else if (choice == keys["SEEKRIGHT"]) {
      if (music.getStatus() != sf::Music::Stopped) {
        float newPos = music.getPlayingOffset().asSeconds() + 5.0f;
        if (newPos > music.getDuration().asSeconds())
          newPos = music.getDuration().asSeconds();
        music.setPlayingOffset(sf::seconds(newPos));
      }
    }
    else if (choice == keys["QUIT"]) {
      running = false;
    }
    // Auto-play next track
    if (music.getStatus() == sf::Music::Stopped && currentTrack != -1) {
      if (repeat) {
        music.play();
      } else {
        if (shuffle) {
          static std::mt19937 rng(std::random_device{}());
          std::uniform_int_distribution<int> dist(0, playlist.size() - 1);
          highlight = dist(rng);
        } else {
          highlight = (highlight + 1) % playlist.size();
        }
        if (!music.openFromFile(playlist[highlight].path)) {
          mvprintw(rows - 1, 0, "Error: Cannot play file.");
        } else {
          music.setVolume(volume);
          music.play();
          currentTrack = highlight;
        }
      }
    }
  }
  // Save state before exit
  //savePlaylistState(playlist, searchQuery, shuffle);
  // Cleanup ncurses
  endwin();
  return EXIT_SUCCESS;
}

// Draw function tracks and status lines
void drawStatus(int currentTrack, int rows, int cols, std::vector<Track> playlist, int highlight, int colorPair, std::string status, int offset, bool shuffle, bool repeat, float volume, std::string &searchQuery, std::unordered_map<std::string, int> keys, int showHideAlbum, int showHideArtist) {
  std::string trackName = (currentTrack >= 0) ? playlist[currentTrack].title : "No track selected";
  if ((int)trackName.size() > cols - 20) {
    trackName = trackName.substr(0, cols - 23) + "...";
  }

  attron(COLOR_PAIR(colorPair) | A_BOLD);
  mvprintw(0, 0, "%s", status.c_str());
  mvprintw(0, 15, "%s", trackName.c_str());
  attroff(COLOR_PAIR(colorPair) | A_BOLD);

  mvprintw(1, 0, "%c %c Navigate | %c Play | %c Pause | SEEK %c left %c right | %c %c Volume UP/DOWN | %c Search | %c Shuffle | %c Repeat | %c Quit", keys["UP"], keys["DOWN"], keys["PLAY"], keys["PAUSE"], keys["SEEKLEFT"], keys["SEEKRIGHT"], keys["VOLUMEUP"], keys["VOLUMEDOWN"], keys["SEARCH"], keys["SHUFFLE"], keys["REPEAT"], keys["QUIT"]);

  // Show playlist (scrollable)
  int visibleRows = rows - 6;
  if (highlight < offset) offset = highlight;
  if (highlight >= offset + visibleRows) offset = highlight - visibleRows + 1;

  for (int i = 0; i < visibleRows && i + offset < (int)playlist.size(); ++i) {
    int idx = i + offset;
    if (idx == highlight) attron(A_REVERSE);
    //mvprintw(i + 2, 0, "%s", playlist[idx].name.c_str());
    mvprintw(i + 2, 0, "%d. %s %s %s %s", i + 1, ((showHideAlbum == 1) ? playlist[idx].album.c_str() : ""), ((showHideArtist == 1) ? playlist[idx].artist.c_str() : ""), playlist[idx].title.c_str(), playlist[idx].duration.c_str());
    if (idx == highlight) attroff(A_REVERSE);
  }

  // Show status
  mvprintw(rows - 4, 0, "Tracks: %u | Shuffle: %s | Repeat: %s | Show Album %s | Show Artist %s | Volume: %u%%", static_cast<unsigned int>(playlist.size()), shuffle ? "ON" : "OFF", repeat ? "ON" : "OFF", showHideAlbum ? "ON" : "OFF", showHideArtist ? "ON" : "OFF" , static_cast<unsigned int>(volume));

  // Show search query
  if (!searchQuery.empty()) {
    mvprintw(rows - 3, 0, "Search: %s", searchQuery.c_str());
  }
}

// Filter playlist by search term
std::vector<Track> filterTracks(const std::vector<Track> &tracks, const std::string &term) {
  if (term.empty()) return tracks;
  std::vector<Track> filtered;
  for (auto &t : tracks) {
    std::string lowerName = t.title;
    std::string lowerTerm = term;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    std::transform(lowerTerm.begin(), lowerTerm.end(), lowerTerm.begin(), ::tolower);
    if (lowerName.rfind(lowerTerm) != std::string::npos) {
      filtered.push_back(t);
    }
  }
  return filtered;
}

// List audio files in directory
std::vector<Track> listAudioFiles(const std::string &path) {
  std::vector<Track> files;
  try {
    for (const auto &entry : std::filesystem::directory_iterator(path)) {
      if (entry.is_regular_file()) {
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".wav" || ext == ".ogg" || ext == ".flac" || ext == ".mp3") {
          files.push_back(readMetadata(entry.path()));
          //files.push_back({entry.path().string(), entry.path().filename().string()});
        }
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "Error reading directory: " << e.what() << "\n";
  }
  return files;
}

// Function to read metadata using TagLib
Track readMetadata(const std::filesystem::path &filePath) {
  Track info;
  mpg123_handle *mh = NULL;
  int err;
  off_t samples;
  long int rate;
  int channeles;
  int encoding;
  unsigned int allOkay = 1U;
  info.path = filePath.string();
  info.title = filePath.filename().string();
  info.artist = "Unknown Artist";
  info.album = "Unknown Album";
  info.duration = "";
  if (mpg123_init() != MPG123_OK) {
    allOkay = 0U;
  }
  if (allOkay == 1U && (mh = mpg123_new(NULL, &err)) == NULL) {
    mpg123_exit();
    allOkay = 0U;
  }
  if (allOkay == 1U && mpg123_open(mh, info.path.c_str()) != MPG123_OK) {
    mpg123_delete(mh);
    mpg123_exit();
    allOkay = 0U;
  }
  if (allOkay == 1U && mpg123_getformat(mh, &rate, &channeles, &encoding) != MPG123_OK) {
    mpg123_close(mh);
    mpg123_delete(mh);
    mpg123_exit();
    allOkay = 0U;
  }
  if (allOkay == 1U && (samples = mpg123_length(mh)) == MPG123_ERR) {
    mpg123_close(mh);
    mpg123_delete(mh);
    mpg123_exit();
    allOkay = 0U;
  }
  TagLib::FileRef f(filePath.c_str());
  if (!f.isNull() && f.tag()) {
    TagLib::Tag *tag = f.tag();
    info.title  = tag->title().isEmpty()  ? filePath.filename().string() : tag->title().to8Bit(true);
    info.artist = tag->artist().isEmpty() ? "Unknown Artist" : tag->artist().to8Bit(true);
    info.album  = tag->album().isEmpty()  ? "Unknown Album" : tag->album().to8Bit(true);
  }
  if (allOkay == 1U) {
    float duration = static_cast<float>(samples) / static_cast<float>(rate);
    info.duration = formatTime(duration);
    mpg123_close(mh);
    mpg123_delete(mh);
    mpg123_exit();
  }
  return info;
}

// Format seconds into mm:ss
std::string formatTime(float duration) {
  int mins = static_cast<int>(duration) / 60;
  int secs = static_cast<int>(duration) % 60;
  char buf[256] = {'\0'};
  snprintf(buf, sizeof(buf) - 1, "%02d:%02d", mins, secs);
  return std::string(buf);
}

// Draw progress bar with time
void drawProgressBarWithTime(float elapsed, float total, int width, int y, int x) {
  float progress = (total > 0) ? elapsed / total : 0.f;
  int filled = static_cast<int>(progress * width);

  // Elapsed time
  mvprintw(y, x, "%s ", formatTime(elapsed).c_str());

  // Progress bar
  addch('[');
  for (int i = 0; i < width; ++i) {
    if (i < filled) addch('=');
    else addch(' ');
  }
  addch(']');

  // Total time
  printw(" %s", formatTime(total).c_str());
}

// Configuration file to be used to save all music files
/*void savePlaylistState(const std::vector<Track> &playlist, const std::string &searchTerm, bool shuffle) {
  std::ofstream out(".0verauPlaylist.txt");
  if (!out) return;
  out << searchTerm << "\n" << shuffle << "\n";
  for (auto &t : playlist) out << t.path << "\n";
}*/

// Configuration file to be used to load all music files
/*bool loadPlaylistState(std::vector<Track> &playlist, std::string &searchTerm, bool &shuffle) {
  std::ifstream in(".0verauPlaylist.txt");
  if (!in) return false;
  int shuf, rep;
  std::getline(in, searchTerm);
  in >> shuf >> rep;
  shuffle = shuf;
  in.ignore();
  playlist.clear();
  std::string path;
  while (std::getline(in, path)) {
    if (std::filesystem::exists(path)) {
      playlist.push_back({path, std::filesystem::path(path).filename().string()});
    }
  }
  return !playlist.empty();
}*/

// Convert string to key code
int keyFromString(const std::string &val) {
  if (val == "ENTER") return '\n';
  if (val.size() == 1) return val[0];
  return -1; // Invalid
}

// Load key bindings from config file
std::unordered_map<std::string, int> loadKeyBindings(const std::string &configPath) {
  std::unordered_map<std::string, int> keys = {
    {"UP", 'i'}, {"DOWN", 'j'}, {"PLAY", 'o'}, {"SEEKLEFT", ','}, {"SEEKRIGHT", '.'},
    {"PAUSE", 'p'}, {"QUIT", 'q'}, {"REPEAT", '@'}, {"SHOW_HIDE_ALBUM", '$'},
    {"SHUFFLE", '!'}, {"SEARCH", '/'}, {"VOLUMEUP", '+'}, {"VOLUMEDOWN", '-'}, {"SHOW_HIDE_ARTIST", '#'},
  };
  std::ifstream file(configPath);
  if (!file.is_open()) { return keys; }
  std::string line;
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#') continue; // Skip comments
    std::istringstream iss(line);
    std::string key, val;
    if (std::getline(iss, key, '=') && std::getline(iss, val)) {
      key.erase(remove_if(key.begin(), key.end(), ::isspace), key.end());
      val.erase(remove_if(val.begin(), val.end(), ::isspace), val.end());
      int code = keyFromString(val);
      if (code != -1) {
        keys[key] = code;
      } else {
        std::cerr << "Invalid key binding: " << key << "=" << val << "\n";
      }
    }
  }
  return keys;
}