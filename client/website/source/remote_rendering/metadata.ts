import { send_file } from "./connection";
import { Layer } from "./frame";
import { CountArray, ViewMetadataArray } from "./wrapper";

interface MetadataRequest
{
    request_id: number;
    time_point_request: number;
}

interface MetadataEntry
{
    request_id:  number;

    image_bytes: number;
    geometry_bytes: number;
    vertex_counts: CountArray;
    index_counts: CountArray;

    time_point_request : number;
    time_point_response : number;

    time_image_decode : number;
    time_geometry_decode : number;

    view_metadata: ViewMetadataArray;
}

export class Metadata
{
    private requests : MetadataRequest[] = [];
    private entries : MetadataEntry[] = [];
    private time_origin = 0.0;

    async store(metadata_path : string) : Promise<boolean>
    {
        const metadata_string = JSON.stringify(this.entries);
        
        const text_encoder = new TextEncoder();
        const file_buffer = text_encoder.encode(metadata_string);

        await send_file(metadata_path, file_buffer);

        return true;
    }

    set_time_origin(time_origin : number)
    {
        this.time_origin = time_origin;
    }

    submit_request(request_id : number)
    {
        const request : MetadataRequest= 
        {
            request_id,
            time_point_request: performance.now() - this.time_origin
        }

        this.requests.push(request);
    }

    submit_layer(layer : Layer)
    {
        const form = layer.form;

        if(form == null)
        {
            return;   
        }

        const request_index = this.requests.findIndex(request =>
        {
            return request.request_id == form.request_id;   
        })

        if(request_index == -1)
        {
            return;
        }

        const request = this.requests.slice(request_index, request_index + 1);

        const entry : MetadataEntry = 
        {
            request_id: form.request_id,
            image_bytes: form.image_bytes,
            geometry_bytes: form.geometry_bytes,
            vertex_counts: form.vertex_counts,
            index_counts: form.index_counts,
            time_point_request: request[0].time_point_request,
            time_point_response: layer.time_point_response - this.time_origin,
            time_image_decode: layer.image_frame.decode_end - layer.image_frame.decode_start,
            time_geometry_decode: layer.geometry_frame.decode_end - layer.geometry_frame.decode_start,
            view_metadata: form.view_metadata
        };

        this.entries.push(entry);
    }

    get_time_origin() : number
    {
        return this.time_origin;
    }
}