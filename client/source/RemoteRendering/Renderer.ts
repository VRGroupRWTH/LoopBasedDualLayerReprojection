import { mat4, quat, vec3, vec4 } from "gl-matrix";
import Mesh from "./Mesh";
import meshFragmentShaderSource from "./Mesh.frag?raw";
import meshVertexShaderSource from "./Mesh.vert?raw";
import { ShaderPogram, createShaderProgram } from "./ShaderProgram";
import Connection, { SERVER_URL } from "./Connection";
import { ANIMATION, CONDITION_FAR_PLANE, CONDITION_NEAR_PLANE, Technique } from "../Conditions";

export interface FrameData {
    requestId: number,
    renderTime: number,
    animationTime: number,
    srcPosition: vec3,
    dstPosition: vec3,
    dstOrientation: quat,
}

class Renderer {
    private canvas: HTMLCanvasElement;
    readonly program: ShaderPogram;
    gl: WebGL2RenderingContext;
    private animationFrameRequest: number;
    mesh: Mesh | undefined;
    fov = Math.PI * 0.5;
    animationStart = -1;
    animationIndex = 0;
    position = vec3.create();
    orientation = quat.create();
    frameData = [] as FrameData[];
    finished: () => void;
    isFinished = false;
    frame = 0;

    constructor(canvas: HTMLCanvasElement, private technique: Technique, private interval: number, private run: number, private connection: Connection, finished: () => void, private replaying?: boolean) {
        const gl = canvas.getContext('webgl2');
        if (!gl) {
            throw new Error("failed to create WebGL2 context");
        }
        this.canvas = canvas;
        this.gl = gl;
        this.program = createShaderProgram(this.gl, meshVertexShaderSource, meshFragmentShaderSource);
        this.animationFrameRequest = replaying ? -1 : requestAnimationFrame(() => this.onDesktopAnimationFrame());
        this.finished = finished;
    }

    setCameraTransform(pos: vec3, orientation: quat) {
        vec3.copy(this.position, pos);
        quat.copy(this.orientation, orientation);
    }

    getPosition() {
        return this.position;
    }

    destroy() {
        cancelAnimationFrame(this.animationFrameRequest);
        this.gl.deleteProgram(this.program.program);
    }

    onDesktopAnimationFrame() {
        this.animationFrameRequest = requestAnimationFrame(() => this.onDesktopAnimationFrame());

        if (this.mesh) {
            if (this.animationStart < 0) {
                this.animationStart = performance.now();
            }
            const animationTime = (performance.now() - this.animationStart) / 1000;
            if (animationTime >= (ANIMATION.at(-1)?.time || 0)) {
                console.log("animation finished");
                this.finished();
                this.isFinished = true;
                // console.log(JSON.stringify(this.frameData));
                // const dateString = new Date().toISOString().replaceAll(':','-');
                // console.log(dateString);
                fetch(`http://${SERVER_URL}/files/${this.technique.name}/${this.interval}/${this.run}-rendering.json?type=log`, {
                    method: 'POST',
                    body: JSON.stringify(this.frameData),
                    mode: 'no-cors',
                });
                return;
            }

            const srcPosition = vec3.create();
            mat4.getTranslation(srcPosition, this.mesh.layers[0].layerViewToWorld[0]);

            // this.animationIndex + 1 can never be >= ANIMATION.length
            while (ANIMATION[this.animationIndex + 1].time < animationTime) {
                this.animationIndex++;
            }

            const a = ANIMATION[this.animationIndex];
            const b = ANIMATION[this.animationIndex + 1];
            const t = (animationTime - a.time) / (b.time - a.time);

            vec3.lerp(this.position, a.position, b.position, t);
            quat.slerp(this.orientation, a.orientation, b.orientation, t);


            const t0 = performance.now();

            this.renderFrame();

            const t1 = performance.now();

            this.frameData.push({
                requestId: this.mesh.requestId,
                renderTime: t1 - t0,
                animationTime,
                srcPosition,
                dstPosition: vec3.clone(this.position),
                dstOrientation: quat.clone(this.orientation),
            });
        }
    }

    renderFrame() {
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
        const f = CONDITION_NEAR_PLANE;
        const n = CONDITION_FAR_PLANE;
        const fn = 1.0 / (f - n);
        mat4.set(
            projectionMatrix,
            1.0 / (aspect * tanFovY), 0.0, 0.0, 0.0,
            0.0, 1.0 / tanFovY, 0.0, 0.0,
            0.0, 0.0, (f+n) * fn, 1.0,
            0.0, 0.0, -2.0 * f * n * fn, 0.0
        );
        this.preRender();
        this.renderView(projectionMatrix);
        this.postRender();
    }

    preRender() {
        this.gl.enable(this.gl.DEPTH_TEST);
        this.gl.depthMask(true);
        this.gl.clearDepth(1.0);
        this.gl.clearColor(0.0, 0.0, 0.0, 1.0);
        console.log('clear');
        this.gl.clear(this.gl.COLOR_BUFFER_BIT | this.gl.DEPTH_BUFFER_BIT);
    }

    renderView(projectionMatrix: mat4) {
        if (this.isFinished) {
            return;
        }

        if (this.mesh) {
            const viewMatrix = mat4.create();
            mat4.translate(viewMatrix, viewMatrix, this.position);
            const orientation = mat4.create();
            mat4.fromQuat(orientation, this.orientation);
            mat4.mul(viewMatrix, viewMatrix, orientation);
            mat4.invert(viewMatrix, viewMatrix);

            const beforeSecond = performance.now();
            this.gl.useProgram(this.program.program);

            for (let i = this.mesh.layers.length - 1; i >= 0; --i) {
                const layer = this.mesh.layers[i];
                layer.render(viewMatrix, projectionMatrix);
                if (i !== 0) {
                    this.gl.clear(this.gl.DEPTH_BUFFER_BIT);
                }
            }
            this.gl.useProgram(null);
        }
    }

    postRender() {
        if (this.replaying) {
            const w = this.canvas.width;
            const h = this.canvas.height;
            const buffer = new Uint8Array(w * h * 4);
            this.gl.readPixels(0, 0, w, h, this.gl.RGBA, this.gl.UNSIGNED_BYTE, buffer);
            const image = new ArrayBuffer(8 + w * h * 3);
            const resolution = new Uint32Array(image, 0, 2);
            resolution[0] = w;
            resolution[1] = h;
            const pixels = new Uint8Array(image, 8, w * h * 3);
            for (let y = 0; y < h; ++y) {
                for (let x = 0; x < w; ++x) {
                    const pixelIndex = y * w + x;
                    const srcOffset = pixelIndex * 4;
                    const dstOffset = pixelIndex * 3;
                    pixels[dstOffset + 0] = buffer[srcOffset + 0];
                    pixels[dstOffset + 1] = buffer[srcOffset + 1];
                    pixels[dstOffset + 2] = buffer[srcOffset + 2];
                }
            }
            fetch(`http://${SERVER_URL}/files/${this.technique.name}/${this.interval}/replay/frame${this.frame}.ppm?type=image`, {
                method: 'POST',
                body: image,
            });
            this.frame++;
        }
    }
}

export default Renderer;
