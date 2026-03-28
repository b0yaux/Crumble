#!/bin/zsh

# Download YouTube video and convert to HAP format for Crumble
# Usage: ythap <youtube_url> [output_name]
# Requires: yt-dlp, ffmpeg with HAP support (brew reinstall ffmpeg --with-snappy)
ythap() {
    local url="$1"
    local name="${2:-output}"
    local output_dir="${CRUMBLE_VIDEOS:-$HOME/Videos/Crumble}"
    
    mkdir -p "$output_dir"
    
    # Check if ffmpeg has HAP support
    if ! ffmpeg -encoders 2>/dev/null | grep -q "hap"; then
        echo "⚠️  FFmpeg doesn't have HAP support"
        echo "Install with: brew reinstall ffmpeg --with-snappy"
        echo "Or use Photo-JPEG fallback: ythapjpeg $url $name"
        return 1
    fi
    
    echo "📥 Downloading from YouTube..."
    yt-dlp -f "bestvideo[ext=mp4]+bestaudio[ext=m4a]/best[ext=mp4]/best" \
           -o "$output_dir/${name}.%(ext)s" \
           "$url"
    
    local downloaded=$(ls -t "$output_dir/${name}".* 2>/dev/null | head -1)
    
    if [[ -f "$downloaded" ]]; then
        local hap_output="$output_dir/${name}.mov"
        echo "🎬 Converting to HAP..."
        if ffmpeg -i "$downloaded" -c:v hap -format hap_q -chunks 4 "$hap_output" 2>/dev/null; then
            rm "$downloaded"
            echo "✅ Done: $hap_output"
        else
            echo "❌ HAP encoding failed"
            echo "Falling back to Photo-JPEG..."
            ffmpeg -i "$downloaded" -c:v mjpeg -q:v 3 -c:a pcm_s16le "${hap_output%.mov}_mjpeg.mov"
            rm "$downloaded"
            echo "✅ Done (MJPEG): ${hap_output%.mov}_mjpeg.mov"
        fi
    else
        echo "❌ Download failed"
        return 1
    fi
}

# Quick version - just paste URL
ythapq() {
    ythap "$1" "crumble_$(date +%s)"
}

# Force Photo-JPEG (if HAP not available)
ythapjpeg() {
    local url="$1"
    local name="${2:-output}"
    local output_dir="${CRUMBLE_VIDEOS:-$HOME/Videos/Crumble}"
    
    mkdir -p "$output_dir"
    echo "📥 Downloading..."
    yt-dlp -f "bestvideo[ext=mp4]+bestaudio/best" -o "$output_dir/${name}.%(ext)s" "$url"
    
    local downloaded=$(ls -t "$output_dir/${name}."* 2>/dev/null | head -1)
    if [[ -f "$downloaded" ]]; then
        echo "🎬 Converting to Photo-JPEG..."
        ffmpeg -i "$downloaded" -c:v mjpeg -q:v 3 -c:a pcm_s16le "$output_dir/${name}.mov"
        rm "$downloaded"
        echo "✅ Done: $output_dir/${name}.mov"
    fi
}
