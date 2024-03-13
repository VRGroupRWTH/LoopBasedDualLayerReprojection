import { mat3, mat4, vec3, vec4 } from "gl-matrix";
import { ARStreamingLib, SessionSettings, VideoCompressionSettings, MeshGenerationSettings, MeshSettings, LayerSettings } from "../../lib/build/ar-streaming-lib";
import Connection from "./Connection";
import FrameDecoder from "./FrameDecoder";
import GeometryDecoder from "./GeometryDecoder";
import Layer from "./Layer";
import Mesh from "./Mesh";
import Renderer, { Mode } from "./Renderer";

const TIMEOUT = 5 * 60 * 1000;
const MOUSE_SENSITIVITY = 0.01;
const MOVEMENT_SPEED = 1.0;
const FRAMERATE = 1.0;
const MODE = "ar";
const PREFIX = "/home/so225523/study_scenes/";
const RESOLUTION = 1024;
const NEAR_PLANE = 0.1;
const FAR_PLANE = 100.0;
const VIDEO_CODEC = "h264";
const CHROMA_SUBSAMPLING = true;
const VIDEO_COMPRESSION_SETTINGS: VideoCompressionSettings = {
    framerate: 1.0,
    quality: 1.0,
};
const DEPTH_MAX = 0.995;
const LAYER_SETTINGS: LayerSettings = {
    useObjectIds: true,
    depthBaseThreshold: 0.5,
    depthSlopeThreshold: 0.5,
};
const LINE_BASED_SETTINGS: MeshGenerationSettings = {
    layer: LAYER_SETTINGS,
    mesh: {
        depthMax: DEPTH_MAX,
        lineBased: {
            laplaceThreshold: 0.005,
            normalScale: Math.PI * 0.25,
            lineLengthMin: 10,
        },
    },
};
const LOOP_BASED_SETTINGS: MeshGenerationSettings = {
    layer: LAYER_SETTINGS,
    mesh: {
        depthMax: DEPTH_MAX,
        loopBased: {
            depthBaseThreshold: 0.005,
            depthSlopeThreshold: 0.005,
            normalThreshold: Math.PI * 0.25,
            useNormals: true,
            useObjectIds: true,
            triangleScale: 3,
        },
    },
};

export interface Settings {
    mode: Mode;
    vases: number[];
    session: SessionSettings;
    video: VideoCompressionSettings;
    mesh: MeshGenerationSettings;
}

const CONDITION_SCENES = [
	{
		"file_name": "bistro_cluttered_small/bistro_cluttered_small_search_items_1.fbx",
		"values": [0,2,6,7,9]
	},
	{
		"file_name": "bistro_cluttered_small/bistro_cluttered_small_search_items_2.fbx",
		"values": [2,3,4,6,7]
	},
	{
		"file_name": "bistro_cluttered_small/bistro_cluttered_small_search_items_3.fbx",
		"values": [1,4,5,6,8]
	},
];

const TRIAL_SCENES = [
	{
		"file_name": "bistro_cluttered_trial/bistro_cluttered_trial_search_items_1.fbx",
		"values": [3,1,4,5,0]
	},
	{
		"file_name": "bistro_cluttered_trial/bistro_cluttered_trial_search_items_2.fbx",
		"values": [2,7,1,8,4]
	},
	{
		"file_name": "bistro_cluttered_trial/bistro_cluttered_trial_search_items_3.fbx",
		"values": [5,9,8,0,1]
	},
	{
		"file_name": "bistro_cluttered_trial/bistro_cluttered_trial_search_items_4.fbx",
		"values": [8,0,6,5,9]
	},
	{
		"file_name": "bistro_cluttered_trial/bistro_cluttered_trial_search_items_5.fbx",
		"values": [9,1,3,5,8]
	}
];

