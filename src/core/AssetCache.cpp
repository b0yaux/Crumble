#include "AssetCache.h"
#include "ofLog.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/samplefmt.h>
#include <libavutil/mathematics.h>
#include <libavutil/channel_layout.h>
}

namespace {

static bool decodeAudioFromVideo(const std::string& path, DecodedAudio& out, int targetRate) {
    AVFormatContext* fmt_ctx = nullptr;
    if (avformat_open_input(&fmt_ctx, path.c_str(), nullptr, nullptr) != 0) {
        ofLogError("AssetCache") << "decodeAudio: failed to open " << path;
        return false;
    }
    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
        ofLogError("AssetCache") << "decodeAudio: no stream info " << path;
        avformat_close_input(&fmt_ctx);
        return false;
    }

    int audio_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audio_idx < 0) {
        ofLogError("AssetCache") << "decodeAudio: no audio stream " << path;
        avformat_close_input(&fmt_ctx);
        return false;
    }

    AVCodecParameters* par = fmt_ctx->streams[audio_idx]->codecpar;
    AVChannelLayout ch_layout = par->ch_layout;
    int inChannels = ch_layout.nb_channels;
    int inRate = par->sample_rate;

    if (inChannels <= 0 || inRate <= 0) {
        ofLogError("AssetCache") << "decodeAudio: bad params " << inChannels << "ch " << inRate << "Hz " << path;
        avformat_close_input(&fmt_ctx);
        return false;
    }

    if (targetRate <= 0) targetRate = inRate;

    const AVCodec* dec = avcodec_find_decoder(par->codec_id);
    if (!dec) {
        ofLogError("AssetCache") << "decodeAudio: unsupported codec " << path;
        avformat_close_input(&fmt_ctx);
        return false;
    }

    AVCodecContext* ctx = avcodec_alloc_context3(nullptr);
    if (!ctx || avcodec_parameters_to_context(ctx, par) < 0 || avcodec_open2(ctx, dec, nullptr) < 0) {
        ofLogError("AssetCache") << "decodeAudio: codec open failed " << path;
        if (ctx) avcodec_free_context(&ctx);
        avformat_close_input(&fmt_ctx);
        return false;
    }

    SwrContext* swr = nullptr;
    if (swr_alloc_set_opts2(&swr,
                            &ch_layout, AV_SAMPLE_FMT_FLT, targetRate,
                            &ctx->ch_layout, (AVSampleFormat)ctx->sample_fmt, ctx->sample_rate,
                            0, nullptr) < 0 || !swr || swr_init(swr) < 0) {
        ofLogError("AssetCache") << "decodeAudio: resampler init failed " << path;
        if (swr) swr_free(&swr);
        avcodec_free_context(&ctx);
        avformat_close_input(&fmt_ctx);
        return false;
    }

    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    std::vector<float> samples;
    out.channels = inChannels;
    out.sampleRate = targetRate;

    while (av_read_frame(fmt_ctx, pkt) >= 0) {
        if (pkt->stream_index == audio_idx) {
            if (avcodec_send_packet(ctx, pkt) >= 0) {
                while (avcodec_receive_frame(ctx, frame) >= 0) {
                    int nch = frame->ch_layout.nb_channels > 0 ? frame->ch_layout.nb_channels : inChannels;
                    int64_t delay = swr_get_delay(swr, ctx->sample_rate);
                    int outCount = (int)av_rescale_rnd(delay + frame->nb_samples,
                                                        targetRate, ctx->sample_rate, AV_ROUND_UP);
                    std::vector<uint8_t> buf(outCount * nch * sizeof(float));
                    uint8_t* outBuf = buf.data();
                    int converted = swr_convert(swr, &outBuf, outCount,
                                                (const uint8_t**)frame->extended_data,
                                                frame->nb_samples);

                    if (converted > 0) {
                        float* f = reinterpret_cast<float*>(outBuf);
                        samples.insert(samples.end(), f, f + converted * nch);
                    }
                }
            }
        }
        av_packet_unref(pkt);
    }

    int64_t delay = swr_get_delay(swr, ctx->sample_rate);
    if (delay > 0) {
        int flushCount = (int)av_rescale_rnd(delay, targetRate, ctx->sample_rate, AV_ROUND_UP);
        std::vector<uint8_t> buf(flushCount * inChannels * sizeof(float));
        uint8_t* outBuf = buf.data();
        int converted = swr_convert(swr, &outBuf, flushCount, nullptr, 0);
        if (converted > 0) {
            float* f = reinterpret_cast<float*>(outBuf);
            samples.insert(samples.end(), f, f + converted * inChannels);
        }
    }

    out.data = std::move(samples);
    out.numFrames = out.data.size() / inChannels;

    av_frame_free(&frame);
    av_packet_free(&pkt);
    swr_free(&swr);
    avcodec_free_context(&ctx);
    avformat_close_input(&fmt_ctx);
    return out.numFrames > 0;
}

} // anonymous namespace

std::shared_ptr<DecodedAudio> AssetCache::getEmbeddedAudio(const std::string& videoPath, int targetRate) {
    std::lock_guard<std::mutex> lock(mutex);

    std::string absolutePath = videoPath;
    std::string resolved = AssetRegistry::get().resolve(videoPath, "audio");
    if (!resolved.empty()) {
        absolutePath = resolved;
    } else {
        absolutePath = ConfigManager::get().resolvePath(videoPath);
    }
    if (absolutePath.empty()) return nullptr;

    auto typeIdx = std::type_index(typeid(DecodedAudio));
    auto& typeCache = caches[typeIdx];
    auto it = typeCache.find(absolutePath);
    if (it != typeCache.end()) {
        auto cached = std::static_pointer_cast<DecodedAudio>(it->second.asset);
        if (cached->sampleRate == targetRate) {
            it->second.lastUsedTime = ofGetElapsedTimef();
            return cached;
        }
        typeCache.erase(it);
    }

    ofLogNotice("AssetCache") << "Decoding embedded audio: " << absolutePath
                              << " (target rate: " << targetRate << " Hz)";

    auto decoded = std::make_shared<DecodedAudio>();
    if (!decodeAudioFromVideo(absolutePath, *decoded, targetRate)) {
        ofLogError("AssetCache") << "Failed to decode embedded audio: " << absolutePath;
        return nullptr;
    }

    ofLogNotice("AssetCache") << "Decoded " << decoded->numFrames << " frames, "
                              << decoded->channels << " ch, " << decoded->sampleRate << " Hz";

    AssetEntry entry;
    entry.asset = decoded;
    entry.lastUsedTime = ofGetElapsedTimef();
    typeCache[absolutePath] = entry;

    return decoded;
}
