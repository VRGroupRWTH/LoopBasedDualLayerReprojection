import { mat4, quat, vec3, vec4 } from "gl-matrix";
import Mesh from "./Mesh";
import meshFragmentShaderSource from "./Mesh.frag?raw";
import meshVertexShaderSource from "./Mesh.vert?raw";
import { ShaderPogram, createShaderProgram } from "./ShaderProgram";
import UIRenderer from "./UIRenderer";
import Connection from "./Connection";
import { Settings } from "./Session";

export type Mode = "desktop" | "ar" | "vr";
const TABLE_OFFSET = 1.5;

function poseToMatrix(pose: any) {
    const viewerPosition = vec3.fromValues(-pose.transform.position.x, -pose.transform.position.y, pose.transform.position.z);
    const viewerOrientation = quat.fromValues(
        pose.transform.orientation.x,
        pose.transform.orientation.y,
        -pose.transform.orientation.z,
        -pose.transform.orientation.w,
    );
    const viewMatrix = mat4.create();
    const rotationMatrix = mat4.create();
    mat4.fromQuat(rotationMatrix, viewerOrientation);
    mat4.invert(rotationMatrix, rotationMatrix);
    const translationMatrix = mat4.create();
    mat4.translate(translationMatrix, translationMatrix, viewerPosition);
    mat4.mul(viewMatrix, rotationMatrix, translationMatrix);
    return viewMatrix;
}

class Renderer {
    private canvas: HTMLCanvasElement;
    readonly program: ShaderPogram;
    gl: WebGL2RenderingContext;
    private animationFrameRequest: number;
    mesh: Mesh | undefined;
    fov = Math.PI * 0.5;
    viewMatrix = mat4.create();
    ui: UIRenderer;
    xrSession?: any;
    refSpace: any;
    xrPosition = [0, 0, 0];
    viewerPose?: any;
    calibrationMatrix = mat4.create();
    firstPoint = vec3.create();
    secondPoint = vec3.create();