const SETTINGS: Settings[] = [
    {
        mode: MODE,
        vases: [],
        session: {
            layerCount: 2,
            scene: "",
            sceneScale: 0.01,
            nearPlane: NEAR_PLANE,
            farPlane: FAR_PLANE,
            resolution: RESOLUTION,
            videoCodec: VIDEO_CODEC,
            chromaSubsampling: CHROMA_SUBSAMPLING,
            sceneExposure: 1.0,
            sceneIndirectIntensity: 1.0,
            meshGeneration: "loop",
        },
        video: VIDEO_COMPRESSION_SETTINGS,
        mesh: LOOP_BASED_SETTINGS,
    },
    {
        mode: MODE,
        vases: [],
        session: {
            layerCount: 1,
            scene: "",
            sceneScale: 0.01,
            nearPlane: NEAR_PLANE,
            farPlane: FAR_PLANE,
            resolution: RESOLUTION,
            videoCodec: VIDEO_CODEC,
            chromaSubsampling: CHROMA_SUBSAMPLING,
            sceneExposure: 1.0,
            sceneIndirectIntensity: 1.0,
            meshGeneration: "loop",
        },
        video: VIDEO_COMPRESSION_SETTINGS,
        mesh: LOOP_BASED_SETTINGS,
    },
    {
        mode: MODE,
        vases: [],
        session: {
            layerCount: 1,
            scene: "",
            sceneScale: 0.01,
            nearPlane: NEAR_PLANE,
            farPlane: FAR_PLANE,
            resolution: RESOLUTION,
            videoCodec: VIDEO_CODEC,
            chromaSubsampling: CHROMA_SUBSAMPLING,
            sceneExposure: 1.0,
            sceneIndirectIntensity: 1.0,
            meshGeneration: "line",
        },
        video: VIDEO_COMPRESSION_SETTINGS,
        mesh: LINE_BASED_SETTINGS,
    },
];

const LATIN_SQUARE = [
    [0, 1, 2],
    [0, 2, 1],
    [1, 0, 2],
    [1, 2, 0],
    [2, 0, 1],
    [2, 1, 0],
];

function makeSettings(participantId: number, condition: number, trial: boolean) {
    const settings = structuredClone(SETTINGS[LATIN_SQUARE[participantId % 6][condition - 1]]);
    const scene = trial ? TRIAL_SCENES[condition - 1] : CONDITION_SCENES[condition - 1];

    settings.vases = structuredClone(scene.values);
    settings.session.scene = PREFIX + scene.file_name;

    return settings;
}

function getConnectionCondition(condition: number, trial: boolean) {
    return condition * 2 + (trial ? 0 : 1);
}

class Session {
    private streamingLib: ARStreamingLib;
    connection: Connection;
    private interval: number;
    private movementInterval: number;
    private moveForward = false;
    private moveBackward = false;
    private moveLeft = false;
    private moveRight = false;
    private moveUp = false;
    private moveDown = false;
    private renderer: Renderer;
    private frameDecoders: [FrameDecoder, FrameDecoder];
    private geometryDecoder: GeometryDecoder;
    private inFlightMeshes = new Map<number, Mesh>();
    private requestTimes = new Map<number, number>();
    private freeLayers: Layer[] = [];
    private position: vec3 = [1, 5, 0];
    private rotationX = 0;
    private rotationY = 0;
    private updatePaused = false;
    private settings: Settings;
    private startTime?: number;
    finished?: (params: {time: number, vases: number[], reason: "correctSelection" | "timeout" | "skipped" }) => void;
    private onError: (event: ErrorEvent) => void;

