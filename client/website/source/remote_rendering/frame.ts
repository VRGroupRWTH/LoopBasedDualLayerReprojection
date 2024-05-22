import { mat4, vec3 } from "gl-matrix";
import { GeometryDecoder, GeometryFrame } from "./geometry_decoder";
import { ImageDecoder, ImageFrame } from "./image_decoder";
import { LayerResponseForm } from "./wrapper";

export const FRAME_LAYER_COUNT : number = 2;
export const LAYER_VIEW_COUNT : number = 6;

export class Layer
{
    form : LayerResponseForm | null = null;

    image_frame : ImageFrame;
    image_complete : boolean = false;

    geometry_frame : GeometryFrame;
    geometry_complete : boolean = false;

    vertex_arrays : WebGLVertexArrayObject[];

    constructor(image_frame : ImageFrame, geometry_frame : GeometryFrame, vertex_arrays : WebGLVertexArrayObject[])
    {
        this.image_frame = image_frame;
        this.geometry_frame = geometry_frame;
        this.vertex_arrays = vertex_arrays;
    }
    
    static compute_projection_matrix() : mat4
    {
        return mat4.create();
    }

    static compute_view_matrix(position : vec3, view_index : number) : mat4
    {
        return mat4.create();
    }
}

export class Frame
{
    private gl : WebGL2RenderingContext;
    layers : Layer[] = [];

    constructor(gl : WebGL2RenderingContext)
    {
        this.gl = gl;
    }

    create(image_decoders : ImageDecoder[], geometry_decoders : GeometryDecoder[]) : boolean
    {
        for(let layer_index = 0; layer_index < FRAME_LAYER_COUNT; layer_index++)
        {
            const image_frame = image_decoders[layer_index].create_frame();
            
            if(image_frame == null)
            {
                return false;   
            }

            const geometry_frame = geometry_decoders[layer_index].create_frame();

            if(geometry_frame == null)
            {
                return false;   
            }

            let vertex_arrays : WebGLVertexArrayObject[] = [];

            for(let view_index = 0; view_index < LAYER_VIEW_COUNT; view_index++)
            {
                const vertex_array = this.gl.createVertexArray();
                
                if(vertex_array == null)
                {
                    return false;   
                }

                vertex_arrays.push(vertex_array);
            }

            this.layers.push(new Layer(image_frame, geometry_frame, vertex_arrays));
        }

        return true;
    }   
    
    destroy(image_decoders : ImageDecoder[], geometry_decoders : GeometryDecoder[])
    {
        for(let layer_index = 0; layer_index < this.layers.length; layer_index++)
        {
            const layer = this.layers[layer_index];
            
            image_decoders[layer_index].destroy_frame(layer.image_frame);
            geometry_decoders[layer_index].destroy_frame(layer.geometry_frame);

            for(const vertex_array of layer.vertex_arrays)
            {
                this.gl.deleteVertexArray(vertex_array);
            }

            layer.vertex_arrays = [];
        }
    }

    setup() : boolean
    {
        if(!this.is_complete())
        {
            return false;   
        }

        for(const layer of this.layers)
        {
            this.gl.bindBuffer(this.gl.ARRAY_BUFFER, layer.geometry_frame.vertex_buffer);

            for(let view_index = 0; view_index < LAYER_VIEW_COUNT; view_index++)
            {
                this.gl.bindVertexArray(layer.vertex_arrays[view_index]);

                this.gl.enableVertexAttribArray(0);



            }

            this.gl.bindBuffer(this.gl.ARRAY_BUFFER, null);
        }

        return true;
    }

    clear()
    {
        for(const layer of this.layers)
        {
            layer.image_frame.close();
            
            layer.image_complete = false;
            layer.geometry_complete = false;
        }
    }

    is_complete() : boolean
    {
        for(const layer of this.layers)
        {
            if(!layer.image_complete || !layer.geometry_complete)
            {
                return false;   
            }
        }

        return true;
    }
}