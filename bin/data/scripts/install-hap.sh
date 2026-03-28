#!/bin/bash
# Crumble HAP Encoder Setup
# One-command setup for YouTube→HAP workflow

set -e

echo "🎬 Crumble HAP Setup"
echo "===================="

# Check if yt-dlp is installed
if ! command -v yt-dlp &> /dev/null; then
    echo "📦 Installing yt-dlp..."
    brew install yt-dlp
fi

# Check if ffmpeg-full is installed (has HAP support)
if brew list ffmpeg-full &>/dev/null; then
    echo "✅ ffmpeg-full already installed"
else
    echo "📦 Installing ffmpeg-full (includes HAP codec)..."
    brew install ffmpeg-full
    
    # Link ffmpeg-full binaries
    echo "🔗 Linking ffmpeg-full..."
    brew link ffmpeg-full --force
fi

# Verify HAP support
if ffmpeg -encoders 2>/dev/null | grep -q "hap"; then
    echo "✅ HAP codec confirmed working!"
    ffmpeg -encoders 2>/dev/null | grep "hap" | head -3
else
    echo "⚠️  HAP codec not found. Trying alternative..."i

# Create output directory
mkdir -p ~/Videos/Crumble

echo ""
echo "✨ Setup complete!"
echo ""
echo "Usage:"
echo "  yt-hap <youtube_url> [output_name]"
echo "  yt-hap https://youtu.be/kpw1zZbO_sE tutorial"
echo ""
echo "Videos will be saved to: ~/Videos/Crumble/"