    constructor(canvas: HTMLCanvasElement, private settings: Settings, private connection: Connection) {
        const gl = canvas.getContext('webgl2');
        if (!gl) {
            throw new Error("failed to create WebGL2 context");
        }
        this.canvas = canvas;
        this.gl = gl;
        this.program = createShaderProgram(this.gl, meshVertexShaderSource, meshFragmentShaderSource);
        this.ui = new UIRenderer(this.canvas, this.gl);

        // quat.euler

        if (settings.mode === "desktop") {
            this.animationFrameRequest = requestAnimationFrame(() => this.onDesktopAnimationFrame());
        } else {
            if (!navigator.xr) {
                throw new Error("XR not supported");
            }
            this.connection.info("making XR compatible");

            this.connection.info("requesting xr session");
            navigator.xr.requestSession(`immersive-${this.settings.mode}`)
                .then(session => {
                    this.connection.info("request granted");
                    this.xrSession = session;
                    session.addEventListener("end", () => this.xrSession = undefined);

                    this.connection.info("updateRenderState");
                    session.updateRenderState({
                        baseLayer: new XRWebGLLayer(session, gl),
                        depthNear: settings.session.nearPlane,
                        depthFar: settings.session.farPlane,
                    });
                    this.connection.info("requestReferenceSpace");
                    session.requestReferenceSpace("local").then(refSpace => {
                        this.refSpace = refSpace;
                        session.requestAnimationFrame((t: number, f: any) => this.onXRAnimationFrame(t, f));
                    });
                    this.connection.info("addEventListener");
                    session.addEventListener("selectstart", event => {
                        const matrix = poseToMatrix(this.viewerPose);
                        const transformedPosition = vec3.create();
                        vec3.transformMat4(transformedPosition, vec3.fromValues(0, 0, 0), matrix);
                        connection.info(transformedPosition);

                        const raySpacePose = event.frame.getPose(event.inputSource.targetRaySpace, this.refSpace);
                        this.connection.info("ray pose");
                        this.connection.info(raySpacePose.transform.position);
                        const rayPositionInRefSpace = raySpacePose.transform.position;

                        const rayOriginInViewSpace = vec4.create();
                        vec4.transformMat4(rayOriginInViewSpace, vec4.fromValues(
                            rayPositionInRefSpace.x,
                            rayPositionInRefSpace.y,
                            rayPositionInRefSpace.z,
                            1.0,
                        ), this.viewerPose.transform.matrix);
                        const rayPosInClipSpace = vec4.create();
                        vec4.transformMat4(rayPosInClipSpace, rayOriginInViewSpace, this.viewerPose.views[0].projectionMatrix);
                        vec4.scale(rayPosInClipSpace, rayPosInClipSpace, 1.0 / rayPosInClipSpace[3]);

                        const x = (rayPosInClipSpace[0] * 0.5 + 0.5) * this.canvas.width;
                        const y = (-rayPosInClipSpace[1] * 0.5 + 0.5) * this.canvas.height;
                        this.ui.click(x, y);
                        this.connection.info({x, y, continue: this.ui.continue, goBack: this.ui.goBack});

                        switch (this.ui.state) {
                            case "calibration-1": 
                                if (this.ui.continue) {
                                    vec3.set(
                                        this.firstPoint,
                                        transformedPosition[0],
                                        0,
                                        transformedPosition[2]
                                    );
                                    this.ui.setState("calibration-2");
                                } else if (this.ui.goBack) {
                                    // cannot go back
                                }
                                break;

                            case "calibration-2": 
                                if (this.ui.continue) {
                                    vec3.set(
                                        this.secondPoint,
                                        transformedPosition[0],
                                        0,
                                        transformedPosition[2]
                                    );
                                    this.ui.setState("pre-run");
                                } else if (this.ui.goBack) {
                                    this.ui.setState("calibration-1");
                                }
                                break;

                            case "pre-run": 
                                if (this.ui.continue && this.mesh) {
                                    this.ui.setState("running");

                                    const translationMatrix = mat4.create();
                                    mat4.translate(translationMatrix, translationMatrix,
                                                   vec3.fromValues(
                                                       this.secondPoint[0],
                                                       this.secondPoint[1] - TABLE_OFFSET,
                                                       this.secondPoint[2],
                                                   )
                                    );

                                    const xAxis = vec3.create();
                                    vec3.sub(xAxis, this.firstPoint, this.secondPoint);
                                    vec3.normalize(xAxis, xAxis);
                                    const yAxis = vec3.fromValues(0, 1, 0);
                                    const zAxis = vec3.create();
                                    vec3.cross(zAxis, xAxis, yAxis);

                                    this.connection.info(xAxis);
                                    this.connection.info(yAxis);
                                    this.connection.info(zAxis);

                                    const rotationMatrix = mat4.fromValues(
                                        xAxis[0], xAxis[1], xAxis[2], 0,
                                        yAxis[0], yAxis[1], yAxis[2], 0,
                                        zAxis[0], zAxis[1], zAxis[2], 0,
                                        0, 0, 0, 1
                                    );
                                    mat4.mul(this.calibrationMatrix, translationMatrix, rotationMatrix);
                                } else if (this.ui.goBack) {
                                    this.ui.setState("calibration-2");
                                }
                                break;
                        }
                        // if (this.state === "calibrating") {
                        //     this.connection.info(this.viewerPose.transform.position);

                        //     if (this.calibrationPoints.length === 2) {
                        //         this.state = "rendering";
                        //         this.connection.info(this.calibrationPoints);
                        //     }
                        // } else {
                        // }
                    });
                })
                .catch(error => {
                    this.connection.info("request failed");
                });
        }
    }

    destroy() {
        if (this.settings.mode === "desktop") {
            cancelAnimationFrame(this.animationFrameRequest);
        } else {
            if (this.xrSession) {
                this.xrSession.cancelAnimationFrame(this.animationFrameRequest);
                this.xrSession.end();
            }
        }
        this.gl.deleteProgram(this.program.program);
    }

    setCamera(position: vec3, rotationX: number, rotationY: number) {
        mat4.identity(this.viewMatrix);
        mat4.translate(this.viewMatrix, this.viewMatrix, position);
        mat4.rotateY(this.viewMatrix, this.viewMatrix, rotationY);
        mat4.rotateX(this.viewMatrix, this.viewMatrix, rotationX);
        mat4.invert(this.viewMatrix, this.viewMatrix);
    }