    constructor(streamingLib: ARStreamingLib, canvas: HTMLCanvasElement, participantId: number, condition: number, trial: boolean) {
        this.streamingLib = streamingLib;

        this.interval = setInterval(() => this.update(), 1000);

        this.settings = makeSettings(participantId, condition, trial);

        // Connecion setup
        this.connection = new Connection(streamingLib, participantId, getConnectionCondition(condition, trial), this.settings.session);
        this.connection.onConnect = () => {
            this.connection.logValues("perSession", this.settings);
        };
        this.connection.onResponse = response => {
            switch (response?.type) {
                case "meshLayer": {
                    if (this.renderer.ui.loading) {
                        this.renderer.ui.loading = false;

                        this.connection.changeVideoCompressionSettings(this.settings.video);
                        this.connection.changeMeshGenerationSettings(this.settings.mesh);
                    }
                    const layerData = response.meshLayer;

                    this.connection.info(`received update for ${layerData.requestId}`);

                    let mesh = this.inFlightMeshes.get(layerData.requestId);
                    if (!mesh) {
                        mesh = new Mesh(layerData.requestId, Array.apply(null, Array(this.settings.session.layerCount)).map(() => this.allocateLayer()));
                        this.inFlightMeshes.set(layerData.requestId, mesh);
                    }

                    const statistics = {
                        viewStatistics: response.meshLayer.viewStatistics,
                        receiveTimestamp: response.receiveTimestamp,
                        messageSize: response.messageSize,
                        requestId: layerData.requestId,
                        layerIndex: layerData.layerIndex,
                        vertexCounts: layerData.vertexCounts,
                        indexCounts: layerData.indexCounts,
                        geometrySize: layerData.geometry.byteLength,
                        frameSize: layerData.frame.byteLength,
                        requestTimestamp: this.requestTimes.get(layerData.requestId) || -1,
                    };

                    mesh.layers[layerData.layerIndex].reset(layerData.indexCounts, layerData.vertexCounts, layerData.viewMatrices, statistics);
                    this.geometryDecoder.decode(layerData.requestId, layerData.layerIndex, layerData.geometry);
                    this.frameDecoders[layerData.layerIndex].decode(layerData.requestId, layerData.frame);

                    // const a = document.createElement('a');
                    // const blob = new Blob([layerData.frame.buffer], {type: "octet/stream"});
                    // const url = window.URL.createObjectURL(blob);
                    // a.href = url;
                    // a.download = "test.h264";
                    // a.click();
                }
            }
        };

        this.onError = (event: ErrorEvent) => {
            // this.info("an error occured");
            // if (this.socket.readyState === this.socket.OPEN) {
            //     this.info({
            //         source, line, column, error, event,
            //     });
            // }
            this.connection.logMessage("error", `${event.message} (${event.filename} ${event.lineno}:${event.colno})`);
            console.error(event.message);
        };
        addEventListener("error", this.onError);

        this.renderer = new Renderer(canvas, this.settings, this.connection);
        if (!trial) {
            this.renderer.ui.showSkip = false;
        }
        this.renderer.setCamera(this.position, this.rotationX, this.rotationY);

        canvas.onmousemove = (event) => {
            if (event.buttons === 1) {
                this.rotationX += event.movementY * MOUSE_SENSITIVITY;
                this.rotationY += event.movementX * MOUSE_SENSITIVITY;
                this.renderer.setCamera(this.position, this.rotationX, this.rotationY);
            }
        };
        canvas.onkeydown = event => {
            switch (event.code) {
                case 'KeyW':
                    this.moveForward = true;
                    break;

                case 'KeyS':
                    this.moveBackward = true;
                    break;

                case 'KeyA':
                    this.moveLeft = true;
                    break;

                case 'KeyD':
                    this.moveRight = true;
                    break;

                case 'KeyQ':
                    this.moveDown = true;
                    break;

                case 'KeyE':
                    this.moveUp = true;
                    break;

                case 'Space':
                    this.updatePaused = !this.updatePaused;
                    break;
            }
        };
        canvas.onkeyup = event => {
            switch (event.code) {
                case 'KeyW':
                    this.moveForward = false;
                    break;

                case 'KeyS':
                    this.moveBackward = false;
                    break;

                case 'KeyA':
                    this.moveLeft = false;
                    break;

                case 'KeyD':
                    this.moveRight = false;
                    break;

                case 'KeyQ':
                    this.moveDown = false;
                    break;

                case 'KeyE':
                    this.moveUp = false;
                    break;
            }
        };

        this.movementInterval = setInterval(() => {
            // TODO: move this somewhere more appropriate
            let selectedCorrectly = true;
            for (let i = 0; i < 10; ++i) {
                const shouldBeSelected = this.settings.vases.indexOf(i) !== -1;
                if (shouldBeSelected !== this.renderer.ui.selected[i]) {
                    selectedCorrectly = false;
                    break;
                }

            }
            if (selectedCorrectly || this.renderer.ui.skipped) {
                if (this.finished) {
                    if (this.startTime === undefined) {
                        this.connection.logMessage("error", "time has not started");
                    } else {
                        this.finished({
                            time: performance.now() - this.startTime,
                            vases: this.renderer.ui.selected,
                            reason: selectedCorrectly ? "correctSelection" : "skipped",
                        });
                    }
                }
            }
            if (this.startTime !== undefined) {
                if (performance.now() - this.startTime >= TIMEOUT) {
                    if (this.finished) {
                        this.finished({
                            time: TIMEOUT,
                            vases: this.renderer.ui.selected,
                            reason: "timeout",
                        });
                    }
                }
            } else {
                if (this.renderer.ui.state === "running") {
                    this.startTime = performance.now();
                }
            }

            const matrix = mat4.create();
            mat4.rotateY(matrix, matrix, this.rotationY);
            mat4.rotateX(matrix, matrix, this.rotationX);
            const right = vec3.fromValues(1, 0, 0);
            const up = vec3.fromValues(0, 1, 0);
            const forward = vec3.fromValues(0, 0, 1);

            vec3.transformMat4(right, right, matrix);
            // vec3.transformMat4(up, up, matrix);
            vec3.transformMat4(forward, forward, matrix);

            const dt = 1 / 16;
            if (this.moveForward) {
                vec3.scaleAndAdd(this.position, this.position, forward, dt * MOVEMENT_SPEED);
            }
            if (this.moveBackward) {
                vec3.scaleAndAdd(this.position, this.position, forward, -dt * MOVEMENT_SPEED);
            }
            if (this.moveLeft) {
                vec3.scaleAndAdd(this.position, this.position, right, -dt * MOVEMENT_SPEED);
            }
            if (this.moveRight) {
                vec3.scaleAndAdd(this.position, this.position, right, dt * MOVEMENT_SPEED);
            }
            if (this.moveUp) {
                vec3.scaleAndAdd(this.position, this.position, up, dt * MOVEMENT_SPEED);
            }
            if (this.moveDown) {
                vec3.scaleAndAdd(this.position, this.position, up, -dt * MOVEMENT_SPEED);
            }
            this.renderer.setCamera(this.position, this.rotationX, this.rotationY);
        }, 16);

        // Video decoder setup
        this.frameDecoders = [
            new FrameDecoder(this.settings.session.videoCodec || "h264"),
            new FrameDecoder(this.settings.session.videoCodec || "h264"),
        ];
        for (let i = 0; i < 2; ++i) {
            this.frameDecoders[i].onFrameDecoded = decodedFrame => {
                const mesh = this.inFlightMeshes.get(decodedFrame.requestId);
                if (!mesh) {
                    console.error(`no in-flight mesh found for request id: ${decodedFrame.requestId}`);
                    decodedFrame.frame.close();
                    return;
                }
                mesh.layers[i].addStatistic("frameDecodingTime", decodedFrame.decodingTime);
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
            // console.log(`${event.decodeTime} (${event.rawDecodeTime})`);
            mesh.layers[event.layerIndex].addStatistic("geometryDecodeTime", event.decodeTime);
            mesh.layers[event.layerIndex].addStatistic("geometryRawDecodeTime", event.rawDecodeTime);
            mesh.layers[event.layerIndex].uploadGeometry(event.indices, event.vertices);
            this.updateCurrentMeshWhenComplete(mesh);
        };
    }

    update() {
        if (!this.updatePaused) {
            // this.connection.info(this.renderer.xrPosition);
            // this.connection.request(this.position[0], this.position[1], this.position[2]);
            const requestId = this.connection.request(this.renderer.xrPosition[0], this.renderer.xrPosition[1], this.renderer.xrPosition[2]);
            this.connection.info(`requested update for (${this.position[0]}, ${this.position[1]}, ${this.position[2]}): ${requestId}`);
            this.requestTimes.set(requestId, performance.now());
        }
    }

    close() {
        this.connection.info("close session!");
        clearInterval(this.interval);
        clearInterval(this.movementInterval);
        removeEventListener("error", this.onError);
        this.connection.disconnect();
        this.renderer.destroy();
    }

    allocateLayer() {
        const layer = this.freeLayers.pop();
        if (layer) {
            return layer;
        } else {
            return new Layer(
                this.renderer.gl,
                this.renderer.program,
                this.settings.session.nearPlane,
                this.settings.session.farPlane,
                this.settings.session.resolution,
                true
            );
        }
    }

    updateCurrentMeshWhenComplete(mesh: Mesh) {
        if (mesh.complete()) {
            const now = performance.now();
            for (const layer of mesh.layers) {
                layer.addStatistic("meshComplete", now);
                this.connection.logValues("perLayerUpdate", layer.statistics);
            }
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
