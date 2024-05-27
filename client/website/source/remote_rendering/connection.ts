import { SessionCreateForm, SessionDestroyForm, RenderRequestForm, MeshSettingsForm, VideoSettingsForm, LayerResponseForm, WrapperModule } from "./wrapper";

export async function receive_scenes() : Promise<string[]>
{
    let response = await fetch("/server/scenes",
    {
        method: "GET"   
    });

    let scene_list = response.json().catch(error =>
    {
        return []; 
    });

    return scene_list;
}

export async function receive_directory_files(directory_path : string) : Promise<string[]>
{
    let response = await fetch("/server/files/" + directory_path, 
    {
        method: "GET" 
    });

    let file_list = response.json().catch(error =>
    {
        return [];
    });

    return file_list;
}

export async function receive_file(file_path : string) : Promise<Uint8Array>
{
    let response = await fetch("/server/files/" + file_path,
    {
        method: "GET"
    });

    let file_content = await response.arrayBuffer().catch(error =>
    {
        return new ArrayBuffer(0); 
    });

    return new Uint8Array(file_content);
}

export async function send_log(file_path : string, content : Uint8Array)
{
    fetch("/server/files/" + file_path + "?type=log",
    {
        method: "POST",
        body: content
    });
}

export async function send_file(file_path : string, content : Uint8Array)
{
    fetch("/server/files/" + file_path + "?type=file",
    {
        method: "POST",
        body: content
    });
}

export async function send_image(file_path : string, width : number, height : number, content : Uint8Array)
{
    let body = new Uint8Array(8 + content.byteLength);
    body.set(content, 8);

    let dimensions = new Uint32Array(body.buffer, 0, 2);
    dimensions[0] = width;
    dimensions[1] = height;
    
    fetch("/server/files/" + file_path + "?type=image",
    {
        method: "POST",
        body
    });
}

export type OnConnectionOpen = () => void;
export type OnConnectionLayerResponse = (form : LayerResponseForm, geometry_data : Uint8Array, image_data : Uint8Array) => void;
export type OnConnectionClose = () => void;

export class Connection
{
    private wrapper : WrapperModule;
    private socket : WebSocket | null = null;

    private on_open : OnConnectionOpen | null = null;
    private on_layer_response : OnConnectionLayerResponse | null = null;
    private on_close : OnConnectionClose | null = null;

    constructor(wrapper : WrapperModule)
    {
        this.wrapper = wrapper;
    }

    create() : boolean
    {
        let port = "";

        if(window.location.port != "")
        {
            port = ":" + window.location.port;
        }

        let url = "wss://" + window.location.hostname + port + "/server-socket";

        this.socket = new WebSocket(url);
        this.socket.binaryType = "arraybuffer";
        this.socket.onmessage = this.on_packet.bind(this);

        this.socket.onopen = () =>
        {
            if(this.on_open != null)
            {
               this.on_open(); 
            }  
        };

        this.socket.onclose = () =>
        {
            if(this.on_close != null)
            {
                this.on_close();   
            }
        };

        this.socket.onerror = (event : Event) => 
        {
            if(this.on_close != null)
            {
                this.on_close();   
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

        this.on_open = null;
        this.on_layer_response = null;
        this.on_close = null;
    }

    send_session_create(form : SessionCreateForm) : boolean
    {
        let packet = this.wrapper.build_session_create_packet(form);

        return this.send_packet(packet);
    }

    send_session_destory(form : SessionDestroyForm) : boolean
    {
        let packet = this.wrapper.build_session_destroy_packet(form);

        return this.send_packet(packet);
    }

    send_render_request(form : RenderRequestForm) : boolean
    {
        let packet = this.wrapper.build_render_request_packet(form);

        return this.send_packet(packet);
    }

    send_mesh_settings(form : MeshSettingsForm) : boolean
    {
        let packet = this.wrapper.build_mesh_settings_packet(form);

        return this.send_packet(packet);
    }

    send_video_settings(form : VideoSettingsForm) : boolean
    {
        let packet = this.wrapper.build_video_settings_packet(form);

        return this.send_packet(packet);
    }

    set_on_open(callback : OnConnectionOpen)
    {
        this.on_open = callback;
    }

    set_on_layer_response(callback : OnConnectionLayerResponse)
    {
        this.on_layer_response = callback;   
    }

    set_on_close(callback : OnConnectionClose)
    {
        this.on_close = callback;
    }

    private send_packet(packet : Uint8Array) : boolean
    {
        if(this.socket == null || this.socket.readyState != this.socket.OPEN)
        {
            return false;   
        }

        this.socket.send(packet);

        return true;
    }

    private on_packet(event : MessageEvent<any>)
    {
        if(!(event.data instanceof ArrayBuffer))
        {
            this.on_close?.();

            return;   
        }

        const data = new Uint8Array(event.data);

        if(data.byteLength < 4)
        {
            this.on_close?.();

            return;
        }

        const packet_type = this.wrapper.parse_packet_type(data);

        switch(packet_type)
        {
        case this.wrapper.PacketType.PACKET_TYPE_LAYER_RESPONSE:
            this.on_layer_response_internal(data);
            break;
        default:
            this.on_close?.();
            break;
        }
    }

    private on_layer_response_internal(data : Uint8Array)
    {
        let form = this.wrapper.parse_layer_response_packet(data);

        const image_offset = data.length - form.image_bytes;
        const geometry_offset = image_offset - form.geometry_bytes;

        const image_data = data.subarray(image_offset, image_offset + form.image_bytes);
        const geometry_data = data.subarray(geometry_offset, geometry_offset + form.geometry_bytes)

        this.on_layer_response?.(form, geometry_data, image_data);
    }
}