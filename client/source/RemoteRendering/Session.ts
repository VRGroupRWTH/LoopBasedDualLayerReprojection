import { mat3, mat4, vec3, vec4 } from "gl-matrix";
import Connection, { SERVER_URL } from "./Connection";
import FrameDecoder from "./FrameDecoder";
import GeometryDecoder from "./GeometryDecoder";
import Layer, { LayerLogData } from "./Layer";
import Mesh from "./Mesh";
import Renderer, { FrameData, Mode, createProjectionMatrix } from "./Renderer";
import { MainModule, Matrix, SessionCreateForm } from "../../wrapper/binary/wrapper";
import { CONDITION_CHROMA_SUBSAMPLING, CONDITION_FAR_PLANE, CONDITION_FILE_NAME, CONDITION_NEAR_PLANE, CONDITION_RESOLUTION, CONDITION_VIDEO_CODEC, TIME_BETWEEN_RUNS, Technique } from "../Conditions";

const TIMEOUT = 5 * 60 * 1000;
const MOUSE_SENSITIVITY = 0.01;
const MOVEMENT_SPEED = 1.0;

interface UpdateLogData {
    layer: LayerLogData[];
    timestamp_request: number;
    timestamp_mesh_complete: number;
    rtt: number;
}

export interface SessionConfig {
    technique: Technique;
    interval: number;
    run: number;
    replayData?: FrameData[];
    sessionType: "benchmark" | "replay1" | "replay2";
}

class Session {
    private wrapper: MainModule;
    connection: Connection;
    private config: SessionConfig;
    private intervalTimer: number;
    private renderer: Renderer;
    private frameDecoders: [FrameDecoder, FrameDecoder];
    private geometryDecoder: GeometryDecoder;
    private inFlightMeshes = new Map<number, Mesh>();
    private requestTimes = new Map<number, number>();
    private freeLayers: Layer[] = [];
    private startTime?: number;
    private settings: SessionCreateForm;
    private layerLogs: UpdateLogData[] = [];
    private currentReplayFrame = 0;

    constructor(wrapper: MainModule, canvas: HTMLCanvasElement, config: SessionConfig, private finished: () => void) {
        this.wrapper = wrapper;
        this.config = config;
        if (this.config.sessionType === "benchmark") {
            this.intervalTimer = setInterval(() => this.update(), this.config.interval);
        } else {
            setTimeout(() => this.update(), TIME_BETWEEN_RUNS);
        }

        const projectionMatrix = mat4.create();

        if (this.config.sessionType === "replay2") {
            mat4.copy(
                projectionMatrix,
                createProjectionMatrix(Math.floor(canvas.clientWidth), Math.floor(canvas.clientHeight))
            );
        } else {
            mat4.perspective(projectionMatrix, Math.PI / 2, 1.0, 0.1, 1000.0);
        }

        let layerCount = 1;
        if (this.config.technique.name === "dual-layer" && this.config.sessionType !== "replay2") {
            layerCount = 2;
        }

        // Connecion setup
        this.settings = {
            mesh_generator: this.wrapper.MeshGeneratorType.MESH_GENERATOR_TYPE_LINE,
            video_codec: this.wrapper.VideoCodecType.VIDEO_CODEC_TYPE_H264,
            video_use_chroma_subsampling: CONDITION_CHROMA_SUBSAMPLING ? 1 : 0,
            projection_matrix: projectionMatrix as Matrix,
            resolution_width: CONDITION_RESOLUTION,
            resolution_height: CONDITION_RESOLUTION,
            layer_count: layerCount,
            view_count: this.config.sessionType === "replay2" ? 1 : 6,
            scene_file_name: CONDITION_FILE_NAME,
            scene_scale: 1.0,
            scene_exposure: 1.0,
            scene_indirect_intensity: 1.0,
            sky_file_name: "",
            sky_intensity: 1.0,
            export_enabled: false,// this.config.sessionType === "replay1",
        }
        this.connection = new Connection(wrapper, this.settings);
        this.connection.onConnect = () => {
        };
        this.connection.onResponse = response => {
            const layerInfo = response.layerInfo;

            this.connection.info(`received update for ${layerInfo.request_id}`);

            if (this.config.sessionType === "replay2") {
                // ignore the update!
                return;
            }

            let mesh = this.inFlightMeshes.get(layerInfo.request_id);
            if (!mesh) {
                mesh = new Mesh(layerInfo.request_id, Array.apply(null, Array(this.settings.layer_count)).map(() => this.allocateLayer()));
                this.inFlightMeshes.set(layerInfo.request_id, mesh);
            }

            mesh.layers[layerInfo.layer_index].reset(layerInfo.index_counts, layerInfo.vertex_counts, layerInfo.view_matrices, response.layerInfo);
            this.frameDecoders[layerInfo.layer_index].decode(layerInfo.request_id, response.image);
            this.geometryDecoder.decode(layerInfo.request_id, layerInfo.layer_index, response.geometry);

            // const a = document.createElement('a');
            // const blob = new Blob([layerData.frame.buffer], {type: "octet/stream"});
            // const url = window.URL.createObjectURL(blob);
            // a.href = url;
            // a.download = "test.h264";
            // a.click();
        };

        this.renderer = new Renderer(canvas, this.config, this.connection, finished);

        // Video decoder setup
        this.frameDecoders = [
            new FrameDecoder(CONDITION_VIDEO_CODEC),
            new FrameDecoder(CONDITION_VIDEO_CODEC),
        ];
        for (let i = 0; i < 2; ++i) {
            this.frameDecoders[i].onFrameDecoded = decodedFrame => {
                const mesh = this.inFlightMeshes.get(decodedFrame.requestId);
                if (!mesh) {
                    console.error(`no in-flight mesh found for request id: ${decodedFrame.requestId}`);
                    decodedFrame.frame.close();
                    return;
                }
                const l = mesh.layers[i].logData
                if (l) {
                    l.time_image_decode = decodedFrame.decodingTime;
                }
                mesh.layers[i].uploadTexture(decodedFrame.frame);
                this.updateCurrentMeshWhenComplete(mesh);
                decodedFrame.frame.close();
            };
        }

        // Geometry decoder setup
        this.geometryDecoder = new GeometryDecoder();
        this.geometryDecoder.onGeometryDecoded = event => {
            const mesh = this.inFlightMeshes.get(event.requestId);
            if (!mesh) {
                console.error(`no in-flight mesh found for request id: ${event.requestId}`);
                return;
            }
            const l = mesh.layers[event.layerIndex].logData
            if (l) {
                l.time_geometry_decode = event.decodeTime;
                l.time_geometry_decode_raw = event.rawDecodeTime;
            }
            mesh.layers[event.layerIndex].uploadGeometry(event.indices, event.vertices);
            this.updateCurrentMeshWhenComplete(mesh);
        };
    }

