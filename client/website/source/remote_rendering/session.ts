import { mat4 } from "gl-matrix";
import { Connection } from "./connection";
import { Display, DisplayType, build_display } from "./display";
import { Frame, FRAME_LAYER_COUNT, Layer } from "./frame";
import { GeometryDecoder } from "./geometry_decoder";
import { ImageDecoder } from "./image_decoder";
import { Renderer } from "./renderer";
import { LayerResponseForm, MatrixArray, MeshGeneratorType, MeshSettingsForm, RenderRequestForm, VideoCodecType, VideoSettingsForm, WrapperModule } from "./wrapper";

export enum SessionMode
{
    Capture,
    Benchmark,
    ReplayMethod,
    ReplayGroundTruth
}

export interface SessionConfig
{
    mode: SessionMode
    animation_src_path : string,
    animation_dst_path : string,
    output_path: string,

    resolution_width: number,
    resolution_height: number,
    layer_count: number,
    render_request_rate: number,

    mesh_generator: MeshGeneratorType,
    mesh_settings : MeshSettingsForm,

    video_codec: VideoCodecType,
    video_settings : VideoSettingsForm
    video_use_chroma_subsampling: number,

    scene_file_name: string,
    scene_scale: number,
    scene_exposure: number,
    scene_indirect_intensity: number,

    sky_file_name: string,
    sky_intensity: number
}

export type OnSessionCalibrate = (display : Display) => void;
export type OnSessionClose = () => void;

export class Session
{
    private config : SessionConfig;
    private wrapper : WrapperModule;
    private canvas : HTMLCanvasElement;
    private gl : WebGL2RenderingContext | null = null;

    private connection : Connection | null = null;
    private display : Display | null = null;
    private renderer : Renderer | null = null;
    private image_decoders : ImageDecoder[] = [];
    private geometry_decoders : GeometryDecoder[] = [];

    private render_request_interval : number | null = 0;
    private render_request_counter = 0;

    private animation_src : Animation = new Animation();
    private animation_dst : Animation = new Animation();

    private frame_pool : Frame[] = [];
    private frame_queue : Frame[] = [];
    private frame_active : Frame | null = null;

    private on_calibrate : OnSessionCalibrate | null = null;
    private on_close : OnSessionClose | null = null;

    constructor(config : SessionConfig, wrapper : WrapperModule, canvas : HTMLCanvasElement)
    {
        this.config = config;
        this.wrapper = wrapper;
        this.canvas = canvas;
    }

    async create(preferred_display : DisplayType) : Promise<boolean>
    {
        this.gl = this.canvas.getContext("webgl2");

        if(this.gl == null)
        {
            return false;   
        }

        if(!await this.create_display(preferred_display))
        {
            return false;   
        }

        if(!this.create_decoders())
        {
            return false;
        }

        this.connection = new Connection(this.wrapper);

        if(!this.connection.create())
        {
            return false;   
        }

        this.renderer = new Renderer(this.canvas, this.gl);
        
        if(!this.renderer.create())
        {
            return false;    
        }

        this.display?.set_on_render(this.on_display_render.bind(this));
        this.display?.set_on_close(this.on_internal_close.bind(this));

        for(const image_decoder of this.image_decoders)
        {
            image_decoder.set_on_decoded(this.on_image_decoded.bind(this));
            image_decoder.set_on_error(this.on_internal_close.bind(this));
        }

        for(const geometry_decoder of this.geometry_decoders)
        {
            geometry_decoder.set_on_decoded(this.on_geometry_decoded.bind(this));
            geometry_decoder.set_on_error(this.on_internal_close.bind(this));
        }

        this.connection.set_on_open(this.on_connection_open.bind(this));
        this.connection.set_on_layer_response(this.on_connection_layer_response.bind(this));
        this.connection.set_on_close(this.on_internal_close.bind(this));

        this.render_request_interval = setInterval(this.on_request_render.bind(this), this.config.render_request_rate);

        //TODO: Start calibration if neccessary

        return true;
    }

