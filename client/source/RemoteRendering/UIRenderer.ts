import { mat4, vec3 } from "gl-matrix";
import { throwOnNull } from "./Layer";
import { ShaderPogram, createShaderProgram } from "./ShaderProgram";
import uiFragmentShaderSource from "./UI.frag?raw";
import uiVertexShaderSource from "./UI.vert?raw";
import zero from "../assets/0.png";
import one from "../assets/1.png";
import two from "../assets/2.png";
import three from "../assets/3.png";
import four from "../assets/4.png";
import five from "../assets/5.png";
import six from "../assets/6.png";
import seven from "../assets/7.png";
import eight from "../assets/8.png";
import nine from "../assets/9.png";
import spinner from "../assets/spinner.png";
import skip from "../assets/skip.png";
import calibrationScreen1 from "../assets/calibration-screen1.png";
import calibrationScreen2 from "../assets/calibration-screen2.png";
import calibrationScreen3 from "../assets/calibration-screen3.png";
import calibrationScreen4 from "../assets/calibration-screen4.png";

const ICON_COUNT = 10;
const CALIBRATION_SCREEN_WIDTH = 794;
const CALIBRATION_SCREEN_HEIGHT = 1123;

function loadTexture(gl: WebGL2RenderingContext, url: string) {
    const texture = throwOnNull(gl.createTexture());

    const image = new Image();
    image.src = url;
    image.onload = () => {
        gl.bindTexture(gl.TEXTURE_2D, texture);
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA8, gl.RGBA, gl.UNSIGNED_BYTE, image);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR_MIPMAP_LINEAR);
        gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
        gl.generateMipmap(gl.TEXTURE_2D);
    };

    return texture;
}

export type State = "calibration-1" | "calibration-2" | "pre-run" | "running";

class UIRenderer {
    private program: ShaderPogram;
    private vertexBuffer: WebGLBuffer;
    private indexBuffer: WebGLBuffer;
    private vertexArray: WebGLVertexArrayObject;
    private textures: WebGLTexture[];
    private spinnerTexture: WebGLTexture;
    private skipTexture: WebGLTexture;
    private calibrationScreens: WebGLTexture[];
    state: State = "calibration-1";
    continue = false;
    goBack = false;
    showSkip = true;
    skipped = false;
    loading = true;
    selected: boolean[];
    private iconSize = 80;

    constructor(private canvas: HTMLCanvasElement, private gl: WebGL2RenderingContext) {
        this.program = createShaderProgram(gl, uiVertexShaderSource, uiFragmentShaderSource);

        this.vertexBuffer = throwOnNull(gl.createBuffer());
        gl.bindBuffer(this.gl.ARRAY_BUFFER, this.vertexBuffer);
        gl.bufferData(
            this.gl.ARRAY_BUFFER,
            new Float32Array([
                -0.5, -0.5,
                -0.5, 0.5,
                0.5, -0.5,
                0.5, 0.5,
            ]),
            this.gl.STATIC_DRAW
        );
        this.gl.bindBuffer(this.gl.ARRAY_BUFFER, null);

        this.indexBuffer = throwOnNull(gl.createBuffer());
        gl.bindBuffer(this.gl.ELEMENT_ARRAY_BUFFER, this.indexBuffer);
        gl.bufferData(
            this.gl.ELEMENT_ARRAY_BUFFER,
            new Uint8Array([
                0, 3, 1,
                0, 2, 3,
            ]),
            this.gl.STATIC_DRAW
        );
        this.gl.bindBuffer(this.gl.ELEMENT_ARRAY_BUFFER, null);

        this.vertexArray = throwOnNull(gl.createVertexArray());
        this.gl.bindVertexArray(this.vertexArray);
        this.gl.enableVertexAttribArray(0);
        this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.vertexBuffer);
        this.gl.vertexAttribPointer(this.program.attributes.in_xy, 2, this.gl.FLOAT, false, 8, 0);
        this.gl.bindVertexArray(null);

