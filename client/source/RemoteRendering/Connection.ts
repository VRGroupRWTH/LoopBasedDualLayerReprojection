// import { ARStreamingLib, LogInterval, ParseResponseResult, SessionSettings, VideoCompressionSettings } from "../../lib/build/ar-streaming-lib";

import { mat4, vec3 } from "gl-matrix";
import { LayerResponseForm, MainModule, MeshSettingsForm, SessionCreateForm, VideoSettingsForm } from "../../wrapper/binary/wrapper";

// const SERVER_URL = "wss://bugwright.vr.rwth-aachen.de/interactive-3d-streaming/ws";
export const SERVER_URL = "localhost:9000";

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

interface ParseResponseResult {
    layerInfo: LayerResponseForm;
    receiveTimestamp: number;
    messageSize: number;
    geometry: Uint8Array;
    image: Uint8Array;
}


class Connection {
    private settings: SessionCreateForm;
    private socket: WebSocket;
    // private streamingLib: ARStreamingLib;
    private shouldDisconnect = false;
    private requestId = 0;
    private logEntries: {[key: string]: string[]} = {};
    private unsentPackets: Uint8Array[] = [];
    private wrapper: MainModule;
    onConnect?: () => void;
    onResponse?: (response: ParseResponseResult) => void;

    constructor(wrapper: MainModule, settings: SessionCreateForm) {
        this.settings = settings;
        this.wrapper = wrapper;
        this.socket = this.connect();
    }

    private connect(): WebSocket {
        console.log('connecting');
        this.socket = new WebSocket(`ws://${SERVER_URL}`);
        this.socket.binaryType = "arraybuffer";
        this.socket.onopen = () => {
            if (this.onConnect) {
                this.onConnect();
            }
            console.log("connection established, sending initialization packet");
            this.socket.send(this.wrapper.build_session_create_packet(this.settings));
            // for (const packet of this.unsentPackets.splice(0, this.unsentPackets.length)) {
            //     this.socket.send(packet);
            // }
        };
        this.socket.onmessage = event => {
            const receiveTimestamp = performance.now();
            if (!(event.data instanceof ArrayBuffer)) {
                console.error("invalid data type");
                return;
            }
            if (this.onResponse) {
                // const beforeParsing = performance.now();
                const data = new Uint8Array(event.data);
                const layerInfo = this.wrapper.parse_layer_response_packet(data);
                // const afterParsing = performance.now();
                // console.log(`parsing time: ${afterParsing - beforeParsing}ms`);


                if (layerInfo) {
                    const imageStart = data.length - layerInfo.image_bytes;
                    const geometryStart = imageStart - layerInfo.geometry_bytes;

                    const image = data.subarray(imageStart);
                    const geometry = data.subarray(geometryStart, imageStart);
                    this.onResponse({layerInfo, receiveTimestamp, messageSize: event.data.byteLength, image, geometry});
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

    request(p: vec3): number {
        if (this.socket.readyState === this.socket.OPEN) {
            const viewMatrices = [
                mat4.create(),
                mat4.create(),
                mat4.create(),
                mat4.create(),
                mat4.create(),
                mat4.create(),
            ];

            for (const m of viewMatrices) {
                mat4.translate(m, m, p);
                mat4.invert(m, m);
            }

            // const v0 = mat4.create();
            // mat4.translate(v0, v0, p);
            // mat

            // const v1 = mat4.create();
            // mat4.rotateY(v1, v1, Math.PI * 0.5);
            // mat4.translate(v1, v1, p);

            // const v2 = mat4.create();
            // mat4.rotateY(v2, v2, -Math.PI * 0.5);
            // mat4.translate(v2, v2, p);

            // const v3 = mat4.create();
            // mat4.rotateY(v3, v3, Math.PI);
            // mat4.translate(v3, v3, p);

            // const v4 = mat4.create();
            // mat4.rotateX(v4, v4, Math.PI * 0.5);
            // mat4.translate(v4, v4, p);

            // const v5 = mat4.create();
            // mat4.rotateX(v5, v5, -Math.PI * 0.5);
            // mat4.translate(v5, v5, p);

            this.socket.send(this.wrapper.build_render_request_packet({
                request_id: this.requestId,
                export_file_names: ["asd", "asd", "asd", "asd"],
                view_matrices: viewMatrices,

            }));
        }
        const requestedId = this.requestId;
        this.requestId++;
        return requestedId;
    }

    changeVideoCompressionSettings(settings: VideoSettingsForm) {
        this.sendPacket(this.wrapper.build_video_settings_packet(settings));
    }

    changeMeshGenerationSettings(settings: MeshSettingsForm) {
        this.sendPacket(this.wrapper.build_mesh_settings_packet(settings));
    }

    logMessage(level: "trace" | "debug" | "info" | "warn" | "error" | "critical", message: any) {
        // if (this.streamingLib) {
        //     let messageString;
        //     if (typeof message === "string") {
        //         messageString = message;
        //     } else {
        //         messageString = JSON.stringify(message, undefined, 2);
        //     }
        //     this.wrapper.
        //     this.sendPacket(this.streamingLib.createLogPacket(level, messageString));
        // }
    }

    info(message: any) {
        this.logMessage("info", message);
    }

    // logValues(frequency: LogInterval, obj: any) {
    //     // if (this.socket.readyState === WebSocket.OPEN) {
    //     //     const values = insertInto("", obj, {});
    //     //     if (this.logEntries[frequency] === undefined) {
    //     //         this.logEntries[frequency] = Object.keys(values);
    //     //         this.socket.send(this.streamingLib.createLogInitPacket(frequency, this.logEntries[frequency]));
    //     //     }
    //     //     this.socket.send(this.streamingLib.createLogWritePacket(frequency, this.logEntries[frequency], values));
    //     // }
    // }

    sendPacket(packet: Uint8Array) {
        if (this.socket.readyState === this.socket.OPEN) {
            this.socket.send(packet);
        } else {
            this.unsentPackets.push(packet);
        }
    }
}

export default Connection;
