import { mat4, vec3 } from "gl-matrix";
import Array6 from "./Array6";
import { ShaderPogram } from "./ShaderProgram";
import { LayerResponseForm, ViewMetadataArray } from "../../wrapper/binary/wrapper";

export function throwOnNull<T>(value: T | null) {
    if (!value) {
        throw new Error("value is null");
    }
    return value;
}

const INITIAL_BUFFER_CAPACITY = 1024 * 1024;
const MIN_FILTER = WebGL2RenderingContext.LINEAR;
const MAG_FILTER = WebGL2RenderingContext.LINEAR;

export interface LayerLogData {
    response: LayerResponseForm
    time_image_decode: number;
    time_geometry_decode: number;
    time_geometry_decode_raw: number;
    time_vertex_upload: number,
    time_index_upload: number,
    time_texture_upload: number,
    time_vao_creation: number,
}

class Layer {
    private indexBufferCapacity = INITIAL_BUFFER_CAPACITY;
    private indexBuffer: WebGLBuffer;
    private vertexBufferCapacity = INITIAL_BUFFER_CAPACITY;
    private vertexBuffer: WebGLBuffer;
    private vertexCounts: Array6<number> = [0, 0, 0, 0, 0, 0];
    private indexCounts: Array6<number> = [0, 0, 0, 0, 0, 0];
    private layerClipToView = mat4.create();
    public layerViewToWorld: Array6<mat4> = [mat4.create(), mat4.create(), mat4.create(), mat4.create(), mat4.create(), mat4.create()];
    private leftToRightHanded = mat4.create();
    private vertexArrays: Array6<WebGLVertexArrayObject>;
    private geometryUpaded = false;

    // OPTION A
    // private resolution: number;
    // private texture_width: number;
    // private texture_height: number;
    // private texture: WebGLTexture;


    // OPTION B
    private videoFrameBuffer: Uint8Array;
    private resolution: number; // The resolution of a single side
    private y_width: number; // The width of the whole texture
    private y_height: number; // The height of the whole texture
    private y_size: number; // The size of the y texture in bytes
    private y: WebGLTexture;

    private uv_width: number; // The width of the whole texture
    private uv_height: number; // The height of the whole texture
    private uv_size: number; // The size of the u and v textures in bytes
    private u: WebGLTexture;
    private v: WebGLTexture;

    private texturesUpdated = false;

    logData?: LayerLogData;

    constructor(
        private gl: WebGL2RenderingContext,
        private shaderProgram: ShaderPogram,
        n: number,
        f: number,
        resolution: number,
        chromaSubSampling: boolean,
    ) {
        this.indexBuffer = throwOnNull(this.gl.createBuffer());
        this.gl.bindBuffer(this.gl.ELEMENT_ARRAY_BUFFER, this.indexBuffer);
        this.gl.bufferData(this.gl.ELEMENT_ARRAY_BUFFER, this.indexBufferCapacity, this.gl.STREAM_DRAW);
        this.gl.bindBuffer(this.gl.ELEMENT_ARRAY_BUFFER, null);

        this.vertexBuffer = throwOnNull(this.gl.createBuffer());
        this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.vertexBuffer);
        this.gl.bufferData(this.gl.ARRAY_BUFFER, this.vertexBufferCapacity, this.gl.STREAM_DRAW);
        this.gl.bindBuffer(this.gl.ARRAY_BUFFER, null);

        this.vertexArrays = [
            throwOnNull(this.gl.createVertexArray()),
            throwOnNull(this.gl.createVertexArray()),
            throwOnNull(this.gl.createVertexArray()),
            throwOnNull(this.gl.createVertexArray()),
            throwOnNull(this.gl.createVertexArray()),
            throwOnNull(this.gl.createVertexArray()),
        ];


        // OPTION A
        // this.resolution = resolution;
        // this.texture_width = resolution * 3;
        // this.texture_height = resolution * 2;
        // this.texture = throwOnNull(this.gl.createTexture());
        // this.gl.bindTexture(this.gl.TEXTURE_2D, this.texture);
        // this.gl.texStorage2D(this.gl.TEXTURE_2D, 1, this.gl.RGBA8, this.texture_width, this.texture_height);
        // this.gl.bindTexture(this.gl.TEXTURE_2D, null);


