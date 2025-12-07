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
#include <SFML/Audio.hpp>
#include <ncurses.h>

struct Track {
  std::string path;
  std::string name;
};

// List audio files in directory
std::vector<Track> listAudioFiles(const std::string &path);
// Format seconds into mm:ss
std::string formatTime(float seconds);
// Draw progress bar with time
void drawProgressBarWithTime(float elapsed, float total, int width, int y, int x);

int main(int argc, char *argv[]) {
  if (argc < 2) { std::cerr << "You must provide some folder with music in it." << std::endl; return EXIT_FAILURE; }
  std::string musicDir = argv[1]; // Change to your music folder
  auto playlist = listAudioFiles(musicDir);
  if (playlist.empty()) { std::cerr << "No audio files found in " << musicDir << "\n"; return EXIT_FAILURE; }

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
  bool shuffle = false, repeat = false, running = true;
  float volume = 100.f;
  std::string searchQuery;

  sf::Music music;
  int currentTrack = -1;

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

    std::string trackName = (currentTrack >= 0) ? playlist[currentTrack].name : "No track selected";
    if ((int)trackName.size() > cols - 20) {
      trackName = trackName.substr(0, cols - 23) + "...";
    }

    attron(COLOR_PAIR(colorPair) | A_BOLD);
    mvprintw(0, 0, "%s", status.c_str());
    attroff(COLOR_PAIR(colorPair) | A_BOLD);

    mvprintw(0, 15, "%s", trackName.c_str());
    mvprintw(1, 0, "Up/Down Arrow Navigate | Enter Play | P Pause | S Stop | +/- Volume | F Search | H Shuffle | R Repeat | Q Quit");

    // Show playlist (scrollable)
    int visibleRows = rows - 6;
    if (highlight < offset) offset = highlight;
    if (highlight >= offset + visibleRows) offset = highlight - visibleRows + 1;

    for (int i = 0; i < visibleRows && i + offset < (int)playlist.size(); ++i) {
      int idx = i + offset;
      if (idx == highlight) attron(A_REVERSE);
      mvprintw(i + 2, 0, "%s", playlist[idx].name.c_str());
      if (idx == highlight) attroff(A_REVERSE);
    }

    // Show status
    mvprintw(rows - 4, 0, "Shuffle: %s | Repeat: %s | Volume: %.0f %%", shuffle ? "ON" : "OFF", repeat ? "ON" : "OFF", volume);

    // Show search query
    if (!searchQuery.empty()) {
      mvprintw(rows - 3, 0, "Search: %s", searchQuery.c_str());
    }

    // Show progress bar if playing
    if (music.getStatus() != sf::Music::Stopped) {
      float elapsed = music.getPlayingOffset().asSeconds();
      float total = music.getDuration().asSeconds();
      drawProgressBarWithTime(elapsed, total, cols - 20, rows - 2, 0);
    }

    wrefresh(stdscr);
    choice = getch();
    switch (choice) {
      case KEY_UP: {
        highlight = (highlight - 1 + playlist.size()) % playlist.size();;
      }
        break;
      case KEY_DOWN: {
        highlight = (highlight + 1) % playlist.size();
      }
        break;
      case '\n': {
        if (!music.openFromFile(playlist[highlight].path)) {
          mvprintw(rows - 1, 0, "Error: Cannot play file.");
        } else {
          music.setVolume(volume);
          music.play();
          currentTrack = highlight;
        }  
      }
        break;
      case 'p': {
        if (music.getStatus() == sf::Music::Playing) music.pause();
        else if (music.getStatus() == sf::Music::Paused) music.play();
      }
        break;
      case '+': {
        volume = std::min(100.f, volume + 5.f);
        music.setVolume(volume);
      }
        break;
      case '-': {
        volume = std::max(0.f, volume - 5.f);
        music.setVolume(volume);
      }
        break;
      case 's': {
        music.stop();
      }
        break;
      case 'h': {
        shuffle = !shuffle;
      }
        break;
      case 'r': {
        repeat = !repeat;
      }
        break;
      case 'f': {
        echo();
        curs_set(1);
        char buf[256];
        mvprintw(rows - 3, 0, "Search: ");
        getnstr(buf, 255);
        searchQuery = buf;
        noecho();
        curs_set(0);
        // Filter playlist
        auto allFiles = listAudioFiles(musicDir);
        playlist.clear();
        for (auto &t : allFiles) {
          if (t.name.find(searchQuery) != std::string::npos) {
            playlist.push_back(t);
          }
        }
        highlight = 0;
        offset = 0;
      }
        break;
      case 'q': {
        running = false;
      }
        break;
      default:
        break;
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
  // Cleanup ncurses
  endwin();
  return EXIT_SUCCESS;
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
          files.push_back({entry.path().string(), entry.path().filename().string()});
        }
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "Error reading directory: " << e.what() << "\n";
  }
  return files;
}

// Format seconds into mm:ss
std::string formatTime(float seconds) {
  int mins = static_cast<int>(seconds) / 60;
  int secs = static_cast<int>(seconds) % 60;
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