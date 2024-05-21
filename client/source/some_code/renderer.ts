import { mat4 } from "gl-matrix";
import { Shader } from "./shader";
import { Frame } from "./session";

export class Renderer
{
    private canvas : HTMLCanvasElement | null = null;
    private gl : WebGLRenderingContext | null = null;

    private layer_shader : Shader = new Shader("Layer Shader");

    create(canvas : HTMLCanvasElement) : boolean
    {
        this.canvas = canvas;
        this.gl = canvas.getContext("webgl2");

        if(this.gl == null)
        {
            return false;   
        }

        //TODO: Create shader

        return true;
    }

    destroy()
    {

    }

    render(frame : Frame, projection_matrix : mat4, view_matrix : mat4)
    {
        this.gl?.enable(gl.DEPTH_TEST);
        
        this.gl?.clearColor(0.0, 0.0, 0.0, 1.0);
        this.gl?.clear(this.gl.COLOR_BUFFER_BIT | this.gl.DEPTH_BUFFER_BIT);



    }
}