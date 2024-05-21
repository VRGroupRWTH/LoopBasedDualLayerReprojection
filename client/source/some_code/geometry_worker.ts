import * as Wrapper from "../../wrapper/binary/wrapper";

type WrapperModule = Wrapper.MainModule;

class GeometryWorker
{
    private wrapper : WrapperModule;
    
    constructor(wrapper : WrapperModule)
    {
        this.wrapper = wrapper;

        addEventListener("message", this.on_decode_request);
    }

    on_decode_request(event : MessageEvent<any>)
    {
        const 

        this.wrapper.decode_geoemtry()
    }
}

const wrapper = await Wrapper.default();
const worker = new GeometryWorker(wrapper);