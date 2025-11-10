/**
 * Audio Utilities for WebSocket Voice Assistant
 * Handles PCM audio recording, encoding, and playback
 */

export interface AudioConfig {
  sampleRate: number;
  channels: number;
  bitsPerSample: number;
}

export const DEFAULT_AUDIO_CONFIG: AudioConfig = {
  sampleRate: 16000,
  channels: 1,
  bitsPerSample: 16,
};

/**
 * Convert Float32Array PCM data to Int16Array
 */
export function float32ToInt16(buffer: Float32Array): Int16Array {
  const int16 = new Int16Array(buffer.length);
  for (let i = 0; i < buffer.length; i++) {
    const s = Math.max(-1, Math.min(1, buffer[i]));
    int16[i] = s < 0 ? s * 0x8000 : s * 0x7fff;
  }
  return int16;
}

/**
 * Convert Int16Array to base64 string
 */
export function int16ToBase64(int16: Int16Array): string {
  const uint8 = new Uint8Array(int16.buffer);
  let binary = "";
  for (let i = 0; i < uint8.length; i++) {
    binary += String.fromCharCode(uint8[i]);
  }
  return btoa(binary);
}

/**
 * Convert Float32Array PCM to base64 (complete pipeline)
 */
export function pcmToBase64(pcmData: Float32Array): string {
  const int16 = float32ToInt16(pcmData);
  return int16ToBase64(int16);
}

/**
 * Convert base64 string to Int16Array
 */
export function base64ToInt16(base64: string): Int16Array {
  const binary = atob(base64);
  const uint8 = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i++) {
    uint8[i] = binary.charCodeAt(i);
  }
  return new Int16Array(uint8.buffer);
}

/**
 * Convert Int16Array to Float32Array for playback
 */
export function int16ToFloat32(int16: Int16Array): Float32Array {
  const float32 = new Float32Array(int16.length);
  for (let i = 0; i < int16.length; i++) {
    float32[i] = int16[i] / (int16[i] < 0 ? 0x8000 : 0x7fff);
  }
  return float32;
}

/**
 * Audio Player class using Web Audio API
 */
export class AudioPlayer {
  private audioContext: AudioContext | null = null;
  private gainNode: GainNode | null = null;
  private audioQueue: Float32Array[] = [];
  private isPlaying = false;

  constructor(private config: AudioConfig = DEFAULT_AUDIO_CONFIG) {}

  async initialize(): Promise<void> {
    this.audioContext = new AudioContext({
      sampleRate: this.config.sampleRate,
    });
    this.gainNode = this.audioContext.createGain();
    this.gainNode.connect(this.audioContext.destination);
  }

  async playBase64Audio(base64Audio: string): Promise<void> {
    if (!this.audioContext || !this.gainNode) {
      await this.initialize();
    }

    const int16 = base64ToInt16(base64Audio);
    const float32 = int16ToFloat32(int16);

    this.audioQueue.push(float32);

    if (!this.isPlaying) {
      await this.playQueue();
    }
  }

  private async playQueue(): Promise<void> {
    if (!this.audioContext || !this.gainNode) return;

    this.isPlaying = true;

    while (this.audioQueue.length > 0) {
      const pcmData = this.audioQueue.shift();
      if (!pcmData) break;

      await this.playPCM(pcmData);
    }

    this.isPlaying = false;
  }

  private async playPCM(pcmData: Float32Array): Promise<void> {
    return new Promise((resolve) => {
      if (!this.audioContext || !this.gainNode) {
        resolve();
        return;
      }

      const audioBuffer = this.audioContext.createBuffer(
        this.config.channels,
        pcmData.length,
        this.config.sampleRate,
      );

      audioBuffer.getChannelData(0).set(pcmData);

      const source = this.audioContext.createBufferSource();
      source.buffer = audioBuffer;
      source.connect(this.gainNode);

      source.onended = () => resolve();
      source.start();
    });
  }

  setVolume(volume: number): void {
    if (this.gainNode) {
      this.gainNode.gain.value = Math.max(0, Math.min(1, volume));
    }
  }

