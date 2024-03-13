import { ARStreamingLib, LogInterval, ParseResponseResult, SessionSettings, VideoCompressionSettings } from "../../lib/build/ar-streaming-lib";

const SERVER_URL = "wss://bugwright.vr.rwth-aachen.de/interactive-3d-streaming/ws";

// http://www.cse.yorku.ca/~oz/hash.html
function djb2(str: string): number {
    let hash = 5381;
    for (let i = 0; i < str.length; ++i) {
        const c = str.charCodeAt(i);
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

const insertInto = (key: string, value: any, out: {[key: string]: number}): {[key: string]: number} => {
    switch (typeof value) {
        case 'number':
            out[key] = value;
            break;

        case 'string':
            out[key] = djb2(value);
            break;

        case 'boolean':
            out[key] = value ? 1 : 0;
            break;

        case 'object':
            if (Array.isArray(value)) {
                for (let i = 0; i < value.length; ++i) {
                    insertInto(`${key}[${i}]`, value[i], out);
                }
            } else {
                const optionalPeriod = key.length > 0 ? "." : "";
                for (const [objKey, objVal] of Object.entries(value)) {
                    insertInto(`${key}${optionalPeriod}${objKey}`, objVal, out);
                }
            }
            break;
    }

    return out;
}


class Connection {
    private participantId: number;
    private condition: number;
    private settings: SessionSettings;
    private socket: WebSocket;
    private streamingLib: ARStreamingLib;
    private shouldDisconnect = false;
    private requestId = 0;
    private logEntries: {[key: string]: string[]} = {};
    private unsentPackets: Uint8Array[] = [];
    onConnect?: () => void;
    onResponse?: (response: ParseResponseResult) => void;

    constructor(streamingLib: ARStreamingLib, participantId: number, condition: number, settings: SessionSettings) {
        this.settings = settings;
        this.streamingLib = streamingLib;
        this.participantId = participantId;
        this.condition = condition;
        this.socket = this.connect();
    }

    private connect(): WebSocket {
        this.socket = new WebSocket(`${SERVER_URL}?participantId=${this.participantId}&condition=${this.condition}`);
        this.socket.binaryType = "arraybuffer";
        this.socket.onopen = () => {
            if (this.onConnect) {
                this.onConnect();
            }
            console.log("connection established, sending initialization packet");
            this.socket.send(this.streamingLib.createSessionInitPacket(this.settings));
            for (const packet of this.unsentPackets.splice(0, this.unsentPackets.length)) {
                this.socket.send(packet);
            }
        };
        this.socket.onmessage = event => {
            const receiveTimestamp = performance.now();
            if (!(event.data instanceof ArrayBuffer)) {
                console.error("invalid data type");
                return;
            }
            if (this.onResponse) {
                // const beforeParsing = performance.now();
                const parsedResponse = this.streamingLib.parseResponse(event.data);
                // const afterParsing = performance.now();
                // console.log(`parsing time: ${afterParsing - beforeParsing}ms`);


                if (parsedResponse) {
                    parsedResponse.receiveTimestamp = receiveTimestamp;
                    parsedResponse.messageSize = event.data.byteLength;
                    this.onResponse(parsedResponse);
                }
            }
        };
        this.socket.onclose = () => {
            if (!this.shouldDisconnect) {
                this.logEntries = {}
                console.warn("connection closed unexpectedly, try to reconnect in 3 seconds");
                setTimeout(() => this.connect(), 3000);
            }
        };
        this.socket.onerror = () => {
            console.error("WebSocket error");
        };
        return this.socket;
    }

    disconnect() {
        this.shouldDisconnect = true;
        this.socket.close();
    }

    request(x: number, y: number, z: number): number {
        if (this.socket.readyState === this.socket.OPEN) {
            this.socket.send(this.streamingLib.createRequestPacket(this.requestId, x, y, z));
        }
        const requestedId = this.requestId;
        this.requestId++;
        return requestedId;
    }

    changeVideoCompressionSettings(settings: VideoCompressionSettings) {
        this.sendPacket(this.streamingLib.createVideoCompressionSettingsPacket(settings));
    }

    changeMeshGenerationSettings(settings: MeshGenerationSettings) {
        this.sendPacket(this.streamingLib.createMeshGenerationSettingsPacket(settings));
    }

    logMessage(level: "trace" | "debug" | "info" | "warn" | "error" | "critical", message: any) {
        if (this.streamingLib) {
            let messageString;
            if (typeof message === "string") {
                messageString = message;
            } else {
                messageString = JSON.stringify(message, undefined, 2);
            }
            this.sendPacket(this.streamingLib.createLogPacket(level, messageString));
        }

    }

    info(message: any) {
        this.logMessage("info", message);
    }

    logValues(frequency: LogInterval, obj: any) {
        if (this.socket.readyState === WebSocket.OPEN) {
            const values = insertInto("", obj, {});
            if (this.logEntries[frequency] === undefined) {
                this.logEntries[frequency] = Object.keys(values);
                this.socket.send(this.streamingLib.createLogInitPacket(frequency, this.logEntries[frequency]));
            }
            this.socket.send(this.streamingLib.createLogWritePacket(frequency, this.logEntries[frequency], values));
        }
    }

    sendPacket(packet: Uint8Array) {
        if (this.socket.readyState === this.socket.OPEN) {
            this.socket.send(packet);
        } else {
            this.unsentPackets.push(packet);
        }
    }
}

export default Connection;
