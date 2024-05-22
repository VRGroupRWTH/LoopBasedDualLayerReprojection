import { Component, Match, Show, Switch, createEffect, createSignal, onCleanup } from "solid-js";
import { Button } from "solid-bootstrap";
import { Session, SessionConfig } from "./session";
import { WrapperModule } from "./wrapper";
import { Display, DisplayType } from "./display";
import { vec3 } from "gl-matrix";

export type OnRemoteRenderingClose = () => void;

export interface RemoteRenderingProps
{
    wrapper : WrapperModule,
    config : SessionConfig,
    preferred_display : DisplayType,
    on_close : OnRemoteRenderingClose
}

enum CalibrationState
{
    Origin,
    SideX
}

class Calibration
{
    state : CalibrationState = CalibrationState.Origin;

    display : Display;
    origin : vec3 = vec3.create();
    side_x : vec3 = vec3.create();

    constructor(display : Display)
    {
        this.display = display;
    }
}

const RemoteRendering : Component<RemoteRenderingProps> = (props) =>
{
    let [canvas, set_canvas] = createSignal<HTMLCanvasElement>();
    let [session, set_session] = createSignal<Session>();
    let [calibration, set_calibration] = createSignal<Calibration>();

    function on_session_calibrate(display : Display)
    {
        set_calibration(new Calibration(display));
    }

    function on_session_close()
    {
        props.on_close();
    }

    function on_calibrate_origin()
    {
        let current_calibration = calibration();

        if(current_calibration == undefined)
        {
            return;   
        }

        current_calibration.origin = current_calibration.display.get_position();
        current_calibration.state = CalibrationState.SideX;

        console.log("asdasd");

        set_calibration(current_calibration);
    }

    function on_calibrate_side_x()
    {
        let current_calibration = calibration();

        if(current_calibration == undefined)
        {
            return;   
        }

        current_calibration.side_x = current_calibration.display.get_position();
        current_calibration.display.calibrate(current_calibration.origin, current_calibration.side_x);

        set_calibration(undefined);
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
            <Show when={calibration() != undefined}>
                <div style="position: absolute; width: 100%; height: 100%; left: 0px; top: 0px; z-index: 200">
                    <Switch>
                        <Match when={calibration()?.state == CalibrationState.Origin}>
                            Calibrate Origin
                            <Button onClick={on_calibrate_origin}>Enter</Button>
                        </Match>
                        <Match when={calibration()?.state == CalibrationState.SideX}>
                            Calibrate Side X
                            <Button onClick={on_calibrate_side_x}>Enter</Button>
                        </Match>
                    </Switch>
                </div>
            </Show>
        </div>
    );
}

export default RemoteRendering;
export * from "./wrapper";
export * from "./session";