        this.textures = [
            loadTexture(gl, zero),
            loadTexture(gl, one),
            loadTexture(gl, two),
            loadTexture(gl, three),
            loadTexture(gl, four),
            loadTexture(gl, five),
            loadTexture(gl, six),
            loadTexture(gl, seven),
            loadTexture(gl, eight),
            loadTexture(gl, nine),
        ];
        this.spinnerTexture = loadTexture(gl, spinner);
        this.skipTexture = loadTexture(gl, skip);
        this.calibrationScreens = [
            loadTexture(gl, calibrationScreen1),
            loadTexture(gl, calibrationScreen2),
            loadTexture(gl, calibrationScreen3),
            loadTexture(gl, calibrationScreen4),
        ];
        this.selected = new Array(10).fill(false);

        this.canvas.addEventListener('click', event => {
            this.click(event.x, event.y);
        });
    }

    setState(state: State) {
        this.state = state;
        this.continue = false;
        this.goBack = false;
    }

    private boxClicked(x: number, y: number, boxCenterX: number, boxCenterY: number, width: number, height: number) {
        const minX = this.canvas.clientWidth * 0.5 + boxCenterX - width * 0.5;
        const maxX = this.canvas.clientWidth * 0.5 + boxCenterX + width * 0.5;
        const minY = this.canvas.clientHeight * 0.5 + boxCenterY - height * 0.5;
        const maxY = this.canvas.clientHeight * 0.5 + boxCenterY + height * 0.5;

        return x >= minX && x <= maxX && y >= minY && y <= maxY;
    }

    click(x: number, y: number) {
        if (this.state === "running") {
            const minX = this.canvas.clientWidth * 0.5 - ICON_COUNT * this.iconSize * 0.5;
            const maxX = this.canvas.clientWidth * 0.5 + ICON_COUNT * this.iconSize * 0.5;
            const minY = this.canvas.clientHeight - this.iconSize;
            const maxY = this.canvas.clientHeight;
            if (x >= minX && x <= maxX && y >= minY && y <= maxY) {
                const iconId = Math.floor((x - minX) / this.iconSize);
                this.selected[iconId] = !this.selected[iconId];
            }

            if (this.showSkip) {
                const minX = this.canvas.clientWidth - 2 * this.iconSize;
                const maxX = this.canvas.clientWidth;
                const minY = 0;
                const maxY = this.iconSize;
                if (x >= minX && x <= maxX && y >= minY && y <= maxY) {
                    this.skipped = true;
                }
            }
        } else {
            if (this.boxClicked(x, y, -390/2, 0, 320, 100)) {
                this.goBack = true;
            }
            if (this.boxClicked(x, y, 390/2, 0, 320, 100)) {
                this.continue = true;
            }
        }
    }

    private renderTexture(texture: WebGLTexture, width: number, height: number, x?: number, y?: number, rotation?: number, color?: vec3) {
        this.gl.bindTexture(this.gl.TEXTURE_2D, texture);
        this.gl.uniform1i(this.program.uniforms.icon_texture, 0);
        if (color) {
            this.gl.uniform4f(this.program.uniforms.icon_color, color[0], color[1], color[2], 1.0);
        } else {
            this.gl.uniform4f(this.program.uniforms.icon_color, 1.0, 1.0, 1.0, 0.0);
        }
        const transform = mat4.create();
        mat4.fromScaling(transform, vec3.fromValues(2 / this.canvas.clientWidth, 2 / this.canvas.clientHeight, 1));
        mat4.translate(transform, transform, vec3.fromValues(x || 0, y || 0, 0));
        mat4.scale(transform, transform, vec3.fromValues(width, height, 1.0));
        mat4.rotateZ(transform, transform, rotation || 0);
        this.gl.uniformMatrix4fv(
            this.program.uniforms.transform,
            false,
            transform
        );
        this.gl.drawElements(this.gl.TRIANGLES, 6, this.gl.UNSIGNED_BYTE, 0);
    }

    render() {
        this.gl.enable(this.gl.BLEND);
        this.gl.blendEquation(this.gl.FUNC_ADD);
        this.gl.blendFunc(this.gl.SRC_ALPHA, this.gl.ONE_MINUS_SRC_ALPHA);

        this.gl.useProgram(this.program.program);
        this.gl.bindVertexArray(this.vertexArray);
        this.gl.bindBuffer(this.gl.ELEMENT_ARRAY_BUFFER, this.indexBuffer);

        this.gl.activeTexture(this.gl.TEXTURE0);

        switch (this.state) {
            case "calibration-1": {
                this.renderTexture(this.calibrationScreens[0], CALIBRATION_SCREEN_WIDTH, CALIBRATION_SCREEN_HEIGHT);
                break;
            }

            case "calibration-2": {
                this.renderTexture(this.calibrationScreens[1], CALIBRATION_SCREEN_WIDTH, CALIBRATION_SCREEN_HEIGHT);
                break;
            }

            case "pre-run": {
                if (this.loading) {
                    this.renderTexture(this.spinnerTexture, this.iconSize, this.iconSize, 0, 100, performance.now() / 100, vec3.fromValues(1, 1, 1));
                    this.renderTexture(this.calibrationScreens[2], CALIBRATION_SCREEN_WIDTH, CALIBRATION_SCREEN_HEIGHT);
                } else {
                    this.renderTexture(this.calibrationScreens[3], CALIBRATION_SCREEN_WIDTH, CALIBRATION_SCREEN_HEIGHT);
                }
                break;
            }

            case "running": {
                if (this.showSkip) {
                    const xOffest = this.canvas.clientWidth * 0.5 - this.iconSize;
                    const yOffest = this.canvas.clientHeight * 0.5 - this.iconSize * 0.5;

                    this.gl.bindTexture(this.gl.TEXTURE_2D, this.skipTexture);
                    this.gl.uniform1i(this.program.uniforms.icon_texture, 0);
                    this.gl.uniform4f(this.program.uniforms.icon_color, 1.0, 0.0, 0.0, 0.0);
                    const transform = mat4.create();
                    mat4.fromScaling(transform, vec3.fromValues(2 / this.canvas.clientWidth, 2 / this.canvas.clientHeight, 1));
                    mat4.translate(transform, transform, vec3.fromValues(xOffest, yOffest, 0));
                    mat4.scale(transform, transform, vec3.fromValues(this.iconSize * 2, this.iconSize, 1.0));
                    this.gl.uniformMatrix4fv(
                        this.program.uniforms.transform,
                        false,
                        transform
                    );
                    this.gl.drawElements(this.gl.TRIANGLES, 6, this.gl.UNSIGNED_BYTE, 0);
                }

                for (let i = 0; i < ICON_COUNT; ++i) {
                    const xOffset = (i + 0.5 - ICON_COUNT * 0.5) * this.iconSize;
                    const yOffest = -this.canvas.clientHeight * 0.5 + this.iconSize * 0.5;

                    this.gl.bindTexture(this.gl.TEXTURE_2D, this.textures[i]);
                    this.gl.uniform1i(this.program.uniforms.icon_texture, 0);

                    if (this.selected[i]) {
                        // this.gl.uniform4f(this.program.uniforms.icon_color, 0.0, 0.537, 0.482, 1.0);
                        this.gl.uniform4f(this.program.uniforms.icon_color, 48/255, 108/255, 244/255, 1.0);
                    } else {
                        // this.gl.uniform4f(this.program.uniforms.icon_color, 0.0, 0.537 * 0.33, 0.482 *  0.33, 1.0);
                        this.gl.uniform4f(this.program.uniforms.icon_color, 121/255, 159/255, 248/255, 1.0);
                    }

                    const transform = mat4.create();
                    mat4.fromScaling(transform, vec3.fromValues(2 / this.canvas.clientWidth, 2 / this.canvas.clientHeight, 1));
                    mat4.translate(transform, transform, vec3.fromValues(xOffset, yOffest, 0.0));
                    mat4.scale(transform, transform, vec3.fromValues(this.iconSize, this.iconSize, 1.0));
                    this.gl.uniformMatrix4fv(
                        this.program.uniforms.transform,
                        false,
                        transform
                    );

                    this.gl.drawElements(this.gl.TRIANGLES, 6, this.gl.UNSIGNED_BYTE, 0);
                }
            }
        }
    }

    destroy() {
        this.gl.deleteBuffer(this.vertexBuffer);
        this.gl.deleteBuffer(this.indexBuffer);
        this.gl.deleteVertexArray(this.vertexArray);
        this.gl.deleteProgram(this.program);
    }
}

export default UIRenderer;
