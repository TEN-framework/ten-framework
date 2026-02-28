//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import ten_addon from "../ten_addon.js";
import { Msg } from "./msg.js";
export var AudioFrameDataFmt;
(function (AudioFrameDataFmt) {
    AudioFrameDataFmt[AudioFrameDataFmt["INTERLEAVE"] = 1] = "INTERLEAVE";
    AudioFrameDataFmt[AudioFrameDataFmt["NON_INTERLEAVE"] = 2] = "NON_INTERLEAVE";
})(AudioFrameDataFmt || (AudioFrameDataFmt = {}));
export class AudioFrame extends Msg {
    constructor(name, createShellOnly) {
        super();
        if (createShellOnly) {
            return;
        }
        ten_addon.ten_nodejs_audio_frame_create(this, name);
    }
    static Create(name) {
        return new AudioFrame(name, false);
    }
    allocBuf(size) {
        ten_addon.ten_nodejs_audio_frame_alloc_buf(this, size);
    }
    lockBuf() {
        return ten_addon.ten_nodejs_audio_frame_lock_buf(this);
    }
    unlockBuf(buf) {
        ten_addon.ten_nodejs_audio_frame_unlock_buf(this, buf);
    }
    getBuf() {
        return ten_addon.ten_nodejs_audio_frame_get_buf(this);
    }
    getTimestamp() {
        return ten_addon.ten_nodejs_audio_frame_get_timestamp(this);
    }
    setTimestamp(timestamp) {
        ten_addon.ten_nodejs_audio_frame_set_timestamp(this, timestamp);
    }
    getSampleRate() {
        return ten_addon.ten_nodejs_audio_frame_get_sample_rate(this);
    }
    setSampleRate(sampleRate) {
        ten_addon.ten_nodejs_audio_frame_set_sample_rate(this, sampleRate);
    }
    getSamplesPerChannel() {
        return ten_addon.ten_nodejs_audio_frame_get_samples_per_channel(this);
    }
    setSamplesPerChannel(samplesPerChannel) {
        ten_addon.ten_nodejs_audio_frame_set_samples_per_channel(this, samplesPerChannel);
    }
    getBytesPerSample() {
        return ten_addon.ten_nodejs_audio_frame_get_bytes_per_sample(this);
    }
    setBytesPerSample(bytesPerSample) {
        ten_addon.ten_nodejs_audio_frame_set_bytes_per_sample(this, bytesPerSample);
    }
    getNumberOfChannels() {
        return ten_addon.ten_nodejs_audio_frame_get_number_of_channels(this);
    }
    setNumberOfChannels(numberOfChannels) {
        ten_addon.ten_nodejs_audio_frame_set_number_of_channels(this, numberOfChannels);
    }
    getDataFmt() {
        return ten_addon.ten_nodejs_audio_frame_get_data_fmt(this);
    }
    setDataFmt(dataFmt) {
        ten_addon.ten_nodejs_audio_frame_set_data_fmt(this, dataFmt);
    }
    getLineSize() {
        return ten_addon.ten_nodejs_audio_frame_get_line_size(this);
    }
    setLineSize(lineSize) {
        ten_addon.ten_nodejs_audio_frame_set_line_size(this, lineSize);
    }
    isEof() {
        return ten_addon.ten_nodejs_audio_frame_is_eof(this);
    }
    setEof(eof) {
        ten_addon.ten_nodejs_audio_frame_set_eof(this, eof);
    }
}
ten_addon.ten_nodejs_audio_frame_register_class(AudioFrame);
//# sourceMappingURL=audio_frame.js.map