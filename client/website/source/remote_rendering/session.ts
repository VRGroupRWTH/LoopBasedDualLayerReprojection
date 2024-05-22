import { Connection } from "./connection";
import { Display, DisplayType, build_display } from "./display";
import { GeometryDecoder, GeometryFrame } from "./geometry_decoder";
import { ImageDecoder, ImageFrame } from "./image_decoder";
import { Renderer } from "./renderer";
import * as Wrapper from "../../wrapper/binary/wrapper"

type WrapperModule = Wrapper.MainModule;

export enum SessionMode
{
    Capture,
    Benchmark,
    ReplayMethod,
    ReplayGroundTruth
}

export interface SessionConfig
{
    mode : SessionMode

    //TODO Add other settings
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
    private image_decoders : [ImageDecoder, ImageDecoder] | null = null;
    private geometry_decoders : [GeometryDecoder, GeometryDecoder] | null = null;

    private frame_pool : Frame[] = [];
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

        this.renderer = new Renderer();
        
        if(!this.renderer.create(this.canvas))
        {
            return false;    
        }

        this.connection = new Connection(this.wrapper);
        this.connection.set_on_open(this.on_connection_open.bind(this));
        this.connection.set_on_close(this.on_internal_close.bind(this));
        this.connection.set_on_error(this.on_internal_error.bind(this));
        this.connection.set_on_layer_response(this.on_connection_layer_response.bind(this));
        
        if(!this.connection.create()) //TODO
        {
            return false;   
        }

        if(this.display != null)
        {
            this.on_calibrate?.(this.display);
        }


        return true;
    }

    destroy()
    {

    }

    set_on_close(callback : OnSessionClose)
    {
        this.on_close = callback;
    }

    set_on_calibrate(callback : OnSessionCalibrate)
    {
        this.on_calibrate = callback;
    }

    private async create_display(preferred_display : DisplayType) : Promise<boolean>
    {
        if(this.canvas == null)
        {
            return false;    
        }

        this.display = build_display(preferred_display, this.canvas, this.gl);

        if(this.display != null)
        {
            if(await this.display.create(this.canvas))
            {
                return true;
            }   
        }

        //Fallback to desktop
        this.display = build_display(DisplayType.Desktop, this.canvas, this.gl);

        if(this.display != null)
        {
            if(await this.display.create(this.canvas))
            {
                return true;
            }   
        }

        return false;
    }

    private create_decoders() : boolean
    {
        this.image_decoders = [new ImageDecoder, new ImageDecoder];
        this.geometry_decoders = [new GeometryDecoder, new GeometryDecoder];

        for(let layer = 0; layer < 2; layer++)
        {


        }

        return true;
    }

    private on_request_render()
    {
        //TODO: Request render using the latest position
        //TODO: Call this function with the frequency defined in the settings

        //TODO: Or use the animation that was captured

        if(this.config.mode == SessionMode.ReplayMethod)
        {

        }
    }

    private on_display_render()
    {
        if(this.mode == SessionMode.Capture || this.mode == SessionMode.Benchmark) //Also save during the benchmark the rendering positions
        {
            this.display?.get_position();
            this.display?.get_view_matrix();
            this.display?.get_view_matrix();
            //time point;
            
            //TODO Save position

            //For benchmark also the src perspective
        }


        if(this.mode == SessionMode.Benchmark)
        {
            //Save the rendering time on the client together with the src and dest perspective
        }



        this.gl?.clearColor(1,0,0,1);
        this.gl?.clear(this.gl.COLOR_BUFFER_BIT);

        console.log("Frame");






    }

    private on_connection_open()
    {

    }

    private on_connection_layer_response(form : Wrapper.LayerResponseForm, geometry_data : Uint8Array, image_data : Uint8Array)
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
        //Catch all close states
    }

    private on_internal_error()
    {
        //Catch all error states
    }
}