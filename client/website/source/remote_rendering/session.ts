import { Animation, AnimationTransform } from "./animation";
import { Connection, send_file, send_image } from "./connection";
import { Display, DisplayType, build_display } from "./display";
import { Frame, FRAME_MAX_LAYER_COUNT, Layer, LAYER_VIEW_COUNT } from "./frame";
import { GeometryDecoder, GeometryFrame } from "./geometry_decoder";
import { ImageDecoder, ImageFrame } from "./image_decoder";
import { Renderer } from "./renderer";
import { LayerResponseForm, Matrix, MatrixArray, MeshGeneratorType, MeshSettingsForm, RenderRequestForm, SessionCreateForm, VideoCodecType, VideoSettingsForm, WrapperModule } from "./wrapper";
import { log_error, log_info } from "./log";
import { mat4, vec2, vec3 } from "gl-matrix";
import { Metadata } from "./metadata";
import { RenderTime } from "./render_time";

const SESSION_FRAME_QUEUE_MAX_LENGTH = 8;
const SESSION_RENDER_TIME_SAMPLE_COUNT = 100;

export enum SessionMode
{
    Capture,
    Benchmark,
    ReplayMethod,
    ReplayGroundTruth,
    RenderTime
}

export interface SessionConfig
{
    mode: SessionMode
    output_path: string,
    animation_file_name : string,

    resolution: number,
    layer_count: number,
    render_request_rate: number,

    mesh_generator: MeshGeneratorType,
    mesh_settings : MeshSettingsForm,

    video_settings : VideoSettingsForm
    video_use_chroma_subsampling: boolean,

    scene_file_name: string,
    scene_scale: number,
    scene_exposure: number,
    scene_indirect_intensity: number,

    sky_file_name: string,
    sky_intensity: number

