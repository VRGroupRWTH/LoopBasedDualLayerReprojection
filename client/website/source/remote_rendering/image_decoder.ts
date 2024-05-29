import { log_error } from "./log";

export class ImageFrame
{
    request_id : number = 0;
    decode_start : number = 0.0;
    decode_end : number = 0.0;

    image : WebGLTexture;
    video_frame : VideoFrame | null = null;

    constructor(image : WebGLTexture)
    {
        this.image = image;
    }

    clear()
    {
        this.decode_start = 0.0;
        this.decode_end = 0.0;

        this.video_frame?.close()
    }
}

export type OnImageDecoderDecoded = (frame : ImageFrame) => void;
export type OnImageDecoderError = () => void;

export class ImageDecoder
{
    private gl : WebGL2RenderingContext;
    private video_decoder : VideoDecoder | null = null;
    private is_configured : boolean = false;

    private frame_queue : ImageFrame[] = [];

    private on_decoded : OnImageDecoderDecoded | null = null;
    private on_error : OnImageDecoderError | null = null;

    constructor(gl : WebGL2RenderingContext)
    {
        this.gl = gl;
    }

    create() : boolean
    {
        try
        {
            const video_parameters : VideoDecoderInit =
            {
                output: this.on_interal_decoded.bind(this),
                error: this.on_interal_error.bind(this)
            };
    
            this.video_decoder = new VideoDecoder(video_parameters);
        }

        catch(error)
        {
            log_error("[ImageDecoder] Can't create video deocder or feature not supported!");

            return false;
        }
        
        return true;
    }

    destroy()
    {
        this.on_decoded = null;
        this.on_error = null;

        if(this.video_decoder != null)
        {
            if(this.video_decoder.state != "closed")
            {
                this.video_decoder.close();                
            }

            this.video_decoder = null;   
        }
    }

    create_frame() : ImageFrame | null
    {
        const image = this.gl.createTexture();

        if(image == null)
        {
            return null;   
        }

        return new ImageFrame(image);
    }

    destroy_frame(frame : ImageFrame)
    {
        this.gl.deleteTexture(frame.image);

        if(frame.video_frame != null)
        {
            frame.video_frame.close();   
        }
    }

    submit_frame(frame : ImageFrame, data : Uint8Array) : boolean
    {
        if(this.video_decoder == null)
        {
            return false;
        }

        if(!this.is_configured)
        {
            let video_codec = "avc1." 
            video_codec += data[5].toString(16).padStart(2, "0");
            video_codec += data[6].toString(16).padStart(2, "0");
            video_codec += data[7].toString(16).padStart(2, "0");

            const video_config : VideoDecoderConfig = 
            {
                codec: video_codec,
                hardwareAcceleration: "prefer-hardware",
                optimizeForLatency: true
            };

            this.video_decoder.configure(video_config);
            this.is_configured = true;
        }

        frame.decode_start = performance.now();

        const chunk : EncodedVideoChunkInit = 
        {
            type: "key",
            timestamp: frame.request_id,
            data
        };

        this.video_decoder.decode(new EncodedVideoChunk(chunk));
    
        this.frame_queue.push(frame);

        return true;
    }

    flush_frames()
    {
        if(this.video_decoder == null)
        {
            return;   
        }

        this.video_decoder.flush();
    }

    set_on_decoded(callback : OnImageDecoderDecoded)
    {
        this.on_decoded = callback;
    }

    set_on_error(callback : OnImageDecoderError)
    {
        this.on_error = callback;
    }

    private on_interal_decoded(video_frame : VideoFrame)
    {
        if(this.on_decoded == null)
        {
            log_error("[Image Decoder] No callback for decoded image set!");

            return;   
        }

        let frame_index = this.frame_queue.findIndex(frame =>
        {
            return frame.request_id == video_frame.timestamp;
        });

        if(frame_index == -1)
        {
            log_error("[Image Decoder] Can't find image frame in frame queue!");

            return;
        }

        let frame = this.frame_queue.splice(frame_index, 1)[0];
        frame.video_frame = video_frame;
        frame.decode_end = performance.now();

        this.gl.bindTexture(this.gl.TEXTURE_2D, frame.image);
        this.gl.texImage2D(this.gl.TEXTURE_2D, 0, this.gl.RGBA, this.gl.RGBA, this.gl.UNSIGNED_BYTE, video_frame);

        this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MIN_FILTER, this.gl.LINEAR);
        this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MAG_FILTER, this.gl.LINEAR);
        this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_WRAP_S, this.gl.CLAMP_TO_EDGE);
        this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_WRAP_T, this.gl.CLAMP_TO_EDGE);

        this.gl.bindTexture(this.gl.TEXTURE_2D, null);

        this.on_decoded(frame);
    }

    private on_interal_error(error : DOMException)
    {
        if(this.on_error == null)
        {
            log_error("No callback for image decoder errors set!");

            return;
        }

        this.on_error();
    }
}