  destroy(): void {
    this.audioQueue = [];
    this.isPlaying = false;
    if (this.audioContext) {
      this.audioContext.close();
      this.audioContext = null;
    }
  }
}

/**
 * Audio Recorder class using MediaRecorder and AudioWorklet
 */
export class AudioRecorder {
  private mediaStream: MediaStream | null = null;
  private audioContext: AudioContext | null = null;
  private sourceNode: MediaStreamAudioSourceNode | null = null;
  private processorNode: ScriptProcessorNode | null = null;
  private onDataCallback: ((base64: string) => void) | null = null;

  constructor(private config: AudioConfig = DEFAULT_AUDIO_CONFIG) {}

  async start(onData: (base64: string) => void): Promise<void> {
    this.onDataCallback = onData;

    // Request microphone access
    this.mediaStream = await navigator.mediaDevices.getUserMedia({
      audio: {
        sampleRate: this.config.sampleRate,
        channelCount: this.config.channels,
        echoCancellation: true,
        noiseSuppression: true,
        autoGainControl: true,
      },
    });

    // Make sure all audio tracks are enabled
    this.mediaStream.getAudioTracks().forEach((t) => (t.enabled = true));

    // Create audio context
    this.audioContext = new AudioContext({
      sampleRate: this.config.sampleRate,
    });

    // Some browsers start AudioContext in "suspended" even on user gesture.
    // Explicitly resume to ensure ScriptProcessor receives frames immediately.
    if (this.audioContext.state === "suspended") {
      try {
        await this.audioContext.resume();
      } catch (_) {
        // ignore — will resume below after connections
      }
    }

    // Create source from media stream
    this.sourceNode = this.audioContext.createMediaStreamSource(
      this.mediaStream,
    );

    // Create processor node (buffer size 4096)
    this.processorNode = this.audioContext.createScriptProcessor(4096, 1, 1);

    this.processorNode.onaudioprocess = (event) => {
      const inputData = event.inputBuffer.getChannelData(0);
      const base64 = pcmToBase64(inputData);

      if (this.onDataCallback) {
        this.onDataCallback(base64);
      }
    };

    // Connect nodes
    this.sourceNode.connect(this.processorNode);
    this.processorNode.connect(this.audioContext.destination);

    // Final resume to guarantee processing kicks in without needing a re-toggle
    if (this.audioContext.state !== "running") {
      try {
        await this.audioContext.resume();
      } catch (err) {
        console.warn("AudioContext resume failed:", err);
      }
    }
  }

  stop(): void {
    if (this.processorNode) {
      this.processorNode.disconnect();
      this.processorNode = null;
    }

    if (this.sourceNode) {
      this.sourceNode.disconnect();
      this.sourceNode = null;
    }

    if (this.mediaStream) {
      this.mediaStream.getTracks().forEach((track) => track.stop());
      this.mediaStream = null;
    }

    if (this.audioContext) {
      this.audioContext.close();
      this.audioContext = null;
    }

    this.onDataCallback = null;
  }

  getStream(): MediaStream | null {
    return this.mediaStream;
  }
}

/**
 * Calculate audio volume from PCM data
 */
export function calculateVolume(pcmData: Float32Array): number {
  let sum = 0;
  for (let i = 0; i < pcmData.length; i++) {
    sum += pcmData[i] * pcmData[i];
  }
  return Math.sqrt(sum / pcmData.length);
}

/**
 * Analyze audio frequencies for visualization
 */
export class AudioAnalyzer {
  private analyser: AnalyserNode | null = null;
  private dataArray: Uint8Array | null = null;

  constructor(
    private audioContext: AudioContext,
    private source: AudioNode,
  ) {
    this.analyser = this.audioContext.createAnalyser();
    this.analyser.fftSize = 256;
    this.dataArray = new Uint8Array(this.analyser.frequencyBinCount);
    this.source.connect(this.analyser);
  }

  getFrequencyData(): Uint8Array | null {
    if (!this.analyser || !this.dataArray) return null;
    this.analyser.getByteFrequencyData(this.dataArray);
    return this.dataArray;
  }

  destroy(): void {
    if (this.analyser) {
      this.analyser.disconnect();
      this.analyser = null;
    }
    this.dataArray = null;
  }
}
