import { MainModule as Wrapper } from "../../wrapper/binary/wrapper";

export async function receive_scenes() : Promise<string[]>
{
    return [];
}

export async function receive_directory_files(directory_path : string) : Promise<string[]>
{
    return [];
}

export async function receive_file(file_path : string) : Promise<Uint8Array>
{
    return new Uint8Array();
}

export async function send_log(file_path : string, content : string)
{

}

export async function send_image(file_path : string, width : number, height : number, content : Uint8Array)
{

}
    
export type OnConnectionLayerResponse = () => void;
export type OnConnectionClose = () => void;
export type OnConnectionError = () => void;    

export class Connection
{
    private wrapper : Wrapper;
    private socket : WebSocket | null = null;

    private on_layer_response : OnConnectionLayerResponse | null = null;
    private on_close : OnConnectionClose | null = null;
    private on_error : OnConnectionError | null = null;

    constructor(wrapper : Wrapper)
    {
        this.wrapper = wrapper;
    }

    create(url : string) : boolean
    {
        this.socket = new WebSocket(url);
        this.socket.binaryType = "arraybuffer";

        

        this.socket.onmessage = this.on_message;

        this.socket.onclose = () =>
        {
            if(this.on_close != null)
            {
                this.on_close();   
            }
        };

        this.socket.onerror = () => 
        {
            if(this.on_error != null)
            {
                this.on_error();
            }   
        }
        



        return true;
    }

    destroy() : void
    {
        if(this.socket != null)
        {
            this.socket.close();
            this.socket = null;
        }
    }

    send_session_create()
    {

    }

    send_session_destory()
    {

    }

    send_mesh_settings()
    {

    }

    send_video_settings()
    {

    }

    send_render_request()
    {

    }

    set_on_layer_response(callback : OnConnectionLayerResponse)
    {
        this.on_layer_response = callback;   
    }

    set_on_close(callback : OnConnectionClose)
    {
        this.on_close = callback;
    }

    set_on_error(callback : OnConnectionError)
    {
        this.on_error = callback;
    }

    private on_message(event : MessageEvent<any>)
    {

    }
}