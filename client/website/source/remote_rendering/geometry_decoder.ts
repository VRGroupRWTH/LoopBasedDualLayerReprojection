import GeometryWorker from "./geometry_worker?worker";

export class GeometryFrame
{
    request_id : number = 0;
    decode_start : number = 0.0;
    deocde_end : number = 0.0;

    indices : ArrayBuffer = new ArrayBuffer(0);
    index_buffer : WebGLBuffer;
    index_buffer_size : number = 0;

    vertices : ArrayBuffer = new ArrayBuffer(0);
    vertex_buffer : WebGLBuffer;
    vertex_buffer_size : number = 0;

    constructor(index_buffer : WebGLBuffer, vertex_buffer : WebGLBuffer)
    {
        this.index_buffer = index_buffer;
        this.vertex_buffer = vertex_buffer;
    }
}

export class GeometryDecodeTask
{
    frame : GeometryFrame;
    data : ArrayBuffer;

    constructor(frame : GeometryFrame, data : ArrayBuffer)
    {
        this.frame = frame;
        this.data = data;
    }
}

export type OnGeometryDecoderDecoded = (frame : GeometryFrame) => void;
export type OnGeometryDecoderError = () => void; 

export class GeometryDecoder
{
    private gl : WebGL2RenderingContext;
    private worker : Worker | null = null;

    private on_decoded : OnGeometryDecoderDecoded | null = null;
    private on_error : OnGeometryDecoderError | null = null;

    constructor(gl : WebGL2RenderingContext)
    {
        this.gl = gl;
    }

    create() : boolean
    {
        this.worker = new GeometryWorker() as Worker;
        this.worker.onmessage = this.on_worker_response;
        this.worker.onerror = this.on_worker_error;

        return true;
    }

    destory()
    {
        if(this.worker != null)
        {
            this.worker.terminate();   
            this.worker = null;
        }
    }

    create_frame() : GeometryFrame | null
    {
        const index_buffer = this.gl.createBuffer();

        if(index_buffer == null)
        {
            return null;   
        }

        const vertex_buffer = this.gl.createBuffer();

        if(vertex_buffer == null)
        {
            return null;   
        }

        return new GeometryFrame(index_buffer, vertex_buffer);
    }

    destroy_frame(frame : GeometryFrame)
    {
        this.gl.deleteBuffer(frame.index_buffer);
        this.gl.deleteBuffer(frame.vertex_buffer);
    }

    //Takes ownership of data and the underlying array buffer as well as of the frame
    submit_frame(frame : GeometryFrame, data : Uint8Array)
    {
        const task = new GeometryDecodeTask(frame, data.buffer);
        
        this.worker?.postMessage(task, [task.data, task.frame.indices, task.frame.vertices]);
    }

    set_on_decoded(callback : OnGeometryDecoderDecoded)
    {
        this.on_decoded = callback;
    }

    set_on_error(callback : OnGeometryDecoderError)
    {
        this.on_error = callback;
    }

    private on_worker_response(response : MessageEvent<GeometryDecodeTask>)
    {
        if(this.on_decoded == null)
        {
            return;
        }

        const frame = response.data.frame;

        if(frame.index_buffer_size < frame.indices.byteLength)
        {
            this.gl.bindBuffer(this.gl.ELEMENT_ARRAY_BUFFER, frame.index_buffer);
            this.gl.bufferData(this.gl.ELEMENT_ARRAY_BUFFER, frame.indices, this.gl.STREAM_DRAW);
            this.gl.bindBuffer(this.gl.ELEMENT_ARRAY_BUFFER, 0);
            
            frame.index_buffer_size = frame.indices.byteLength;
        }

        else
        {
            this.gl.bindBuffer(this.gl.ELEMENT_ARRAY_BUFFER, frame.index_buffer);
            this.gl.bufferSubData(this.gl.ELEMENT_ARRAY_BUFFER, 0, frame.indices);
            this.gl.bindBuffer(this.gl.ELEMENT_ARRAY_BUFFER, 0);
        }

        if(frame.vertex_buffer_size < frame.vertices.byteLength)
        {
            this.gl.bindBuffer(this.gl.ARRAY_BUFFER, frame.vertex_buffer);
            this.gl.bufferData(this.gl.ARRAY_BUFFER, frame.vertices, this.gl.STREAM_DRAW);
            this.gl.bindBuffer(this.gl.ARRAY_BUFFER, 0);
            
            frame.vertex_buffer_size = frame.vertices.byteLength;
        }

        else
        {
            this.gl.bindBuffer(this.gl.ARRAY_BUFFER, frame.vertex_buffer);
            this.gl.bufferSubData(this.gl.ARRAY_BUFFER, 0, frame.vertices);
            this.gl.bindBuffer(this.gl.ARRAY_BUFFER, 0);
        }

        this.on_decoded(frame);
    }

    private on_worker_error(error : ErrorEvent)
    {
        if(this.on_error == null)
        {
            return;
        }

        this.on_error();
    }
}