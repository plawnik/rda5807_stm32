# PCD8544 layout editor

The editor is a self-contained offline tool for preparing the 84 x 48 pixel
main-screen layout. It does not require a web server, package manager or an
Internet connection.

## Start

Open `index.html` directly in a current browser. On Windows, double-clicking
the file is sufficient.

## Workflow

1. Use **Rysuj** and **Gumka** to edit individual display pixels.
2. Use **Nowy obszar** and drag a rectangle around every functional part of
   the screen.
3. Give each area a clear name and describe what it should show.
4. Select **Pobierz JSON** and send the generated
   `*.pcd8544-layout.json` file with the firmware requirements.

The JSON contains:

- the screen name and general notes;
- all regions with descriptions and exact `x`, `y`, `width`, `height` values;
- 48 human-readable rows of 84 binary pixels;
- the same bitmap packed as 504 PCD8544 page bytes, with bit 0 representing
  the top pixel in each eight-pixel page.

The editor can load its own JSON files, supports undo/redo and stores the
current project in browser local storage.
