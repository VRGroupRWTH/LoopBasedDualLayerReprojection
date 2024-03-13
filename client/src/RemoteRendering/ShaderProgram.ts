function createShader(gl: WebGL2RenderingContext, type: number, source: string) {
    const shader = gl.createShader(type);
    if (!shader) {
        throw new Error("failed to create shader");
    }
    gl.shaderSource(shader, source);
    gl.compileShader(shader);
    const status = gl.getShaderParameter(shader, gl.COMPILE_STATUS);
    if (!status) {
        const info = gl.getShaderInfoLog(shader);
        gl.deleteShader(shader);
        throw new Error(`failed to compile shader: ${info}`);
    }
    return shader;
}

export interface ShaderPogram {
    program: WebGLProgram;
    uniforms: {[name: string]: WebGLUniformLocation};
    attributes: {[name: string]: number};
}

export function createShaderProgram(gl: WebGL2RenderingContext, vsSource: string, fsSource: string) {
    const vertexShader = createShader(gl, gl.VERTEX_SHADER, vsSource);
    let fragmentShader;
    try {
        fragmentShader = createShader(gl, gl.FRAGMENT_SHADER, fsSource);
    } catch (error) {
        gl.deleteShader(vertexShader);
        throw error;
    }

    const program = gl.createProgram();
    if (!program) {
        gl.deleteShader(vertexShader);
        gl.deleteShader(fragmentShader);
        throw new Error("failed to create shader program");
    }
    gl.attachShader(program, vertexShader);
    gl.attachShader(program, fragmentShader);
    gl.linkProgram(program);
    gl.deleteShader(vertexShader);
    gl.deleteShader(fragmentShader);

    if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
        const info = gl.getProgramInfoLog(program);
        gl.deleteProgram(program);
        throw new Error(`failed to link shader program: ${info}`);
    }

    const shaderProgram: ShaderPogram = {
        program,
        uniforms: {},
        attributes: {},
    };

    const activeUniforms = gl.getProgramParameter(program, gl.ACTIVE_UNIFORMS);
    for (let i = 0; i < activeUniforms; ++i) {
        const uniform = gl.getActiveUniform(program, i);
        if (!uniform) {
            throw new Error("should not happen");
        }
        const location = gl.getUniformLocation(program, uniform.name);
        if (location === null) {
            throw new Error("should not happen");
        }
        shaderProgram.uniforms[uniform.name] = location;
    }

    const activeAttributes = gl.getProgramParameter(program, gl.ACTIVE_ATTRIBUTES);
    for (let i = 0; i < activeAttributes; ++i) {
        const attribute = gl.getActiveAttrib(program, i);
        if (!attribute) {
            throw new Error("should not happen");
        }
        const location = gl.getAttribLocation(program, attribute.name);
        if (location === null) {
            throw new Error("should not happen");
        }
        shaderProgram.attributes[attribute.name] = location;
    }

    return shaderProgram;
}
