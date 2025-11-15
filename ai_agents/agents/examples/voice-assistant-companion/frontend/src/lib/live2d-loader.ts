// Live2D loader that ensures PIXI is available before loading Live2D
import PIXI from './pixi-setup';

export async function loadLive2DModel() {
    // Wait a bit to ensure PIXI is fully set up
    await new Promise(resolve => setTimeout(resolve, 200));

    // Now dynamically import Live2D
    const { Live2DModel } = await import('pixi-live2d-display/cubism4');

    return { Live2DModel, PIXI };
}
