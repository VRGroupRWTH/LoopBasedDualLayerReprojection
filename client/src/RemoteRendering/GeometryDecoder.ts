import DecoderWorker from "./GeometryDecodeWorker?worker";

interface GeometryDecodedEvent {
    requestId: number;
    layerIndex: number;
    decodeTime: number
    rawDecodeTime: number;
    vertices: Uint8Array;
    indices: Uint8Array;
};

class GeometryDecoder {
    private worker = new DecoderWorker() as Worker;
    onGeometryDecoded?: (event: GeometryDecodedEvent) => void;

    constructor() {
        this.worker.onmessage = event => {
            const decodeEndTime = performance.now();
            const decodeTime = decodeEndTime - event.data.decodeInitiationTime;
            if (this.onGeometryDecoded) {
                this.onGeometryDecoded({
                    requestId: event.data.requestId,
                    layerIndex: event.data.layerIndex,
                    decodeTime,
                    rawDecodeTime: event.data.rawDecodeTime,
                    vertices: event.data.vertices,
                    indices: event.data.indices,
                });
            }
        };
    }

    decode(requestId: number, layerIndex: number, data: Uint8Array) {
        const decodeInitiationTime = performance.now();
        this.worker.postMessage({ requestId, layerIndex, data, decodeInitiationTime }, [data.buffer]);
    }
}

export default GeometryDecoder;
