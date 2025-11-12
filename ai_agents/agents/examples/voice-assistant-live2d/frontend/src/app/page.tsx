"use client";

import React, { useCallback, useEffect, useRef, useState } from "react";

// Force dynamic rendering
export const dynamic = "force-dynamic";

import dynamicImport from "next/dynamic";
import { Baloo_2, Quicksand } from "next/font/google";
import type {
  ExpressionConfig,
  Live2DHandle,
  MouthConfig,
  MotionConfig,
} from "@/components/Live2DCharacter";

// Dynamically import Live2D component to prevent SSR issues
const ClientOnlyLive2D = dynamicImport(
  () => import("@/components/ClientOnlyLive2D"),
  {
    ssr: false,
    loading: () => (
      <div className="flex h-full items-center justify-center">
        <div className="text-center">
          <div className="mx-auto mb-4 h-12 w-12 animate-spin rounded-full border-white border-b-2"></div>
          <p className="text-white/70">Loading Live2D Model...</p>
        </div>
      </div>
    ),
  }
);

import { apiPing, apiStartService, apiStopService } from "@/lib/request";
import type { AgoraConfig, Live2DModel, TranscriptMessage } from "@/types";

type VoiceCommandRule = {
  triggers: string[];
  expressions?: string[];
  reset?: boolean;
  resetFirst?: boolean;
};

type BackgroundTheme = {
  baseColor: string;
  primaryGradient: string;
  radialOverlay: {
    gradient: string;
    opacity: number;
  };
  patternOverlay: {
    image: string;
    opacity: number;
  };
  accentOverlay: {
    image: string;
    opacity: number;
  };
};

type CharacterProfile = Live2DModel & {
  headline: string;
  description: string;
  quote: string;
  voiceType: "male" | "female";
  mouthConfig: MouthConfig;
  expressions?: ExpressionConfig[];
  motions?: MotionConfig[];
  backgroundTheme: BackgroundTheme;
  connectionGreeting?: string;
  agentGreeting: string;
  floatingElements?: FloatingElement[];
  immersiveStage?: boolean;
};

type FloatingElementType =
  | "flower"
  | "swirl"
  | "spark"
  | "heart"
  | "star"
  | "melon_seed"
  | "corn_cob"
  | "snow_cookie"
  | "hot_spring_steam"
  | "water_leaf"
  | "citrus_slice"
  | "herb_bundle"
  | "water_ripple";

type FloatingElement = {
  type: FloatingElementType;
  top: number;
  left: number;
  size: number;
  rotate: number;
  scale: number;
  animation: string;
  duration: number;
  delay: string;
  opacity: number;
};

const DEFAULT_REMOTE_MODELS_BASE_URL =
  "https://ten-framework-assets.s3.amazonaws.com/live2d-models";

const remoteModelsBaseUrl = (
  process.env.NEXT_PUBLIC_LIVE2D_REMOTE_MODELS_BASE_URL ||
  DEFAULT_REMOTE_MODELS_BASE_URL
).replace(/\/$/, "");

const buildRemoteModelAssetPath = (folder: string, fileName: string) =>
  `${remoteModelsBaseUrl}/${folder}/${fileName}`;

const defaultFloatingElements: FloatingElement[] = [
  {
    type: "flower",
    top: 6,
    left: 10,
    size: 140,
    rotate: -8,
    scale: 1,
    animation: "float0",
    duration: 16,
    delay: "0s",
    opacity: 0.92,
  },
  {
    type: "flower",
    top: 32,
    left: 72,
    size: 120,
    rotate: 12,
    scale: 0.9,
    animation: "float2",
    duration: 18,
    delay: "1.5s",
    opacity: 0.85,
  },
  {
    type: "swirl",
    top: 18,
    left: 52,
    size: 160,
    rotate: 4,
    scale: 1.1,
    animation: "float1",
    duration: 20,
    delay: "0.8s",
    opacity: 0.75,
  },
  {
    type: "spark",
    top: 58,
    left: 18,
    size: 150,
    rotate: -14,
    scale: 1,
    animation: "float2",
    duration: 17,
    delay: "2.2s",
    opacity: 0.8,
  },
  {
    type: "spark",
    top: 74,
    left: 70,
    size: 130,
    rotate: 18,
    scale: 0.95,
    animation: "float0",
    duration: 15,
    delay: "1s",
    opacity: 0.78,
  },
  {
    type: "swirl",
    top: 72,
    left: 42,
    size: 120,
    rotate: -6,
    scale: 0.85,
    animation: "float1",
    duration: 19,
    delay: "2.6s",
    opacity: 0.72,
  },
  {
    type: "flower",
    top: 82,
    left: 5,
    size: 110,
    rotate: 6,
    scale: 0.8,
    animation: "float0",
    duration: 22,
    delay: "3s",
    opacity: 0.65,
  },
  {
    type: "flower",
    top: 12,
    left: 82,
    size: 110,
    rotate: -18,
    scale: 0.8,
    animation: "float1",
    duration: 18,
    delay: "2s",
    opacity: 0.7,
  },
  {
    type: "heart",
    top: 40,
    left: 12,
    size: 90,
    rotate: 14,
    scale: 0.85,
    animation: "float2",
    duration: 14,
    delay: "0.4s",
    opacity: 0.78,
  },
  {
    type: "heart",
    top: 22,
    left: 88,
    size: 80,
    rotate: -22,
    scale: 0.75,
    animation: "float0",
    duration: 16,
    delay: "1.8s",
    opacity: 0.8,
  },
  {
    type: "star",
    top: 8,
    left: 46,
    size: 100,
    rotate: 6,
    scale: 0.9,
    animation: "float1",
    duration: 13,
    delay: "0.6s",
    opacity: 0.68,
  },
  {
    type: "star",
    top: 62,
    left: 88,
    size: 95,
    rotate: -12,
    scale: 0.82,
    animation: "float2",
    duration: 21,
    delay: "2.4s",
    opacity: 0.7,
  },
  {
    type: "heart",
    top: 66,
    left: 30,
    size: 85,
    rotate: -6,
    scale: 0.8,
    animation: "float0",
    duration: 18,
    delay: "1.2s",
    opacity: 0.74,
  },
  {
    type: "flower",
    top: 50,
    left: 52,
    size: 105,
    rotate: 18,
    scale: 0.88,
    animation: "float1",
    duration: 19,
    delay: "3.2s",
    opacity: 0.82,
  },
  {
    type: "star",
    top: 86,
    left: 60,
    size: 90,
    rotate: 10,
    scale: 0.78,
    animation: "float2",
    duration: 17,
    delay: "2.8s",
    opacity: 0.72,
  },
];

