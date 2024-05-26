import { GeometryDecodeTask } from "./geometry_decoder";
import { WrapperModule, load_wrapper_module } from "./wrapper";

class GeometryWorker
{
    private wrapper : WrapperModule;
    
    constructor(wrapper : WrapperModule)
    {
        this.wrapper = wrapper;

        addEventListener("message", this.on_decode_task.bind(this));
    }

    on_decode_task(event : MessageEvent<GeometryDecodeTask>)
    {
        const data = event.data.data;
        const indices = event.data.indices;
        const vertices = event.data.vertices;

        const geometry = this.wrapper.decode_geoemtry(data, indices, vertices);

        if(geometry == undefined)
        {
            throw new Error("Can't decode geometry!");
        }

        event.data.indices = geometry.indices;
        event.data.vertices = geometry.vertices;

        postMessage(event.data, [data.buffer, indices.buffer, vertices.buffer]);
    }
}

const wrapper = await load_wrapper_module();
const worker = new GeometryWorker(wrapper);