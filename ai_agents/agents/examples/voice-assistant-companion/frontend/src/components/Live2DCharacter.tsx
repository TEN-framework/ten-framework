"use client";

import { MotionSync } from "live2d-motionsync/stream";
import { useEffect, useRef, useState } from "react";
// Import PIXI setup first to ensure global availability
import PIXI from "@/lib/pixi-setup";
// IRemoteAudioTrack will be imported dynamically
import { cn } from "@/lib/utils";

interface Live2DCharacterProps {
  modelPath: string;
  audioTrack?: any;
  className?: string;
  onModelLoaded?: () => void;
  onModelError?: (error: Error) => void;
}

export default function Live2DCharacter({
  modelPath,
  audioTrack,
  className,
  onModelLoaded,
  onModelError,
}: Live2DCharacterProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const motionSyncRef = useRef<MotionSync | null>(null);
  const appRef = useRef<any>(null);
  const [isModelLoaded, setIsModelLoaded] = useState(false);
  const [isClient, setIsClient] = useState(false);
  const audioElementRef = useRef<HTMLAudioElement | null>(null);
  const [motionSyncEnabled, setMotionSyncEnabled] = useState(true); // Re-enabled for lip sync
  const isDisconnectingRef = useRef(false);

  // Ensure component only renders on client side
  useEffect(() => {
    setIsClient(true);

    // Add global error handler for MotionSync errors
    const handleGlobalError = (event: ErrorEvent) => {
      if (event.message?.includes("addLast")) {
        console.error(
          "[Live2DCharacter] MotionSync error caught globally:",
          event
        );
        setMotionSyncEnabled(false);
        // Prevent the error from propagating
        event.preventDefault();
        return false;
      }
    };

    const handleUnhandledRejection = (event: PromiseRejectionEvent) => {
      if (event.reason?.toString().includes("addLast")) {
        console.error(
          "[Live2DCharacter] MotionSync promise rejection caught:",
          event.reason
        );
        setMotionSyncEnabled(false);
        event.preventDefault();
      }
    };

    window.addEventListener("error", handleGlobalError);
    window.addEventListener("unhandledrejection", handleUnhandledRejection);

    return () => {
      window.removeEventListener("error", handleGlobalError);
      window.removeEventListener(
        "unhandledrejection",
        handleUnhandledRejection
      );
    };
  }, []);

  useEffect(() => {
    // Ensure only runs on client side
    if (typeof window === "undefined") return;

    // Ensure we have a valid model path
    if (!modelPath) {
      console.log(
        "[Live2DCharacter] No model path provided, skipping initialization"
      );
      return;
    }

    // Wait for Live2D core library to load
    const waitForLive2DCore = () => {
      return new Promise<void>((resolve) => {
        if (typeof window !== "undefined" && (window as any).Live2DCubismCore) {
          resolve();
          return;
        }

        const checkInterval = setInterval(() => {
          if (
            typeof window !== "undefined" &&
            (window as any).Live2DCubismCore
          ) {
            clearInterval(checkInterval);
            resolve();
          }
        }, 100);

        // Timeout handling
        setTimeout(() => {
          clearInterval(checkInterval);
          console.error("Live2D Cubism Core failed to load within timeout");
        }, 10000);
      });
    };

    const initLive2D = async () => {
      try {
        console.log(
          "[Live2DCharacter] Initializing with model path:",
          modelPath
        );
        await waitForLive2DCore();

        // Small delay to prevent rapid re-initialization
        await new Promise((resolve) => setTimeout(resolve, 100));

        // Clean up any existing PIXI application
        if (appRef.current) {
          console.log(
            "[Live2DCharacter] Cleaning up existing PIXI application"
          );
          try {
            // Stop the application first
            appRef.current.stop();

            // Destroy with aggressive cleanup
            appRef.current.destroy(true, {
              children: true,
              texture: true,
              baseTexture: true,
            });

            // Clear the canvas context
            if (canvasRef.current) {
              const ctx = canvasRef.current.getContext("2d");
              if (ctx) {
                ctx.clearRect(
                  0,
                  0,
                  canvasRef.current.width,
                  canvasRef.current.height
                );
              }
            }
          } catch (destroyError) {
            console.warn(
              "[Live2DCharacter] Error destroying PIXI app:",
              destroyError
            );
          }
          appRef.current = null;
        }

        // Ensure canvas is clean and ready
        if (canvasRef.current) {
          const canvas = canvasRef.current;
          const ctx =
            canvas.getContext("webgl2") ||
            canvas.getContext("webgl") ||
            canvas.getContext("2d");
          if (ctx) {
            // Clear any existing context
            if ("clear" in ctx) {
              ctx.clear(ctx.COLOR_BUFFER_BIT || 0x00004000);
            }
          }
        }

        // Create new PIXI application with Canvas renderer to avoid WebGL shader issues
        console.log(
          "[Live2DCharacter] Creating PIXI Application with Canvas renderer..."
        );
        const app = new PIXI.Application({
          view: canvasRef.current!,
          autoStart: true,
          resizeTo: canvasRef.current?.parentElement || window,
          backgroundColor: 0x000000,
          backgroundAlpha: 0,
          forceCanvas: true, // Force Canvas renderer to avoid WebGL shader issues
          antialias: false, // Disable antialiasing to reduce GPU usage
          powerPreference: "low-power", // Use integrated GPU to avoid crashes
        });
        console.log(
          "[Live2DCharacter] PIXI Application created successfully:",
          app
        );

        appRef.current = app;

        // Wait a moment for PIXI application to fully initialize
        await new Promise((resolve) => setTimeout(resolve, 100));

        // Validate PIXI application is properly initialized
        console.log("[Live2DCharacter] Validating PIXI Application...");
        console.log("[Live2DCharacter] App:", app);
        console.log("[Live2DCharacter] App.stage:", app?.stage);
        if (!app || !app.stage) {
          throw new Error(
            "PIXI Application or stage is not properly initialized"
          );
        }
        console.log("[Live2DCharacter] PIXI Application validation passed");

        // Load Live2D with proper PIXI setup
        const { Live2DModel } = await import("@/lib/live2d-loader").then(
          (loader) => loader.loadLive2DModel()
        );
        let model: any;

        console.log("[Live2DCharacter] Loading model from:", modelPath);
        model = await Live2DModel.from(modelPath);

        // Validate model is loaded before adding to stage
        if (!model) {
          throw new Error("Failed to load Live2D model");
        }

        app.stage.addChild(model);

        // Adjust model size and position
        const parent = canvasRef.current?.parentElement;
        if (parent) {
          model.scale.set(parent.clientHeight / model.height);
          model.x = (parent.clientWidth - model.width) / 2;
        }

        // Initialize MotionSync if available and enabled
        if (motionSyncEnabled) {
          try {
            const motionSyncUrl = modelPath.replace(
              ".model3.json",
              ".motionsync3.json"
            );
            console.log(
              "[Live2DCharacter] Attempting to load MotionSync from:",
              motionSyncUrl
            );

            // Check if MotionSync file exists by making a HEAD request
            const response = await fetch(motionSyncUrl, { method: "HEAD" });
            if (response.ok) {
              // Wait a bit for the model to be fully initialized
              await new Promise((resolve) => setTimeout(resolve, 1000));

              // Validate that the model and internalModel are properly initialized
              if (model?.internalModel?.coreModel) {
                console.log(
                  "[Live2DCharacter] Creating MotionSync instance..."
                );
                const motionSync = new MotionSync(model.internalModel);

                console.log("[Live2DCharacter] Loading MotionSync from URL...");
                await motionSync.loadMotionSyncFromUrl(motionSyncUrl);

                motionSyncRef.current = motionSync;
                console.log("[Live2DCharacter] MotionSync loaded successfully");
              } else {
                console.warn(
                  "[Live2DCharacter] Model internal structure not ready for MotionSync"
                );
                motionSyncRef.current = null;
              }
            } else {
              console.log(
                "[Live2DCharacter] MotionSync file not found, skipping MotionSync initialization"
              );
              motionSyncRef.current = null;
            }
          } catch (motionSyncError) {
            console.error(
              "[Live2DCharacter] MotionSync initialization failed:",
              motionSyncError
            );
            setMotionSyncEnabled(false);
            motionSyncRef.current = null;
          }
        } else {
          console.log(
            "[Live2DCharacter] MotionSync disabled due to previous errors"
          );
          motionSyncRef.current = null;
        }

        setIsModelLoaded(true);
        onModelLoaded?.();
        console.log("Live2D Model is ready.");
      } catch (error) {
        console.error("Failed to initialize Live2D:", error);
        onModelError?.(error as Error);
      }
    };

    initLive2D();

    // Cleanup function
    return () => {
      console.log("[Live2DCharacter] Cleaning up Live2D resources");
      if (appRef.current) {
        appRef.current.destroy(false, true);
        appRef.current = null;
      }
      motionSyncRef.current = null;
      setIsModelLoaded(false);
    };
  }, [modelPath, onModelLoaded, onModelError, motionSyncEnabled]);

  // Effect for handling audioTrack from Agora
  useEffect(() => {
    const motionSync = motionSyncRef.current;
    // Ensure model is loaded (MotionSync is optional)
    if (!isModelLoaded) return;

    if (audioTrack?.getMediaStreamTrack) {
      console.log(
        "[Live2DCharacter] Received audioTrack, creating MediaStream."
      );
      isDisconnectingRef.current = false; // Reset disconnect flag

      // Create MediaStream from Agora audio track
      const stream = new MediaStream([audioTrack.getMediaStreamTrack()]);

      // Pass stream to motionSync for playback and lip sync (if available)
      if (motionSync && motionSyncEnabled && !isDisconnectingRef.current) {
        try {
          // Wrap in a promise to catch any async errors
          Promise.resolve()
            .then(() => {
              if (!isDisconnectingRef.current && motionSync) {
                motionSync.play(stream);
                console.log(
                  "[Live2DCharacter] MotionSync audio playback started"
                );
              }
            })
            .catch((playError) => {
              console.error(
                "[Live2DCharacter] MotionSync play promise error:",
                playError
              );
              setMotionSyncEnabled(false);
            });
        } catch (motionSyncPlayError) {
          console.error(
            "[Live2DCharacter] MotionSync play error:",
            motionSyncPlayError
          );
          setMotionSyncEnabled(false);
          // Continue without MotionSync if it fails
        }
      } else {
        console.log(
          "[Live2DCharacter] MotionSync not available or disabled, audio will play without lip sync"
        );
      }

      // Also create and play hidden <audio> element to ensure actual sound
      try {
        if (!audioElementRef.current) {
          const audio = document.createElement("audio");
          audio.autoplay = true;
          // playsInline needed in iOS/Safari to avoid fullscreen
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
            console.warn(
              "[Live2DCharacter] Autoplay blocked, waiting for user gesture.",
              err
            );
          });
        }
      } catch (err) {
        console.error("[Live2DCharacter] Failed to play audio:", err);
      }

      // Reset lip sync when audio track ends
      audioTrack.getMediaStreamTrack().onended = () => {
        console.log("[Live2DCharacter] Audio track ended.");
        isDisconnectingRef.current = true; // Set disconnect flag

        if (motionSync && !isDisconnectingRef.current) {
          try {
            motionSync.reset();
          } catch (resetError) {
            console.error(
              "[Live2DCharacter] MotionSync reset error:",
              resetError
            );
          }
        }
        if (audioElementRef.current) {
          try {
            audioElementRef.current.pause();
            audioElementRef.current.srcObject = null;
            audioElementRef.current.remove();
          } catch {}
          audioElementRef.current = null;
        }
      };
    } else {
      // If no audioTrack (including null during disconnect), reset lip sync
      console.log("[Live2DCharacter] No audioTrack, resetting MotionSync.");
      isDisconnectingRef.current = true; // Set disconnect flag

      if (motionSync && !isDisconnectingRef.current) {
        try {
          // Add a small delay to ensure any ongoing audio processing completes
          setTimeout(() => {
            try {
              if (!isDisconnectingRef.current && motionSync) {
                motionSync.reset();
              }
            } catch (resetError) {
              console.error(
                "[Live2DCharacter] MotionSync reset error:",
                resetError
              );
            }
          }, 100);
        } catch (resetError) {
          console.error(
            "[Live2DCharacter] MotionSync reset error:",
            resetError
          );
        }
      }
      if (audioElementRef.current) {
        try {
          audioElementRef.current.pause();
          audioElementRef.current.srcObject = null;
          audioElementRef.current.remove();
        } catch {}
        audioElementRef.current = null;
      }
    }

    return () => {
      // Clean up and reset when component unmounts or audioTrack changes
      isDisconnectingRef.current = true; // Set disconnect flag

      if (motionSync && !isDisconnectingRef.current) {
        try {
          motionSync.reset();
        } catch (resetError) {
          console.error(
            "[Live2DCharacter] MotionSync reset error:",
            resetError
          );
        }
      }
      if (audioElementRef.current) {
        try {
          audioElementRef.current.pause();
          audioElementRef.current.srcObject = null;
          audioElementRef.current.remove();
        } catch {}
        audioElementRef.current = null;
      }
    };
  }, [audioTrack, isModelLoaded, motionSyncEnabled]);

  // Component unmount cleanup
  useEffect(() => {
    return () => {
      console.log(
        "[Live2DCharacter] Component unmounting, cleaning up all resources"
      );

      // Clean up MotionSync first with delay
      isDisconnectingRef.current = true; // Set disconnect flag

      if (motionSyncRef.current) {
        try {
          setTimeout(() => {
            try {
              if (!isDisconnectingRef.current && motionSyncRef.current) {
                motionSyncRef.current.reset();
              }
            } catch (resetError) {
              console.error(
                "[Live2DCharacter] MotionSync reset error during unmount:",
                resetError
              );
            }
          }, 50);
        } catch (error) {
          console.error("[Live2DCharacter] MotionSync cleanup error:", error);
        }
        motionSyncRef.current = null;
      }

      // Clean up audio element
      if (audioElementRef.current) {
        try {
          audioElementRef.current.pause();
          audioElementRef.current.srcObject = null;
          audioElementRef.current.remove();
        } catch {}
        audioElementRef.current = null;
      }

      // Clean up PIXI app last
      if (appRef.current) {
        try {
          // Stop the application first
          appRef.current.stop();

          // Destroy with aggressive cleanup
          appRef.current.destroy(true, {
            children: true,
            texture: true,
            baseTexture: true,
          });

          // Clear the canvas context
          if (canvasRef.current) {
            const ctx = canvasRef.current.getContext("2d");
            if (ctx) {
              ctx.clearRect(
                0,
                0,
                canvasRef.current.width,
                canvasRef.current.height
              );
            }
          }
        } catch (destroyError) {
          console.error(
            "[Live2DCharacter] PIXI app destroy error:",
            destroyError
          );
        }
        appRef.current = null;
      }
    };
  }, []);

  // Show loading state during SSR or before client hydration
  if (!isClient) {
    return (
      <div
        className={cn(
          "relative h-full w-full bg-gradient-to-b from-blue-50 to-blue-100 dark:from-gray-900 dark:to-gray-800",
          className
        )}
      >
        <div className="absolute inset-0 flex items-center justify-center">
          <div className="text-center">
            <div className="mx-auto mb-4 h-12 w-12 animate-spin rounded-full border-primary border-b-2"></div>
            <p className="text-muted-foreground">Loading Live2D Model...</p>
          </div>
        </div>
      </div>
    );
  }

  return (
    <div className={cn("relative h-full w-full", className)}>
      <canvas
        ref={canvasRef}
        key={`live2d-canvas-${modelPath}`} // Force canvas recreation when model changes
        style={{ display: "block" }}
      />
      {!isModelLoaded && (
        <div className="absolute inset-0 z-10 flex items-center justify-center bg-black bg-opacity-50 text-white">
          <div className="text-center">
            <div className="mx-auto mb-4 h-12 w-12 animate-spin rounded-full border-white border-b-2"></div>
            <p>Loading Live2D Model...</p>
          </div>
        </div>
      )}
    </div>
  );
}