    export_color: boolean,
    export_depth: boolean,
    export_mesh: boolean,
    export_feature_lines: boolean
}

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

    private request_interval : number | null = 0;
    private request_counter = 0;

    private animation_input : Animation = new Animation();
    private animation_output : Animation = new Animation();

    private layer_metadata : Metadata[] = [];
    private render_time : RenderTime | null = null;

    private frame_pool : Frame[] = [];
    private frame_queue : Frame[] = [];
    private frame_active : Frame | null = null;
    private frame_dropped : number = -1;

    private on_close : OnSessionClose | null = null;

    constructor(config : SessionConfig, wrapper : WrapperModule, canvas : HTMLCanvasElement)
    {
        this.config = config;
        this.wrapper = wrapper;
        this.canvas = canvas;
    }

    async create(preferred_display : DisplayType) : Promise<boolean>
    {
        if(this.config.layer_count > FRAME_MAX_LAYER_COUNT)
        {
            log_error("[Session] Layer count too high!");

            return false;   
        }

        this.gl = this.canvas.getContext("webgl2");

        if(this.gl == null)
        {
            return false;   
        }

        if(this.config.mode == SessionMode.Benchmark || this.config.mode == SessionMode.ReplayMethod || this.config.mode == SessionMode.ReplayGroundTruth || this.config.mode == SessionMode.RenderTime)
        {
            if(!await this.animation_input.load(this.config.animation_file_name))
            {
                return false;
            }
        }

        if(this.config.mode == SessionMode.Benchmark)
        {
            for(let layer_index = 0; layer_index < this.config.layer_count; layer_index++)
            {
                this.layer_metadata.push(new Metadata());
            }
        }

        if(this.config.mode == SessionMode.RenderTime)
        {
            this.render_time = new RenderTime(this.gl, SESSION_RENDER_TIME_SAMPLE_COUNT);
        }

        if(!this.create_decoders())
        {
            return false;
        }

        for(let layer_index = 0; layer_index < this.config.layer_count; layer_index++)
        {
            const image_decoder = this.image_decoders[layer_index];
            image_decoder.set_on_decoded(image_frame => this.on_decoded(image_frame, layer_index, image_frame.request_id));
            image_decoder.set_on_error(this.on_shutdown.bind(this));    
        }

        for(let layer_index = 0; layer_index < this.config.layer_count; layer_index++)
        {
            const geometry_decoder = this.geometry_decoders[layer_index];
            geometry_decoder.set_on_decoded(geometry_frame => this.on_decoded(geometry_frame, layer_index, geometry_frame.request_id));
            geometry_decoder.set_on_error(this.on_shutdown.bind(this));
        }

        this.renderer = new Renderer(this.wrapper, this.gl);
        
        if(!this.renderer.create())
        {
            return false;
        }

        let calibrate = false;

        if(this.config.mode == SessionMode.Capture) //Only calibrate in the capture mode
        {
            calibrate = true;
        }

        if(!await this.create_display(preferred_display, calibrate))
        {
            return false;   
        }

        this.display?.set_on_render(this.on_render.bind(this));
        this.display?.set_on_close(this.on_shutdown.bind(this));
        await this.display?.show();

        this.connection = new Connection(this.wrapper);
        this.connection.set_on_open(this.on_connect.bind(this));
        this.connection.set_on_layer_response(this.on_response.bind(this));
        this.connection.set_on_close(this.on_shutdown.bind(this));
        
        if(!this.connection.create())
        {
            return false;   
        }

        return true;
    }

    destroy()
    {
        this.on_close = null;

        if(this.request_interval != null)
        {
            clearInterval(this.request_interval);
            this.request_interval = null;
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

    set_on_close(callback : OnSessionClose)
    {
        this.on_close = callback;
    }

    private async create_display(preferred_display : DisplayType, calibrate : boolean) : Promise<boolean>
    {
        if(this.gl == null)
        {
            return false;   
        }

        this.display = build_display(preferred_display, this.canvas, this.gl);
        
        if(this.display != null)
        {
            if(await this.display.create(calibrate))
            {
                return true;
            }

            this.display.destroy();
        }

        log_error("[Session] Can't create preferred display device!");

        //Fallback to desktop
        this.display = build_display(DisplayType.Desktop, this.canvas, this.gl);

        if(this.display != null)
        {
            if(await this.display.create(calibrate))
            {
                return true;
            }   

            this.display.destroy();
        }

        log_error("[Session] Can't create fallback display device!");
        
        return false;
    }

    private create_decoders() : boolean
    {
        if(this.gl == null)
        {
            return false;   
        }

        for(let layer_index = 0; layer_index < this.config.layer_count; layer_index++)
        {
            const image_decoder = new ImageDecoder(this.gl);
            
            if(!image_decoder.create())
            {
                log_error("[Session] Can't create image decoder!");

                return false;
            }

            const geometry_decoder = new GeometryDecoder(this.gl);

            if(!geometry_decoder.create())
            {
                log_error("[Session] Can't create geometry decoder!");

                return false;
            }

            this.image_decoders.push(image_decoder);
            this.geometry_decoders.push(geometry_decoder);
        }

        return true;
    }

    private on_connect()
    {
        if(this.connection == null || this.display == null)
        {
            return;
        }

        //Set the time origin directly after the connection has been established
        const time_origin = performance.now();
        this.animation_input.set_time_origin(time_origin);
        this.animation_output.set_time_origin(time_origin);

        if(this.config.mode == SessionMode.Capture)
        {
            this.animation_output.set_projection_matrix(this.display.get_projection_matrix());
        }

        else if(this.config.mode == SessionMode.Benchmark)
        {
            this.animation_output.set_projection_matrix(this.animation_input.get_projection_matrix());

            for(let metadata of this.layer_metadata)
            {
                metadata.set_time_origin(time_origin);   
            }
        }

        else if(this.config.mode == SessionMode.RenderTime)
        {
            this.render_time?.set_time_origin(time_origin);
        }

        let projection_matrix = Layer.compute_projection_matrix() as Matrix;
        let resolution_width = this.config.resolution;
        let resolution_height = this.config.resolution;
        let layer_count = this.config.layer_count;
        let view_count = LAYER_VIEW_COUNT;
        let export_enabled = false;

        if(this.config.mode == SessionMode.ReplayMethod)
        {
            export_enabled = true;
        }

        else if(this.config.mode == SessionMode.ReplayGroundTruth)
        {
            const display_resolution = this.display.get_resolution();

            projection_matrix = this.animation_input.get_projection_matrix() as Matrix;
            resolution_width = display_resolution[0];
            resolution_height = display_resolution[1];
            layer_count = 1;
            view_count = 1;
            export_enabled = true;
        }

        const session_create : SessionCreateForm =
        {
            mesh_generator: this.config.mesh_generator,
            video_codec: this.wrapper.VideoCodecType.VIDEO_CODEC_TYPE_H264,
            video_use_chroma_subsampling: this.config.video_use_chroma_subsampling,
            projection_matrix,
            resolution_width,
            resolution_height,
            layer_count,
            view_count,
            scene_file_name: this.config.scene_file_name,
            scene_scale: this.config.scene_scale,
            scene_exposure: this.config.scene_exposure,
            scene_indirect_intensity: this.config.scene_indirect_intensity,
            sky_file_name: this.config.sky_file_name,
            sky_intensity: this.config.sky_intensity,
            export_enabled
        };

        if(!this.connection.send_session_create(session_create))
        {
            log_error("[Session] Can't send session create form!");

            return this.on_shutdown();
        }

        if(!this.connection.send_mesh_settings(this.config.mesh_settings))
        {
            log_error("[Session] Can't send mesh settings form!");

            return this.on_shutdown();
        }

        if(!this.connection.send_video_settings(this.config.video_settings))
        {
            log_error("[Session] Can't send video settings form!");

            return this.on_shutdown();
        }

        if(this.config.mode == SessionMode.ReplayMethod || this.config.mode == SessionMode.ReplayGroundTruth || this.config.mode == SessionMode.RenderTime)
        {
            this.on_request();
        }

        else
        {
            this.request_interval = setInterval(this.on_request.bind(this), this.config.render_request_rate);
        }

        const settings = 
        {
            session_setting: session_create,
            mesh_settings: this.config.mesh_settings,
            video_settings: this.config.video_settings
        };

        const text_encoder = new TextEncoder();
        const settings_string = text_encoder.encode(JSON.stringify(settings, (key, value) =>
        {
            if(key == "mesh_generator")
            {
                if(value == this.wrapper.MeshGeneratorType.MESH_GENERATOR_TYPE_QUAD)
                {
                    return "quad";   
                }

                else if(value == this.wrapper.MeshGeneratorType.MESH_GENERATOR_TYPE_LINE)
                {
                    return "line";   
                }

                else if(value == this.wrapper.MeshGeneratorType.MESH_GENERATOR_TYPE_LOOP)
                {
                    return "loop";   
                }
            }

            else if(key == "video_codec")
            {
                if(value == this.wrapper.VideoCodecType.VIDEO_CODEC_TYPE_H264)
                {
                    return "h264";   
                }

                else if(value == this.wrapper.VideoCodecType.VIDEO_CODEC_TYPE_H265)
                {
                    return "h265";   
                }

                else if(value == this.wrapper.VideoCodecType.VIDEO_CODEC_TYPE_H264)
                {
                    return "av1";   
                }
            }

            else if(key == "mode")
            {
                if(value == this.wrapper.VideoCodecMode.VIDEO_CODEC_MODE_CONSTANT_BITRATE)
                {
                    return "constant bitrate";
                }

                else if(value == this.wrapper.VideoCodecMode.VIDEO_CODEC_MODE_CONSTANT_QUALITY)
                {
                    return "constant quality";
                }
            }

            return value;
        }));

        send_file(this.config.output_path + "settings.json", settings_string);
    }

    private on_request()
    {
        if(this.connection == null || this.display == null)
        {
            return;   
        }

        let request : RenderRequestForm = 
        {
            request_id: this.request_counter,
            export_file_names: ["", "", "", ""],
            view_matrices: Layer.compute_view_matrices(vec3.fromValues(0.0, 0.0, 0.0)) as MatrixArray
        };

        if(this.config.mode == SessionMode.Capture)
        {
            const request_position = this.display.get_position();
            request.view_matrices = Layer.compute_view_matrices(request_position) as MatrixArray;

            this.request_counter++;
        }

        else if(this.config.mode == SessionMode.Benchmark)
        {
            if(this.animation_input.has_finished())
            {
                return this.on_shutdown();
            }

            for(let metadata of this.layer_metadata)
            {
                metadata.submit_request(request.request_id);
            }

            const transform = this.animation_input.get_transform();
            const inverse_transform = mat4.create();
            mat4.invert(inverse_transform, transform.dst_transform); //dst_transfrom is a view matrix. Need to invert to get world space position of camera

            const request_position = vec3.create();
            mat4.getTranslation(request_position, inverse_transform);

            request.view_matrices = Layer.compute_view_matrices(request_position) as MatrixArray;

            this.request_counter++;
        }

        else
        {
            const transform = this.animation_input.get_transform_at(this.request_counter);

            if(transform == null)
            {
                return; //Wait for all request to be rendered before closing the session
            }

            if(this.config.mode == SessionMode.ReplayMethod)
            {
                request.view_matrices = Layer.compute_view_matrices(transform.src_position) as MatrixArray;

                if(this.config.export_color)
                {
                    request.export_file_names[this.wrapper.ExportType.EXPORT_TYPE_COLOR.value] = this.config.output_path + "color/color_" + this.request_counter + ".ppm";
                }

                if(this.config.export_depth)
                {
                    request.export_file_names[this.wrapper.ExportType.EXPORT_TYPE_DEPTH.value] = this.config.output_path + "depth/depth_" + this.request_counter + ".pfm";
                }

                if(this.config.export_mesh)
                {
                    request.export_file_names[this.wrapper.ExportType.EXPORT_TYPE_MESH.value] = this.config.output_path + "mesh/mesh_" + this.request_counter + ".obj";
                }

                if(this.config.export_feature_lines)
                {
                    request.export_file_names[this.wrapper.ExportType.EXPORT_TYPE_FEATURE_LINES.value] = this.config.output_path + "feature_lines/feature_lines_" + this.request_counter + ".obj";
                }
            }

            else if(this.config.mode == SessionMode.ReplayGroundTruth)
            {
                request.view_matrices[0] = transform.dst_transform as Matrix;
                request.export_file_names[this.wrapper.ExportType.EXPORT_TYPE_COLOR.value] = this.config.output_path + "color_ground_truth/color_ground_truth_" + this.request_counter + ".ppm";
            }

            else if(this.config.mode == SessionMode.RenderTime)
            {
                request.view_matrices = Layer.compute_view_matrices(transform.src_position) as MatrixArray;
            }
        }

        if(!this.connection.send_render_request(request))
        {
            log_error("[Session] Can't send render request form!");

            return this.on_shutdown();
        }
    }

    private async on_response(form : LayerResponseForm, geometry_data : Uint8Array, image_data : Uint8Array)
    {
        if(this.gl == null)
        {
            return;
        }

        if(this.config.mode == SessionMode.ReplayGroundTruth)
        {
            if(form.request_id == this.request_counter)
            { 
                this.request_counter++;
    
                this.on_request(); //Send the next frame request to the server   
            }

            if(this.request_counter == this.animation_input.get_frame_count())
            {
                this.on_shutdown(); //The server has rendered the last frame that was requested. Therefore close the session
            }

            return;
        }

        let frame = this.frame_queue.find(frame =>
        {
            for(const layer of frame.layers)
            {
                if(layer.form == null)   
                {
                    continue;   
                }

                return layer.form.request_id == form.request_id;
            }

            return false;
        });

        if(frame == undefined)
        {
            if(this.config.mode == SessionMode.Capture || this.config.mode == SessionMode.Benchmark)
            {
                if(this.frame_queue.length > SESSION_FRAME_QUEUE_MAX_LENGTH) //Frame queue is full. Reject all new frames that are older than the current request id.
                {
                    this.frame_dropped = form.request_id;
                    
                    log_info("[Session] Dropping server response for request id " + form.request_id + " and layer " + form.layer_index + " !");

                    return;
                }

                if(form.request_id <= this.frame_dropped)
                {
                    log_info("[Session] Dropping server response for request id " + form.request_id + " and layer " + form.layer_index + " !");

                    return;
                }
            }

            frame = this.frame_pool.pop();

            if(frame == undefined)
            {
                frame = new Frame(this.wrapper, this.gl);   
                        
                if(!frame.create(this.config.layer_count, this.image_decoders, this.geometry_decoders))
                {
                    log_error("[Session] Can't create frame!");
    
                    return this.on_shutdown();
                }
            }

            this.frame_queue.push(frame);
        }

        const layer_index = form.layer_index;
        const layer = frame.layers[layer_index];
        layer.form = form;
        layer.image_frame.request_id = form.request_id;
        layer.geometry_frame.request_id = form.request_id;
        layer.time_point_response = performance.now();

        if(!this.image_decoders[layer_index].submit_frame(layer.image_frame, image_data))
        {
            log_error("[Session] Can't submit image frame!");

            return this.on_shutdown();
        }

        if(!this.geometry_decoders[layer_index].submit_frame(layer.geometry_frame, geometry_data))
        {
            log_error("[Session] Can't submit geometry frame!");   

            return this.on_shutdown();
        }

        if(this.config.mode == SessionMode.ReplayMethod || this.config.mode == SessionMode.RenderTime)
        {
            let response_complete = true;

            for(const layer of frame.layers)
            {
                if(layer.form == null)
                {
                    response_complete = false;
                    
                    break;
                }
            }

            if(response_complete)
            {
                if(form.request_id == this.request_counter)
                { 
                    this.request_counter++;
        
                    this.on_request(); //Send the next frame request to the server   
                }

                if(this.request_counter == this.animation_input.get_frame_count())
                {
                    for(const image_decoder of this.image_decoders)   
                    {
                        image_decoder.flush_frames(); //Flush decoders to get the remaning images
                    }
                }
            }
        }
    }

    private on_decoded(decoded_frame : ImageFrame | GeometryFrame, layer_index : number, required_id : number)
    {
        const frame_index = this.frame_queue.findIndex(frame =>
        {
            const form = frame.layers[layer_index].form;

            if(form == null)
            {
                return false;
            }

            return form.request_id == required_id;
        });

        if(frame_index == -1)
        {
            log_error("[Session] Can't find frame!");

            return this.on_shutdown();
        }
            
        const frame = this.frame_queue[frame_index];

        if(decoded_frame instanceof ImageFrame)
        {
            frame.layers[layer_index].image_complete = true;
        }

        else if(decoded_frame instanceof GeometryFrame)
        {
            frame.layers[layer_index].geometry_complete = true;
        }

        else
        {
            log_error("[Session] Unkown decoded resource!");

            return this.on_shutdown();
        }

        if(this.config.mode == SessionMode.Capture || this.config.mode == SessionMode.Benchmark)
        {
            if(frame.is_complete())
            {
                this.frame_queue.splice(frame_index, 1);
    
                if(!frame.setup())
                {
                    log_error("[Session] Can't setup frame!");
    
                    return this.on_shutdown();
                }
    
                let outdated_frame = null;
    
                if(this.frame_active != null)
                {
                    const active_form = this.frame_active.layers[0].form;
    
                    if(active_form == null || active_form.request_id < required_id)
                    {
                        outdated_frame = this.frame_active;
                        this.frame_active = frame;
                    }
    
                    else
                    {
                        outdated_frame = frame;
                    }
                }
    
                else
                {
                    this.frame_active = frame;
                }
    
                if(outdated_frame != null)
                {
                    if(this.config.mode == SessionMode.Benchmark) //Store metdata of the frame
                    {
                        for(let layer_index = 0; layer_index < this.config.layer_count; layer_index++)
                        {
                            this.layer_metadata[layer_index].submit_layer(outdated_frame.layers[layer_index]);
                        }   
                    }

                    outdated_frame.clear();
                    
                    this.frame_pool.push(outdated_frame);
                }
            }
        }
    }

    private on_render()
    {
        if(this.display == null || this.renderer == null)
        {
            return;   
        }

        let projection_matrix = this.display.get_projection_matrix();
        let view_matrix = this.display.get_view_matrix();

        if(this.config.mode == SessionMode.Capture)
        {
            if(this.frame_active == null)
            {
                return;   
            }

            const src_position = vec3.create();
            vec3.zero(src_position);

            this.animation_output.add_transform(new AnimationTransform(src_position, view_matrix));
        }

        else if(this.config.mode == SessionMode.Benchmark)
        {
            if(this.frame_active == null)
            {
                return;   
            }

            const from = this.frame_active.layers[0].form;

            if(from == null)
            {
                log_error("[Session] Can't form of active frame!");
                
                return this.on_shutdown();
            }

            const inverse_view_matrix = mat4.create();
            mat4.invert(inverse_view_matrix, from.view_matrices[0]); //Need to invert to get world space position of camera

            const src_position = vec3.create();
            mat4.getTranslation(src_position, inverse_view_matrix);

            const animation_transform = this.animation_input.get_transform();
            view_matrix = animation_transform.dst_transform;
            projection_matrix = this.animation_input.get_projection_matrix();

            this.animation_output.add_transform(new AnimationTransform(src_position, view_matrix));
        }

        else
        {
            const frame = this.frame_queue.at(0);

            if(frame == null)
            {
                return;   
            }

            if(!frame.is_complete())
            {
                return;   
            }

            this.frame_active = frame;
            this.frame_queue.shift();

            if(!this.frame_active.setup())
            {
                log_error("[Session] Can't setup frame!");

                return this.on_shutdown();
            }
    
            const form = this.frame_active.layers[0].form;

            if(form == null)
            {
                log_error("[Session] Can't form of active frame!");
                
                return this.on_shutdown();
            }

            const animation_transform = this.animation_input.get_transform_at(form.request_id);

            if(animation_transform == null)
            {
                return this.on_shutdown();
            }

            view_matrix = animation_transform.dst_transform;
            projection_matrix = this.animation_input.get_projection_matrix();
        }

        const view_image_size = vec2.create();
        view_image_size[0] = this.config.resolution;
        view_image_size[1] = this.config.resolution;

        if(this.config.mode == SessionMode.RenderTime)
        {
            const form = this.frame_active.layers[0].form;

            if(form == null)
            {
                log_error("[Session] Can't form of active frame!");

                return this.on_shutdown();
            }

            this.render_time?.begin(form.request_id);

            for(let sample = 0; sample < SESSION_RENDER_TIME_SAMPLE_COUNT; sample++)
            {
                this.renderer.render(this.display, this.frame_active, projection_matrix, view_matrix, view_image_size);
            }

            this.render_time?.end(form.request_id);
        }

        else
        {
            this.renderer.render(this.display, this.frame_active, projection_matrix, view_matrix, view_image_size);
        }
        
        if(this.config.mode == SessionMode.ReplayMethod || this.config.mode == SessionMode.ReplayGroundTruth || this.config.mode == SessionMode.RenderTime)
        {
            const form = this.frame_active.layers[0].form;

            if(form == null)
            {
                log_error("[Session] Can't form of active frame!");

                return this.on_shutdown();
            }

            if(this.config.mode == SessionMode.ReplayMethod)
            {
                const resolution = this.display.get_resolution();
                const buffer_size = resolution[0] * resolution[1] * 4;
                const buffer = new Uint8Array(buffer_size);
    
                this.gl?.readPixels(0, 0, resolution[0], resolution[1], this.gl.RGBA, this.gl.UNSIGNED_BYTE, buffer); //Aligned to multiple of 4
    
                send_image(this.config.output_path + "color_client/color_client_" + form.request_id +".ppm", resolution[0], resolution[1], buffer);
            }

            this.frame_active.clear();
            this.frame_pool.push(this.frame_active);
            this.frame_active = null;

            if(form.request_id == this.animation_input.get_frame_count() - 1)
            {
                return this.on_shutdown(); //Last frame was rendered. Therefore close the session
            }
        }
    }

    private async on_shutdown()
    {
        if(this.gl != null)
        {
            this.gl.clearColor(1.0, 1.0, 1.0, 1.0);   
            this.gl.clear(this.gl.COLOR_BUFFER_BIT);
        }

        if(this.config.mode == SessionMode.Capture)
        {
            await this.animation_output.store(this.config.output_path + "animation_capture.json");
        }

        else if(this.config.mode == SessionMode.Benchmark)
        {
            await this.animation_output.store(this.config.output_path + "animation_benchmark.json");   

            for(let layer_index = 0; layer_index < this.config.layer_count; layer_index++)
            {
                await this.layer_metadata[layer_index].store(this.config.output_path + "metadata_layer_" + layer_index + ".json");
            }
        }

        else if(this.config.mode == SessionMode.RenderTime)
        {
            await this.render_time?.store(this.config.output_path + "render_time.json");
        }

        if(this.on_close != null)
        {
            this.on_close();    
        }
    }
}