const kevinFloatingElements: FloatingElement[] = [
  {
    type: "corn_cob",
    top: 12,
    left: 14,
    size: 170,
    rotate: -12,
    scale: 1,
    animation: "float1",
    duration: 22,
    delay: "0s",
    opacity: 0.85,
  },
  {
    type: "melon_seed",
    top: 34,
    left: 68,
    size: 100,
    rotate: 22,
    scale: 0.95,
    animation: "float0",
    duration: 18,
    delay: "1.4s",
    opacity: 0.9,
  },
  {
    type: "snow_cookie",
    top: 56,
    left: 18,
    size: 140,
    rotate: -10,
    scale: 1,
    animation: "float2",
    duration: 20,
    delay: "0.8s",
    opacity: 0.82,
  },
  {
    type: "melon_seed",
    top: 72,
    left: 84,
    size: 110,
    rotate: -26,
    scale: 1,
    animation: "float1",
    duration: 17,
    delay: "2.6s",
    opacity: 0.88,
  },
  {
    type: "corn_cob",
    top: 68,
    left: 52,
    size: 160,
    rotate: 18,
    scale: 0.88,
    animation: "float0",
    duration: 21,
    delay: "1.8s",
    opacity: 0.8,
  },
  {
    type: "snow_cookie",
    top: 18,
    left: 82,
    size: 120,
    rotate: -18,
    scale: 0.9,
    animation: "float2",
    duration: 19,
    delay: "1.1s",
    opacity: 0.8,
  },
  {
    type: "melon_seed",
    top: 42,
    left: 32,
    size: 118,
    rotate: -4,
    scale: 0.92,
    animation: "float1",
    duration: 16,
    delay: "0.5s",
    opacity: 0.84,
  },
  {
    type: "corn_cob",
    top: 82,
    left: 8,
    size: 150,
    rotate: 12,
    scale: 0.95,
    animation: "float2",
    duration: 23,
    delay: "3s",
    opacity: 0.78,
  },
];

