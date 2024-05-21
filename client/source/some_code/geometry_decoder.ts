
export class GeometryFrame
{
    request_id : number = 0;
    decode_start : number = 0.0;
    deocde_end : number = 0.0;
}

export class GeometryDecodeRequest
{
    frame : GeometryFrame;
    data : ArrayBuffer;

    indices : ArrayBuffer | null = null;
    vertices : ArrayBuffer | null = null;

    constructor(frame : GeometryFrame, data : Uint8Array)
    {
        this.frame = frame;
        this.data = data;
    }
}

export type OnGeometryDecoderDecoded = (frame : GeometryFrame) => void;
export type OnGeometryDecoderError = () => void; 

export class GeometryDecoder
{
    private worker : Worker | null = null;
    private gl : WebGLRenderingContext | null = null;

    private on_decoded : OnGeometryDecoderDecoded | null = null;
    private on_error : OnGeometryDecoderError | null = null;

    create(gl : WebGLRenderingContext) : boolean
    {
        this.gl = gl;

        return true;
    }

    destory()
    {

    }

    create_frame() : GeometryFrame
    {

    }

    destroy_frame(frame : GeometryFrame)
    {

    }

    submit_frame(frame : GeometryFrame, data : Uint8Array)
    {

    }

    set_on_decoded(callback : OnGeometryDecoderDecoded)
    {
        this.on_decoded = callback;
    }

    set_on_error(callback : OnGeometryDecoderError)
    {
        this.on_error = callback;
    }
}