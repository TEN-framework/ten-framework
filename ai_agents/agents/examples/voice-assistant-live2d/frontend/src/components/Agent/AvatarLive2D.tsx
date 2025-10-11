"use client";

import React, { useEffect, useRef, useState } from "react";
import * as PIXI from "pixi.js";
import { Live2DModel } from "pixi-live2d-display/cubism4";
import { MotionSync } from "live2d-motionsync/stream";
import { IMicrophoneAudioTrack } from "agora-rtc-sdk-ng";

// 确保 PIXI.Ticker 在插件中可用
// @ts-ignore
window.PIXI = PIXI;

const MODEL_URL = "/models/kei_vowels_pro/kei_vowels_pro.model3.json";
const MOTION_SYNC_URL = "/models/kei_vowels_pro/kei_vowels_pro.motionsync3.json";


// --- React 组件 ---

interface AvatarLive2DProps {
  audioTrack?: IMicrophoneAudioTrack;
}

export default function AvatarLive2D({ audioTrack }: AvatarLive2DProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const motionSyncRef = useRef<MotionSync | null>(null);
  const [isModelLoaded, setIsModelLoaded] = useState(false);
  const audioElementRef = useRef<HTMLAudioElement | null>(null);

  useEffect(() => {
    // 确保只在客户端执行
    if (typeof window === "undefined") return;

    // 等待 Live2D 核心库加载完成
    const waitForLive2DCore = () => {
      return new Promise<void>((resolve) => {
        if (typeof window !== "undefined" && (window as any).Live2DCubismCore) {
          resolve();
          return;
        }

        const checkInterval = setInterval(() => {
          if (typeof window !== "undefined" && (window as any).Live2DCubismCore) {
            clearInterval(checkInterval);
            resolve();
          }
        }, 100);

        // 超时处理
        setTimeout(() => {
          clearInterval(checkInterval);
          console.error("Live2D Cubism Core failed to load within timeout");
        }, 10000);
      });
    };

    const initLive2D = async () => {
      try {
        await waitForLive2DCore();

        const app = new PIXI.Application({
          view: canvasRef.current!,
          autoStart: true,
          resizeTo: canvasRef.current?.parentElement || window,
          backgroundColor: 0x000000,
          backgroundAlpha: 0,
        });

        let model: Live2DModel;

        model = await Live2DModel.from(MODEL_URL);
        app.stage.addChild(model);

        // 调整模型大小和位置
        const parent = canvasRef.current?.parentElement;
        if (parent) {
          model.scale.set(parent.clientHeight / model.height);
          model.x = (parent.clientWidth - model.width) / 2;
        }

        // 初始化 MotionSync
        const motionSync = new MotionSync(model.internalModel);
        await motionSync.loadMotionSyncFromUrl(MOTION_SYNC_URL);
        motionSyncRef.current = motionSync;

        setIsModelLoaded(true);
        console.log("Live2D Model and MotionSync are ready.");

        return () => {
          app.destroy(false, true); // 清理 PIXI 应用
        };
      } catch (error) {
        console.error("Failed to initialize Live2D:", error);
      }
    };

    initLive2D();
  }, []);

  // 用于处理来自 Agora 的 audioTrack 的 Effect
  useEffect(() => {
    const motionSync = motionSyncRef.current;
    // 确保模型和 motionSync 实例都已加载
    if (!motionSync || !isModelLoaded) return;

    if (audioTrack) {
      console.log("[AvatarLive2D] 接收到 audioTrack, 正在创建 MediaStream。");

      // 从 Agora 音轨创建 MediaStream
      const stream = new MediaStream([audioTrack.getMediaStreamTrack()]);

      // 将 stream 传递给 motionSync 进行播放和口型同步
      motionSync.play(stream);

      // 同时创建并播放隐藏的 <audio> 元素，确保实际发声
      try {
        if (!audioElementRef.current) {
          const audio = document.createElement("audio");
          audio.autoplay = true;
          // 在 iOS/Safari 中需要 playsInline 以避免全屏
          (audio as any).playsInline = true;
          audio.muted = false;
          audio.volume = 1.0;
          audio.style.display = "none";
          document.body.appendChild(audio);
          audioElementRef.current = audio;
        }
        const audioEl = audioElementRef.current!;
        audioEl.srcObject = stream;
        const playPromise = audioEl.play();
        if (playPromise && typeof playPromise.then === "function") {
          playPromise.catch((err: unknown) => {
            console.warn("[AvatarLive2D] 自动播放被阻止，等待用户手势触发。", err);
          });
        }
      } catch (err) {
        console.error("[AvatarLive2D] 播放音频失败:", err);
      }

      // 当音轨结束时重置口型
      audioTrack.getMediaStreamTrack().onended = () => {
        console.log("[AvatarLive2D] Audio track ended.");
        motionSync.reset();
        if (audioElementRef.current) {
          try {
            audioElementRef.current.pause();
            audioElementRef.current.srcObject = null;
            audioElementRef.current.remove();
          } catch { }
          audioElementRef.current = null;
        }
      };
    } else {
      // 如果没有 audioTrack，则重置口型
      console.log("[AvatarLive2D] No audioTrack, resetting MotionSync.");
      motionSync.reset();
      if (audioElementRef.current) {
        try {
          audioElementRef.current.pause();
          audioElementRef.current.srcObject = null;
          audioElementRef.current.remove();
        } catch { }
        audioElementRef.current = null;
      }
    }

    return () => {
      // 在组件卸载或 audioTrack 改变时，清理并重置
      motionSync.reset();
      if (audioElementRef.current) {
        try {
          audioElementRef.current.pause();
          audioElementRef.current.srcObject = null;
          audioElementRef.current.remove();
        } catch { }
        audioElementRef.current = null;
      }
    };
  }, [audioTrack, isModelLoaded]);


  return (
    <div className="relative h-full w-full">
      <canvas ref={canvasRef} />
      {!isModelLoaded && (
        <div className="absolute inset-0 z-10 flex items-center justify-center bg-black bg-opacity-50 text-white">
          Loading Model...
        </div>
      )}
    </div>
  );
}