const chubbieFloatingElements: FloatingElement[] = [
  {
    type: "hot_spring_steam",
    top: 18,
    left: 22,
    size: 200,
    rotate: -6,
    scale: 1.05,
    animation: "float0",
    duration: 22,
    delay: "0s",
    opacity: 0.7,
  },
  {
    type: "water_leaf",
    top: 36,
    left: 70,
    size: 150,
    rotate: 18,
    scale: 1,
    animation: "float2",
    duration: 19,
    delay: "1.3s",
    opacity: 0.78,
  },
  {
    type: "citrus_slice",
    top: 64,
    left: 16,
    size: 140,
    rotate: -12,
    scale: 0.95,
    animation: "float1",
    duration: 20,
    delay: "0.9s",
    opacity: 0.82,
  },
  {
    type: "water_ripple",
    top: 78,
    left: 72,
    size: 190,
    rotate: 0,
    scale: 1,
    animation: "float0",
    duration: 24,
    delay: "1.7s",
    opacity: 0.65,
  },
  {
    type: "herb_bundle",
    top: 30,
    left: 86,
    size: 130,
    rotate: -24,
    scale: 0.9,
    animation: "float1",
    duration: 18,
    delay: "2s",
    opacity: 0.76,
  },
  {
    type: "hot_spring_steam",
    top: 50,
    left: 30,
    size: 160,
    rotate: 8,
    scale: 1,
    animation: "float2",
    duration: 21,
    delay: "2.4s",
    opacity: 0.7,
  },
  {
    type: "water_leaf",
    top: 12,
    left: 58,
    size: 140,
    rotate: -18,
    scale: 0.9,
    animation: "float1",
    duration: 17,
    delay: "1s",
    opacity: 0.74,
  },
  {
    type: "citrus_slice",
    top: 84,
    left: 10,
    size: 150,
    rotate: 16,
    scale: 0.9,
    animation: "float2",
    duration: 23,
    delay: "2.8s",
    opacity: 0.8,
  },
];
const characterOptions: CharacterProfile[] = [
  {
    id: "kei",
    name: "Kei",
    path: buildRemoteModelAssetPath("kei_vowels_pro", "kei_vowels_pro.model3.json"),
    preview: buildRemoteModelAssetPath("kei_vowels_pro", "preview.svg"),
    headline: "Your Charming Clever Companion",
    description:
      "Kei is a friendly guide who lights up every conversation. Connect with her for thoughtful answers, gentle encouragement, and a dash of anime sparkle whenever you need it.",
    quote: "Hi! I’m Kei. Let me know how I can make your day easier.",
    voiceType: "female",
    mouthConfig: {
      type: "open",
      openId: "ParamMouthOpenY",
      formId: "ParamMouthForm",
    },
    backgroundTheme: {
      baseColor: "#fff9fd",
      primaryGradient:
        "linear-gradient(160deg,#ffeaf3 0%,#fffaf2 40%,#e3f1ff 100%)",
      radialOverlay: {
        gradient:
          "radial-gradient(circle at top,#ffdff2 0%,transparent 60%),radial-gradient(circle at bottom,#d2e8ff 0%,transparent 65%)",
        opacity: 0.75,
      },
      patternOverlay: {
        image:
          "repeating-linear-gradient(135deg, rgba(255, 255, 255, 0.32) 0px, rgba(255, 255, 255, 0.32) 2px, transparent 2px, transparent 18px)",
        opacity: 0.2,
      },
      accentOverlay: {
        image:
          "repeating-radial-gradient(circle at 20% 20%, rgba(255, 204, 224, 0.25) 0px, rgba(255, 204, 224, 0.25) 12px, transparent 12px, transparent 48px)",
        opacity: 0.22,
      },
    },
    connectionGreeting: "My name is Kei.",
    agentGreeting:
      "My name is Kei, nice to meet you! I’m your anime assistant. What’s your name?",
  },
  {
    id: "mao",
    name: "Mao",
    path: buildRemoteModelAssetPath("mao_pro", "mao_pro.model3.json"),
    preview: buildRemoteModelAssetPath("mao_pro", "preview.svg"),
    headline: "Your Calm & Empathetic Storyteller",
    description:
      "Mao listens with patience and responds with lyrical warmth. Invite them for unhurried chats, reflective insights, and a gentle rhythm to balance out your day.",
    quote: "Hello, I’m Mao. I’m all ears whenever you want to share.",
    voiceType: "female",
    mouthConfig: {
      type: "corners",
      upId: "ParamMouthUp",
      downId: "ParamMouthDown",
    },
    backgroundTheme: {
      baseColor: "#f8fbff",
      primaryGradient:
        "linear-gradient(160deg,#f0f7ff 0%,#fef1ff 45%,#e6fff7 100%)",
      radialOverlay: {
        gradient:
          "radial-gradient(circle at top,#dcedff 0%,transparent 58%),radial-gradient(circle at bottom,#f7e0ff 0%,transparent 64%)",
        opacity: 0.78,
      },
      patternOverlay: {
        image:
          "repeating-linear-gradient(140deg, rgba(255, 255, 255, 0.28) 0px, rgba(255, 255, 255, 0.28) 3px, transparent 3px, transparent 20px)",
        opacity: 0.18,
      },
      accentOverlay: {
        image:
          "repeating-radial-gradient(circle at 75% 25%, rgba(211, 227, 255, 0.26) 0px, rgba(211, 227, 255, 0.26) 14px, transparent 14px, transparent 52px)",
        opacity: 0.24,
      },
    },
    connectionGreeting: "My name is Mao.",
    agentGreeting:
      "My name is Mao, and I’m here to listen with you. What should we talk about?",
    expressions: [
      { name: "neutral", label: "Neutral", default: true },
      { name: "gentle_smile", label: "Gentle Smile", onSpeaking: true },
      { name: "bright", label: "Bright Eyes" },
    ],
    motions: [
      {
        name: "soft_swirl",
        label: "Soft Idle",
        group: "Idle",
        index: 0,
        autoPlay: true,
        loop: true,
        priority: 1,
      },
      {
        name: "delight",
        label: "Delight Gesture",
        group: "Gesture",
        index: 0,
        onSpeakingStart: true,
        priority: 2,
      },
      {
        name: "wave",
        label: "Friendly Wave",
        group: "Special",
        index: 2,
        priority: 2,
      },
    ],
  },
  {
    id: "kevin",
    name: "Kevin the Marmot",
    path: buildRemoteModelAssetPath("marmot", "L065.model3.json"),
    preview: buildRemoteModelAssetPath(
      "marmot",
      "a0e01b1556549807c52770f1d517fb9.png"
    ),
    headline: "Your Snack-Fueled Hype Marmot",
    description:
      "Kevin the Marmot keeps spirits high with cozy chatter, snack recs, and solid productivity nudges. Drop in for grounded advice, quick laughs, and the warmest marmot energy on the planet.",
    quote: "Yo! Kevin the Marmot here. Ready to hustle, snack, or both?",
    voiceType: "male",
    mouthConfig: {
      type: "open",
      openId: "ParamMouthOpenY",
      formId: "ParamMouthForm",
    },
    backgroundTheme: {
      baseColor: "#eefbf0",
      primaryGradient:
        "linear-gradient(150deg,#ecffe7 0%,#dcf7e4 40%,#c6f0d5 100%)",
      radialOverlay: {
        gradient:
          "radial-gradient(circle at top,#bdf3c8 0%,transparent 64%),radial-gradient(circle at bottom,#d9ffe9 0%,transparent 70%)",
        opacity: 0.62,
      },
      patternOverlay: {
        image:
          "repeating-linear-gradient(130deg, rgba(255, 255, 255, 0.32) 0px, rgba(255, 255, 255, 0.32) 3px, transparent 3px, transparent 18px)",
        opacity: 0.2,
      },
      accentOverlay: {
        image:
          "repeating-radial-gradient(circle at 18% 22%, rgba(122, 200, 135, 0.32) 0px, rgba(122, 200, 135, 0.32) 12px, transparent 12px, transparent 46px)",
        opacity: 0.28,
      },
    },
    connectionGreeting: "My name is Kevin the Marmot.",
    agentGreeting:
      "My name is Kevin the Marmot! Ready to hustle, snack, or plan something fun together?",
    floatingElements: kevinFloatingElements,
    immersiveStage: true,
    expressions: [
      { name: "neutral", label: "Relaxed", default: true },
      { name: "greet", label: "Big Smile", onSpeaking: true },
      { name: "cheeky", label: "Cheeky" },
    ],
    motions: [
      {
        name: "idle",
        label: "Cheerful Idle",
        group: "Idle",
        index: 0,
        autoPlay: true,
        loop: true,
        priority: 1,
      },
      {
        name: "bite_one",
        label: "Snack Bite",
        group: "Snack",
        index: 0,
        onSpeakingStart: true,
        priority: 2,
      },
      {
        name: "bite_two",
        label: "Chomp Again",
        group: "Snack",
        index: 1,
        priority: 2,
      },
    ],
  },
  {
    id: "chubbie",
    name: "Chubbie",
    path: buildRemoteModelAssetPath(
      "capybara",
      "capybara_v001_bear_rabbit.model3.json"
    ),
    preview: buildRemoteModelAssetPath("capybara", "icon.jpg"),
    headline: "Your Laid-Back Capybara Confidant",
    description:
      "Chubbie the Capybara brings spa-day calm, steady encouragement, and snack-time strategy. Settle in for mellow vibes, gentle guidance, and the coziest companion energy around.",
    quote: "Hey there, I’m Chubbie. Fancy a soak, a snack, or some easy wins?",
    voiceType: "male",
    mouthConfig: {
      type: "open",
      openId: "ParamMouthOpenY",
      formId: "ParamMouthForm",
    },
    backgroundTheme: {
      baseColor: "#f6f0e6",
      primaryGradient:
        "linear-gradient(148deg,#fff2d8 0%,#ffe7d3 36%,#e6f4ff 98%)",
      radialOverlay: {
        gradient:
          "radial-gradient(circle at top,#ffe0ba 0%,transparent 58%),radial-gradient(circle at bottom,#cde8ff 0%,transparent 70%)",
        opacity: 0.72,
      },
      patternOverlay: {
        image:
          "repeating-linear-gradient(142deg, rgba(255, 255, 255, 0.26) 0px, rgba(255, 255, 255, 0.26) 3px, transparent 3px, transparent 20px)",
        opacity: 0.2,
      },
      accentOverlay: {
        image:
          "repeating-radial-gradient(circle at 32% 78%, rgba(214, 170, 118, 0.32) 0px, rgba(214, 170, 118, 0.32) 14px, transparent 14px, transparent 52px)",
        opacity: 0.28,
      },
    },
    connectionGreeting: "My name is Chubbie.",
    agentGreeting:
      "I’m Chubbie the Capybara. Let’s take it easy—what can I help you relax or focus on today?",
    floatingElements: chubbieFloatingElements,
    immersiveStage: true,
    expressions: [
      { name: "toggle_blush_tab_r", label: "Blush Glow" },
      { name: "toggle_bow_tab_b", label: "Bow Accessory" },
      { name: "toggle_apron_m_3", label: "Apron Ready" },
      { name: "toggle_maid_hairclip_m_1", label: "Hair Clip" },
      { name: "toggle_monocle_g_3", label: "Monocle" },
      { name: "toggle_glasses_g_1", label: "Reading Glasses" },
      { name: "toggle_sunglasses_g_2", label: "Sun Shades" },
      { name: "toggle_black_dress_m_2", label: "Black Outfit" },
      { name: "toggle_bathtub_tab_q", label: "Hot Tub Mode" },
      { name: "toggle_orange_m_4", label: "Orange Snack" },
      { name: "toggle_mustache_tab_h", label: "Whisker Stache" },
      { name: "toggle_dark_face_tab_d", label: "Shadow Mood" },
      { name: "toggle_bear_rabbit_tab_9", label: "Creator Cameo" },
    ],
    motions: [
      {
        name: "breathing_idle",
        label: "Gentle Float",
        group: "Idle",
        index: 0,
        autoPlay: true,
        loop: true,
        priority: 1,
      },
      {
        name: "neon_glow",
        label: "Neon Highlight",
        group: "Special",
        index: 0,
        onSpeakingStart: true,
        loop: true,
        priority: 2,
      },
    ],
  },
];

const headlineFont = Baloo_2({
  subsets: ["latin"],
  weight: ["600", "700"],
});

const subtitleFont = Quicksand({
  subsets: ["latin"],
  weight: ["400", "500"],
});

