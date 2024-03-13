export type VideoCodec = "h264" | "h265" | "av1";
export interface SessionSettings {
    layerCount: number;
    resolution: number;
    nearPlane: number;
    farPlane: number;
    meshGeneration: "loop" | "line";
    videoCodec: VideoCodec;
    chromaSubsampling: boolean;
    scene: string;
    sceneScale: number;
    sceneExposure: number;
    sceneIndirectIntensity: number;
}

export interface ConstantBitrateVideoCompressionSettings {
    framerate: number;
    bitrate: number;
}

export interface ConstantQualityVideoCompressionSettings {
    framerate: number;
    quality: number;
}

export type VideoCompressionSettings = ConstantBitrateVideoCompressionSettings | ConstantQualityVideoCompressionSettings;

export interface LineBasedSettings {
    laplaceThreshold: number;
    normalScale: number;
    lineLengthMin: number;
}

export interface LoopBasedSettings {
    depthBaseThreshold: number;
    depthSlopeThreshold: number;
    normalThreshold: number;
    triangleScale: number;
    useNormals: boolean;
    useObjectIds: boolean;
}

export interface LayerSettings {
    depthBaseThreshold: number;
    depthSlopeThreshold: number;
    useObjectIds: boolean;
}

export interface LineBasedMeshSettings {
    depthMax: number;
    lineBased: LineBasedSettings;
}

export interface LoopBasedMeshSettings {
    depthMax: number;
    loopBased: LoopBasedSettings;
}

export type MeshSettings = LineBasedMeshSettings | LoopBasedMeshSettings;

export interface MeshGenerationSettings {
    layer: LayerSettings;
    mesh: MeshSettings;
}

export type Matrix = [number, number, number, number, number, number, number, number, number, number, number, number, number, number, number, number];

export interface MeshLayerData {
    requestId: number;
    layerIndex: number;
    vertexCounts: [number, number, number, number, number, number];
    indexCounts: [number, number, number, number, number, number];
    viewMatrices: [Matrix, Matrix, Matrix, Matrix, Matrix, Matrix];
    geometry: Uint8Array;
    frame: Uint8Array;
    viewStatistics: any;
}

export interface MeshLayerDataResponse {
    type: "meshLayer";
    meshLayer: MeshLayerData;
}

export type ParseResponseResult = {
    receiveTimestamp: number;
    messageSize: number;
} & (MeshLayerDataResponse);

export interface DecodeGeometryResult {
    verticesPointer: number;
    verticesByteLength: number;
    indicesPointer: number;
    indicesByteLength: number;
    rawDecodeTime: number;
}

export type LogInterval = "perSession" | "perLayerUpdate" | "perFrame";

export class ARStreamingLib {
    createLogPacket(level: "trace" | "debug" | "info" | "warn" | "error" | "critical", message: string): Uint8Array;
    createLogInitPacket(interval: LogInterval, entries: string[]): Uint8Array;
    createLogWritePacket(interval: LogInterval, entries: string[], values: {[entry: string]: number}): Uint8Array;
    createSessionInitPacket(settings: SessionSettings): Uint8Array;
    createMeshGenerationSettingsPacket(settings: MeshGenerationSettings): Uint8Array;
    createVideoCompressionSettingsPacket(settings: VideoCompressionSettings): Uint8Array;
    createRequestPacket(id: number, x: number, y: number, z: number): Uint8Array;
    parseResponse(response: ArrayBuffer): ParseResponseResult | null;
    decodeMesh(geometry: Uint8Array): DecodeGeometryResult | null;
}

declare function createARStreamingLib(): Promise<ARStreamingLib>;

export default createARStreamingLib;
