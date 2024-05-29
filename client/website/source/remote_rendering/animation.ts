import { mat4, quat, vec3 } from "gl-matrix";
import { receive_file, send_file } from "./connection";

export class AnimationTransform
{
    src_position : vec3;   //World space position
    dst_transform : mat4;  //View matrix

    constructor(src_position : vec3, dst_transform : mat4)
    {
        this.src_position = src_position;
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
    private projection_matrix = mat4.create();
    private key_frames : AnimationKeyFrame[] = [];
    private time_origin = 0.0;

    constructor()
    {
        mat4.identity(this.projection_matrix);
    }

    async load(animation_path : string) : Promise<boolean>
    {
        const file = await receive_file(animation_path);
        
        if(file.byteLength == 0)
        {
            return false;
        }

        const text_decoder = new TextDecoder;
        const file_string = text_decoder.decode(file);

        const animation = JSON.parse(file_string);

        if(!("projection_matrix" in animation) || !("key_frames" in animation))
        {
            return false;   
        }

        this.projection_matrix = animation.projection_matrix;
        this.key_frames = animation.key_frames;

        if(!Array.isArray(this.key_frames))
        {
            return false;
        }

        for(const key_frame of this.key_frames)
        {
            if(!("time" in key_frame) || !("src_position" in key_frame) || !("dst_position" in key_frame) || !("dst_orientation" in key_frame))
            {
                return false;   
            }   
        }

        return true;
    }

    async store(animation_path : string) : Promise<boolean>
    {
        const animation = 
        {
            projection_matrix: this.projection_matrix,
            key_frames: this.key_frames,
        };

        const animation_string = JSON.stringify(animation);
        
        const text_encoder = new TextEncoder();
        const file_buffer = text_encoder.encode(animation_string);

        await send_file(animation_path, file_buffer);

        return true;
    }

    set_time_origin(time_origin : number)
    {
        this.time_origin = time_origin;
    }

    set_projection_matrix(projection_matrix : mat4)
    {
        this.projection_matrix = projection_matrix;
    }

    add_transform(transform : AnimationTransform)
    {
        const time = performance.now() - this.time_origin;

        const dst_position = vec3.create();
        mat4.getTranslation(dst_position, transform.dst_transform);

        const dst_orientation = quat.create();
        mat4.getRotation(dst_orientation, transform.dst_transform);

        const key_frame : AnimationKeyFrame =
        {
            time,
            src_position: transform.src_position,
            dst_position,
            dst_orientation
        }

        this.key_frames.push(key_frame);
    }

    get_transform() : AnimationTransform
    {
        if(this.key_frames.length == 0)
        {
            const src_position = vec3.create();
            const dst_transform = mat4.create();
            vec3.zero(src_position);
            mat4.identity(dst_transform);

            return new AnimationTransform(src_position, dst_transform);
        }

        const time = performance.now() - this.time_origin;
        let key_frame1 = this.key_frames[0];
        let key_frame2 = this.key_frames[0];

        for(const key_frame of this.key_frames)
        {
            if(key_frame.time > time)
            {
                break;
            }   

            key_frame1 = key_frame2;
            key_frame2 = key_frame;
        }

        let weight = (time - key_frame1.time) / (key_frame2.time - key_frame1.time);
        weight = Math.min(Math.max(weight, 0.0), 1.0);

        const src_position = vec3.create();
        vec3.lerp(src_position, key_frame1.src_position, key_frame2.src_position, weight);

        const dst_position = vec3.create();
        vec3.lerp(dst_position, key_frame1.dst_position, key_frame2.dst_position, weight);

        const dst_orientation = quat.create();
        quat.slerp(dst_orientation, key_frame1.dst_orientation, key_frame2.dst_orientation, weight);

        const dst_transform = mat4.create();
        mat4.fromRotationTranslation(dst_transform, dst_orientation, dst_position);

        return new AnimationTransform(src_position, dst_transform);
    }

    get_transform_at(index : number) : AnimationTransform | null
    {
        if(index >= this.key_frames.length)
        {
            return null;   
        }

        const key_frame = this.key_frames[index];

        const dst_transform = mat4.create();
        mat4.fromRotationTranslation(dst_transform, key_frame.dst_orientation, key_frame.dst_position);

        return new AnimationTransform(key_frame.src_position, dst_transform);
    }

    get_frame_count() : number
    {
        return this.key_frames.length;
    }

    get_time_origin() : number
    {
        return this.time_origin;
    }

    get_projection_matrix() : mat4
    {
        return this.projection_matrix;
    }

    has_finished() : boolean
    {
        const time = performance.now() - this.time_origin;
        const key_frame = this.key_frames.at(-1);

        if(key_frame == null)
        {
            return true;
        }

        return key_frame.time < time;
    }
}