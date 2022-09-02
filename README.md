# ahoviewer

A GTK image viewer, manga reader, and booru browser.

## Building
### Dependencies
* C++ Compiler that supports the C++17 standard is required. (g++ 7, clang 5)
* meson `>= 0.47.0`
* gtkmm-3.0 `>= 3.22.0`
* glibmm-2.4 `>= 2.46.0`
* libgsic++ `>= 2.6.0`
* libconfig++ `>= 1.4`
* libcurl `>= 7.58.0`
* libxml2
* gstreamer-1.0 `optional`
    * gst-plugins-bad `runtime`
    * gst-plugins-base `runtime`
    * gst-plugins-good `runtime`
    * gst-plugins-vpx `runtime`
    * gst-plugins-libav `runtime`
* libpeas `>=1.22.0` `optional`
* libsecret `optional`
    * gnome-keyring `runtime`
* libunrar `optional`
* libzip `optional`

```
meson build
cd build
ninja
sudo ninja install
```

If you don't want to compile with debug symbols replace the first command with:
```
meson build --buildtype=release
```

## Usage

    ahoviewer

or

    ahoviewer file[.zip|.rar|.webm|.(supported gdk-pixbuf file)]

### Screenshot
![Booru Browser](https://user-images.githubusercontent.com/1155344/91631124-e4bd4280-e99c-11ea-9432-72194d9b7aeb.gif)

### Plugins
Some example and usable plugins can be found at [ahodesuka/ahoviewer-plugins](https://github.com/ahodesuka/ahoviewer-plugins)

Plugins should be installed into `$XDG_DATA_HOME/ahoviewer/plugins`, `$XDG_DATA_HOME` is `~/.local/share` by default.
This is `%LOCALAPPDATA%\ahoviewer\plugins` on Windows.
The .typelib file that is compiled must be installed via `ninja install`, or you will need to set the
`GI_TYPELIB_PATH` environment variable to the directory where it is located after compilation.

### FAQ
1. **My password/API key is not saved after restarting ahoviewer**
   * See my comment in [issue #20](https://github.com/ahodesuka/ahoviewer/issues/20#issuecomment-157997909)
2. **I get *No results found* on Danbooru**
   * Danbooru only allows normal users to use 2 tags, if you have `Maximum post rating` set to anything other than `Explicit` it will count as one tag.
3. **I wish to use a proxy with ahoviewer**
   * This can be done by setting the `http_proxy` and `https_proxy` environment variables respectively, this works on both GNU/Linux and Windows.  See my comment in [issue #61](https://github.com/ahodesuka/ahoviewer/issues/61#issuecomment-354694187) for details.

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
| Save Image                                | Shift+s         |
| Save Image As                             | Control+s       |
| Delete Image                              | Shift+Delete    |
| Open Booru Post in Web Browser            | Control+Shift+o |
| Copy Booru Post URL to Clipboard          | Control+y       |
| Copy Booru Image URL to Clipboard         | y               |
| Copy Booru Image Data to Clipboard        | Control+Shift+y |
