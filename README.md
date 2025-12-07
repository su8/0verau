![](1snap.png)

# 0verhau
ncurses music player similar to cmus

# Compile

```bash
make -j8  # 8 cores/threads to use in parallel compile
sudo/doas make install
```

# Requirements

In Debian it's `sudo apt install libncurses5-dev libncursesw5-dev libsfml-dev`, in your other OS's search for `lib ncurses  libsfml-dev`.

---

### Note

When you search for a song name and find it, make sure to erase the input with your backspace (above than Enter key), so the player to show the rest songs.