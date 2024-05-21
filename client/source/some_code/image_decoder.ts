import { log_error } from "./log";

export class ImageFrame
{
    request_id : number = 0;
    decode_start : number = 0.0;
    decode_end : number = 0.0;
    video_frame : VideoFrame | null = null;
    image : WebGLTexture | null = null;
}

export type OnImageDecoderDecoded = (frame : ImageFrame) => void;
export type OnImageDecoderError = () => void;

export class ImageDecoder
{
    private video_decoder : VideoDecoder | null = null;
    private gl : WebGLRenderingContext | null = null;

    private frame_pool : ImageFrame[] = [];
    private frame_queue : ImageFrame[] = [];
    private is_configured : boolean = false;

    private on_decoded : OnImageDecoderDecoded | null = null;
    private on_error : OnImageDecoderError | null = null;

    create(gl : WebGLRenderingContext) : boolean
    {
        const video_parameters : VideoDecoderInit =
        {
            output: this.on_interal_decoded,
            error: this.on_interal_error
        };

        this.video_decoder = new VideoDecoder(video_parameters);
        this.gl = gl;

        return true;
    }

    destroy()
    {
        if(this.video_decoder != null)
        {
            this.video_decoder.close();
            this.video_decoder = null;   
        }


    }

    submit(request_id : number, data : Uint8Array) : boolean
    {
        if(this.video_decoder == null)
        {
            return false;   
        }

        if(!this.is_configured)
        {
            const video_config : VideoDecoderConfig = 
            {
                codec: "asd",
                hardwareAcceleration: "prefer-hardware",
                optimizeForLatency: true
            };

            this.video_decoder.configure(video_config);
        }

        let frame = this.frame_pool.pop();

        if(frame == null)
        {
            frame = new ImageFrame();
        }

        frame.request_id = request_id;
        frame.decode_start = performance.now();

        this.frame_queue.push(frame);

        const chunk : EncodedVideoChunkInit = 
        {
            type: "key",
            timestamp: request_id,
            data
        };

        this.video_decoder.decode(new EncodedVideoChunk(chunk));

        return true;
    }

    release(frame : ImageFrame)
    {

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
            log_error("No callback for decoded image set!");

            return;   
        }

        let image = this.gl?.createTexture();

        if(image == null)
        {
            log_error("Can't create image for image frame!");

            return;   
        }

        this.gl?.bindTexture(this.gl.TEXTURE_2D, image);
        this.gl?.texImage2D(this.gl.TEXTURE_2D, 0, this.gl.RGBA, this.gl.RGBA, this.gl.UNSIGNED_BYTE, video_frame);

        this.gl?.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MIN_FILTER, this.gl.NEAREST);
        this.gl?.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MAG_FILTER, this.gl.NEAREST);
        this.gl?.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_WRAP_S, this.gl.CLAMP_TO_EDGE);
        this.gl?.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_WRAP_T, this.gl.CLAMP_TO_EDGE);

        this.gl?.bindTexture(this.gl.TEXTURE_2D, 0);

        let frame_index = this.frame_queue.findIndex(frame =>
        {
            frame.request_id == video_frame.timestamp;
        });

        if(frame_index == -1)
        {
            log_error("Can't find image frame in frame queue!");

            return;
        }

        let frame = this.frame_queue.splice(frame_index, 1)[0];
        frame.video_frame = video_frame;
        frame.image = image;

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

export class GeometryFrame
{
    request_id : number = 0;
    decode_start : number = 0.0;
    deocde_end : number = 0.0;
}

export class GeometryDecoder
{

}