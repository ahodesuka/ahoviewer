# ahoviewer

A GTK2 image viewer, manga reader, and booru browser.

## Building
### Dependencies
* gtkmm-2.4 `>= 2.20.0`
* glibmm-2.4 `>= 2.36.0`
* libconfig++ `>= 1.4`
* libcurl `>= 7.32.0`
* libxml2
* gstreamer-1.0 `optional`
    * gst-plugins-base `runtime`
    * gst-plugins-good `runtime`
    * gst-plugins-vpx or gst-plugins-libav `runtime`
* libsecret `optional`
    * gnome-keyring `runtime`
* libunrar `optional`
* libzip `optional`

```
./bootstrap
make
sudo make install
```

An ebuild can be found
[here](https://gist.githubusercontent.com/ahodesuka/9c06310db83087f3be2d2a0c4a0ac062/raw/b645a5e9c5f914cffafc49fc5cfd4fa8a18cdb74/ahoviewer-9999.ebuild)

## Usage

    ahoviewer

or

    ahoviewer file[.zip|.rar|.webm|.*]

### Screenshot
![Booru Browser](https://camo.githubusercontent.com/ad37a28fc1f47a41d1c79409ab31e3e01a1507e9/68747470733a2f2f692e696d6775722e636f6d2f486e47656368662e676966)

### FAQ
1. **My password/API key is not saved after restarting ahoviewer**
   * See my comment in [issue #20](https://github.com/ahodesuka/ahoviewer/issues/20#issuecomment-157997909)
2. **I get *No results found* on Danbooru**
   * Danbooru only allows normal users to use 2 tags, if you have `Maximum post rating` set to anything other than `Explicit` it will count as one tag.

#### Default Keybindings
| Function                                  | Key             |
| ----------------------------------------- | --------------- |
| Open File                                 | Control+o       |
| Open Preferences                          | p               |
| Close local image list or booru tab       | Control+w       |
| Quit                                      | Control+q       |
| Fullscreen                                | f               |
| Toggle Manga Mode                         | g               |
| Set Auto Fit Mode                         | a               |
| Set Fit Width Mode                        | w               |
| Set Fit Height Mode                       | h               |
| Set Manual Zoom Mode                      | m               |
| Zoom In                                   | Control+=       |
| Zoom Out                                  | Control+-       |
| Reset Zoom                                | Control+0       |
| Toggle Menubar                            | Control+m       |
| Toggle Statusbar                          | Control+b       |
| Toggle Scrollbars                         | Control+l       |
| Toggle Thumbnail bar (local image list)   | t               |
| Toggle Booru Browser                      | b               |
| Hide all (hides the above widgets)        | i               |
| Next Image                                | Page Down       |
| Previous Image                            | Page Up         |
| First Image                               | Home            |
| Last Image                                | End             |
| Toggle Slideshow                          | s               |
| New Booru Browser Tab                     | Control+t       |
| Save Booru Browser Tab                    | Control+Shift+s |
| Save Current Booru Image                  | Control+s       |
| Open Booru Post in Web Browser            | Control+Shift+o |
| Copy Booru Image URL to Clipboard         | y               |
