export const EFFECT_ASSET_MAP: Record<string, string> = {
  gold_rain: "/festival/effects/gold-dollar.gif",
  fireworks: "/festival/effects/fireworks.svg",
};

export const FORTUNE_CARD_ASSET_MAP: Record<string, string> = {
  fortune_rich: "/festival/cards/fortune_rich.png",
  fortune_love: "/festival/cards/fortune_love.png",
  fortune_lazy: "/festival/cards/fortune_lazy.png",
  fortune_body: "/festival/cards/fortune_body.png",
  fortune_career: "/festival/cards/fortune_career.svg",
};

export const getEffectAsset = (effectName: string): string => {
  return EFFECT_ASSET_MAP[effectName] ?? EFFECT_ASSET_MAP.gold_rain;
};

export const getFortuneCardAsset = (imageId: string): string => {
  return FORTUNE_CARD_ASSET_MAP[imageId] ?? FORTUNE_CARD_ASSET_MAP.fortune_rich;
};