    update() {
        if (this.config.sessionType === "benchmark") {
            console.log(this.renderer.getPosition());
            const requestId = this.connection.request(this.renderer.getPosition());
            this.requestTimes.set(requestId, performance.now());
        } else if (this.config.replayData) {
            if (this.currentReplayFrame >= this.config.replayData.length) {
                this.finished();
                return;
            }
            const frame = this.config.replayData[this.currentReplayFrame];

            if (this.config.sessionType === "replay1") {
                if (this.renderer.mesh?.requestId !== frame.requestId) { // Do we need a new mesh?
                    if (this.requestTimes.get(frame.requestId) === undefined) { // Was the mesh already been requested?
                        this.connection.requestWithId(frame.srcPosition, frame.requestId);
                        this.requestTimes.set(frame.requestId, performance.now());
                        console.log(frame.srcPosition);
                    }
                    setTimeout(() => this.update(), 100); // give the server some time to generate the mesh!
                } else {
                    this.renderer.setCameraTransform(frame.dstPosition, frame.dstOrientation);
                    this.renderer.renderFrame();
                    this.currentReplayFrame++;
                    setTimeout(() => this.update(), 5); // here we can go faster
                }
            } else if (this.config.sessionType === "replay2") {
                this.connection.requestWithId(frame.dstPosition, frame.requestId);
                console.log(frame.dstPosition);
                this.currentReplayFrame++;
                setTimeout(() => this.update(), 100); // give the server some time
            }
        }
    }

    close() {
        this.connection.info("close session!");
        clearInterval(this.intervalTimer);
        this.connection.disconnect();
        this.renderer.destroy();

        fetch(`http://${SERVER_URL}/files/${this.config.technique.name}/${this.config.interval}/${this.config.run}-updates.json?type=log`, {
            method: 'POST',
            body: JSON.stringify(this.layerLogs),
            mode: 'no-cors',
        });
    }

    allocateLayer() {
        const layer = this.freeLayers.pop();
        if (layer) {
            return layer;
        } else {
            return new Layer(
                this.renderer.gl,
                this.renderer.program,
                CONDITION_NEAR_PLANE,
                CONDITION_FAR_PLANE,
                CONDITION_RESOLUTION,
                CONDITION_CHROMA_SUBSAMPLING,
            );
        }
    }

    updateCurrentMeshWhenComplete(mesh: Mesh) {
        if (mesh.complete()) {
            const now = performance.now();
            const requestTime = this.requestTimes.get(mesh.requestId) || NaN;
            const logData: UpdateLogData = {
                timestamp_mesh_complete: now,
                timestamp_request: requestTime,
                rtt: now - requestTime,
                layer: mesh.layers.map(l => structuredClone(l.logData))
            };
            this.layerLogs.push(logData);
            this.requestTimes.delete(mesh.requestId);

            this.inFlightMeshes.delete(mesh.requestId);
            const oldMesh = this.renderer.mesh;
            this.renderer.mesh = mesh;

            if (oldMesh) {
                for (const layer of oldMesh.layers) {
                    layer.invalidate();
                    this.freeLayers.push(layer);
                }
            }
        }
    }
}

export default Session;
