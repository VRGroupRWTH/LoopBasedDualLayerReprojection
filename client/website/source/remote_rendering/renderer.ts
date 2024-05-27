import { mat4, vec2 } from "gl-matrix";
import { Shader, ShaderType } from "./shader";
import { Frame, Layer, LAYER_VIEW_COUNT } from "./frame";
import { WrapperModule } from "./wrapper";
import { Display } from "./display";
import { log_error } from "./log";

import layer_vertex_shader from "../../shaders/layer_shader.vert?raw";
import layer_fragment_shader from "../../shaders/layer_shader.frag?raw";

export class Renderer
{
    private wrapper : WrapperModule;
    private gl : WebGL2RenderingContext;

    private layer_shader : Shader;

    constructor(wrapper : WrapperModule, gl : WebGL2RenderingContext)
    {
        this.wrapper = wrapper;
        this.gl = gl;

        this.layer_shader = new Shader(gl, "Layer Shader");
    }

    create() : boolean
    {
        if(!this.layer_shader.attach_shader(layer_vertex_shader, ShaderType.Vertex))
        {
            return false;
        }

        if(!this.layer_shader.attach_shader(layer_fragment_shader, ShaderType.Fragment))
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

    render(display : Display, frame : Frame, projection_matrix : mat4, view_matrix : mat4, view_image_size : vec2)
    {
        const display_resolution = display.get_resolution();
        this.gl.viewport(0, 0, display_resolution[0], display_resolution[1]);

        this.gl.enable(this.gl.DEPTH_TEST);
        
        this.gl.clearDepth(1.0);
        this.gl.clearColor(0.0, 0.0, 0.0, 1.0);
        this.gl.clear(this.gl.COLOR_BUFFER_BIT | this.gl.DEPTH_BUFFER_BIT);

        this.layer_shader.use_shader();

        for(const layer of frame.layers.toReversed())
        {
            const form = layer.form;

            if(form == null)
            {
                log_error("Can't render layer since form was not set!");
                
                continue;
            }

            this.gl.activeTexture(this.gl.TEXTURE0);
            this.gl.bindTexture(this.gl.TEXTURE_2D, layer.image_frame.image);

            let index_offset = 0;
            let vertex_offset = 0;

            for(let view_index = 0; view_index < LAYER_VIEW_COUNT; view_index++)
            {
                const layer_projection_matrix = Layer.compute_projection_matrix();
                const layer_view_matrix = form.view_matrices[view_index] as mat4;

                const layer_matrix = mat4.create();
                mat4.multiply(layer_matrix, layer_projection_matrix, layer_view_matrix);
                mat4.invert(layer_matrix, layer_matrix);
                mat4.multiply(layer_matrix, view_matrix, layer_matrix);
                mat4.multiply(layer_matrix, projection_matrix, layer_matrix);
                
                this.layer_shader.uniform_mat4("layer_matrix", layer_matrix);

                const view_image_offset = Layer.compute_image_offset(view_image_size, view_index);
                
                this.layer_shader.uniform_int("image_frame", 0);
                this.layer_shader.uniform_uvec2("view_image_offset", view_image_offset);
                this.layer_shader.uniform_uvec2("view_image_size", view_image_size);

                this.gl.bindVertexArray(layer.vertex_arrays[view_index]);
                this.gl.drawRangeElements(this.gl.TRIANGLES, vertex_offset, vertex_offset + form.vertex_counts[view_index], form.index_counts[view_index], this.gl.UNSIGNED_INT, this.wrapper.INDEX_SIZE * index_offset);
                this.gl.bindVertexArray(null);

                index_offset += form.index_counts[view_index];
                vertex_offset += form.vertex_counts[view_index];
            }    

            this.gl.activeTexture(this.gl.TEXTURE0);
            this.gl.bindTexture(this.gl.TEXTURE_2D, null);

            //Clear the depth buffer between the layer and render the layers from far to near on top of each other. 
            //Prevents intersection of two layers
            this.gl.clear(this.gl.DEPTH_BUFFER_BIT); 
        }

        this.layer_shader.use_default();

        this.gl.disable(this.gl.DEPTH_TEST);
    }
}