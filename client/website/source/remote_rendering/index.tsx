import { Component, createEffect, createSignal, onCleanup, onMount } from "solid-js";
import { Session, SessionConfig } from "./session";
import { WrapperModule } from "./wrapper";
import { DisplayType } from "./display";

export interface RemoteRenderingProps
{
    wrapper : WrapperModule,
    config : SessionConfig,
    preferred_display : DisplayType,
    register_reset : (reset : () => void) => void;
    on_close : () => void
}

const RemoteRendering : Component<RemoteRenderingProps> = (props) =>
{
    let [canvas, set_canvas] = createSignal<HTMLCanvasElement>();
    let [session, set_session] = createSignal<Session>();

    async function on_open()
    {
        const canvas_element = canvas();

        if(canvas_element == undefined)
        {
            return;
        }

        let session = new Session(props.config, props.wrapper, canvas_element);
        session.set_on_close(on_close);

        if(!await session.create(props.preferred_display))
        {
            return;
        }

        set_session(session);
    }

    function on_close()
    {
        session()?.destroy();

        set_canvas(undefined);
        set_session(undefined);

        props.on_close();
    }

    function on_reset()
    {
        on_close();
        on_open();
    }

    createEffect(async () => 
    {
        on_open();
    });

    onCleanup(() =>
    {
        on_close();
    });

    props.register_reset(on_reset);

    return (
        <canvas ref={set_canvas} tabIndex={1000} style="outline: none; position: absolute; width: 100%; height: 100%; left: 0px; top: 0px; z-index: 100"/>        
    );
}

export default RemoteRendering;
export * from "./wrapper";
export * from "./session";
export { DisplayType } from "./display";
export { receive_scenes, receive_directory_files, receive_file } from "./connection";
export { log_debug, log_info, log_error } from "./log";