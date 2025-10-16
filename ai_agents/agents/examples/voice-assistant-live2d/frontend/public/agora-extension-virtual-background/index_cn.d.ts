
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
 * @public
 *
 * 插件实例的属性。用于 {@link setOptions} 方法。
 */
export declare type VirtualBackgroundEffectOptions = {
    /**
     * 设置背景类型。
     *
     * "color": 纯色
     * "img": 图片
     * "blur": 虚化
     * "none": 无背景(透明)
     */
    type: string;
    /**
     * 设置背景颜色，值为有效的CSS颜色。
     *
     */
    color?: string;
    /**
     * 设置自定义背景图。
     *
     * 如果出现 "texture bound to texture unit 2 is not renderable. It might be non-power-of-2 or have incompatible texture filtering (maybe)?" 报错，请检查图片的分辨率相乘后是否是 2 的倍数。
     */
    source?: HTMLImageElement | HTMLVideoElement;
    /**
     * 设置背景模糊程度。
     *
     * 1: 轻度
     * 2: 中度
     * 3: 重度
     */
    blurDegree?: Number;
    /**
     * 背景填充方式。
     *
     * "contain": 保持原图比例显示完整背景，用黑色填充不足部分
     * "cover": 保持原图比例，覆盖显示区域，裁切超出内容
     * "fill": 拉伸背景以填充显示区域
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
