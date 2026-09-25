# Optional splash source assets

The repository deliberately does not contain the 33 images from the referenced
[ArtStation project](https://www.artstation.com/artwork/nYy1nX).  The project
page does not provide a redistribution licence, while ArtStation's terms state
that original content remains the creator's property.  A public firmware
repository therefore needs the author's explicit permission or a separately
published compatible licence before those files can be committed or converted.

After obtaining permission, place the original GIF/PNG files in this directory,
record the author, source URL and licence here, then convert them with:

```bash
python -m pip install Pillow
python tools/convert_splash.py animation.gif --name splash_name > custom_splash.h
```

The generated arrays use `[run length, byte value]` pairs and can be decoded by
`splash_rle_decode()`.  Original assets should never be committed without a
licence file or a written permission reference.
