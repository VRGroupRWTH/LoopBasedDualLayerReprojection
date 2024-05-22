import { log_error } from "./log";

export enum ShaderType
{
    Vertex = WebGLRenderingContext.VERTEX_SHADER,
    Fragment = WebGLRenderingContext.FRAGMENT_SHADER
}

export class Shader
{
    private gl : WebGL2RenderingContext;
    private name : string;

    private program : WebGLProgram | null = null;
    private attributes : {[name: string]: number} = {};
    private uniforms : {[name: string]: WebGLUniformLocation} = {};

    constructor(gl : WebGL2RenderingContext, name : string)
    {
        this.gl = gl;
        this.name = name;
    }

    attach_shader(source : string, type : ShaderType) : boolean
    {
        const shader = gl.createShader(type);

        if(shader == null)
        {
            return false;
        }

        gl.shaderSource(shader, source);
        gl.compileShader(shader);
        const log = gl.getShaderInfoLog(shader);

        if(log != null)
        {
            log_error("Can't compile shader '" + this.name + "': " + log);

            return false;
        }

        error //TODO: Check sompile status

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

    create_program() : boolean
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

    destroy_program()
    {
        if(this.program != null)
        {
            
        }
    }

    use_shader()
    {

    }

    use_default()
    {
        
    }

    private extract_attributes() : boolean
    {
        

        return true;
    }

    private extract_uniforms() : boolean
    {

        return true;
    }
}