const renderFloatingShape = (type: FloatingElementType): JSX.Element | null => {
  switch (type) {
    case "flower":
      return (
        <svg viewBox="0 0 120 120" className="h-full w-full">
          <path
            d="M60 14 C 44 20 32 38 36 52 C 20 50 18 64 32 71 C 24 84 36 101 52 95 C 56 108 70 108 74 95 C 90 101 102 84 94 71 C 108 64 106 50 90 52 C 94 38 82 20 66 14 C 63 12 61 12 60 14 Z"
            fill="rgba(255,190,213,0.28)"
            stroke="#ff92bb"
            strokeWidth={4}
            strokeLinejoin="round"
          />
          <path
            d="M60 34 C 50 38 44 48 46 56 C 38 56 34 62 38 68 C 34 74 40 84 50 80 C 52 90 68 90 70 80 C 80 84 86 74 82 68 C 86 62 82 56 74 56 C 76 48 70 38 60 34 Z"
            fill="rgba(255,245,250,0.6)"
            stroke="#ff92bb"
            strokeWidth={3}
            strokeLinejoin="round"
          />
          <circle cx="60" cy="60" r="6" fill="#ff92bb" opacity="0.75" />
        </svg>
      );
    case "swirl":
      return (
        <svg viewBox="0 0 140 140" className="h-full w-full">
          <path
            d="M30 86 C 24 52 58 34 86 40 C 108 46 124 70 112 92 C 102 110 78 116 62 104 C 52 96 52 76 68 70 C 78 66 88 74 84 84"
            fill="none"
            stroke="#8fb9ff"
            strokeWidth={6}
            strokeLinecap="round"
          />
          <path
            d="M44 38 C 58 26 90 26 110 46"
            fill="none"
            stroke="#c9deff"
            strokeWidth={4}
            strokeLinecap="round"
            strokeDasharray="12 14"
          />
          <circle cx="90" cy="102" r="6" fill="#d5e7ff" opacity="0.9" />
        </svg>
      );
    case "spark":
      return (
        <svg viewBox="0 0 140 140" className="h-full w-full">
          <path
            d="M70 14 L78 56 L118 48 L86 72 L104 110 L70 88 L36 110 L54 72 L22 48 L62 56 Z"
            fill="rgba(255,235,196,0.5)"
            stroke="#ffce7a"
            strokeWidth={5}
            strokeLinejoin="round"
          />
          <path
            d="M70 32 L74 54 L96 50 L78 64 L86 84 L70 72 L54 84 L62 64 L44 50 L66 54 Z"
            fill="rgba(255,248,225,0.7)"
            stroke="#ffce7a"
            strokeWidth={3}
            strokeLinejoin="round"
          />
        </svg>
      );
    case "heart":
      return (
        <svg viewBox="0 0 120 120" className="h-full w-full">
          <path
            d="M60 98 C 58 96 18 70 18 44 C 18 30 30 20 44 20 C 52 20 58 24 60 30 C 62 24 68 20 76 20 C 90 20 102 30 102 44 C 102 70 62 96 60 98 Z"
            fill="rgba(255,205,223,0.55)"
            stroke="#ff8fb4"
            strokeWidth={5}
            strokeLinejoin="round"
          />
          <path
            d="M45 42 C 46 34 54 32 60 38"
            fill="none"
            stroke="#ffe1ec"
            strokeWidth={4}
            strokeLinecap="round"
            strokeDasharray="10 8"
          />
        </svg>
      );
    case "star":
      return (
        <svg viewBox="0 0 120 120" className="h-full w-full">
          <path
            d="M60 12 L70 44 L104 46 L78 68 L86 102 L60 84 L34 102 L42 68 L16 46 L50 44 Z"
            fill="rgba(255,248,210,0.6)"
            stroke="#ffd27f"
            strokeWidth={4.5}
            strokeLinejoin="round"
          />
          <path
            d="M60 26 L66 44 L84 46 L70 58 L74 76 L60 66 L46 76 L50 58 L36 46 L54 44 Z"
            fill="rgba(255,242,220,0.75)"
            stroke="#ffd27f"
            strokeWidth={3}
            strokeLinejoin="round"
          />
          <circle cx="60" cy="72" r="5" fill="#ffe7b1" opacity="0.9" />
        </svg>
      );
    case "melon_seed":
      return (
        <svg viewBox="0 0 60 100" className="h-full w-full">
          <path
            d="M30 4 C 16 20 10 54 30 96 C 50 54 44 20 30 4 Z"
            fill="#5d3a19"
          />
          <path
            d="M30 18 C 22 32 20 54 30 82 C 40 54 38 32 30 18 Z"
            fill="#f3d5a2"
            opacity="0.85"
          />
        </svg>
      );
    case "corn_cob":
      return (
        <svg viewBox="0 0 160 220" className="h-full w-full">
          <path
            d="M80 206 C 46 168 42 112 80 26 C 118 112 114 168 80 206 Z"
            fill="#f9d55b"
          />
          <path
            d="M80 18 C 44 78 44 150 78 210"
            stroke="#4daa45"
            strokeWidth={22}
            strokeLinecap="round"
            opacity="0.9"
          />
          <path
            d="M80 18 C 116 78 116 150 82 210"
            stroke="#6ac14d"
            strokeWidth={22}
            strokeLinecap="round"
            opacity="0.82"
          />
          {[0, 1, 2, 3].map((row) =>
            [0, 1, 2].map((col) => (
              <rect
                key={`${row}-${col}`}
                x={56 + col * 18}
                y={62 + row * 24}
                width={14}
                height={18}
                rx={4}
                fill="#ffe796"
                opacity={0.9 - row * 0.08}
              />
            ))
          )}
        </svg>
      );
    case "snow_cookie":
      return (
        <svg viewBox="0 0 160 160" className="h-full w-full">
          <circle cx="80" cy="80" r="70" fill="#fff5da" stroke="#f3b27a" strokeWidth={5} />
          <circle cx="80" cy="80" r="46" fill="#ffe9bc" opacity="0.85" />
          {[{ x: 80, y: 32 }, { x: 104, y: 52 }, { x: 116, y: 84 }, { x: 56, y: 52 }, { x: 40, y: 88 }, { x: 84, y: 122 }].map(
            (dot, idx) => (
              <circle key={idx} cx={dot.x} cy={dot.y} r="6" fill="#f0c072" opacity="0.8" />
            )
          )}
          <path
            d="M80 46 L88 74 L118 80 L88 88 L80 114 L72 88 L42 80 L72 74 Z"
            fill="#fff9e8"
            opacity="0.8"
          />
        </svg>
      );
    case "hot_spring_steam":
      return (
        <svg viewBox="0 0 220 220" className="h-full w-full">
          <path
            d="M78 180 C 40 154 44 118 70 96 C 88 80 96 64 88 44 C 80 24 88 8 110 12 C 142 18 148 56 132 80 C 118 102 120 120 136 134 C 162 158 150 192 110 198 C 96 200 86 192 78 180 Z"
            fill="rgba(255,255,255,0.38)"
            stroke="rgba(255,210,182,0.6)"
            strokeWidth={6}
          />
          <path
            d="M120 190 C 92 166 96 140 116 124 C 132 112 136 92 126 74"
            stroke="rgba(255,210,182,0.6)"
            strokeWidth={6}
            strokeLinecap="round"
            opacity="0.7"
          />
        </svg>
      );
    case "water_leaf":
      return (
        <svg viewBox="0 0 180 180" className="h-full w-full">
          <path
            d="M90 14 C 46 52 34 118 68 164 C 110 154 142 110 150 64 C 154 42 132 10 90 14 Z"
            fill="#5fb897"
          />
          <path
            d="M92 30 C 60 60 50 110 76 150"
            stroke="#2f8568"
            strokeWidth={8}
            strokeLinecap="round"
            opacity="0.8"
          />
          <path
            d="M90 14 C 78 46 82 80 110 118"
            stroke="#93e6c7"
            strokeWidth={8}
            strokeLinecap="round"
            opacity="0.75"
          />
        </svg>
      );
    case "citrus_slice":
      return (
        <svg viewBox="0 0 180 180" className="h-full w-full">
          <circle cx="90" cy="90" r="80" fill="#ffe4b8" stroke="#f6aa66" strokeWidth={8} />
          {[0, 60, 120].map((angle) => (
            <path
              key={angle}
              d={`M90 90 L${90 + 70 * Math.cos((Math.PI / 180) * angle)} ${90 + 70 * Math.sin((Math.PI / 180) * angle)} A70 70 0 0 1 ${90 + 70 * Math.cos((Math.PI / 180) * (angle + 60))} ${90 + 70 * Math.sin((Math.PI / 180) * (angle + 60))} Z`}
              fill="#fff3d5"
              opacity="0.85"
            />
          ))}
          <circle cx="90" cy="90" r="16" fill="#fff" opacity="0.75" />
        </svg>
      );
    case "herb_bundle":
      return (
        <svg viewBox="0 0 160 200" className="h-full w-full">
          <path
            d="M80 160 C 74 130 60 96 34 62 C 66 80 90 100 108 130 C 124 156 114 182 90 188 C 84 190 82 174 80 160 Z"
            fill="#6d9d5b"
          />
          <path
            d="M86 158 C 92 128 112 104 144 82 C 122 116 110 146 110 176"
            stroke="#508044"
            strokeWidth={10}
            strokeLinecap="round"
            opacity="0.8"
          />
          <path
            d="M74 170 L86 176"
            stroke="#c68a4d"
            strokeWidth={10}
            strokeLinecap="round"
          />
        </svg>
      );
    case "water_ripple":
      return (
        <svg viewBox="0 0 220 220" className="h-full w-full">
          <circle cx="110" cy="110" r="90" fill="none" stroke="rgba(168,214,233,0.55)" strokeWidth={8} />
          <circle cx="110" cy="110" r="60" fill="none" stroke="rgba(168,214,233,0.4)" strokeWidth={6} />
          <circle cx="110" cy="110" r="32" fill="rgba(196,238,255,0.35)" />
        </svg>
      );
    default:
      return null;
  }
};

