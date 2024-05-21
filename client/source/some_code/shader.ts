import { log_error } from "./log";

export enum ShaderType
{
    Vertex = WebGLRenderingContext.VERTEX_SHADER,
    Fragment = WebGLRenderingContext.FRAGMENT_SHADER
}

export class Shader
{
    private name : string;
    private program : WebGLProgram | null = null;
    private attributes : {[name: string]: number} = {};
    private uniforms : {[name: string]: WebGLUniformLocation} = {};

    constructor(name : string)
    {
        this.name = name;
    }

    attach_shader(gl : WebGLRenderingContext, source : string, type : ShaderType) : boolean
    {
        const shader = gl.createShader(type);

        if(shader == null)
        {
            return false;
        }

        gl.shaderSource(shader, source);
        gl.compileShader(shader);
        const log = gl.getShaderInfoLog(shader);

        error //TODO: Check compile status

        if(log != null)
        {
            log_error("Can't compile shader '" + this.name + "': " + log);

            return false;
        }

        if(this.program == null)
        {
            this.program = gl.createProgram();   
        }

        if(this.program == null)
        {
            return false;   
        }

        gl.attachShader(this.program, shader);
        gl.deleteShader(shader);

        return false;
    }


    create_program(gl : WebGLRenderingContext) : boolean
    {
        if(this.program == null)
        {
            return false;   
        }

        gl.linkProgram(this.program);
        const log = gl.getProgramInfoLog(this.program);

        error //TODO: Check link status

        if(log != null)
        {
            log_error("Can't link shader '" + this.name + "': " + log);

            return false;
        }

        if(!this.extract_attributes(gl))
        {
            return false;   
        }

        if(!this.extract_uniforms(gl))
        {
            return false;   
        }

        return true;
    }

    destroy_program(gl : WebGLRenderingContext)
    {
        if(this.program != null)
        {
            
        }
    }

    private extract_attributes(gl : WebGLRenderingContext) : boolean
    {
        

        return true;
    }

    private extract_uniforms(gl : WebGLRenderingContext) : boolean
    {

        return true;
    }
}