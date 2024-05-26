import { Component, createEffect, createSignal, onCleanup } from "solid-js";
import { Session, SessionConfig } from "./session";
import { WrapperModule } from "./wrapper";
import { DisplayType } from "./display";

export type OnRemoteRenderingClose = () => void;

export interface RemoteRenderingProps
{
    wrapper : WrapperModule,
    config : SessionConfig,
    preferred_display : DisplayType,
    on_close : OnRemoteRenderingClose
}

const RemoteRendering : Component<RemoteRenderingProps> = (props) =>
{
    let [canvas, set_canvas] = createSignal<HTMLCanvasElement>();
    let [session, set_session] = createSignal<Session>();

    function on_session_close()
    {
        props.on_close();
    }

    createEffect(async () => 
    {
        const canvas_element = canvas();

        if(canvas_element == undefined)
        {
            return;
        }

        let session = new Session(props.config, props.wrapper, canvas_element);
        session.set_on_close(on_session_close);

        if(!await session.create(props.preferred_display))
        {
            //Show Error

            return;
        }

        set_session(session);
    });
    
    onCleanup(() =>
    {

    });

    return (
        <canvas ref={set_canvas} tabIndex={1000} style="outline: none; position: absolute; width: 100%; height: 100%; left: 0px; top: 0px; z-index: 100"/>        
    );
}

export default RemoteRendering;
export * from "./wrapper";
export * from "./session";