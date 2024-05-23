import { mat4, quat, vec3 } from "gl-matrix";
import { receive_file } from "./connection";

export class AnimationTransform
{
    src_transform : mat4;
    dst_transform : mat4;

    constructor(src_transform : mat4, dst_transform : mat4)
    {
        this.src_transform = src_transform;
        this.dst_transform = dst_transform;
    }
}

interface AnimationKeyFrame
{
    time : number,
    src_position : vec3,
    dst_position : vec3,
    dst_orientation : quat
}

export class Animation
{
    private key_frames : AnimationKeyFrame[] = [];
    private start_time = 0.0;

    async load(animation_path : string) : Promise<boolean>
    {
        const file = await receive_file(animation_path);
        
        const text_encoder = new TextDecoder;
        const file_string = text_encoder.decode(file);



        return true;
    }

    store(animation_path : string) : boolean
    {

    }

    set_transform(transform : AnimationTransform)
    {
        const key_frame : AnimationKeyFrame
        {
            
        };
    }

    get_transform() : AnimationTransform
    {

    }

    is_complete() : boolean
    {

    }
}