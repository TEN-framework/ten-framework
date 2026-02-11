"use client";

import React from "react";
import { getFortuneCardAsset, useAppDispatch, useAppSelector } from "@/common";
import { hideFortuneModal } from "@/store/reducers/global";

export default function FortuneModal() {
  const dispatch = useAppDispatch();
  const modal = useAppSelector((state) => state.global.fortuneModal);

  if (!modal?.open) {
    return null;
  }

  const cardSrc = getFortuneCardAsset(modal.imageId);

  return (
    <div className="fixed inset-0 z-[130] flex items-center justify-center">
      <button
        type="button"
        aria-label="Close fortune modal"
        className="absolute inset-0 bg-black/65"
        onClick={() => dispatch(hideFortuneModal())}
      />
      <div className="relative z-[131] w-[min(90vw,420px)] rounded-xl border border-[#7a5a20] bg-[#1a1208] p-4 shadow-2xl">
        <img
          src={cardSrc}
          alt={modal.imageId}
          className="h-auto w-full rounded-lg border border-[#8a6725]"
        />
        <button
          type="button"
          className="mt-4 w-full rounded-md bg-[#d8a53a] px-4 py-2 font-semibold text-[#2b1b06] hover:bg-[#e4b24c]"
          onClick={() => dispatch(hideFortuneModal())}
        >
          收下祝福
        </button>
      </div>
    </div>
  );
}
