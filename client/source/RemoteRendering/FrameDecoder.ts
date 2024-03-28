// import { VideoCodec } from "../../lib/build/ar-streaming-lib";

export interface DecodedFrame {
    requestId: number;
    frame: VideoFrame;
    decodingTime: number;
}

class FrameDecoder {
    private videoDecoder: VideoDecoder;
    private configured = false;
    private decodingStartTimes = new Map<number, number>();
    onFrameDecoded?: (decodedFrame: DecodedFrame) => void;

    constructor(private codec: "h264") {
        this.videoDecoder = new VideoDecoder({
            output: frame => {
                const decodingEndTime = performance.now();
                const requestId = frame.timestamp;
                const decodingStartTime = this.decodingStartTimes.get(requestId);
                if (typeof decodingStartTime === 'number') {
                    this.decodingStartTimes.delete(requestId);
                    if (this.onFrameDecoded) {
                        this.onFrameDecoded({
                            frame,
                            requestId,
                            decodingTime: decodingEndTime - decodingStartTime,
                        });
                    }
                } else {
                    console.warn(`cannot find decoding start time for request ${requestId}`);
                }
            },
            error: error => {
                console.error(error);
            },
        });
    }

    decode(requestId: number, frame: Uint8Array) {
        const hexByte = (n: number) => {
            const hex = n.toString(16);
            if (hex.length === 1) {
                return `0${hex}`;
            } else if (hex.length === 2) {
                return hex;
            } else {
                throw new Error(`${n} is not in a valid range (0 - 255)`);
            }
        }
        if (!this.configured) {
            let codec;
            switch (this.codec) {
                case "h264":
                    codec = `avc1.${hexByte(frame[5])}${hexByte(frame[6])}${hexByte(frame[7])}`;
                    break;

                default:
                    throw new Error(`invalid codec: ${this.codec}`);
            }

            console.log(`configure video decoder: ${codec}`);
            this.videoDecoder.configure({
                codec,
                // hardwareAcceleration: "prefer-hardware",
                optimizeForLatency: true,
            });
            this.configured = true;
        }

        this.decodingStartTimes.set(requestId, performance.now());
        this.videoDecoder.decode(new EncodedVideoChunk({
            type: "key",
            timestamp: requestId,
            data: frame,
        }));
    }
};

export default FrameDecoder;