export default function Home() {
  const [isConnected, setIsConnected] = useState(false);
  const [isMuted, setIsMuted] = useState(false);
  const [isConnecting, setIsConnecting] = useState(false);
  const [selectedModel, setSelectedModel] = useState<CharacterProfile>(
    characterOptions[0]
  );
  const [remoteAudioTrack, setRemoteAudioTrack] = useState<any>(null);
  const [agoraService, setAgoraService] = useState<any>(null);
  const [pingInterval, setPingInterval] = useState<NodeJS.Timeout | null>(null);
  const [isAssistantSpeaking, setIsAssistantSpeaking] = useState(false);
  const live2dRef = useRef<Live2DHandle | null>(null);
  const [modelLoadedTick, setModelLoadedTick] = useState(0);
  const processedVoiceCommandIdsRef = useRef<string[]>([]);
  const handleModelLoaded = useCallback(
    () => setModelLoadedTick((tick) => tick + 1),
    []
  );
  const prevSpeakingRef = useRef(false);
  const applyVoiceRule = useCallback(async (rule: VoiceCommandRule) => {
    const controller = live2dRef.current;
    if (!controller) {
      return;
    }
    try {
      if (rule.reset) {
        await controller.setExpression(undefined);
        return;
      }
      const expressions = rule.expressions ?? [];
      const shouldResetFirst =
        rule.resetFirst !== undefined ? rule.resetFirst : expressions.length > 0;
      if (shouldResetFirst) {
        await controller.setExpression(undefined);
      }
      for (const expression of expressions) {
        await controller.setExpression(expression);
      }
    } catch (error) {
      console.warn("[VoiceCommand] Failed to apply voice-triggered expression", error);
    }
  }, []);

  useEffect(() => {
    // Dynamically import Agora service only on client side
    if (typeof window !== "undefined") {
      import("@/services/agora").then((module) => {
        const service = module.agoraService;
        setAgoraService(service);

        // Set up callbacks for Agora service
        service.setOnConnectionStatusChange(handleConnectionChange);
        service.setOnRemoteAudioTrack(handleAudioTrackChange);
      });
    }

    // Cleanup ping interval on unmount
    return () => {
      stopPing();
    };
  }, []);

  const handleConnectionChange = (status: any) => {
    setIsConnected(status.rtc === "connected");
  };

  const handleAudioTrackChange = (track: any) => {
    setRemoteAudioTrack(track);
  };

  const handleModelSelect = (modelId: string) => {
    const candidate = characterOptions.find((model) => model.id === modelId);
    if (candidate && candidate.id !== selectedModel.id) {
      prevSpeakingRef.current = false;
      setSelectedModel(candidate);
    }
  };

  useEffect(() => {
    let interval: NodeJS.Timeout | null = null;
    const track = remoteAudioTrack;

    const readLevel = () => {
      if (!track) return 0;
      if (typeof track.getVolumeLevel === "function") {
        return track.getVolumeLevel();
      }
      if (typeof track.getCurrentLevel === "function") {
        return track.getCurrentLevel();
      }
      return 0;
    };

    if (track) {
      interval = setInterval(() => {
        try {
          const level = readLevel();
          const speaking = level > 0.05;
          setIsAssistantSpeaking((prev) =>
            prev === speaking ? prev : speaking
          );
        } catch (err) {
          console.warn("Unable to read remote audio level:", err);
          setIsAssistantSpeaking(false);
        }
      }, 160);
    } else {
      setIsAssistantSpeaking(false);
    }

    return () => {
      if (interval) {
        clearInterval(interval);
      }
    };
  }, [remoteAudioTrack]);

  useEffect(() => {
    prevSpeakingRef.current = false;
  }, [selectedModel.id, modelLoadedTick]);

  useEffect(() => {
    processedVoiceCommandIdsRef.current = [];
  }, [selectedModel.id]);

  useEffect(() => {
    const controller = live2dRef.current;
    if (!controller) {
      return;
    }
    const defaultExpression = selectedModel.expressions?.find((expression) => expression.default);
    if (defaultExpression) {
      void controller.setExpression(defaultExpression.name);
    } else {
      void controller.setExpression(undefined);
    }
    const idleMotion = selectedModel.motions?.find((motion) => motion.autoPlay);
    if (idleMotion) {
      void controller.playMotion(idleMotion.name, { priority: idleMotion.priority });
    }
  }, [selectedModel, modelLoadedTick]);

  useEffect(() => {
    const controller = live2dRef.current;
    const wasSpeaking = prevSpeakingRef.current;
    prevSpeakingRef.current = isAssistantSpeaking;
    if (!controller) {
      return;
    }
    if (isAssistantSpeaking && !wasSpeaking) {
      const speakingExpression = selectedModel.expressions?.find((expression) => expression.onSpeaking);
      if (speakingExpression) {
        void controller.setExpression(speakingExpression.name);
      }
      const speakingMotion = selectedModel.motions?.find((motion) => motion.onSpeakingStart);
      if (speakingMotion) {
        void controller.playMotion(speakingMotion.name, { priority: speakingMotion.priority });
      }
    } else if (!isAssistantSpeaking && wasSpeaking) {
      const defaultExpression = selectedModel.expressions?.find((expression) => expression.default);
      if (defaultExpression) {
        void controller.setExpression(defaultExpression.name);
      } else {
        void controller.setExpression(undefined);
      }
    }
  }, [isAssistantSpeaking, selectedModel]);

  useEffect(() => {
    if (!agoraService) {
      return;
    }

    const rules: VoiceCommandRule[] =
      selectedModel.id === "chubbie"
        ? [
            {
              triggers: [
                "change your outfit",
                "change outfit",
                "can you change your outfit",
                "switch your outfit",
              ],
              expressions: ["toggle_black_dress_m_2"],
              resetFirst: true,
            },
            {
              triggers: [
                "default outfit",
                "back to normal outfit",
                "regular outfit",
                "original outfit",
              ],
              reset: true,
            },
            {
              triggers: [
                "remove your outfit",
                "take off your outfit",
                "outfit off",
                "clothes off",
              ],
              reset: true,
            },
            {
              triggers: ["wear the apron", "put on the apron", "apron on"],
              expressions: ["toggle_apron_m_3"],
              resetFirst: true,
            },
            {
              triggers: ["take off the apron", "remove the apron", "apron off"],
              reset: true,
            },
            {
              triggers: ["wear your sunglasses", "put on sunglasses", "sunglasses on"],
              expressions: ["toggle_sunglasses_g_2"],
              resetFirst: true,
            },
            {
              triggers: ["wear your glasses", "put on glasses", "glasses on", "reading glasses"],
              expressions: ["toggle_glasses_g_1"],
              resetFirst: true,
            },
          ]
        : selectedModel.id === "kevin"
        ? [
            {
              triggers: [
                "big smile",
                "give me a smile",
                "smile for me",
                "smile kevin",
              ],
              expressions: ["greet"],
              resetFirst: true,
            },
            {
              triggers: ["cheeky face", "be cheeky", "give me a cheeky grin"],
              expressions: ["cheeky"],
              resetFirst: true,
            },
            {
              triggers: ["relax face", "back to relaxed", "neutral face", "reset face"],
              reset: true,
            },
          ]
        : [];

    if (rules.length === 0) {
      return;
    }

    const targetModelId = selectedModel.id;

    const cleanup = agoraService.addTranscriptListener((message: TranscriptMessage) => {
      if (selectedModel.id !== targetModelId) {
        return;
      }
      if (message?.isUser === false) {
        return;
      }
      if (message.isFinal === false) {
        return;
      }
      const base = message.text?.toLowerCase() ?? "";
      if (!base.trim()) {
        return;
      }

      const clean = (value: string) =>
        value
          .toLowerCase()
          .replace(/[^a-z0-9\s]/g, " ")
          .replace(/\s+/g, " ")
          .trim();

      const removeStopWords = (value: string) =>
        value
          .replace(/\b(can|you|could|would|will|please|your|the|a|to)\b/g, " ")
          .replace(/\s+/g, " ")
          .trim();

      const primary = clean(base);
      const simplified = removeStopWords(primary);

      const haystacks = new Set<string>();
      if (primary) {
        haystacks.add(primary);
      }
      if (simplified) {
        haystacks.add(simplified);
      }
      if (haystacks.size === 0) {
        return;
      }

      const matchesTrigger = (trigger: string) => {
        const normalizedTrigger = clean(trigger);
        if (!normalizedTrigger) {
          return false;
        }
        const triggerWords = normalizedTrigger.split(" ");
        for (const hay of haystacks) {
          if (hay.includes(normalizedTrigger)) {
            return true;
          }
          const hayWords = hay.split(" ");
          if (triggerWords.every((word) => hayWords.includes(word))) {
            return true;
          }
        }
        return false;
      };

      const matchedRule = rules.find((rule) =>
        rule.triggers.some((trigger) => matchesTrigger(trigger))
      );
      if (!matchedRule) {
        return;
      }
      const processed = processedVoiceCommandIdsRef.current;
      if (processed.includes(message.id)) {
        return;
      }
      processed.push(message.id);
      if (processed.length > 200) {
        processed.shift();
      }
      void applyVoiceRule(matchedRule);
    });

    return cleanup;
  }, [agoraService, selectedModel.id, applyVoiceRule]);

  const startPing = () => {
    if (pingInterval) {
      stopPing();
    }
    const interval = setInterval(() => {
      apiPing("test-channel");
    }, 3000);
    setPingInterval(interval);
  };

  const stopPing = () => {
    if (pingInterval) {
      clearInterval(pingInterval);
      setPingInterval(null);
    }
  };

  const handleMicToggle = () => {
    if (agoraService) {
      try {
        if (isMuted) {
          agoraService.unmuteMicrophone();
          setIsMuted(false);
        } else {
          agoraService.muteMicrophone();
          setIsMuted(true);
        }
      } catch (error) {
        console.error("Error toggling microphone:", error);
      }
    }
  };

  const handleConnectToggle = async () => {
    if (agoraService) {
      try {
        if (isConnected) {
          setIsConnecting(true);
          // Stop the agent service first
          try {
            await apiStopService("test-channel");
            console.log("Agent stopped");
          } catch (error) {
            console.error("Failed to stop agent:", error);
          }

          await agoraService.disconnect();
          setIsConnected(false);
          stopPing(); // Stop ping when disconnecting
          setIsConnecting(false);
        } else {
          setIsConnecting(true);
          // Fetch Agora credentials from API server using the correct endpoint
          const response = await fetch("/api/token/generate", {
            method: "POST",
            headers: {
              "Content-Type": "application/json",
            },
            body: JSON.stringify({
              request_id: Math.random().toString(36).substring(2, 15),
              uid: Math.floor(Math.random() * 100000),
              channel_name: "test-channel",
            }),
          });

          if (!response.ok) {
            throw new Error(
              `Failed to get Agora credentials: ${response.statusText}`
            );
          }

          const responseData = await response.json();

          // Handle the response structure from agent server
          const credentials = responseData.data || responseData;

          const agoraConfig: AgoraConfig = {
            appId: credentials.appId || credentials.app_id,
            channel: credentials.channel_name,
            token: credentials.token,
            uid: credentials.uid,
          };

          console.log("Agora config:", agoraConfig);
          const success = await agoraService.connect(agoraConfig);
          if (success) {
            setIsConnected(true);

            // Sync microphone state with Agora service
            setIsMuted(agoraService.isMicrophoneMuted());

            // Start the agent service
            try {
              const startResult = await apiStartService({
                channel: agoraConfig.channel,
                userId: agoraConfig.uid || 0,
                graphName: "voice_assistant_live2d",
                language: "en",
                voiceType: selectedModel.voiceType,
                properties: {
                  llm: {
                    greeting: selectedModel.agentGreeting,
                  },
                  main_control: {
                    greeting: selectedModel.agentGreeting,
                  },
                },
              });

              console.log("Agent started:", startResult);
            } catch (error) {
              console.error("Failed to start agent:", error);
            }

            startPing();
          } else {
            throw new Error("Failed to connect to Agora");
          }
          setIsConnecting(false);
        }
      } catch (error) {
        console.error("Connection error:", error);
        setIsConnecting(false);
      }
    }
  };

  const renderCharacterSwitch = () => (
    <div className="flex w-full max-w-md flex-wrap items-center justify-center gap-3 rounded-full bg-white/60 p-2 shadow-sm backdrop-blur">
      {characterOptions.map((model) => {
        const isActive = model.id === selectedModel.id;
        return (
          <button
            key={model.id}
            type="button"
            onClick={() => handleModelSelect(model.id)}
            className={`rounded-full px-5 py-2 text-sm font-semibold transition ${
              isActive
                ? "bg-[#2f3dbd] text-white"
                : "bg-white/85 text-[#586094] hover:bg-white"
            }`}
          >
            {model.name}
          </button>
        );
      })}
    </div>
  );

  const backgroundTheme = selectedModel.backgroundTheme;
  const floatingElements =
    selectedModel.floatingElements ?? defaultFloatingElements;
  const floatingShadowColor =
    selectedModel.id === "kevin"
      ? "rgba(118, 205, 135, 0.38)"
      : selectedModel.id === "chubbie"
        ? "rgba(186, 140, 104, 0.38)"
        : "rgba(210, 180, 255, 0.35)";
  const isImmersiveStage = Boolean(selectedModel.immersiveStage);
  const stageWrapperClass = isImmersiveStage
    ? "relative w-full max-w-5xl"
    : "relative w-full max-w-3xl";
  const stageGlowClass = isImmersiveStage
    ? "-inset-20 absolute -z-10 rounded-[140px] blur-3xl pointer-events-none"
    : "-inset-5 absolute -z-10 rounded-[40px] bg-gradient-to-br from-[#ffe1f1]/60 via-[#d8ecff]/60 to-[#fff6d9]/60 blur-3xl pointer-events-none";
  const stageGlowStyle: React.CSSProperties | undefined = isImmersiveStage
    ? selectedModel.id === "kevin"
      ? {
          background:
            "radial-gradient(circle at 50% 24%, rgba(138,214,155,0.55) 0%, rgba(54,138,88,0.24) 46%, transparent 74%)",
          opacity: 0.6,
        }
      : {
          background:
            "radial-gradient(circle at 50% 30%, rgba(255,206,164,0.48) 0%, rgba(170,126,88,0.26) 50%, transparent 78%)",
          opacity: 0.7,
        }
    : undefined;
  const stageInnerClass = isImmersiveStage
    ? "relative z-10 flex flex-col items-center gap-6 px-2 pt-4 pb-8 md:px-6 md:pt-6 md:pb-12"
    : "relative z-10 overflow-hidden rounded-[32px] border border-white/80 bg-white/80 px-5 pt-6 pb-8 shadow-[0_24px_60px_rgba(200,208,255,0.35)] backdrop-blur-xl md:px-8";
  const stageHeaderClass = isImmersiveStage
    ? selectedModel.id === "kevin"
      ? "flex w-full items-center justify-between font-semibold text-[#2a5b37] text-[0.62rem] uppercase tracking-[0.32em]"
      : "flex w-full items-center justify-between font-semibold text-[#594434] text-[0.62rem] uppercase tracking-[0.32em]"
    : "flex w-full items-center justify-between font-semibold text-[#87a0ff] text-[0.6rem] uppercase tracking-[0.3em]";
  const headerIndicatorClass = isImmersiveStage
    ? isConnected
      ? "inline-flex h-2.5 w-2.5 rounded-full bg-[#4ecb7a]"
      : "inline-flex h-2.5 w-2.5 rounded-full bg-[#ffb0c1]"
    : isConnected
      ? "inline-flex h-2.5 w-2.5 rounded-full bg-[#7dd87d]"
      : "inline-flex h-2.5 w-2.5 rounded-full bg-[#ff9bae]";
  const stageCanvasWrapperClass = isImmersiveStage
    ? "relative mt-2 w-full"
    : "relative mt-4";
  const live2dClassName = isImmersiveStage
    ? selectedModel.id === "kevin"
      ? "h-[34rem] w-full md:h-[48rem] drop-shadow-[0_30px_90px_rgba(86,170,108,0.5)]"
      : "h-[34rem] w-full md:h-[48rem] drop-shadow-[0_30px_90px_rgba(174,130,90,0.48)]"
    : "h-[26rem] w-full rounded-[28px] border border-white/70 bg-gradient-to-b from-white/60 to-[#f5e7ff]/40 md:h-[34rem]";
  const quoteClass = isImmersiveStage
    ? selectedModel.id === "kevin"
      ? "mt-6 text-center text-[#2f5538] text-sm md:text-base font-medium"
      : "mt-6 text-center text-[#5b4635] text-sm md:text-base font-medium"
    : "mt-4 text-center text-[#6f6a92] text-xs md:text-sm";

  return (
    <div
      className="relative min-h-[100svh] overflow-hidden text-[#2f2d4b]"
      style={{ backgroundColor: backgroundTheme.baseColor }}
    >
      <div className="absolute inset-0">
        <div
          className="absolute inset-0"
          style={{ backgroundImage: backgroundTheme.primaryGradient }}
        />
        <div
          className="absolute inset-0 mix-blend-screen"
          style={{
            backgroundImage: backgroundTheme.radialOverlay.gradient,
            opacity: backgroundTheme.radialOverlay.opacity,
          }}
        />
        <div
          className="absolute inset-0"
          style={{
            backgroundImage: backgroundTheme.patternOverlay.image,
            opacity: backgroundTheme.patternOverlay.opacity,
          }}
        />
        <div
          className="absolute inset-0 mix-blend-multiply"
          style={{
            backgroundImage: backgroundTheme.accentOverlay.image,
            opacity: backgroundTheme.accentOverlay.opacity,
          }}
        />
      </div>

      <div className="pointer-events-none absolute inset-0 overflow-hidden">
        {floatingElements.map((item, idx) => (
          <div
            key={`${item.type}-${idx}`}
            className="absolute"
            style={{
              top: `${item.top}%`,
              left: `${item.left}%`,
              width: `${item.size}px`,
              height: `${item.size}px`,
              transform: `rotate(${item.rotate}deg) scale(${item.scale})`,
              animation: `${item.animation} ${item.duration}s ease-in-out infinite`,
              animationDelay: item.delay,
              opacity: item.opacity,
              filter: `drop-shadow(0 18px 40px ${floatingShadowColor})`,
            }}
          >
            {renderFloatingShape(item.type)}
          </div>
        ))}
      </div>

      <div className="relative z-10 flex min-h-[100svh] flex-col items-center justify-center gap-6 px-4 py-6 md:px-6 lg:gap-10">
        <header className="max-w-xl space-y-3 text-center lg:max-w-2xl">
          <span className="inline-flex items-center rounded-full bg-white/70 px-3.5 py-0.5 font-semibold text-[#ff79a8] text-[0.65rem] uppercase tracking-[0.25em] shadow-sm">
            Say hello to {selectedModel.name}
          </span>
          <h1
            className={`${headlineFont.className} text-3xl text-[#2f2d4b] leading-snug tracking-tight md:text-[2.75rem] md:leading-tight`}
          >
            {selectedModel.headline}
          </h1>
          <p
            className={`${subtitleFont.className} text-[#6f6a92] text-sm md:text-base`}
          >
            {selectedModel.description}
          </p>
        </header>

        <main className="flex w-full max-w-5xl flex-col items-center gap-8">
          {renderCharacterSwitch()}

          <div className={stageWrapperClass}>
            <div className={stageGlowClass} style={stageGlowStyle} />
            <div className={stageInnerClass}>
              <div className={stageHeaderClass}>
                <span>{selectedModel.name}</span>
                <span className="flex items-center gap-2">
                  <span className={headerIndicatorClass} />
                  {isConnected ? "Online" : "Waiting"}
                </span>
              </div>
              <div className={stageCanvasWrapperClass}>
                <ClientOnlyLive2D
                  key={selectedModel.id}
                  ref={live2dRef}
                  modelPath={selectedModel.path}
                  audioTrack={remoteAudioTrack}
                  mouthConfig={selectedModel.mouthConfig}
                  expressions={selectedModel.expressions}
                  motions={selectedModel.motions}
                  onModelLoaded={handleModelLoaded}
                  className={live2dClassName}
                />
              </div>
              <p className={quoteClass}>
                “{selectedModel.quote}”
              </p>
            </div>
          </div>

          <div className="flex w-full max-w-3xl flex-col items-center gap-4">
            <div className="flex flex-wrap items-center justify-center gap-2 font-medium text-[0.7rem] md:text-xs">
              <span
                className={`inline-flex items-center gap-2 rounded-full px-4 py-2 ${
                  isConnected
                    ? "bg-[#e6f8ff] text-[#236d94]"
                    : "bg-[#ffe8ef] text-[#b34f6a]"
                }`}
              >
                <span
                  className={`h-2.5 w-2.5 rounded-full ${
                    isConnected ? "bg-[#38a8d8]" : "bg-[#f0708f]"
                  }`}
                />
                {isConnected
                  ? selectedModel.connectionGreeting ??
                    `My name is ${selectedModel.name}.`
                  : "Not connected"}
              </span>
              <span
                className={`inline-flex items-center gap-2 rounded-full px-4 py-2 ${
                  isMuted
                    ? "bg-[#ffe8ef] text-[#b34f6a]"
                    : "bg-[#ecfce1] text-[#2f7d3e]"
                }`}
              >
                <span
                  className={`h-2.5 w-2.5 rounded-full ${
                    isMuted ? "bg-[#f0708f]" : "bg-[#4cc073]"
                  }`}
                />
                {isMuted ? "Mic muted" : "Mic open"}
              </span>
            </div>

            <div className="flex items-center justify-center gap-4">
              <button
                onClick={handleMicToggle}
                disabled={!isConnected}
                className={`relative flex h-14 w-14 items-center justify-center rounded-2xl border text-lg shadow-lg transition-all duration-200 ${
                  !isConnected
                    ? "cursor-not-allowed border-[#e9e7f7] bg-white text-[#b7b4c9] opacity-60"
                    : isMuted
                      ? "border-[#ffcfe0] bg-[#ffe7f0] text-[#b44f6c] hover:bg-[#ffd9e8]"
                      : "border-[#cde5ff] bg-[#e7f3ff] text-[#2f63a1] hover:bg-[#d8ecff]"
                }`}
              >
                {isMuted ? (
                  <svg className="h-6 w-6" fill="currentColor" viewBox="0 0 24 24">
                    <path d="M12 14c1.66 0 3-1.34 3-3V5c0-1.66-1.34-3-3-3S9 3.34 9 5v6c0 1.66 1.34 3 3 3z" />
                    <path d="M17 11c0 2.76-2.24 5-5 5s-5-2.24-5-5H5c0 3.53 2.61 6.43 6 6.92V21h2v-3.08c3.39-.49 6-3.39 6-6.92h-2z" />
                    <path
                      d="M3 3l18 18"
                      stroke="currentColor"
                      strokeLinecap="round"
                      strokeWidth="2"
                    />
                  </svg>
                ) : (
                  <svg className="h-6 w-6" fill="currentColor" viewBox="0 0 24 24">
                    <path d="M12 14c1.66 0 3-1.34 3-3V5c0-1.66-1.34-3-3-3S9 3.34 9 5v6c0 1.66 1.34 3 3 3z" />
                    <path d="M17 11c0 2.76-2.24 5-5 5s-5-2.24-5-5H5c0 3.53 2.61 6.43 6 6.92V21h2v-3.08c3.39-.49 6-3.39 6-6.92h-2z" />
                  </svg>
                )}
              </button>

              <button
                onClick={handleConnectToggle}
                disabled={isConnecting}
                className={`relative flex h-14 w-60 items-center justify-center gap-2 rounded-2xl border px-6 text-center font-semibold text-sm leading-tight shadow-lg transition-all duration-200 ${
                  isConnecting
                    ? "cursor-progress border-[#cde5ff] bg-[#e7f3ff] text-[#5a6a96]"
                    : isConnected
                      ? "border-[#ffcfe0] bg-[#ffe6f3] text-[#b44f6c] hover:bg-[#ffd9eb]"
                      : "border-[#cbeec4] bg-[#e7f8df] text-[#2f7036] hover:bg-[#def6d2]"
                }`}
              >
                {isConnecting ? (
                  <>
                    <svg
                      className="h-4 w-4 animate-spin"
                      fill="none"
                      viewBox="0 0 24 24"
                    >
                      <circle
                        className="opacity-25"
                        cx="12"
                        cy="12"
                        r="10"
                        stroke="currentColor"
                        strokeWidth="4"
                      />
                      <path
                        className="opacity-75"
                        fill="currentColor"
                        d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"
                      />
                    </svg>
                    <span className="text-center text-sm">
                      Calling {selectedModel.name}...
                    </span>
                  </>
                ) : isConnected ? (
                  <>
                    <svg className="h-4 w-4" fill="currentColor" viewBox="0 0 24 24">
                      <rect x="6" y="6" width="12" height="12" rx="2" />
                    </svg>
                    <span className="text-center text-sm">End session</span>
                  </>
                ) : (
                  <>
                    <svg className="h-4 w-4" fill="currentColor" viewBox="0 0 24 24">
                      <path d="M8 5v14l11-7z" />
                    </svg>
                    <span className="text-center text-sm">
                      Connect with {selectedModel.name}
                    </span>
                  </>
                )}
              </button>
            </div>
          </div>
        </main>
      </div>
    </div>
  );
}
