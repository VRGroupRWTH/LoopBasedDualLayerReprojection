import { mat4, quat, vec3 } from "gl-matrix";
import { receive_file, send_log } from "./connection";

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
        
        if(file.byteLength == 0)
        {
            return false;   
        }

        const text_decoder = new TextDecoder;
        const file_string = text_decoder.decode(file);

        this.key_frames = JSON.parse(file_string);

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
        const key_frame_string = JSON.stringify(this.key_frames);
        
        const text_encoder = new TextEncoder();
        const file_buffer = text_encoder.encode(key_frame_string);

        await send_log(animation_path, file_buffer);

        return true;
    }

    reset()
    {
        this.start_time = performance.now();
    }

    add_transform(transform : AnimationTransform)
    {
        const time = performance.now() - this.start_time;

        const src_position = vec3.create();
        mat4.getTranslation(src_position, transform.src_transform);

        const dst_position = vec3.create();
        mat4.getTranslation(dst_position, transform.dst_transform);

        const dst_orientation = quat.create();
        mat4.getRotation(dst_orientation, transform.dst_transform);

        const key_frame : AnimationKeyFrame =
        {
            time,
            src_position,
            dst_position,
            dst_orientation
        }

        this.key_frames.push(key_frame);
    }

    get_transform() : AnimationTransform
    {
        if(this.key_frames.length == 0)
        {
            const src_transform = mat4.create();
            const dst_transform = mat4.create();
            mat4.identity(src_transform);
            mat4.identity(dst_transform);

            return new AnimationTransform(src_transform, dst_transform);
        }

        const time = performance.now() - this.start_time;
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

        const src_transform = mat4.create();
        mat4.fromTranslation(src_transform, src_position);

        const dst_transform = mat4.create();
        mat4.fromRotationTranslation(dst_transform, dst_orientation, dst_position);

        return new AnimationTransform(src_transform, dst_transform);
    }

    has_finished() : boolean
    {
        const time = performance.now() - this.start_time;
        const key_frame = this.key_frames.at(-1);

        if(key_frame == null)
        {
            return true;
        }

        return key_frame.time < time;
    }
}