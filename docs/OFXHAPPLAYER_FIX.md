# ofxHapPlayer macOS Fix

The upstream ofxHapPlayer uses bundled ffmpeg (v58) which has incompatible headers
with modern macOS SDK. This fork uses system ffmpeg from Homebrew.

## Changes from upstream

### addon_config.mk (osx section)

```makefile
osx:
	ADDON_LDFLAGS = -L/opt/homebrew/lib -lavformat -lavcodec -lavutil -lswresample -lsnappy
	ADDON_LIBS_EXCLUDE = libs/ffmpeg
	ADDON_LIBS_EXCLUDE += libs/snappy
	ADDON_INCLUDES_EXCLUDE = libs/ffmpeg/include/libavformat
	ADDON_INCLUDES_EXCLUDE += libs/ffmpeg/include/libavutil
	ADDON_INCLUDES_EXCLUDE += libs/ffmpeg/include/libavcodec
	ADDON_INCLUDES_EXCLUDE += libs/ffmpeg/include/libswresample
	ADDON_INCLUDES_EXCLUDE += libs/snappy/include
```

## Requirements

- macOS with Homebrew
- `brew install ffmpeg sdl2`

## Why this fix?

1. Bundled ffmpeg v58 headers conflict with C++17/modern macOS SDK
2. System ffmpeg (v62+) has `swr_alloc_set_opts2` which the code requires
3. Excluding bundled libs and using system libraries ensures compatibility

## Upstream PR

Consider submitting these changes upstream to benefit allmacOS users.