    onDesktopAnimationFrame() {
        this.animationFrameRequest = requestAnimationFrame(() => this.onDesktopAnimationFrame());
        if (this.canvas.width !== Math.floor(this.canvas.clientWidth)
            || this.canvas.height !== Math.floor(this.canvas.clientWidth)) {
                this.canvas.width = Math.floor(this.canvas.clientWidth);
                this.canvas.height = Math.floor(this.canvas.clientHeight);
            }

        this.gl.viewport(0, 0, this.canvas.width, this.canvas.height);

        // Do not use mat4.perspective because it assumes a right handed coordinate system.
        const projectionMatrix = mat4.create();
        const fovY = Math.PI * 0.5;
        const tanFovY = Math.tan(fovY * 0.5);
        const aspect = this.canvas.width / this.canvas.height;
        const f = this.settings.session.nearPlane;
        const n = this.settings.session.farPlane;
        const fn = 1.0 / (f - n);
        mat4.set(
            projectionMatrix,
            1.0 / (aspect * tanFovY), 0.0, 0.0, 0.0,
            0.0, 1.0 / tanFovY, 0.0, 0.0,
            0.0, 0.0, (f+n) * fn, 1.0,
            0.0, 0.0, -2.0 * f * n * fn, 0.0
        );
        this.preRender();
        this.renderView(this.viewMatrix, projectionMatrix);
        this.postRender();
    }

    onXRAnimationFrame(t: number, frame: any) {
        this.animationFrameRequest = this.xrSession.requestAnimationFrame((t: any, f: any) => this.onXRAnimationFrame(t, f));

        const session = frame.session;
        const pose = frame.getViewerPose(this.refSpace);
        this.viewerPose = pose;
        const viewMatrix = mat4.create();

        if (this.ui.state === "running") {
            mat4.copy(viewMatrix, poseToMatrix(pose));
            mat4.invert(viewMatrix, viewMatrix);
            mat4.mul(viewMatrix, viewMatrix, this.calibrationMatrix);

            const invertedViewMatrix = mat4.clone(viewMatrix);
            mat4.invert(invertedViewMatrix, invertedViewMatrix);
            this.xrPosition = [
                invertedViewMatrix[12],
                invertedViewMatrix[13],
                invertedViewMatrix[14],
            ];
        }

        const glLayer = session.renderState.baseLayer;
        this.preRender();
        for (const view of pose.views) {
            const {x, y, width, height} = glLayer.getViewport(view);
            this.gl.viewport(x, y, width, height);
            const projection = mat4.create();
            mat4.copy(projection, view.projectionMatrix);
            mat4.scale(projection, projection, vec3.fromValues(1, 1, -1));
            this.renderView(viewMatrix, projection);
            break;
        }
        this.gl.viewport(0, 0, this.canvas.width, this.canvas.height);
        this.postRender();

        if (this.canvas.width !== Math.floor(this.canvas.clientWidth)
            || this.canvas.height !== Math.floor(this.canvas.clientWidth)) {
            this.canvas.width = Math.floor(this.canvas.clientWidth);
            this.canvas.height = Math.floor(this.canvas.clientHeight);
        }
    }

    preRender() {
        this.gl.enable(this.gl.DEPTH_TEST);
        this.gl.depthMask(true);
        this.gl.clearDepth(1.0);
        if (this.ui.state === "running") {
            this.gl.clearColor(0.0, 0.0, 0.0, 1.0);
        } else {
            this.gl.clearColor(0.0, 0.0, 0.0, 0.75);
        }
        this.gl.clear(this.gl.COLOR_BUFFER_BIT | this.gl.DEPTH_BUFFER_BIT);
    }

    renderView(viewMatrix: mat4, projectionMatrix: mat4) {
        const frameStatistics = {
            timestamp: performance.now(),
            total: -1,
            layers: Array.apply(null, Array(this.settings.session.layerCount)).map(() => -1),
            requestId: -1,
            ui: -1,
        };

        if (this.mesh && this.ui.state === "running") {
            frameStatistics.requestId = this.mesh.requestId;
            const beforeSecond = performance.now();
            this.gl.useProgram(this.program.program);

            for (let i = this.mesh.layers.length - 1; i >= 0; --i) {
                const layer = this.mesh.layers[i];
                const before = performance.now();
                layer.render(viewMatrix, projectionMatrix);
                if (i !== 0) {
                    this.gl.clear(this.gl.DEPTH_BUFFER_BIT);
                }
                const after = performance.now();
                frameStatistics.layers[i] = after - before;
            }
            this.gl.useProgram(null);
        }

        const beforeUI = performance.now();
        this.ui.render();
        const afterUI = performance.now();

        frameStatistics.ui = afterUI - beforeUI;
        frameStatistics.total = afterUI - frameStatistics.timestamp;
        this.connection.logValues("perFrame", frameStatistics);
    }

    postRender() {
    }
}

export default Renderer;