        // OPTION B
        this.resolution = resolution;
        this.y_width = this.resolution * 3;
        this.y_height = this.resolution * 2;
        this.y_size = this.y_width * this.y_height;
        this.y = throwOnNull(this.gl.createTexture());
        this.gl.bindTexture(this.gl.TEXTURE_2D, this.y);
        this.gl.texStorage2D(this.gl.TEXTURE_2D, 1, this.gl.R8, this.y_width, this.y_height);
        this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MIN_FILTER, MIN_FILTER);
        this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MAG_FILTER, MAG_FILTER);

        this.uv_width = chromaSubSampling ? this.y_width / 2 : this.y_width;
        this.uv_height = chromaSubSampling ? this.y_height / 2 : this.y_height;
        this.uv_size = this.uv_width * this.uv_height;

        this.u = throwOnNull(this.gl.createTexture());
        this.gl.bindTexture(this.gl.TEXTURE_2D, this.u);
        this.gl.texStorage2D(this.gl.TEXTURE_2D, 1, this.gl.R8, this.uv_width, this.uv_height);
        this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MIN_FILTER, MIN_FILTER);
        this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MAG_FILTER, MAG_FILTER);

        this.v = throwOnNull(this.gl.createTexture());
        this.gl.bindTexture(this.gl.TEXTURE_2D, this.v);
        this.gl.texStorage2D(this.gl.TEXTURE_2D, 1, this.gl.R8, this.uv_width, this.uv_height);
        this.videoFrameBuffer = new Uint8Array(this.y_size + 2 * this.uv_size);
        this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MIN_FILTER, MIN_FILTER);
        this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MAG_FILTER, MAG_FILTER);
        
        const fn = 1.0 / (f + n);
        mat4.set(
            this.layerClipToView,
            1.0, 0.0, 0.0, 0.0,
            0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, (f+n)*fn, 1.0,
            0.0, 0.0, -2*n*f*fn, 0.0,
        );
        mat4.invert(this.layerClipToView, this.layerClipToView);
        mat4.fromScaling(this.leftToRightHanded, vec3.fromValues(1,1,-1));
    }

    destroy() {
        this.gl.deleteBuffer(this.vertexBuffer);
        this.gl.deleteBuffer(this.indexBuffer);
        for (const vertexArray of this.vertexArrays) {
            this.gl.deleteVertexArray(vertexArray);
        }
        // OPTION A
        // this.gl.deleteTexture(this.texture);

        // OPTION B
        this.gl.deleteTexture(this.y);
        this.gl.deleteTexture(this.u);
        this.gl.deleteTexture(this.v);
    }

    complete() {
        return this.geometryUpaded && this.texturesUpdated;
    }

    reset(indexCounts: Array6<number>, vertexCounts: Array6<number>, viewMatrices: Array6<mat4>, response: LayerResponseForm) {
        if (this.geometryUpaded || this.texturesUpdated) {
            throw new Error("could not reset layer, it must be invalidated first");
        }
        this.indexCounts = indexCounts;
        this.vertexCounts = vertexCounts;
        for (let i = 0; i < 6; ++i) {
            mat4.invert(this.layerViewToWorld[i], viewMatrices[i])
        }
        this.logData = {
            response,
            time_geometry_decode: -1,
            time_image_decode: -1,
            time_index_upload: -1,
            time_texture_upload: -1,
            time_vao_creation: -1,
            time_vertex_upload: -1,
        }
    }

    invalidate() {
        this.geometryUpaded = false;
        this.texturesUpdated = false;
    }

    uploadGeometry(indices: Uint8Array, vertices: Uint8Array) {
        // Upload indices
        const beforeIndexUpload = performance.now();
        if (indices.byteLength > this.indexBufferCapacity) {
            this.gl.deleteBuffer(this.indexBuffer);
            this.indexBuffer = throwOnNull(this.gl.createBuffer());
            this.indexBufferCapacity = Math.ceil(indices.byteLength / INITIAL_BUFFER_CAPACITY) * INITIAL_BUFFER_CAPACITY;
            this.gl.bindBuffer(this.gl.ELEMENT_ARRAY_BUFFER, this.indexBuffer);
            this.gl.bufferData(this.gl.ELEMENT_ARRAY_BUFFER, this.indexBufferCapacity, this.gl.STREAM_DRAW);
        } else {
            this.gl.bindBuffer(this.gl.ELEMENT_ARRAY_BUFFER, this.indexBuffer);
        }
        this.gl.bufferSubData(this.gl.ELEMENT_ARRAY_BUFFER, 0, indices);
        this.gl.bindBuffer(this.gl.ELEMENT_ARRAY_BUFFER, null);

        // Upload vertices
        const beforeVertexUpload = performance.now();
        if (vertices.byteLength > this.vertexBufferCapacity) {
            this.gl.deleteBuffer(this.vertexBuffer);
            this.vertexBuffer = throwOnNull(this.gl.createBuffer());
            this.vertexBufferCapacity = Math.ceil(vertices.byteLength / INITIAL_BUFFER_CAPACITY) * INITIAL_BUFFER_CAPACITY;
            this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.vertexBuffer);
            this.gl.bufferData(this.gl.ARRAY_BUFFER, this.vertexBufferCapacity, this.gl.STREAM_DRAW);
        } else {
            this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.vertexBuffer);
        }
        this.gl.bufferSubData(this.gl.ARRAY_BUFFER, 0, vertices);
        this.gl.bindBuffer(this.gl.ARRAY_BUFFER, null);

        // Create vertex arrays
        const beforeVertexArrayCreation = performance.now();
        let vertexOffset = 0;
        const VERTEX_SIZE = 8;
        for (let i = 0; i < 6; ++i) {
            this.gl.bindVertexArray(this.vertexArrays[i]);
            this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.vertexBuffer);

            this.gl.enableVertexAttribArray(0);
            this.gl.vertexAttribPointer(
                this.shaderProgram.attributes.in_xy,
                2, this.gl.UNSIGNED_SHORT, false, VERTEX_SIZE,
                vertexOffset + 0
            );

            this.gl.enableVertexAttribArray(1);
            this.gl.vertexAttribPointer(
                this.shaderProgram.attributes.in_z,
                1, this.gl.FLOAT, false, VERTEX_SIZE,
                vertexOffset + 4
            );
            vertexOffset += this.vertexCounts[i] * VERTEX_SIZE;
        }
        this.gl.bindVertexArray(null);
        this.geometryUpaded = true;

        const end = performance.now();
        if (!this.logData) {
            throw Error("should not happen");
        }
        this.logData.time_index_upload = beforeVertexUpload - beforeIndexUpload;
        this.logData.time_vertex_upload = beforeVertexArrayCreation - beforeVertexUpload;
        this.logData.time_vao_creation = end - beforeVertexArrayCreation;
    }

    uploadTexture(frame: VideoFrame) {
        const before = performance.now();
        // OPTION A
        // this.gl.bindTexture(this.gl.TEXTURE_2D, this.texture);
        // this.gl.texSubImage2D(this.gl.TEXTURE_2D, 0, 0, 0, this.gl.RGBA, this.gl.UNSIGNED_BYTE, frame);
        // // this.gl.generateMipmap(this.gl.TEXTURE_2D);
        // this.gl.bindTexture(this.gl.TEXTURE_2D, null);
        // // this.textures = true;
        //

        // OPTION B
        frame.copyTo(this.videoFrameBuffer);

        this.gl.bindTexture(this.gl.TEXTURE_2D, this.y);
        const y = new Uint8Array(this.videoFrameBuffer.buffer, 0, this.y_size);
        this.gl.texSubImage2D(this.gl.TEXTURE_2D, 0, 0, 0, this.y_width, this.y_height, this.gl.RED, this.gl.UNSIGNED_BYTE, y);

        this.gl.bindTexture(this.gl.TEXTURE_2D, this.u);
        const u = new Uint8Array(this.videoFrameBuffer.buffer, this.y_size, this.uv_size);
        this.gl.texSubImage2D(this.gl.TEXTURE_2D, 0, 0, 0, this.uv_width, this.uv_height, this.gl.RED, this.gl.UNSIGNED_BYTE, u);

        this.gl.bindTexture(this.gl.TEXTURE_2D, this.v);
        const v = new Uint8Array(this.videoFrameBuffer.buffer, this.y_size + this.uv_size, this.uv_size);
        this.gl.texSubImage2D(this.gl.TEXTURE_2D, 0, 0, 0, this.uv_width, this.uv_height, this.gl.RED, this.gl.UNSIGNED_BYTE, v);

        const after = performance.now();
        if (!this.logData) {
            throw Error("should not happen");
        }
        this.logData.time_texture_upload = after - before;
        this.texturesUpdated = true;
    }

    render(viewMatrix: mat4, projectionMatrix: mat4) {
        this.gl.activeTexture(this.gl.TEXTURE0);
        this.gl.bindTexture(this.gl.TEXTURE_2D, this.y);
        this.gl.uniform1i(this.shaderProgram.uniforms.texture_y, 0);

        this.gl.activeTexture(this.gl.TEXTURE1);
        this.gl.bindTexture(this.gl.TEXTURE_2D, this.u);
        this.gl.uniform1i(this.shaderProgram.uniforms.texture_u, 1);

        this.gl.activeTexture(this.gl.TEXTURE2);
        this.gl.bindTexture(this.gl.TEXTURE_2D, this.v);
        this.gl.uniform1i(this.shaderProgram.uniforms.texture_v, 2);

        const screenToClip = mat4.create();
        mat4.identity(screenToClip);
        mat4.translate(screenToClip, screenToClip, vec3.fromValues(-1.0, -1.0, -1.0));
        mat4.scale(screenToClip, screenToClip, vec3.fromValues(2 / this.resolution, 2 / this.resolution, 2.0));

        this.gl.uniform2f(this.shaderProgram.uniforms.y_texture_size, this.y_width, this.y_height);
        this.gl.uniform2f(this.shaderProgram.uniforms.uv_texture_size, this.uv_width, this.uv_height);
        this.gl.uniform2f(this.shaderProgram.uniforms.resolution, this.resolution, this.resolution);

        const INDEX_SIZE = 4;
        let indexOffset = 0;
        const matrix = mat4.create();
        for (let i = 0; i < 6; ++i) {
            this.gl.bindVertexArray(this.vertexArrays[i]);
            this.gl.bindBuffer(this.gl.ELEMENT_ARRAY_BUFFER, this.indexBuffer);

            mat4.identity(matrix);
            mat4.mul(matrix, matrix, projectionMatrix);
            mat4.mul(matrix, matrix, viewMatrix);
            // mat4.mul(matrix, matrix, this.leftToRightHanded);
            mat4.mul(matrix, matrix, this.layerViewToWorld[i]);
            mat4.mul(matrix, matrix, this.layerClipToView);
            mat4.mul(matrix, matrix, screenToClip);
            this.gl.uniformMatrix4fv(this.shaderProgram.uniforms.matrix, false, matrix);

            const offset_x = (i % 3) * this.resolution;
            const offset_y = Math.floor(i / 3) * this.resolution;
            this.gl.uniform2f(this.shaderProgram.uniforms.offset, offset_x, offset_y);

            this.gl.drawElements(
                this.gl.TRIANGLES,
                this.indexCounts[i],
                this.gl.UNSIGNED_INT,
                indexOffset * INDEX_SIZE
            );
            indexOffset += this.indexCounts[i];
        }


        this.gl.activeTexture(this.gl.TEXTURE2);
        this.gl.bindTexture(this.gl.TEXTURE_2D, null);

        this.gl.activeTexture(this.gl.TEXTURE1);
        this.gl.bindTexture(this.gl.TEXTURE_2D, null);

        this.gl.activeTexture(this.gl.TEXTURE0);
        this.gl.bindTexture(this.gl.TEXTURE_2D, null);
    }
}

export default Layer;
