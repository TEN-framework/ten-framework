
import { EventEmitter } from 'events';
import { Extension } from 'agora-rte-extension';
import type { IBaseProcessor } from 'agora-rte-extension';
import type { IExtension } from 'agora-rte-extension';
import { IProcessorContext } from 'agora-rte-extension';
import { VideoProcessor } from 'agora-rte-extension';

export declare type IVirtualBackgroundExtension = IExtension<IVirtualBackgroundProcessor>;

export declare interface IVirtualBackgroundProcessor extends IBaseProcessor {
    init(wasmDir: string): Promise<void>;
    release(): Promise<void>;
    setOptions(options: VirtualBackgroundEffectOptions): void;
    onoverload?: () => void;
    getProcessedTrack(): Promise<MediaStreamTrack | null>;
}

/**
 * Options of the virtual background plugin. Used when calling {@link setOptions}.
 */
export declare type VirtualBackgroundEffectOptions = {
    /**
     * Sets the background type.
     *
     * "color": pure color
     * "img": image
     * "blur": blurring
     */
    type: string;
    /**
     * Sets the background color, the value should be a CSS supported color.
     */
    color?: string;
    /**
     * Sets the background image.
     *
     * If the error "texture bound to texture unit 2 is not renderable. It might be non-power-of-2 or have incompatible texture filtering (maybe)?" appears, please check whether the product of the picture's width and height is a multiple of 2.
     */
    source?: HTMLImageElement | HTMLVideoElement;
    /**
     * Sets background blurring degree.
     *
     * 1: low
     * 2: medium
     * 3: high
     */
    blurDegree?: Number;
    /**
     * How an element responds to the height and width of its content box.
     *
     * "contain": increases or decreases the size of the background to fill the video whilst preserving its aspect-ratio
     * "cover": the background will fill the height and width of its box, once again maintaining its aspect ratio but often cropping the video.
     * "fill": stretches the background to fit the video.
     */
    fit?: 'contain' | 'cover' | 'fill';
};

declare class VirtualBackgroundExtension extends Extension<VirtualBackgroundProcessor> implements IVirtualBackgroundExtension {
    constructor();
    checkCompatibility(): boolean;
    protected _createProcessor(): VirtualBackgroundProcessor;
}
export default VirtualBackgroundExtension;

declare class VirtualBackgroundProcessor extends VideoProcessor implements IVirtualBackgroundProcessor {
    name: string;
    private segWorker;
    private processed_track;
    eventBus: EventEmitter;
    private analyzer;
    onoverload?: () => void;
    private initialized;
    private piped;
    private forceEnable;
    private avgCost;
    private stats;
    constructor();
    init(wasmDir: string): Promise<void>;
    setOptions(options: VirtualBackgroundEffectOptions): void;
    getProcessedTrack(): Promise<MediaStreamTrack | null>;
    protected onEnableChange(enabled: boolean): Promise<void>;
    private getStats;
    protected onTrack(inputTrack: MediaStreamTrack, context: IProcessorContext): Promise<void>;
    release(): Promise<void>;
    protected onPiped(context: IProcessorContext): void;
    protected onUnpiped(): void;
}

export { }
