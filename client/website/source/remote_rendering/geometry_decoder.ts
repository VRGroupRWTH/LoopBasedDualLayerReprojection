import GeometryWorker from "./geometry_worker?worker";
import { log_error } from "./log";

export class GeometryFrame
{
    request_id : number = 0;
    decode_start : number = 0.0;
    deocde_end : number = 0.0;

    indices : Uint8Array = new Uint8Array(0);
    index_buffer : WebGLBuffer;
    index_buffer_size : number = 0;

    vertices : Uint8Array = new Uint8Array(0);
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
    request_id : number;

    data : Uint8Array;
    indices : Uint8Array;
    vertices : Uint8Array;

    constructor(request_id : number, data : Uint8Array, indices : Uint8Array, vertices : Uint8Array)
    {
        this.request_id = request_id;
        this.data = data;
        this.indices = indices;
        this.vertices = vertices;
    }
}

export type OnGeometryDecoderDecoded = (frame : GeometryFrame) => void;
export type OnGeometryDecoderError = () => void; 

export class GeometryDecoder
{
    private gl : WebGL2RenderingContext;
    private worker : Worker | null = null;

    private frame_queue : GeometryFrame[] = [];

    private on_decoded : OnGeometryDecoderDecoded | null = null;
    private on_error : OnGeometryDecoderError | null = null;

    constructor(gl : WebGL2RenderingContext)
    {
        this.gl = gl;
    }

    create() : boolean
    {
        this.worker = new GeometryWorker() as Worker;
        this.worker.onmessage = this.on_worker_response.bind(this);
        this.worker.onerror = this.on_worker_error.bind(this);

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
    submit_frame(frame : GeometryFrame, data : Uint8Array) : boolean
    {
        if(this.worker == null)
        {
            return false;  
        }

        const task = new GeometryDecodeTask(frame.request_id, data, frame.indices, frame.vertices);
        this.worker.postMessage(task, [task.data.buffer, task.indices.buffer, task.vertices.buffer]);

        this.frame_queue.push(frame);

        return true;
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

        const task = response.data;

        let frame_index = this.frame_queue.findIndex(frame =>
        {
            return frame.request_id == task.request_id;
        });

        if(frame_index == -1)
        {
            log_error("[Geometry Decoder] Can't find geometry frame in frame queue!");

            return;
        }

        const frame = this.frame_queue.splice(frame_index, 1)[0];
        frame.indices = task.indices;
        frame.vertices = task.vertices;

        if(frame.index_buffer_size < frame.indices.byteLength)
        {
            this.gl.bindBuffer(this.gl.ELEMENT_ARRAY_BUFFER, frame.index_buffer);
            this.gl.bufferData(this.gl.ELEMENT_ARRAY_BUFFER, frame.indices, this.gl.STREAM_DRAW);
            this.gl.bindBuffer(this.gl.ELEMENT_ARRAY_BUFFER, null);
            
            frame.index_buffer_size = frame.indices.byteLength;
        }

        else
        {
            this.gl.bindBuffer(this.gl.ELEMENT_ARRAY_BUFFER, frame.index_buffer);
            this.gl.bufferSubData(this.gl.ELEMENT_ARRAY_BUFFER, 0, frame.indices);
            this.gl.bindBuffer(this.gl.ELEMENT_ARRAY_BUFFER, null);
        }

        if(frame.vertex_buffer_size < frame.vertices.byteLength)
        {
            this.gl.bindBuffer(this.gl.ARRAY_BUFFER, frame.vertex_buffer);
            this.gl.bufferData(this.gl.ARRAY_BUFFER, frame.vertices, this.gl.STREAM_DRAW);
            this.gl.bindBuffer(this.gl.ARRAY_BUFFER, null);
            
            frame.vertex_buffer_size = frame.vertices.byteLength;
        }

        else
        {
            this.gl.bindBuffer(this.gl.ARRAY_BUFFER, frame.vertex_buffer);
            this.gl.bufferSubData(this.gl.ARRAY_BUFFER, 0, frame.vertices);
            this.gl.bindBuffer(this.gl.ARRAY_BUFFER, null);
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