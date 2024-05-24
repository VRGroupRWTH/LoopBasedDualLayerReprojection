import { mat4, vec2, vec3 } from "gl-matrix";
import { log_error } from "./log";

export enum ShaderType
{
    Vertex = WebGLRenderingContext.VERTEX_SHADER,
    Fragment = WebGLRenderingContext.FRAGMENT_SHADER
}

class ShaderUniform
{
    info : WebGLActiveInfo;
    location : WebGLUniformLocation;

    constructor(info : WebGLActiveInfo, location : WebGLUniformLocation)
    {
        this.info = info;
        this.location = location;
    }
}

export class Shader
{
    private gl : WebGL2RenderingContext;
    private name : string;

    private program : WebGLProgram | null = null;
    private uniforms : {[name: string] : ShaderUniform} = {};

    constructor(gl : WebGL2RenderingContext, name : string)
    {
        this.gl = gl;
        this.name = name;
    }

    attach_shader(source : string, type : ShaderType) : boolean
    {
        const shader = this.gl.createShader(type);

        if(shader == null)
        {
            return false;
        }

        this.gl.shaderSource(shader, source);
        this.gl.compileShader(shader);

        const status = this.gl.getShaderParameter(shader, this.gl.COMPILE_STATUS);

        if(!status)
        {
            log_error("Can't compile shader '" + this.name + "' !");

            const log = this.gl.getShaderInfoLog(shader);

            if(log != null)
            {
                log_error(log);
            }

            return false;
        }

        if(this.program == null)
        {
            this.program = this.gl.createProgram();   
        }

        if(this.program == null)
        {
            return false;   
        }

        this.gl.attachShader(this.program, shader);
        this.gl.deleteShader(shader);

        return true;
    }

    create_program() : boolean
    {
        if(this.program == null)
        {
            return false;   
        }

        this.gl.linkProgram(this.program);

        const status = this.gl.getProgramParameter(this.program, this.gl.LINK_STATUS);

        if(!status)
        {
            log_error("Can't link shader '" + this.name + "' !");

            const log = this.gl.getProgramInfoLog(this.program);

            if(log != null)
            {
                log_error(log);
            }

            return false;
        }

        if(!this.extract_uniforms())
        {
            return false;   
        }

        return true;
    }

    destroy_program()
    {
        if(this.program != null)
        {
            this.gl.deleteProgram(this.program);

            this.program = null;
            this.uniforms = {};
        }
    }

    use_shader()
    {
        this.gl.useProgram(this.program);
    }

    use_default()
    {
        this.gl.useProgram(null);
    }

    uniform_int(name : string, value : number)
    {
        const location = this.check_uniform(name, [ this.gl.INT, this.gl.SAMPLER_2D], 1);

        if(location != null)
        {
            this.gl.uniform1i(location, value);   
        }
    }

    uniform_uint(name : string, value : number)
    {
        const location = this.check_uniform(name, [ this.gl.UNSIGNED_INT], 1);

        if(location != null)
        {
            this.gl.uniform1ui(location, value);   
        }
    }

    uniform_uvec2(name : string, value : vec2)
    {
        const location = this.check_uniform(name, [ this.gl.UNSIGNED_INT_VEC2 ], 1);

        if(location != null)
        {
            this.gl.uniform2ui(location, value[0], value[1]);   
        }
    }

    uniform_uvec3(name : string, value : vec3)
    {
        const location = this.check_uniform(name, [ this.gl.UNSIGNED_INT_VEC3 ], 1);

        if(location != null)
        {
            this.gl.uniform3ui(location, value[0], value[1], value[2]);   
        }
    }

    uniform_float(name : string, value : number)
    {
        const location = this.check_uniform(name, [ this.gl.FLOAT ], 1);

        if(location != null)
        {
            this.gl.uniform1f(location, value);   
        }
    }

    uniform_vec2(name : string, value : vec2)
    {
        const location = this.check_uniform(name, [ this.gl.FLOAT_VEC2 ], 1);

        if(location != null)
        {
            this.gl.uniform2f(location, value[0], value[1]);   
        }
    }

    uniform_vec3(name : string, value : vec3)
    {
        const location = this.check_uniform(name, [ this.gl.FLOAT_VEC3 ], 1);

        if(location != null)
        {
            this.gl.uniform3f(location, value[0], value[1], value[2]);   
        }
    }

    uniform_mat4(name : string, value : mat4)
    {
        const location = this.check_uniform(name, [ this.gl.FLOAT_MAT4 ], 1);

        if(location != null)
        {
            this.gl.uniformMatrix4fv(location, false, value);
        }
    }

    private check_uniform(name : string, types : number[], size : number) : WebGLUniformLocation | null
    {
        if(!(name in this.uniforms))
        {
            log_error("Can't find uniform '" + name + "' !");

            return null;
        }

        const uniform = this.uniforms[name];

        if(!types.includes(uniform.info.type) || uniform.info.size != size)
        {
            log_error("Can't assign uniform '" + name + "' due to invalid type!");

            return null;
        }

        return uniform.location;
    }

    private extract_uniforms() : boolean
    {
        if(this.program == null)
        {
            return false;   
        }

        const uniform_count = this.gl.getProgramParameter(this.program, this.gl.ACTIVE_UNIFORMS);

        for(let uniform_index = 0; uniform_index < uniform_count; uniform_index++)
        {
            const uniform = this.gl.getActiveUniform(this.program, uniform_index);

            if(uniform == null)
            {
                continue;   
            }

            const uniform_location = this.gl.getUniformLocation(this.program, uniform.name);

            if(uniform_location == null)
            {
                continue;   
            }

            this.uniforms[uniform.name] = new ShaderUniform(uniform, uniform_location);
        }

        return true;
    }
}