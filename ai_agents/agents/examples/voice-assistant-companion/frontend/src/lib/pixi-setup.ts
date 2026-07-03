// PIXI setup module - ensures PIXI is available globally before any Live2D operations
import * as PIXI from 'pixi.js';

// Set up PIXI globally for pixi-live2d-display compatibility
if (typeof window !== 'undefined') {
    // @ts-ignore
    window.PIXI = PIXI;
    // @ts-ignore
    globalThis.PIXI = PIXI;
}

// Export PIXI for direct use
export { PIXI };
export default PIXI;
