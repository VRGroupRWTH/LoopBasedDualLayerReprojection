import { send_file } from "./connection";
import { log_error } from "./log";

interface RenderTimeEntry
{
    request_id:  number;

    time_point_begin: number;
    time_point_end: number;
    time: number;
}

export class RenderTime
{
    private gl : WebGL2RenderingContext;
    private entries : RenderTimeEntry[] = [];
    private promises : Promise<void>[] = [];
    private sample_count = 0.0;
    private time_origin = 0.0;

    constructor(gl : WebGL2RenderingContext, sample_count : number)
    {
        this.gl = gl;
        this.sample_count = sample_count;
    }

    async store(time_path : string) : Promise<boolean>
    {
        for(const promise of this.promises)
        {
            await promise;
        }

        const entry_string = JSON.stringify(this.entries);
        
        const text_encoder = new TextEncoder();
        const file_buffer = text_encoder.encode(entry_string);

        await send_file(time_path, file_buffer);

        return true;
    }

    set_time_origin(time_origin : number)
    {
        this.time_origin = time_origin;
    }

    begin(request_id : number)
    {
        const entry : RenderTimeEntry = 
        {
            request_id,
            time_point_begin: 0.0,
            time_point_end: 0.0,
            time: 0.0
        };

        this.entries.push(entry);

        const promise = this.wait(time_point =>
        {
            let index = this.entries.findIndex(entry =>
            {
                return entry.request_id == request_id;
            });

            let entry = this.entries[index];
            entry.time_point_begin = time_point - this.time_origin;
            entry.time = (entry.time_point_end - entry.time_point_begin) / this.sample_count;
        });

        this.promises.push(promise);
    }

    end(request_id : number)
    {
        const promise = this.wait(time_point =>
        {
            let index = this.entries.findIndex(entry =>
            {
                return entry.request_id == request_id;
            });

            let entry = this.entries[index];
            entry.time_point_end = time_point - this.time_origin;
            entry.time = (entry.time_point_end - entry.time_point_begin) / this.sample_count;
        });

        this.promises.push(promise);
    }

    private wait(callback : (time_point : number) => void) : Promise<void>
    {
        const fence = this.gl.fenceSync(this.gl.SYNC_GPU_COMMANDS_COMPLETE, 0);

        if(fence == null)
        {
            callback(performance.now());

            return Promise.resolve();
        }

        this.gl.flush();

        return new Promise(resolve => 
        {
            const check = () =>
            {
                const status = this.gl.clientWaitSync(fence, 0, 0);
    
                if(status == this.gl.ALREADY_SIGNALED || status == this.gl.CONDITION_SATISFIED)
                {
                    callback(performance.now());
                }
    
                else if(status == this.gl.TIMEOUT_EXPIRED)
                {
                    return setTimeout(check);
                }
    
                else if(status == this.gl.WAIT_FAILED)
                {
                    log_error("[RenderTime] Wait falied!");
                }
    
                this.gl.deleteSync(fence);
                resolve();
            }
    
            setTimeout(check);
        });
    }
}