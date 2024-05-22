import { mat4 } from "gl-matrix";
import { Shader } from "./shader";
import { Frame, Layer, LAYER_VIEW_COUNT } from "./frame";

export class Renderer
{
    private canvas : HTMLCanvasElement;
    private gl : WebGL2RenderingContext;

    private layer_shader : Shader;

    constructor(canvas : HTMLCanvasElement, gl : WebGL2RenderingContext)
    {
        this.canvas = canvas;
        this.gl = gl;

        this.layer_shader = new Shader(gl, "Layer Shader");
    }

    create() : boolean
    {
        if(!this.layer_shader.attach_shader())
        {
            return false;   
        }

        if(!this.layer_shader.attach_shader())
        {
            return false;   
        }

        if(!this.layer_shader.create_program())
        {
            return false;   
        }

        return true;
    }

    destroy()
    {
        this.layer_shader.destroy_program();
    }

    render(frame : Frame, projection_matrix : mat4, view_matrix : mat4)
    {
        this.gl.enable(this.gl.DEPTH_TEST);
        
        this.gl.clearDepth(1.0);
        this.gl.clearColor(0.0, 0.0, 0.0, 1.0);
        this.gl.clear(this.gl.COLOR_BUFFER_BIT | this.gl.DEPTH_BUFFER_BIT);

        this.layer_shader.use_shader();

        for(const layer of frame)
        {
            for(let view_index = 0; view_index < LAYER_VIEW_COUNT; view_index++)
            {
                const src_projection_matrix = Layer.compute_projection_matrix();
                const src_view_matrix = Layer.compute_view_matrix(view_index);



                

                
            }    
        }

        this.layer_shader.use_default();

        this.gl.disable(this.gl.DEPTH_TEST);
    }
}