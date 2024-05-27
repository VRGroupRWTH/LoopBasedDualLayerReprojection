import { mat4, vec2, vec3 } from "gl-matrix";
import { GeometryDecoder, GeometryFrame } from "./geometry_decoder";
import { ImageDecoder, ImageFrame } from "./image_decoder";
import { LayerResponseForm, WrapperModule } from "./wrapper";
import { log_error } from "./log";

export const FRAME_MAX_LAYER_COUNT : number = 2;
export const LAYER_VIEW_COUNT : number = 6;
export const LAYER_NEAR_DISTANCE : number = 0.1;
export const LAYER_FAR_DISTANCE : number = 100.0;

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
        const projection_matrix = mat4.create();
        mat4.perspective(projection_matrix, Math.PI / 2.0, 1.0, LAYER_FAR_DISTANCE, LAYER_NEAR_DISTANCE); //Switch far and near distance due to mat4.perspective() implementation

        //mat4.perspective() creates an left-handed projection matrix that needs to be converted to right-handed
        projection_matrix[10] = -projection_matrix[10];
        projection_matrix[14] = -projection_matrix[14];

        return projection_matrix;
    }

    static compute_view_matrices(position : vec3) : mat4[]
    {
        const forward = 
        [
            vec3.fromValues( 1.0, 0.0, 0.0), //One row for +x, -z, -x without seams
            vec3.fromValues( 0.0, 0.0,-1.0),
            vec3.fromValues(-1.0, 0.0, 0.0),
            vec3.fromValues( 0.0, 1.0, 0.0), //One row for +y, +z, -y without seams
            vec3.fromValues( 0.0, 0.0, 1.0),
            vec3.fromValues( 0.0,-1.0, 0.0)
        ];

        const up =
        [
            vec3.fromValues( 0.0, 1.0, 0.0),
            vec3.fromValues( 0.0, 1.0, 0.0),
            vec3.fromValues( 0.0, 1.0, 0.0),
            vec3.fromValues( 1.0, 0.0, 0.0),
            vec3.fromValues( 1.0, 0.0, 0.0),
            vec3.fromValues( 1.0, 0.0, 0.0)
        ]

        let view_matrices = [];

        for(let view_index = 0; view_index < LAYER_VIEW_COUNT; view_index++)
        {
            const target = vec3.create();
            vec3.add(target, position, forward[view_index]);
    
            const view_matrix = mat4.create();
            mat4.lookAt(view_matrix, position, target, up[view_index]);

            view_matrices.push(view_matrix);
        }

        return view_matrices;
    }

    static compute_image_offset(image_size : vec2, view_index : number) : vec2
    {
        const image_offset = vec2.create();
        
        if(view_index < 3)
        {
            image_offset[0] = image_size[0] * view_index;
            image_offset[1] = 0.0;
        }

        else
        {
            image_offset[0] = image_size[0] * (view_index - 3);
            image_offset[1] = image_size[1];
        }

        return image_offset;
    }
}

export class Frame
{
    private wrapper : WrapperModule;
    private gl : WebGL2RenderingContext;
    layers : Layer[] = [];

    constructor(wrapper : WrapperModule, gl : WebGL2RenderingContext)
    {
        this.wrapper = wrapper;
        this.gl = gl;
    }

    create(layer_count : number, image_decoders : ImageDecoder[], geometry_decoders : GeometryDecoder[]) : boolean
    {
        for(let layer_index = 0; layer_index < layer_count; layer_index++)
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
            const form = layer.form;

            if(form == null)
            {
                log_error("Can't setup frame since layer form was not set!");

                return false;
            }

            let vertex_offset = 0;

            for(let view_index = 0; view_index < LAYER_VIEW_COUNT; view_index++)
            {
                this.gl.bindVertexArray(layer.vertex_arrays[view_index]);

                this.gl.bindBuffer(this.gl.ELEMENT_ARRAY_BUFFER, layer.geometry_frame.index_buffer);
                this.gl.bindBuffer(this.gl.ARRAY_BUFFER, layer.geometry_frame.vertex_buffer);

                this.gl.enableVertexAttribArray(0);
                this.gl.vertexAttribIPointer(0, 2, this.gl.UNSIGNED_SHORT, this.wrapper.VERTEX_SIZE, vertex_offset);

                this.gl.enableVertexAttribArray(1);
                this.gl.vertexAttribPointer(1, 1, this.gl.FLOAT, false, this.wrapper.VERTEX_SIZE, vertex_offset + this.wrapper.VERTEX_OFFSET_Z);

                this.gl.bindVertexArray(null);

                vertex_offset += this.wrapper.VERTEX_SIZE * form.vertex_counts[view_index];
            }

            this.gl.bindBuffer(this.gl.ELEMENT_ARRAY_BUFFER, null);
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

            layer.form = null;
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