    destroy()
    {
        if(this.render_request_interval != null)
        {
            clearInterval(this.render_request_interval);
            this.render_request_interval = null;
        }

        if(this.connection != null)
        {
            this.connection.destroy();
            this.connection = null; 
        }

        for(const frame of this.frame_pool)
        {
            frame.destroy(this.image_decoders, this.geometry_decoders);
        }
    
        for(const frame of this.frame_queue)
        {
            frame.destroy(this.image_decoders, this.geometry_decoders);
        }
        
        if(this.frame_active != null)
        {
            this.frame_active.destroy(this.image_decoders, this.geometry_decoders);
        }

        this.frame_pool = [];
        this.frame_queue = [];
        this.frame_active = null;

        for(const image_decoder of this.image_decoders)
        {
            image_decoder.destroy();
        }

        for(const geometry_decoder of this.geometry_decoders)
        {
            geometry_decoder.destory();    
        }

        this.image_decoders = [];
        this.geometry_decoders = [];

        if(this.display != null)
        {
            this.display.destroy();
            this.display = null;
        }

        if(this.renderer != null)
        {
            this.renderer.destroy();
            this.renderer = null;   
        }
    }

    set_on_calibrate(callback : OnSessionCalibrate)
    {
        this.on_calibrate = callback;
    }

    set_on_close(callback : OnSessionClose)
    {
        this.on_close = callback;
    }

    private async create_display(preferred_display : DisplayType) : Promise<boolean>
    {
        if(this.gl == null)
        {
            return false;   
        }

        this.display = build_display(preferred_display, this.canvas, this.gl);
        
        if(this.display != null)
        {
            if(await this.display.create())
            {
                return true;
            }   
        }

        //Fallback to desktop
        this.display = build_display(DisplayType.Desktop, this.canvas, this.gl);

        if(this.display != null)
        {
            if(await this.display.create())
            {
                return true;
            }   
        }

        return false;
    }

    private create_decoders() : boolean
    {
        if(this.gl == null)
        {
            return false;   
        }

        for(let layer_index = 0; layer_index < FRAME_LAYER_COUNT; layer_index++)
        {
            const image_decoder = new ImageDecoder(this.gl);
            
            if(!image_decoder.create())
            {
                return false;   
            }

            const geometry_decoder = new GeometryDecoder(this.gl);

            if(!geometry_decoder.create())
            {
                return false;
            }

            this.image_decoders.push(image_decoder);
            this.geometry_decoders.push(geometry_decoder);
        }

        return true;
    }

    private load_animation()
    {

    }

    private store_animation()
    {

    }

    private compute_animation_view_matrix() : mat4
    {




    }    

    private on_request_render()
    {
        //Only call if the client is connected to the server

        if(this.connection == null || this.display == null)
        {
            return;   
        }

        if(this.config.mode == SessionMode.Capture)
        {
            console.log(this.config.render_request_rate);

            let request : RenderRequestForm = 
            {
                request_id: this.render_request_counter,
                export_file_names: ["", "", "", ""],
                view_matrices: Layer.compute_view_matrices(this.display?.get_position()) as MatrixArray
            };

            this.connection.send_render_request(request);
            this.render_request_counter++;
        }

        else if(this.config.mode == SessionMode.Benchmark || this.config.mode == SessionMode.ReplayMethod)
        {
            




        }

        else if(this.config.mode == SessionMode.ReplayGroundTruth)
        {
            
        }




        //TODO: Request render using the latest position
        //TODO: Call this function with the frequency defined in the settings

        //TODO: Or use the animation that was captured

        if(this.config.mode == SessionMode.ReplayMethod)
        {

        }
    }

    private on_display_render()
    {



        //if(this.mode == SessionMode.Capture || this.mode == SessionMode.Benchmark) //Also save during the benchmark the rendering positions
        {
            this.display?.get_position();
            this.display?.get_view_matrix();
            this.display?.get_view_matrix();
            //time point;
            
            //TODO Save position

            //For benchmark also the src perspective
        }


        //if(this.mode == SessionMode.Benchmark)
        {
            //Save the rendering time on the client together with the src and dest perspective
        }

        this.gl?.clearColor(0.2,0.4,0.1,1);
        this.gl?.clear(this.gl.COLOR_BUFFER_BIT);

        






    }

    private on_connection_open()
    {

    }

    private on_connection_layer_response(form : LayerResponseForm, geometry_data : Uint8Array, image_data : Uint8Array)
    {
        
    }

    private on_image_decoded()
    {
        if(this.mode == SessionMode.Benchmark && layer_complete)
        {
            //Save the performance numbers of the incomming layers. Not here but where the frames are built   
        }
    }

    private on_geometry_decoded()
    {
        if(this.mode == SessionMode.Benchmark && layer_complete)
        {
            //Save the performance numbers of the incomming layers. Not here but where the frames are built   
        }
    }

    private on_internal_close()
    {
        //Catch all cases that lead to a termination of the application
    }
}