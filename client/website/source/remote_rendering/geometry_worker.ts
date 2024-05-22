import * as Wrapper from "../../wrapper/binary/wrapper";
import { GeometryDecodeTask } from "./geometry_decoder";

type WrapperModule = Wrapper.MainModule;

class GeometryWorker
{
    private wrapper : WrapperModule;
    
    constructor(wrapper : WrapperModule)
    {
        this.wrapper = wrapper;

        addEventListener("message", this.on_decode_task);
    }

    on_decode_task(event : MessageEvent<GeometryDecodeTask>)
    {
        const data = new Uint8Array(event.data.data);
        const indices = new Uint8Array(event.data.frame.indices);
        const vertices = new Uint8Array(event.data.frame.vertices);

        if(!this.wrapper.decode_geoemtry(data, indices, vertices))
        {
            throw new Error("Can't decode geometry!");
        }

        postMessage(event.data, [event.data.data, event.data.frame.indices, event.data.frame.vertices]);
    }
}

const wrapper = await Wrapper.default();
const worker = new GeometryWorker(wrapper);