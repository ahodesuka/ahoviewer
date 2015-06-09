# ahoviewer

A GTK2 image viewer, manga reader, and booru browser.

## Building
### Dependencies
* gtkmm-2.4 `>= 2.20.0`
* glibmm-2.4 `>= 2.36.0`
* libconfig++ `>= 1.4`
* libcurl `>= 7.16.0`
* pugixml
* gstreamer `optional`
    * gst-plugins-bad (opengl)
* libunrar `optional`
* libzip `optional`

## Usage

    $ ahoviewer

or

    $ ahoviewer file

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
| Save Booru Browser Tab                    | Control+s       |
| Save Current Booru Image                  | Contrl+Shift+s  |
