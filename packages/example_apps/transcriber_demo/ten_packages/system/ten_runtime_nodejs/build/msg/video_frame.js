//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import ten_addon from "../ten_addon.js";
import { Msg } from "./msg.js";
export var PixelFmt;
(function (PixelFmt) {
    PixelFmt[PixelFmt["RGB24"] = 1] = "RGB24";
    PixelFmt[PixelFmt["RGBA"] = 2] = "RGBA";
    PixelFmt[PixelFmt["BGR24"] = 3] = "BGR24";
    PixelFmt[PixelFmt["BGRA"] = 4] = "BGRA";
    PixelFmt[PixelFmt["I422"] = 5] = "I422";
    PixelFmt[PixelFmt["I420"] = 6] = "I420";
    PixelFmt[PixelFmt["NV21"] = 7] = "NV21";
    PixelFmt[PixelFmt["NV12"] = 8] = "NV12";
})(PixelFmt || (PixelFmt = {}));
export class VideoFrame extends Msg {
    constructor(name, createShellOnly) {
        super();
        if (createShellOnly) {
            return;
        }
        ten_addon.ten_nodejs_video_frame_create(this, name);
    }
    static Create(name) {
        return new VideoFrame(name, false);
    }
    allocBuf(size) {
        ten_addon.ten_nodejs_video_frame_alloc_buf(this, size);
    }
    lockBuf() {
        return ten_addon.ten_nodejs_video_frame_lock_buf(this);
    }
    unlockBuf(buf) {
        ten_addon.ten_nodejs_video_frame_unlock_buf(this, buf);
    }
    getBuf() {
        return ten_addon.ten_nodejs_video_frame_get_buf(this);
    }
    getWidth() {
        return ten_addon.ten_nodejs_video_frame_get_width(this);
    }
    setWidth(width) {
        ten_addon.ten_nodejs_video_frame_set_width(this, width);
    }
    getHeight() {
        return ten_addon.ten_nodejs_video_frame_get_height(this);
    }
    setHeight(height) {
        ten_addon.ten_nodejs_video_frame_set_height(this, height);
    }
    getTimestamp() {
        return ten_addon.ten_nodejs_video_frame_get_timestamp(this);
    }
    setTimestamp(timestamp) {
        ten_addon.ten_nodejs_video_frame_set_timestamp(this, timestamp);
    }
    getPixelFmt() {
        return ten_addon.ten_nodejs_video_frame_get_pixel_fmt(this);
    }
    setPixelFmt(pixelFmt) {
        ten_addon.ten_nodejs_video_frame_set_pixel_fmt(this, pixelFmt);
    }
    isEof() {
        return ten_addon.ten_nodejs_video_frame_is_eof(this);
    }
    setEof(eof) {
        ten_addon.ten_nodejs_video_frame_set_eof(this, eof);
    }
}
ten_addon.ten_nodejs_video_frame_register_class(VideoFrame);
//# sourceMappingURL=video_frame.js.map