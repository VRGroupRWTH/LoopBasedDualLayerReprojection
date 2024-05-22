import { Component, createEffect, createSignal, onCleanup } from "solid-js";
import { Session, SessionConfig } from "./session";
import { WrapperModule } from "./wrapper";
import { Display, DisplayType } from "./display";

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

    function on_session_calibrate(display : Display)
    {

    }

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
        session.set_on_calibrate(on_session_calibrate);
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
        <div style="position: relative; width: 100%; height: 100%">
            <canvas ref={set_canvas} style="position: absolute; width: 100%; height: 100%; left: 0px; top: 0px; z-index: 100"/>
            <div style="position: absolute; width: 100%; height: 100%; left: 0px; top: 0px; z-index: 200">
                Overlay
            </div>
        </div>
    );
}

export default RemoteRendering;
export * from "./wrapper";
export * from "./session";